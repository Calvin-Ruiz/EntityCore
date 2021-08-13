#ifndef SET_MGR_HPP
#define SET_MGR_HPP

#include <vulkan/vulkan.h>

class VulkanMgr;

class SetMgr {
public:
    SetMgr(VulkanMgr &master, int maxSet, uint32_t maxUniformSet = 0, uint32_t maxTextureSet = 0, uint32_t maxStorageBufferSet = 0, bool temporarySets = false);
    ~SetMgr();
    //! Effectively destroy previously created descriptor set
    void update();
    //! Internal use only
    void destroySet(VkDescriptorSet &set);
    //! Internal use only
    VkDescriptorPool &getDescriptorPool() {return pool;}
private:
    VulkanMgr &master;
    VkDescriptorPool pool;
    std::vector<VkDescriptorSet> toDestroy;
};

#endif /* end of include guard: SET_MGR_HPP */
