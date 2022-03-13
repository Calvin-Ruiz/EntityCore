#include "EntityCore/Core/BufferMgr.hpp"
#include "TransferMgr.hpp"
#include <cassert>

TransferMgr::TransferMgr(BufferMgr &mgr, uint32_t size) : mgr(mgr), size(size)
{
    buffer = mgr.acquireBuffer(size);
    ptr = mgr.getPtr(buffer);
    barrier.bufferBarrier(buffer, VK_PIPELINE_STAGE_2_HOST_BIT_KHR, VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_ACCESS_2_HOST_WRITE_BIT_KHR, VK_ACCESS_2_TRANSFER_READ_BIT_KHR);
    barrier.build();
    buffer.size = 0;
}

TransferMgr::~TransferMgr()
{
    buffer.size = size;
    mgr.releaseBuffer(buffer);
}

void *TransferMgr::beginPlanCopy(uint32_t _size)
{
    assert(!planningCopy);
    if (buffer.size + _size > size)
        return nullptr;
    planningCopy = true;
    return (void *) (((char *) ptr) + buffer.size);
}

void TransferMgr::endPlanCopy(SubBuffer &dst, uint32_t _size)
{
    assert(planningCopy);
    planningCopy = false;
    if (_size == 0)
        return;
    pendingCopy[dst.buffer].push_back({(VkDeviceSize) (buffer.offset + buffer.size), (VkDeviceSize) dst.offset, (VkDeviceSize) _size});
    buffer.size += _size;
}

void *TransferMgr::planCopy(SubBuffer &dst)
{
    assert(!planningCopy);
    void *ret = (void *) (((char *) ptr) + buffer.size);
    pendingCopy[dst.buffer].push_back({(VkDeviceSize) (buffer.offset + buffer.size), (VkDeviceSize) dst.offset, (VkDeviceSize) dst.size});
    buffer.size += dst.size;
    if (buffer.size > size) {
        buffer.size -= dst.size;
        pendingCopy[dst.buffer].pop_back();
        return nullptr;
    }
    return ret;
}

void *TransferMgr::planCopy(SubBuffer &dst, int offset, int _size)
{
    assert(!planningCopy);
    void *ret = (void *) (((char *) ptr) + buffer.size);
    pendingCopy[dst.buffer].push_back({(VkDeviceSize) (buffer.offset + buffer.size), (VkDeviceSize) (dst.offset + offset), (VkDeviceSize) _size});
    buffer.size += _size;
    if (buffer.size > size) {
        buffer.size -= _size;
        pendingCopy[dst.buffer].pop_back();
        return nullptr;
    }
    return ret;
}

void TransferMgr::planCopyBetween(SubBuffer &src, SubBuffer &dst)
{
    const VkBufferCopy copy {(VkDeviceSize) src.offset, (VkDeviceSize) dst.offset, (VkDeviceSize) dst.size};
    if (src.buffer == buffer.buffer) {
        pendingCopy[dst.buffer].push_back(copy);
    } else {
        pendingExternalCopy[{src.buffer, dst.buffer}].push_back(copy);
    }
}

void TransferMgr::planCopyBetween(SubBuffer &src, SubBuffer &dst, int size)
{
    const VkBufferCopy copy {(VkDeviceSize) src.offset, (VkDeviceSize) dst.offset, (VkDeviceSize) size};
    if (src.buffer == buffer.buffer) {
        pendingCopy[dst.buffer].push_back(copy);
    } else {
        pendingExternalCopy[{src.buffer, dst.buffer}].push_back(copy);
    }
}

void TransferMgr::planCopyBetween(SubBuffer &src, SubBuffer &dst, int size, int srcOffset, int dstOffset)
{
    const VkBufferCopy copy {(VkDeviceSize) src.offset + srcOffset, (VkDeviceSize) dst.offset + dstOffset, (VkDeviceSize) size};
    if (src.buffer == buffer.buffer) {
        pendingCopy[dst.buffer].push_back(copy);
    } else {
        pendingExternalCopy[{src.buffer, dst.buffer}].push_back(copy);
    }
}

void TransferMgr::copy(VkCommandBuffer &cmd)
{
    barrier.placeBarrier(cmd);
    buffer.size = 0;
    // record copy
    for (auto &v : pendingCopy) {
        if (!v.second.empty()) {
            vkCmdCopyBuffer(cmd, buffer.buffer, v.first, v.second.size(), v.second.data());
            v.second.clear();
        }
    }
    for (auto &v : pendingExternalCopy) {
        if (!v.second.empty()) {
            vkCmdCopyBuffer(cmd, v.first.first, v.first.second, v.second.size(), v.second.data());
            v.second.clear();
        }
    }
}
