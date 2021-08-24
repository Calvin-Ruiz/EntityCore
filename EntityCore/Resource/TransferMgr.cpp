#include "EntityCore/Core/BufferMgr.hpp"
#include "TransferMgr.hpp"

TransferMgr::TransferMgr(BufferMgr &mgr, int size) : mgr(mgr), size(size)
{
    buffer = mgr.acquireBuffer(size);
    ptr = mgr.getPtr(buffer);
    buffer.size = 0;

}

TransferMgr::~TransferMgr()
{
    buffer.size = size;
    mgr.releaseBuffer(buffer);
}

void *TransferMgr::planCopy(SubBuffer &dst)
{
    void *ret = (void *) (((char *) ptr) + buffer.size);
    pendingCopy[dst.buffer].push_back({(VkDeviceSize) (buffer.offset + buffer.size), (VkDeviceSize) dst.offset, (VkDeviceSize) dst.size});
    buffer.size += dst.size;
    if (buffer.size > size) {
        pendingCopy[dst.buffer].pop_back();
        return nullptr;
    }
    return ret;
}

void *TransferMgr::planCopy(SubBuffer &dst, int offset, int _size)
{
    void *ret = (void *) (((char *) ptr) + buffer.size);
    pendingCopy[dst.buffer].push_back({(VkDeviceSize) (buffer.offset + buffer.size), (VkDeviceSize) (dst.offset + offset), (VkDeviceSize) _size});
    buffer.size += _size;
    if (buffer.size > size) {
        pendingCopy[dst.buffer].pop_back();
        return nullptr;
    }
    return ret;
}

void TransferMgr::copy(VkCommandBuffer &cmd)
{
    buffer.size = 0;
    // record copy
    for (auto &v : pendingCopy) {
        if (!v.second.empty()) {
            vkCmdCopyBuffer(cmd, buffer.buffer, v.first, v.second.size(), v.second.data());
            v.second.clear();
        }
    }
}
