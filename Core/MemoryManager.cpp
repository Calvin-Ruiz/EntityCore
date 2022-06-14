#include "VulkanMgr.hpp"
#include "MemoryManager.hpp"
#include <mutex>
#include <sstream>

MemoryManager::MemoryManager(VulkanMgr &master, uint32_t chunkSize, uint32_t _batchCount) :
    master(master), refDevice(master.refDevice), batch(_batchCount ? _batchCount : 1), chunkSize(chunkSize), usingBatches(_batchCount > 0)
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
    for (auto &b : batch) {
        for (auto &memTypes : b.memory) {
            for (auto &mem : memTypes.memoryChunks) {
                vkFreeMemory(refDevice, mem, nullptr);
            }
        }
    }
}

SubMemory MemoryManager::malloc(const VkMemoryRequirements &memRequirements, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags preferedProperties, uint32_t memoryBatch)
{
    SubMemory subMemory;
    subMemory.memory = VK_NULL_HANDLE;
    subMemory.memoryBatch = memoryBatch;
    findMemoryIndex(memRequirements, properties, preferedProperties, &subMemory);
    batch[memoryBatch].mtx.lock();
    if (subMemory.memoryIndex != UINT32_MAX) // If false, there is no compatible memory type
        acquireSubMemory(memRequirements, &subMemory);
    if (subMemory.memory != VK_NULL_HANDLE) // If false, there is no memory acquired
        allocateInSubMemory(memRequirements, &subMemory);
    batch[memoryBatch].mtx.unlock();
    return subMemory;
}

SubMemory MemoryManager::dmalloc(const VkMemoryRequirements &memRequirements, const VkMemoryDedicatedAllocateInfo &dedicatedInfo, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags preferedProperties)
{
    SubMemory subMemory;
    subMemory.memory = VK_NULL_HANDLE;
    subMemory.memoryBatch = UINT32_MAX;
    findMemoryIndex(memRequirements, properties, preferedProperties, &subMemory);
    if (availableDeviceMemory <= 64 + chunkSize / 1024 / 1024 && !hasReleasedUnusedMemory && memProperties.memoryProperties.memoryTypes[subMemory.memoryIndex].heapIndex == deviceMemoryHeap) {
        master.releaseUnusedMemory();
        hasReleasedUnusedMemory = true;
        displayResources();
    }
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
    return subMemory;
}

void MemoryManager::free(SubMemory &subMemory)
{
    if (subMemory.memory == VK_NULL_HANDLE) return;
    if (subMemory.size == (VkDeviceSize) -1) {
        vkFreeMemory(refDevice, subMemory.memory, nullptr);
    } else {
        merge(&subMemory);
        insert(subMemory);
    }
}

void MemoryManager::mapMemory(SubMemory &subMemory, void **data)
{
    if (subMemory.memoryBatch == UINT32_MAX) {
        if (vkMapMemory(refDevice, subMemory.memory, 0, VK_WHOLE_SIZE, 0, data) != VK_SUCCESS)
            throw std::runtime_error("Faild to map memory");
        return;
    }
    batch[subMemory.memoryBatch].mtx.lock();
    MappedMemory &mapmem = batch[subMemory.memoryBatch].mappedMemory[subMemory.memory];
    if (++mapmem.nbMapping == 1) {
        if (vkMapMemory(refDevice, subMemory.memory, 0, VK_WHOLE_SIZE, 0, &mapmem.data) != VK_SUCCESS)
            throw std::runtime_error("Faild to map memory");
    }
    batch[subMemory.memoryBatch].mtx.unlock();
    *data = static_cast<char *>(mapmem.data) + subMemory.offset; // static_cast for GCC
}

