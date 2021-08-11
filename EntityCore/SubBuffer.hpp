#ifndef SUB_BUFFER_HPP
#define SUB_BUFFER_HPP

#include <vulkan/vulkan.h>

struct SubBuffer {
    VkBuffer buffer; // The VkBuffer from which the SubBuffer owned a portion
    int offset; // The offset in the VkBuffer of the owned portion by this SubBuffer
    int size; // The size in the VkBuffer of the portion owned by this SubBuffer
    bool possibleUniform; // INTERNALLY USED, Tell if uniform can be allocated in this subBuffer
};

#endif /* end of include guard: SUB_BUFFER_HPP */
