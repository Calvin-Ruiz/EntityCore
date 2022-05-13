#ifndef MEMORY_MANAGER_HPP
#define MEMORY_MANAGER_HPP

#include <vulkan/vulkan.h>
#include <list>
#include <vector>
#include <array>
#include <map>
#include "EntityCore/SubMemory.hpp"
#include <mutex>

class VulkanMgr;

struct MappedMemory {
    int nbMapping = 0;
    void *data = nullptr;
};

struct MemoryQuerry {
    size_t total; // Physical capacity
    size_t available; // Estimation of the memory available for this application
    size_t used; // Estimation of the memory allocated by this application
    size_t free; // Estimation of the remaining memory
    VkMemoryPropertyFlags flags; // Property of this memory, has the flag VK_MEMORY_HEAP_DEVICE_LOCAL_BIT if located on the GPU
};

/**
*   \brief Handle memory allocation and mapping
*/
class MemoryManager
{
public:
    MemoryManager(VulkanMgr &master, uint32_t _chunkSize, uint32_t _batchCount = 0);
    ~MemoryManager();
    //! Allocate memory from a given batch
    //! @return SubMemory.memory can be VK_NULL_HANDLE if memory allocation has failed
    SubMemory malloc(const VkMemoryRequirements &memRequirements, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags preferedProperties = 0, uint32_t allocationBatch = 0);
    //! Allocate dedicated memory
    SubMemory dmalloc(const VkMemoryRequirements &memRequirements, const VkMemoryDedicatedAllocateInfo &dedicatedInfo, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags preferedProperties = 0);
    //! Release allocated memory
    void free(SubMemory &subMemory);
    //! Map memory
    void mapMemory(SubMemory &subMemory, void **data);
    //! Unmap memory
    void unmapMemory(SubMemory &subMemory);
    uint32_t getChunkSize() const {return chunkSize;}
    //! Tell frame has complete
    void endOfFrame() {hasReleasedUnusedMemory = false;}
    //! Querry available ressources to decide what to do
    std::vector<MemoryQuerry> querryMemory();
    //! Display memory fragmentation of one batch
    void displayFragmentation(int memoryBatch);
    //! Release unused chunks of memory
    void releaseUnusedChunks();
private:
    //! Assign the memory index according to requirements
    void findMemoryIndex(const VkMemoryRequirements &memRequirements, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags preferedProperties, SubMemory *subMemory);
    //! Assign the smallest compatible SubMemory
    void acquireSubMemory(const VkMemoryRequirements &memRequirements, SubMemory *subMemory);
    //! Allocate requested size and alignment inside SubMemory, releasing unused memory range(s)
    void allocateInSubMemory(const VkMemoryRequirements &memRequirements, SubMemory *subMemory);
    //! Insert SubMemory in availableSpaces of corresponding memory type
    void insert(SubMemory &subMemory);
    //! Merge with SubMemory of which the begin or the end is common
    void merge(SubMemory *subMemory);
    //! Return SubMemory which cover the whole newly allocated chunk of memory
    SubMemory allocateChunk(uint32_t memoryIndex, uint32_t memoryBatch, uint32_t specificChunkSize = UINT32_MAX, bool registerChunk = true);
    //! Write memory statistics in log
    void displayResources();
    VulkanMgr &master;
    bool hasReleasedUnusedMemory = false; // Tell if releaseUnusedMemory have been called this frame
    uint16_t availableDeviceMemory; // Available GPU memory in MiB
    uint16_t deviceMemoryHeap; // GPU memory heap index
    const VkDevice &refDevice;
    VkPhysicalDeviceMemoryProperties2 memProperties{};
    VkPhysicalDeviceMemoryBudgetPropertiesEXT memBudjet{};
    typedef struct Memory {
        std::list<SubMemory> availableSpaces;
        std::list<VkDeviceMemory> memoryChunks;
    } Memory;
    std::mutex mtx; // General mutex, for global operations
    struct MemoryBatch {
        std::mutex mtx;
        std::array<Memory, VK_MAX_MEMORY_TYPES> memory;
        std::map<VkDeviceMemory, MappedMemory> mappedMemory;
    };
    std::vector<MemoryBatch> batch;
    const uint32_t chunkSize;
    const bool usingBatches;
};

#endif /* end of include guard: MEMORY_MANAGER_HPP */
