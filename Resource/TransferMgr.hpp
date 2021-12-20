#include "EntityCore/SubBuffer.hpp"
#include "SyncEvent.hpp"
#include <map>
#include <vector>
class BufferMgr;

class TransferMgr {
public:
    TransferMgr(BufferMgr &mgr, int size);
    ~TransferMgr();

    void *beginPlanCopy(uint32_t size); // pre-plan copy with a maximal size
    void endPlanCopy(SubBuffer &dst, uint32_t size); // Finalize pre-planned copy defined final size
    void *planCopy(SubBuffer &dst);
    void *planCopy(SubBuffer &dst, int offset, int size);
    void planCopyBetween(SubBuffer &src, SubBuffer &dst);
    void planCopyBetween(SubBuffer &src, SubBuffer &dst, int size);
    void planCopyBetween(SubBuffer &src, SubBuffer &dst, int size, int srcOffset, int dstOffset);
    void copy(VkCommandBuffer &cmd); // Record copy and reset allocation
private:
    BufferMgr &mgr;
    SyncEvent barrier;
    void *ptr;
    std::map<VkBuffer, std::vector<VkBufferCopy>> pendingCopy;
    std::map<std::pair<VkBuffer, VkBuffer>, std::vector<VkBufferCopy>> pendingExternalCopy;
    SubBuffer buffer; // note : buffer.size is the size of currently used SubBuffer space
    const int size;
    bool planningCopy = false;;
};
