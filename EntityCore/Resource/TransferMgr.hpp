#include "EntityCore/SubBuffer.hpp"
#include <map>
#include <vector>
class BufferMgr;

class TransferMgr {
public:
    TransferMgr(BufferMgr &mgr, int size);
    ~TransferMgr();

    void *planCopy(SubBuffer &dst);
    void *planCopy(SubBuffer &dst, int offset, int size);
    void copy(VkCommandBuffer &cmd); // Record copy and reset allocation
private:
    BufferMgr &mgr;
    void *ptr;
    std::map<VkBuffer, std::vector<VkBufferCopy>> pendingCopy;
    SubBuffer buffer; // note : buffer.size is the size of currently used SubBuffer space
    const int size;
};
