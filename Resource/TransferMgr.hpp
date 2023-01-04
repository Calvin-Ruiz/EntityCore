#include "EntityCore/SubBuffer.hpp"
#include "SyncEvent.hpp"
#include <map>
#include <vector>
#include <cassert>
class BufferMgr;

class TransferMgr {
public:
    TransferMgr(BufferMgr &mgr, uint32_t size);
    ~TransferMgr();

    template <typename T = void>
    T *beginPlanCopy(uint32_t _size) { // pre-plan copy with a maximal size
        assert(!planningCopy);
        if (buffer.size + _size > size)
            return nullptr;
        planningCopy = true;
        return (T *) (((char *) ptr) + buffer.size);
    }

    void endPlanCopy(SubBuffer &dst, uint32_t _size) { // Finalize pre-planned copy defined final size
        assert(planningCopy);
        planningCopy = false;
        if (_size == 0)
            return;
        pendingCopy[dst.buffer].push_back({(VkDeviceSize) (buffer.offset + buffer.size), (VkDeviceSize) dst.offset, (VkDeviceSize) _size});
        buffer.size += _size;
    }

    template <typename T = void>
    T *planCopy(SubBuffer &dst) {
        assert(!planningCopy);
        T *ret = (T *) (((char *) ptr) + buffer.size);
        pendingCopy[dst.buffer].push_back({(VkDeviceSize) (buffer.offset + buffer.size), (VkDeviceSize) dst.offset, (VkDeviceSize) dst.size});
        buffer.size += dst.size;
        if (buffer.size > size) {
            buffer.size -= dst.size;
            pendingCopy[dst.buffer].pop_back();
            return nullptr;
        }
        return ret;
    }

    template <typename T = void>
    T *planCopy(SubBuffer &dst, int offset, int _size) {
        assert(!planningCopy);
        T *ret = (T *) (((char *) ptr) + buffer.size);
        pendingCopy[dst.buffer].push_back({(VkDeviceSize) (buffer.offset + buffer.size), (VkDeviceSize) (dst.offset + offset), (VkDeviceSize) _size});
        buffer.size += _size;
        if (buffer.size > size) {
            buffer.size -= _size;
            pendingCopy[dst.buffer].pop_back();
            return nullptr;
        }
        return ret;
    }

    void planCopyBetween(SubBuffer &src, SubBuffer &dst);
    void planCopyBetween(SubBuffer &src, SubBuffer &dst, int size);
    void planCopyBetween(SubBuffer &src, SubBuffer &dst, int size, int srcOffset, int dstOffset);
    void copy(VkCommandBuffer cmd); // Record copy and reset allocation
    // Return amount of memory currently used
    uint32_t getUsedSpace() const {return buffer.size;};
    // Return amount of memory which can be used until next copy()
    uint32_t getRemainingSpace() const {return size - buffer.size;};
    // Return the ratio of memory used
    float getUsedRatio() const {return buffer.size / (float) size;}
private:
    BufferMgr &mgr;
    SyncEvent barrier;
    void *ptr;
    std::map<VkBuffer, std::vector<VkBufferCopy>> pendingCopy;
    std::map<std::pair<VkBuffer, VkBuffer>, std::vector<VkBufferCopy>> pendingExternalCopy;
    SubBuffer buffer; // note : buffer.size is the size of currently used SubBuffer space
    const uint32_t size;
    bool planningCopy = false;;
};
