#include "VulkanMgr.hpp"
#include "BufferMgr.hpp"

int BufferMgr::uniformOffsetAlignment;

BufferMgr::BufferMgr(VulkanMgr &master, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags preferedProperties, int bufferBlocSize) : master(master), bufferBlocSize(bufferBlocSize)
{
    if (!master.createBuffer(bufferBlocSize, usage, properties, buffer, memory, preferedProperties)) {
        master.putLog("Failed to create buffer bloc", LogType::ERROR);
        return;
    }
    master.setObjectName(buffer, VK_OBJECT_TYPE_BUFFER, "BufferMgr");
    SubBuffer subBuffer;
    subBuffer.buffer = buffer;
    subBuffer.offset = 0;
    subBuffer.size = bufferBlocSize;
    insert(subBuffer);
    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        master.mapMemory(memory, &data);
}

BufferMgr::~BufferMgr()
{
    if (isAlive) {
        isAlive = false;
        releaseThread->join();
    }
    vkDestroyBuffer(master.refDevice, buffer, nullptr);
    master.free(memory);
}

SubBuffer BufferMgr::acquireBuffer(int size, bool isUniform)
{
    // std::lock_guard<std::mutex> lock(mutex);
    SubBuffer buffer;
    buffer.buffer = VK_NULL_HANDLE;
    std::list<std::list<SubBuffer>>::iterator availableSubBuffer;
    if (!isUniform) {
        availableSubBuffer = std::find_if(availableSubBufferZones.begin(), availableSubBufferZones.end(),
          [size](auto &value){return (value.back().size >= size);});
        if (availableSubBuffer != availableSubBufferZones.end()) {
            auto optimalAvailableSubBuffer = std::find_if(availableSubBuffer, availableSubBufferZones.end(),
            [size](auto &value){return !(value.back().size < size || value.back().possibleUniform);});
            if (optimalAvailableSubBuffer != availableSubBufferZones.end()) availableSubBuffer = optimalAvailableSubBuffer;
            buffer = availableSubBuffer->back();
            availableSubBuffer->pop_back();
            if (availableSubBuffer->size() == 0) availableSubBufferZones.erase(availableSubBuffer);
        }
    } else {
        for (availableSubBuffer = availableSubBufferZones.begin(); availableSubBuffer != availableSubBufferZones.end(); ++availableSubBuffer) {
            if (availableSubBuffer->front().size < size)
                continue;
            for (auto it = availableSubBuffer->begin(); it != availableSubBuffer->end(); ++it) {
                if (!it->possibleUniform)
                    break;
                if (it->offset % uniformOffsetAlignment > 0 && it->size + it->offset % uniformOffsetAlignment - uniformOffsetAlignment < size)
                    continue;
                buffer = *it;
                availableSubBuffer->erase(it);
                if (availableSubBuffer->size() == 0) availableSubBufferZones.erase(availableSubBuffer);
                if (buffer.offset % uniformOffsetAlignment > 0) {
                    SubBuffer tmp = buffer;
                    tmp.size = uniformOffsetAlignment - buffer.offset % uniformOffsetAlignment;
                    buffer.offset += tmp.size;
                    buffer.size -= tmp.size;
                    insert(tmp);
                }
                break;
            }
            if (buffer.buffer != VK_NULL_HANDLE)
                break;
        }
    }
    if (buffer.buffer == VK_NULL_HANDLE) {
        master.putLog("Can't allocate buffer in global buffer !", LogType::ERROR);
    } else {
        if (buffer.size > size) {
            SubBuffer tmp = buffer;
            tmp.size -= size;
            tmp.offset += size;
            buffer.size = size;
            insert(tmp);
        }
        if (buffer.offset + buffer.size > maxOffset)
            maxOffset = buffer.offset + buffer.size;
    }
    return buffer;
}

