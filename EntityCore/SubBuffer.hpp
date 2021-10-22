#ifndef SUB_BUFFER_HPP
#define SUB_BUFFER_HPP

#include <vulkan/vulkan.h>

struct SubBuffer {
    VkBuffer buffer; // The VkBuffer from which the SubBuffer owned a portion
    uint32_t offset; // The offset in the VkBuffer of the owned portion by this SubBuffer
    uint32_t size; // The size in the VkBuffer of the portion owned by this SubBuffer
};

#endif /* end of include guard: SUB_BUFFER_HPP */
