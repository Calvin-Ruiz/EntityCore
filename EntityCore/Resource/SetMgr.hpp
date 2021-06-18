#ifndef SET_MGR_HPP
#define SET_MGR_HPP

#include <vulkan/vulkan.h>

class VulkanMgr;

class SetMgr {
public:
    SetMgr(VulkanMgr &master, int maxSet, int maxUniformSet = 0, int maxTextureSet = 0, int maxStorageBufferSet = 0);
    ~SetMgr();
    //! Internal use only
    VkDescriptorPool &getDescriptorPool() {return pool;}
private:
    VulkanMgr &master;
    VkDescriptorPool pool;
};

#endif /* end of include guard: SET_MGR_HPP */