void MemoryManager::unmapMemory(SubMemory &subMemory)
{
    if (subMemory.memoryBatch == UINT32_MAX) {
        vkUnmapMemory(refDevice, subMemory.memory);
        return;
    }
    batch[subMemory.memoryBatch].mtx.lock();
    MappedMemory &mapmem = batch[subMemory.memoryBatch].mappedMemory[subMemory.memory];
    if (--mapmem.nbMapping == 0) {
        vkUnmapMemory(refDevice, subMemory.memory);
    }
    batch[subMemory.memoryBatch].mtx.unlock();
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
    uint32_t memoryBatch = subMemory->memoryBatch;
    if (memRequirements.size > chunkSize) {
        // Required memory is bigger than a chunk, allocate a chunk specifically sized for this allocation
        *subMemory = allocateChunk(subMemory->memoryIndex, memoryBatch, memRequirements.size, false);
        return;
    }
    const auto itEnd = batch[memoryBatch].memory[subMemory->memoryIndex].availableSpaces.end();
    for (auto it = batch[memoryBatch].memory[subMemory->memoryIndex].availableSpaces.begin(); it != itEnd; ++it) {
        if (memRequirements.size <= it->size
            && (it->offset % memRequirements.alignment == 0
                || memRequirements.size + memRequirements.alignment
                - (it->offset % memRequirements.alignment) <= it->size)) {
            *subMemory = *it;
            batch[memoryBatch].memory[subMemory->memoryIndex].availableSpaces.erase(it);
            break;
        }
    }
    if (subMemory->memory == VK_NULL_HANDLE) {
        if (!usingBatches && memProperties.memoryProperties.memoryTypes[subMemory->memoryIndex].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            // Host visible memory should be allocated separately in order to allow concurrent access to different buffer
            *subMemory = allocateChunk(subMemory->memoryIndex, memoryBatch, memRequirements.size, false);
            return;
        }
        if (!hasReleasedUnusedMemory && availableDeviceMemory <= 64 + chunkSize / 1024 / 1024 && memProperties.memoryProperties.memoryTypes[subMemory->memoryIndex].heapIndex == deviceMemoryHeap) {
            batch[memoryBatch].mtx.unlock();
            master.releaseUnusedMemory();
            batch[memoryBatch].mtx.lock();
            hasReleasedUnusedMemory = true;
            displayResources();
            acquireSubMemory(memRequirements, subMemory);
        } else {
            *subMemory = allocateChunk(subMemory->memoryIndex, memoryBatch);
        }
    }
}

void MemoryManager::allocateInSubMemory(const VkMemoryRequirements &memRequirements, SubMemory *subMemory)
{
    if (subMemory->size == (VkDeviceSize) -1)
        return;
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
    auto &memory = batch[subMemory.memoryBatch].memory;
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
    auto &memory = batch[subMemory->memoryBatch].memory;

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

SubMemory MemoryManager::allocateChunk(uint32_t memoryIndex, uint32_t memoryBatch, uint32_t specificChunkSize, bool registerChunk)
{
    SubMemory subMemory;
    subMemory.offset = 0;
    subMemory.memoryBatch = memoryBatch;
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = subMemory.size = (specificChunkSize == UINT32_MAX) ? chunkSize : specificChunkSize;
    allocInfo.memoryTypeIndex = subMemory.memoryIndex = memoryIndex;

    std::ostringstream oss;
    std::string heapType = (memProperties.memoryProperties.memoryHeaps[memProperties.memoryProperties.memoryTypes[memoryIndex].heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? "GPU" : "local";

    if (vkAllocateMemory(refDevice, &allocInfo, nullptr, &subMemory.memory) == VK_SUCCESS) {
        oss << "Allocate chunk of " << subMemory.size / 1024 / 1024 << " MiB in " << heapType << " memory.";
        master.putLog(oss.str(), LogType::DEBUG);
        if (registerChunk)
            batch[memoryBatch].memory[memoryIndex].memoryChunks.push_back(subMemory.memory);
        else
            subMemory.size = (VkDeviceSize) -1;
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
    if (onDisplayResources.test_and_set())
        return;
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
    onDisplayResources.clear();
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

void MemoryManager::displayFragmentation(int memoryBatch)
{
    auto &b = batch[memoryBatch];
    b.mtx.lock();
    master.putLog("----- Fragmentation of memory batch " + std::to_string(memoryBatch) + " -----", LogType::DEBUG);
    for (int i = 0; i < VK_MAX_MEMORY_TYPES; ++i) {
        if (b.memory[i].availableSpaces.empty())
            continue;
        std::ostringstream oss;
        oss << "Heap " << i << " (" << ((memProperties.memoryProperties.memoryHeaps[memProperties.memoryProperties.memoryTypes[i].heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) ? "GPU" : "local") << ") :\n";
        for (auto &as : b.memory[i].availableSpaces)
            oss << "\t" << "memory=" << as.memory << ", offset=" << as.offset << " (" << as.offset/1024/1024 << " Mo)" << ", size=" << as.size << " (" << as.size/1024/1024 << "/" << chunkSize/1024/1024 << "Mo)\n";
        master.putLog(oss.str(), LogType::DEBUG);
    }
    b.mtx.unlock();
}

void MemoryManager::releaseUnusedChunks()
{
    if (onReleaseMemory.test_and_set())
        return;
    for (auto &b : batch) {
        b.mtx.lock();
        for (auto &m : b.memory) {
            for (auto it = m.availableSpaces.begin(); it != m.availableSpaces.end();) {
                if (it->size != chunkSize) {
                    ++it;
                    continue;
                }
                m.memoryChunks.remove(it->memory);
                vkFreeMemory(refDevice, it->memory, nullptr);
                m.availableSpaces.erase(it++);
            }
        }
        b.mtx.unlock();
    }
    onReleaseMemory.clear();
}
