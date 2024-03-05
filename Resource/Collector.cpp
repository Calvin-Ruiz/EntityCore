#include "Collector.hpp"
#include "EntityCore/Core/VulkanMgr.hpp"

Collector::Collector(VulkanMgr &vkmgr) :
    vkmgr(vkmgr)
{}

Collector::~Collector()
{
    VkDevice device = vkmgr.refDevice;
    for (VkFence f : fences)
        vkDestroyFence(device, f, nullptr);
    for (VkSemaphore s : semaphores)
        vkDestroySemaphore(device, s, nullptr);
    for (VkCommandPool cp : commandPools)
        vkDestroyCommandPool(device, cp, nullptr);
}

VkFence Collector::createFence(bool signaled, const std::string &name)
{
    VkFence fence;
    VkFenceCreateInfo info {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, (signaled) ? static_cast<VkFenceCreateFlags>(VK_FENCE_CREATE_SIGNALED_BIT) : 0u};
    if (vkCreateFence(vkmgr.refDevice, &info, nullptr, &fence) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    if (!name.empty())
        vkmgr.setObjectName(fence, VK_OBJECT_TYPE_FENCE, name);
    fences.push_front(fence);
    return fence;
}

VkSemaphore Collector::createSemaphore(const std::string &name)
{
    VkSemaphore semaphore;
    VkSemaphoreCreateInfo info {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};
    if (vkCreateSemaphore(vkmgr.refDevice, &info, nullptr, &semaphore) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    if (!name.empty())
        vkmgr.setObjectName(semaphore, VK_OBJECT_TYPE_SEMAPHORE, name);
    semaphores.push_front(semaphore);
    return semaphore;
}

VkSemaphore Collector::createSemaphore(uint64_t initialValue, const std::string &name)
{
    VkSemaphore semaphore;
    VkSemaphoreTypeCreateInfo typeInfo {VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr, VK_SEMAPHORE_TYPE_TIMELINE_KHR, initialValue};
    VkSemaphoreCreateInfo info {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &typeInfo, 0};
    if (vkCreateSemaphore(vkmgr.refDevice, &info, nullptr, &semaphore) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    if (!name.empty())
        vkmgr.setObjectName(semaphore, VK_OBJECT_TYPE_SEMAPHORE, name);
    semaphores.push_front(semaphore);
    return semaphore;
}

VkCommandPool Collector::create(const VkCommandPoolCreateInfo &info, const std::string &name)
{
    VkCommandPool commandPool;
    if (vkCreateCommandPool(vkmgr.refDevice, &info, nullptr, &commandPool) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    if (!name.empty())
        vkmgr.setObjectName(commandPool, VK_OBJECT_TYPE_COMMAND_POOL, name);
    commandPools.push_front(commandPool);
    return commandPool;
}
