#include "VulkanMgr.hpp"
#include "MemoryManager.hpp"
#include <mutex>
#include <sstream>

MemoryManager::MemoryManager(VulkanMgr &master, uint32_t chunkSize) : master(master), refDevice(master.refDevice), chunkSize(chunkSize)
{
    memBudjet.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
    memBudjet.pNext = nullptr;
    memProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    memProperties.pNext = &memBudjet;
    displayResources();
}

MemoryManager::~MemoryManager()
{
    vkDeviceWaitIdle(refDevice);
    for (auto &memTypes : memory) {
        for (auto &mem : memTypes.memoryChunks) {
            vkFreeMemory(refDevice, mem, nullptr);
        }
    }
}

SubMemory MemoryManager::malloc(const VkMemoryRequirements &memRequirements, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags preferedProperties)
{
    SubMemory subMemory;
    subMemory.memory = VK_NULL_HANDLE;
    findMemoryIndex(memRequirements, properties, preferedProperties, &subMemory);
    mtx.lock();
    if (subMemory.memoryIndex != UINT32_MAX) // If false, there is no compatible memory type
        acquireSubMemory(memRequirements, &subMemory);
    if (subMemory.memory != VK_NULL_HANDLE) // If false, there is no memory acquired
        allocateInSubMemory(memRequirements, &subMemory);
    mtx.unlock();
    return subMemory;
}

SubMemory MemoryManager::dmalloc(const VkMemoryRequirements &memRequirements, VkImage image, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags preferedProperties)
{
    SubMemory subMemory;
    subMemory.memory = VK_NULL_HANDLE;
    findMemoryIndex(memRequirements, properties, preferedProperties, &subMemory);
    mtx.lock();
    if (availableDeviceMemory <= 64 + chunkSize / 1024 / 1024 && !hasReleasedUnusedMemory && memProperties.memoryProperties.memoryTypes[subMemory.memoryIndex].heapIndex == deviceMemoryHeap) {
        master.releaseUnusedMemory();
        hasReleasedUnusedMemory = true;
        displayResources();
    }
    VkMemoryDedicatedAllocateInfo dedicatedInfo{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, nullptr, image, VK_NULL_HANDLE};
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &dedicatedInfo, memRequirements.size, subMemory.memoryIndex};
    std::ostringstream oss;
    if (vkAllocateMemory(refDevice, &allocInfo, nullptr, &subMemory.memory) == VK_SUCCESS) {
        oss << "Dedicated allocation of " << memRequirements.size / 1024 / 1024 << " MiB of GPU memory.";
        master.putLog(oss.str(), LogType::DEBUG);
        subMemory.offset = 0;
        subMemory.size = (VkDeviceSize) -1;
    } else {
        oss << "Failed dedicated allocation of " << memRequirements.size / 1024 / 1024 << " MiB of GPU memory.";
        master.putLog(oss.str(), LogType::ERROR);
    }
    if (!hasReleasedUnusedMemory)
        displayResources();
    mtx.unlock();
    return subMemory;
}

void MemoryManager::free(SubMemory &subMemory)
{
    if (subMemory.memory == VK_NULL_HANDLE) return;
    mtx.lock();
    if (subMemory.size == (VkDeviceSize) -1) {
        vkFreeMemory(refDevice, subMemory.memory, nullptr);
    } else {
        merge(&subMemory);
        insert(subMemory);
    }
    mtx.unlock();
}

void MemoryManager::mapMemory(SubMemory &subMemory, void **data)
{
    mtx.lock();
    MappedMemory &mapmem = mappedMemory[subMemory.memory];
    if (mapmem.nbMapping++ == 0) {
        if (vkMapMemory(refDevice, subMemory.memory, 0, VK_WHOLE_SIZE, 0, &mapmem.data) != VK_SUCCESS)
            throw std::runtime_error("Faild to map memory");
    }
    mtx.unlock();
    *data = static_cast<char *>(mapmem.data) + subMemory.offset; // static_cast for GCC
}

