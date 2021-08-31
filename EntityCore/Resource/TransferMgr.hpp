#include "EntityCore/SubBuffer.hpp"
#include <map>
#include <vector>
class BufferMgr;

class TransferMgr {
public:
    TransferMgr(BufferMgr &mgr, int size);
    ~TransferMgr();

    void *beginPlanCopy(int size); // pre-plan copy with a maximal size
    void endPlanCopy(SubBuffer &dst, int size); // Finalize pre-planned copy defined final size
    void *planCopy(SubBuffer &dst);
    void *planCopy(SubBuffer &dst, int offset, int size);
    void planCopyBetween(SubBuffer &src, SubBuffer &dst);
    void copy(VkCommandBuffer &cmd); // Record copy and reset allocation
private:
    BufferMgr &mgr;
    void *ptr;
    std::map<VkBuffer, std::vector<VkBufferCopy>> pendingCopy;
    std::map<std::pair<VkBuffer, VkBuffer>, std::vector<VkBufferCopy>> pendingExternalCopy;
    SubBuffer buffer; // note : buffer.size is the size of currently used SubBuffer space
    const int size;
    bool planningCopy = false;;
};
