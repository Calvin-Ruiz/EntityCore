#ifndef BUFFER_MGR_HPP
#define BUFFER_MGR_HPP

#include <vulkan/vulkan.h>
#include "EntityCore/SubMemory.hpp"
#include "EntityCore/SubBuffer.hpp"
#include <vector>
#include <memory>
#include <list>
#include <thread>
#include <mutex>

class VulkanMgr;

/**
*   \brief Manage SubBuffer allocation like MemoryManager
*/
class BufferMgr {
public:
    BufferMgr(VulkanMgr &master, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags preferedProperties, int bufferBlocSize = 512*1024);
    ~BufferMgr();
    SubBuffer acquireBuffer(int size, bool isUniform = false);
    void releaseBuffer(SubBuffer &subBuffer);
    void *getPtr(SubBuffer &subBuffer);
    // Make the changes from the device visible
    void invalidate(SubBuffer &subBuffer);
    void invalidate(const std::vector<SubBuffer> &subBuffers);
    static void setUniformOffsetAlignment(int alignment) {uniformOffsetAlignment = alignment;}
    static void copy(VkCommandBuffer &cmd, SubBuffer &src, SubBuffer &dst);
    static void copy(VkCommandBuffer &cmd, SubBuffer &src, SubBuffer &dst, int size);
private:
    void releaseBuffer(); // Release next buffer in stack
    static void startMainloop(BufferMgr *self);
    std::unique_ptr<std::thread> releaseThread;
    std::vector<SubBuffer> releaseStack;
    bool isAlive = false;
    std::mutex mutex, mutexQueue;
    int maxOffset = 0;
    VulkanMgr &master;
    VkBuffer buffer;
    SubMemory memory;
    void *data = nullptr;
    void insert(SubBuffer &subBuffer);
    static int uniformOffsetAlignment;
    //! each std::list in this std::list are equally sized SubBuffer
    std::list<std::list<SubBuffer>> availableSubBufferZones;
    //std::list<SubBuffer> availableSubBuffer;
    const int bufferBlocSize;
};

#endif /* end of include guard: BUFFER_MGR_HPP */
