#include "EntityCore/Core/VulkanMgr.hpp"
#include "SetMgr.hpp"

SetMgr::SetMgr(VulkanMgr &master, int maxSet, uint32_t maxUniformSet, uint32_t maxTextureSet, uint32_t maxStorageBufferSet, bool temporarySets) : master(master)
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
    if (temporarySets) {
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    }

    if (vkCreateDescriptorPool(master.refDevice, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("echec de la creation de la pool de descripteurs!");
    }
}

SetMgr::~SetMgr()
{
    vkDestroyDescriptorPool(master.refDevice, pool, nullptr);
}

void SetMgr::update()
{
    if (!toDestroy.empty()) {
        vkFreeDescriptorSets(master.refDevice, pool, toDestroy.size(), toDestroy.data());
        toDestroy.clear();
    }
}

void SetMgr::destroySet(VkDescriptorSet &set)
{
    toDestroy.push_back(set);
}
