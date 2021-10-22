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
*   There must never been concurrent access to SubBuffer memory acquired from the same BufferMgr
*/
class BufferMgr {
public:
    BufferMgr(VulkanMgr &master, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags preferedProperties, uint32_t bufferBlocSize = 512*1024, const std::string &name = "\0", bool uniformBuffer = false);
    ~BufferMgr();
    SubBuffer acquireBuffer(int size);
    void releaseBuffer(SubBuffer &subBuffer);
    //! For per-frame buffer allocation (don't use acquireBuffer nor releaseBuffer on this BufferMgr when using this)
    SubBuffer fastAcquireBuffer(uint32_t size);
    // Release every previously acquired SubBuffer, mustn't be used with acquireBuffer
    void reset();
    // Return a pointer to
    void *getPtr(SubBuffer &subBuffer);
    // Make the changes from the device visible (if host_cached)
    void invalidate();
    // Make the changes from the device visible (if host_cached)
    void invalidate(SubBuffer &subBuffer);
    // Make the changes from the device visible (if host_cached)
    void invalidate(const std::vector<SubBuffer> &subBuffers);
    // Make the changes from the gpu visible (if host_cached)
    void flush();
    // Make the changes from the gpu visible (if host_cached)
    void flush(SubBuffer &subBuffer);
    // Make the changes from the gpu visible (if host_cached)
    void flush(const std::vector<SubBuffer> &subBuffers);
    // Set a name to the Buffer hold by BufferMgr (visible from debug layer and nvidia nsight graphics)
    void setName(const std::string &name);
    // internally used, set the alignment requirement for uniform
    static void setUniformOffsetAlignment(int alignment) {uniformOffsetAlignment = alignment;}
    // Record a copy from one SubBuffer to another SubBuffer
    static void copy(VkCommandBuffer &cmd, SubBuffer &src, SubBuffer &dst);
    // Record a copy of size octects from one SubBuffer to another SubBuffer
    static void copy(VkCommandBuffer &cmd, SubBuffer &src, SubBuffer &dst, int size);
    // Record a copy of size octects from one SubBuffer to another SubBuffer with an offset
    static void copy(VkCommandBuffer &cmd, SubBuffer &src, SubBuffer &dst, int size, int srcOffset, int dstOffset);
    inline VkBuffer &getBuffer() {
        return buffer;
    }
private:
    void releaseBuffer(); // Release next buffer in stack
    static void startMainloop(BufferMgr *self);
    std::unique_ptr<std::thread> releaseThread;
    std::vector<SubBuffer> releaseStack;
    bool isAlive = false;
    std::mutex mutex, mutexQueue;
    uint32_t maxOffset = 0;
    VulkanMgr &master;
    VkBuffer buffer;
    SubMemory memory;
    std::string name;
    void *data = nullptr;
    void insert(SubBuffer &subBuffer);
    static int uniformOffsetAlignment;
    //! each std::list in this std::list are equally sized SubBuffer
    std::list<std::list<SubBuffer>> availableSubBufferZones;
    //std::list<SubBuffer> availableSubBuffer;
    const uint32_t bufferBlocSize;
    const bool uniformBuffer;
};

#endif /* end of include guard: BUFFER_MGR_HPP */
