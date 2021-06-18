#include "EntityCore/Core/VulkanMgr.hpp"
#include "SetMgr.hpp"

SetMgr::SetMgr(VulkanMgr &master, int maxSet, int maxUniformSet, int maxTextureSet, int maxStorageBufferSet) : master(master)
{
    std::vector<VkDescriptorPoolSize> poolSize;
    if (maxUniformSet)
        poolSize.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxUniformSet});
    if (maxTextureSet)
        poolSize.push_back({VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxTextureSet});
    if (maxStorageBufferSet)
        poolSize.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, maxStorageBufferSet});

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = poolSize.size();
    poolInfo.pPoolSizes = poolSize.data();
    poolInfo.maxSets = maxSet;

    if (vkCreateDescriptorPool(master.refDevice, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("echec de la creation de la pool de descripteurs!");
    }
}

SetMgr::~SetMgr()
{
    vkDeviceWaitIdle(master.refDevice);
    vkDestroyDescriptorPool(master.refDevice, pool, nullptr);
}
