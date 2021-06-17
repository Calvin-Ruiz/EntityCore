#ifndef SUB_MEMORY_HPP
#define SUB_MEMORY_HPP

struct SubMemory {
    VkDeviceMemory memory;
    VkDeviceSize offset;
    VkDeviceSize size;
    uint32_t memoryIndex;
};

#endif /* end of include guard: SUB_MEMORY_HPP */