void BufferMgr::insert(SubBuffer &subBuffer)
{
    subBuffer.possibleUniform = (subBuffer.offset % uniformOffsetAlignment == 0) || (subBuffer.size > uniformOffsetAlignment - subBuffer.offset % uniformOffsetAlignment);
    for (auto it = availableSubBufferZones.begin(); it != availableSubBufferZones.end(); ++it) {
        if (it->front().size == subBuffer.size) {
            if (subBuffer.possibleUniform)
                it->push_front(subBuffer);
            else
                it->push_back(subBuffer);
            if (it->size() == 500 && !isAlive && false) {
                isAlive = true;
                releaseThread = std::make_unique<std::thread>(startMainloop, this);
            }
            return;
        } else if (it->front().size > subBuffer.size) {
            std::list<SubBuffer> tmpList;
            tmpList.push_back(subBuffer);
            availableSubBufferZones.insert(it, tmpList);
            return;
        }
    }
    std::list<SubBuffer> tmpList;
    tmpList.push_back(subBuffer);
    availableSubBufferZones.push_back(tmpList);
}

void BufferMgr::releaseBuffer(SubBuffer &subBuffer)
{
    // mutexQueue.lock();
    releaseStack.push_back(subBuffer);
    // mutexQueue.unlock();
    if (!isAlive)
        releaseBuffer();
}

void BufferMgr::releaseBuffer()
{
    // mutexQueue.lock();
    SubBuffer subBuffer = releaseStack.back();
    releaseStack.pop_back();
    // mutexQueue.unlock();
    int buffBegin = subBuffer.offset;
    int buffEnd = buffBegin + subBuffer.size;

    // mutex.lock();
    for (auto availableSubBuffer = availableSubBufferZones.begin(); availableSubBuffer != availableSubBufferZones.end(); ++availableSubBuffer) {
        for (auto it = availableSubBuffer->begin(); it != availableSubBuffer->end(); ++it) {
            if (it->offset == buffEnd) {
                subBuffer.size += it->size;
            } else if (it->offset + it->size == buffBegin) {
                subBuffer.offset = it->offset;
                subBuffer.size += it->size;
            } else {
                continue;
            }
            if (it == availableSubBuffer->begin()) {
                availableSubBuffer->erase(it);
                it = availableSubBuffer->begin();
            } else {
                auto tmpIt = it;
                --it;
                availableSubBuffer->erase(tmpIt);
            }
        }
        if (availableSubBuffer->size() == 0) {
            if (availableSubBuffer == availableSubBufferZones.begin()) {
                availableSubBufferZones.erase(availableSubBuffer);
                availableSubBuffer = availableSubBufferZones.begin();
            } else {
                auto tmpIt = availableSubBuffer;
                --availableSubBuffer;
                availableSubBufferZones.erase(tmpIt);
            }
        }
    }
    if (subBuffer.offset + subBuffer.size >= maxOffset && subBuffer.offset < maxOffset) {
        maxOffset = subBuffer.offset;
    }
    insert(subBuffer);
    // mutex.unlock();
}

void *BufferMgr::getPtr(SubBuffer &subBuffer)
{
    return static_cast<char *>(data) + subBuffer.offset; // static_cast for GCC
}

void BufferMgr::startMainloop(BufferMgr *self)
{
    while (true) {
        while (self->isAlive && self->releaseStack.empty())
            std::this_thread::yield();
        if (!self->isAlive)
            return;
        self->releaseBuffer();
    }
}

void BufferMgr::invalidate(SubBuffer &subBuffer)
{
    VkMappedMemoryRange range {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, memory.memory, memory.offset + subBuffer.offset, (VkDeviceSize) subBuffer.size};

    vkInvalidateMappedMemoryRanges(master.refDevice, 1, &range);
}

void BufferMgr::invalidate(const std::vector<SubBuffer> &subBuffers)
{
    std::vector<VkMappedMemoryRange> ranges;

    ranges.reserve(subBuffers.size());
    for (const auto &b : subBuffers) {
        ranges.push_back({VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, memory.memory, memory.offset + b.offset, (VkDeviceSize) b.size});
    }
    vkInvalidateMappedMemoryRanges(master.refDevice, ranges.size(), ranges.data());
}
