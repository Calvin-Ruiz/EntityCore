#ifndef COLLECTOR_HPP_
#define COLLECTOR_HPP_

#include <vulkan/vulkan.h>
#include <forward_list>
#include <string>

class VulkanMgr;

// This class allow simplified creation and destruction of some vulkan objects
class Collector {
public:
    Collector(VulkanMgr &vkmgr);
    ~Collector();

    VkFence createFence(bool signaled, const std::string &name = {});
    VkSemaphore createSemaphore(const std::string &name = {});
    VkSemaphore createSemaphore(uint64_t initialValue, const std::string &name = {});
    VkCommandPool create(const VkCommandPoolCreateInfo &info, const std::string &name = {});
private:
    VulkanMgr &vkmgr;
    std::forward_list<VkFence> fences;
    std::forward_list<VkSemaphore> semaphores;
    std::forward_list<VkCommandPool> commandPools;
};

#endif /* end of include guard: COLLECTOR_HPP_ */
