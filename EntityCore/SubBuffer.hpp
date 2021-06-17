#ifndef SUB_BUFFER_HPP
#define SUB_BUFFER_HPP

struct SubBuffer {
    VkBuffer buffer;
    int offset;
    int size;
    bool possibleUniform; // Tell if uniform can be allocated in this subBuffer
};

#endif /* end of include guard: SUB_BUFFER_HPP */
