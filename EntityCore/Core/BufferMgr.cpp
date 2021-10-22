#include "VulkanMgr.hpp"
#include "BufferMgr.hpp"

int BufferMgr::uniformOffsetAlignment;

BufferMgr::BufferMgr(VulkanMgr &master, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags preferedProperties, int bufferBlocSize, const std::string &name, bool uniformBuffer) : master(master), name(name), bufferBlocSize(bufferBlocSize), uniformBuffer(uniformBuffer)
{
    if (!master.createBuffer(bufferBlocSize, usage, properties, buffer, memory, preferedProperties)) {
        master.putLog("Failed to create buffer bloc", LogType::ERROR);
        return;
    }
    if (name.empty())
        master.setObjectName(buffer, VK_OBJECT_TYPE_BUFFER, "BufferMgr");
    else
        master.setObjectName(buffer, VK_OBJECT_TYPE_BUFFER, name.c_str());
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

SubBuffer BufferMgr::acquireBuffer(int size)
{
    if (uniformBuffer) {
        size = ((size - 1) / uniformOffsetAlignment + 1) * uniformOffsetAlignment;
    }
    // std::lock_guard<std::mutex> lock(mutex);
    SubBuffer buffer;
    buffer.buffer = VK_NULL_HANDLE;
    std::list<std::list<SubBuffer>>::iterator availableSubBuffer;
    availableSubBuffer = std::find_if(availableSubBufferZones.begin(), availableSubBufferZones.end(),
      [size](auto &value){return (value.back().size >= size);});
    if (availableSubBuffer != availableSubBufferZones.end()) {
        buffer = availableSubBuffer->back();
        availableSubBuffer->pop_back();
        if (availableSubBuffer->size() == 0)
            availableSubBufferZones.erase(availableSubBuffer);
    }
    if (buffer.buffer == VK_NULL_HANDLE) {
        master.putLog("Can't allocate buffer in '" + name + "' !", LogType::ERROR);
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
    for (auto it = availableSubBufferZones.begin(); it != availableSubBufferZones.end(); ++it) {
        if (it->front().size == subBuffer.size) {
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

SubBuffer BufferMgr::fastAcquireBuffer(int size)
{
    if (maxOffset + size > memory.size) {
        return {};
    }
    SubBuffer tmp {buffer, maxOffset, size};
    maxOffset += size;
    return tmp;
}

void BufferMgr::reset()
{
    maxOffset = 0;
}

void BufferMgr::startMainloop(BufferMgr *self)
{
    while (true) {
        while (self->isAlive && self->releaseStack.empty())
            std::this_thread::sleep_for(std::chrono::microseconds(400));
        if (!self->isAlive)
            return;
        self->releaseBuffer();
    }
}

void BufferMgr::invalidate()
{
    VkMappedMemoryRange range {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, memory.memory, memory.offset, (VkDeviceSize) maxOffset};
    vkInvalidateMappedMemoryRanges(master.refDevice, 1, &range);
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

void BufferMgr::flush()
{
    VkMappedMemoryRange range {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, memory.memory, memory.offset, (VkDeviceSize) maxOffset};

    vkFlushMappedMemoryRanges(master.refDevice, 1, &range);
}

void BufferMgr::flush(SubBuffer &subBuffer)
{
    VkMappedMemoryRange range {VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, memory.memory, memory.offset + subBuffer.offset, (VkDeviceSize) subBuffer.size};

    vkFlushMappedMemoryRanges(master.refDevice, 1, &range);
}

void BufferMgr::flush(const std::vector<SubBuffer> &subBuffers)
{
    std::vector<VkMappedMemoryRange> ranges;

    ranges.reserve(subBuffers.size());
    for (const auto &b : subBuffers) {
        ranges.push_back({VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr, memory.memory, memory.offset + b.offset, (VkDeviceSize) b.size});
    }
    vkFlushMappedMemoryRanges(master.refDevice, ranges.size(), ranges.data());
}

void BufferMgr::copy(VkCommandBuffer &cmd, SubBuffer &src, SubBuffer &dst)
{
    VkBufferCopy region {(VkDeviceSize) src.offset, (VkDeviceSize) dst.offset, (VkDeviceSize) src.size};

    vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &region);
}

void BufferMgr::copy(VkCommandBuffer &cmd, SubBuffer &src, SubBuffer &dst, int range)
{
    VkBufferCopy region {(VkDeviceSize) src.offset, (VkDeviceSize) dst.offset, (VkDeviceSize) range};

    vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &region);
}

void BufferMgr::copy(VkCommandBuffer &cmd, SubBuffer &src, SubBuffer &dst, int range, int srcOffset, int dstOffset)
{
    VkBufferCopy region {(VkDeviceSize) (src.offset + srcOffset), (VkDeviceSize) (dst.offset + dstOffset), (VkDeviceSize) range};

    vkCmdCopyBuffer(cmd, src.buffer, dst.buffer, 1, &region);
}

void BufferMgr::setName(const std::string &_name)
{
    name = _name;
    master.setObjectName(buffer, VK_OBJECT_TYPE_BUFFER, name.c_str());
}
