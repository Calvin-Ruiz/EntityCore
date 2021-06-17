#ifndef BUFFER_CPY_HPP
#define BUFFER_CPY_HPP

struct BufferCpy {
    int arraySize;
    VkBuffer src;
    VkBuffer dst;
    VkBufferCopy region;
};

struct MultiBufferCpy {
    int arraySize;
    VkBuffer src;
    VkBuffer dst;
    std::vector<VkBufferCopy> regions;
};

struct ImageCpy {
    int arraySize;
    VkImageLayout initialLayout;
    VkImageLayout finalLayout;
    VkBuffer buffer;
    VkImage image;
    std::vector<VkBufferImageCopy> regions;
};

#endif /* end of include guard: BUFFER_CPY_HPP */