void MemoryManager::unmapMemory(SubMemory &subMemory)
{
    mtx.lock();
    MappedMemory &mapmem = mappedMemory[subMemory.memory];
    if (mapmem.nbMapping-- == 1) {
        vkUnmapMemory(refDevice, subMemory.memory);
    }
    mtx.unlock();
}

void MemoryManager::findMemoryIndex(const VkMemoryRequirements &memRequirements, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags preferedProperties, SubMemory *subMemory)
{
    preferedProperties |= properties;

    for (uint32_t i = 0; i < memProperties.memoryProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) && (memProperties.memoryProperties.memoryTypes[i].propertyFlags & preferedProperties) == preferedProperties) {
            subMemory->memoryIndex = i;
            return;
        }
    }

    for (uint32_t i = 0; i < memProperties.memoryProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) && (memProperties.memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            subMemory->memoryIndex = i;
            return;
        }
    }
    subMemory->memoryIndex = UINT32_MAX;
}

void MemoryManager::acquireSubMemory(const VkMemoryRequirements &memRequirements, SubMemory *subMemory)
{
    const auto itEnd = memory[subMemory->memoryIndex].availableSpaces.end();
    for (auto it = memory[subMemory->memoryIndex].availableSpaces.begin(); it != itEnd; ++it) {
        if (memRequirements.size <= it->size
            && (it->offset % memRequirements.alignment == 0
                || memRequirements.size + memRequirements.alignment
                - (it->offset % memRequirements.alignment) <= it->size)) {
            *subMemory = *it;
            memory[subMemory->memoryIndex].availableSpaces.erase(it);
            break;
        }
    }
    if (subMemory->memory == VK_NULL_HANDLE) {
        if (memProperties.memoryProperties.memoryTypes[subMemory->memoryIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            // Host visible memory should be allocated separately in order to allow concurrent access to different buffer
            *subMemory = allocateChunk(subMemory->memoryIndex, memRequirements.size);
            return;
        }
        if (!hasReleasedUnusedMemory && availableDeviceMemory <= 64 + chunkSize / 1024 / 1024 && memProperties.memoryProperties.memoryTypes[subMemory->memoryIndex].heapIndex == deviceMemoryHeap) {
            master.releaseUnusedMemory();
            hasReleasedUnusedMemory = true;
            displayResources();
            acquireSubMemory(memRequirements, subMemory);
        } else {
            *subMemory = allocateChunk(subMemory->memoryIndex);
        }
    }
}

void MemoryManager::allocateInSubMemory(const VkMemoryRequirements &memRequirements, SubMemory *subMemory)
{
    SubMemory tmp = *subMemory;
    tmp.size = tmp.offset % memRequirements.alignment;
    if (tmp.size > 0) {
        // release unused space below this one
        tmp.size = memRequirements.alignment - tmp.size;
        subMemory->offset += tmp.size;
        subMemory->size -= tmp.size;
        insert(tmp);
    }
    // release unused space after this one
    tmp.offset = subMemory->offset + memRequirements.size;
    tmp.size = subMemory->size - memRequirements.size;
    subMemory->size = memRequirements.size;
    if (tmp.size > 0)
        insert(tmp);
}

void MemoryManager::insert(SubMemory &subMemory)
{
    const auto itEnd = memory[subMemory.memoryIndex].availableSpaces.end();
    for (auto it = memory[subMemory.memoryIndex].availableSpaces.begin(); it != itEnd; ++it) {
        if (it->size >= subMemory.size) {
            memory[subMemory.memoryIndex].availableSpaces.insert(it, subMemory);
            return;
        }
    }
    memory[subMemory.memoryIndex].availableSpaces.push_back(subMemory);
}

void MemoryManager::merge(SubMemory *subMemory)
{
    VkDeviceSize memBegin = subMemory->offset;
    VkDeviceSize memEnd = memBegin + subMemory->size;

    const auto itEnd = memory[subMemory->memoryIndex].availableSpaces.end();
    for (auto it = memory[subMemory->memoryIndex].availableSpaces.begin(); it != itEnd; ++it) {
        if (it->memory != subMemory->memory)
            continue;
        if (it->offset == memEnd) {
            subMemory->size += it->size;
        } else if (it->offset + it->size == memBegin) {
            subMemory->offset = it->offset;
            subMemory->size += it->size;
        } else {
            continue;
        }
        if (it == memory[subMemory->memoryIndex].availableSpaces.begin()) {
            memory[subMemory->memoryIndex].availableSpaces.erase(it);
            it = memory[subMemory->memoryIndex].availableSpaces.begin();
        } else {
            auto tmpIt = it;
            --it;
            memory[subMemory->memoryIndex].availableSpaces.erase(tmpIt);
        }
    }
}

SubMemory MemoryManager::allocateChunk(uint32_t memoryIndex, uint32_t specificChunkSize)
{
    SubMemory subMemory;
    subMemory.offset = 0;
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = subMemory.size = (specificChunkSize == UINT32_MAX) ? chunkSize : specificChunkSize;
    allocInfo.memoryTypeIndex = subMemory.memoryIndex = memoryIndex;

    std::ostringstream oss;
    std::string heapType = (memProperties.memoryProperties.memoryHeaps[memProperties.memoryProperties.memoryTypes[memoryIndex].heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? "GPU" : "local";

    if (vkAllocateMemory(refDevice, &allocInfo, nullptr, &subMemory.memory) == VK_SUCCESS) {
        oss << "Allocate chunk of " << subMemory.size / 1024 / 1024 << " MiB in " << heapType << " memory.";
        master.putLog(oss.str(), LogType::DEBUG);
        memory[memoryIndex].memoryChunks.push_back(subMemory.memory);
    } else {
        oss << "Failed to allocate chunk of " << subMemory.size / 1024 / 1024 << " MiB in " << heapType << " memory.";
        master.putLog(oss.str(),  LogType::ERROR);
        subMemory.memory = VK_NULL_HANDLE;
    }

    displayResources();
    return subMemory;
}

void MemoryManager::displayResources()
{
    vkGetPhysicalDeviceMemoryProperties2(master.getPhysicalDevice(), &memProperties);
    std::ostringstream oss;
    for (uint32_t i = 0; i < memProperties.memoryProperties.memoryHeapCount; ++i) {
        if (memProperties.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            deviceMemoryHeap = i;
            availableDeviceMemory = (memBudjet.heapBudget[i] - memBudjet.heapUsage[i]) / 1024 / 1024;
        }
        oss << ((memProperties.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? "GPU" : "local") << " memory";
        oss << "\ttotal : " << memProperties.memoryProperties.memoryHeaps[i].size / 1024 / 1024 << " MiB   \tavailable : " << memBudjet.heapBudget[i] / 1024 / 1024 << " MiB\tused : " << memBudjet.heapUsage[i] / 1024 / 1024 << " MiB    \tfree : " << (memBudjet.heapBudget[i] - memBudjet.heapUsage[i]) / 1024 / 1024 << " MiB";
        master.putLog(oss.str(), LogType::DEBUG);
        oss.str(std::string());
    }
}

std::vector<MemoryQuerry> MemoryManager::querryMemory()
{
    vkGetPhysicalDeviceMemoryProperties2(master.getPhysicalDevice(), &memProperties);
    std::vector<MemoryQuerry> querry(memProperties.memoryProperties.memoryHeapCount);
    for (uint32_t i = 0; i < memProperties.memoryProperties.memoryHeapCount; ++i) {
        querry[i].total = memProperties.memoryProperties.memoryHeaps[i].size;
        querry[i].available = memBudjet.heapBudget[i];
        querry[i].used = memBudjet.heapUsage[i];
        querry[i].free = querry[i].available - querry[i].used;
        querry[i].flags = memProperties.memoryProperties.memoryHeaps[i].flags;
    }
    return querry;
}
