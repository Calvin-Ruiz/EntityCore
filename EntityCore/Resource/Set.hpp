#ifndef SET_HPP
#define SET_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <forward_list>
#include "EntityCore/SubBuffer.hpp"

class VulkanMgr;
class SetMgr;
class PipelineLayout;
class Texture;
template <typename T> class Uniform;

/**
*   \brief Handle binding for pipelineLayout layout
*   Include uniform, uniform texture and storage buffer
*/
class Set {
public:
    //! Create a set which can only be push
    Set() = default;
    //! create a Set which can be bind
    //! Do not bind anything to it while bound to commandBuffer which can be submitted
    //! If not temporary, the Set memory won't be available for another Set when destroyed
    Set(VulkanMgr &master, SetMgr &mgr, PipelineLayout *_layout, int setBinding = -1, bool initialize = true, bool temporary = false);
    ~Set();
    //! Bind uniform to this set
    template <typename T>
    void bindUniform(std::unique_ptr<Uniform<T>> &buffer, uint32_t binding) {
        bindUniform(buffer->getBuffer(), binding);
    }
    //! Bind uniform to this set
    void bindUniform(SubBuffer &buffer, uint32_t binding, uint32_t range = -1, int offset = 0);
    //! Bind texture to this set
    void bindTexture(Texture &texture, uint32_t binding, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    //! Bind storage buffer to this set
    void bindStorageBuffer(SubBuffer &buffer, uint32_t binding, uint32_t range, int offset = 0);
    //! Bind uniform location
    //! @return virtualUniformId used as reference in setVirtualUniform
    int bindVirtualUniform(SubBuffer &buffer, uint32_t binding, uint32_t range, uint32_t arraySize = 1);
    //! Attach uniform to uniform location, can be used regardless to previous binding to commandBuffer
    void setVirtualUniform(int offset, int virtualUniformID);
    //void bindTexture(TextureImage *texture, int binding);
    VkDescriptorSet *get();
    //! Internal use only
    std::vector<uint32_t> &getDynamicOffsets() {return dynamicOffsets;}
    //! Internal use only
    std::vector<VkWriteDescriptorSet> &getWrites() {return writeSet;}
    //! For ThreadedCommandBuilder only
    void swapWrites(std::vector<VkWriteDescriptorSet> &writeSetExt) {writeSet.swap(writeSetExt);}
    //! Remove all previous binding, for push set only
    void clear() {writeSet.clear();}
    //! Manually update bindings
    void update();
    //! Destroy descriptor set hold, require the Set to be temporary
    void uninit();
private:
    //! Allocate descriptorSet from SetMgr
    void init();
    // create descriptorSet, return true on success
    bool createDescriptorSet();
    VulkanMgr &master;
    SetMgr &mgr;
    VkDescriptorSetAllocateInfo allocInfo{};
    std::forward_list<VkDescriptorBufferInfo> bufferInfo;
    std::forward_list<VkDescriptorImageInfo> imageInfo;
    std::vector<VkWriteDescriptorSet> writeSet;
    std::vector<uint32_t> dynamicOffsets;
    VkDescriptorSet set = VK_NULL_HANDLE;
    bool initialized = false;
    bool temporary = false;
};

#endif /* end of include guard: SET_HPP */
