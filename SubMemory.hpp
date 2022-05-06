#ifndef SUB_MEMORY_HPP
#define SUB_MEMORY_HPP

struct SubMemory {
    VkDeviceMemory memory; // The VkDeviceMemory from which the SubMemory owned a portion
    VkDeviceSize offset; // The offset in the VkDeviceMemory of the owned portion by this SubMemory
    VkDeviceSize size; // The size in the VkDeviceMemory of the portion owned by this SubMemory
    uint32_t memoryIndex; // The memory index of the VkDeviceMemory
    uint32_t memoryBatch; // The batch this SubMemory come from
};

#endif /* end of include guard: SUB_MEMORY_HPP */
