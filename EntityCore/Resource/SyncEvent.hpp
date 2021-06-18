/*
** EPITECH PROJECT, 2020
** EntityCore
** File description:
** SyncEvent.hpp
*/

#ifndef SYNCEVENT_HPP_
#define SYNCEVENT_HPP_

#include <vulkan/vulkan.h>
#include "EntityCore/SubMemory.hpp"
#include "EntityCore/SubBuffer.hpp"
#include <vector>

class VulkanMgr;
class Texture;

class SyncEvent {
public:
    // Create SyncEvent for event dependency
    SyncEvent(VulkanMgr *master, bool deviceOnly = true);
    // Create SyncEvent for pipeline barrier
    SyncEvent(VkDependencyFlags dependencies = 0);
    virtual ~SyncEvent();
    SyncEvent(const SyncEvent &cpy) = delete;
    SyncEvent &operator=(const SyncEvent &src) = delete;

    // ===== SETUP ===== //
    void globalBarrier(VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess);
    void bufferBarrier(SubBuffer &buffer, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess);
    void bufferBarrier(VkBuffer buffer, uint32_t offset, uint32_t size, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess);
    void imageBarrier(Texture &texture, VkImageLayout srcLayout, VkImageLayout dstLayout, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess, uint32_t miplevel = 0, uint32_t miplevelcount = VK_REMAINING_MIP_LEVELS);
    void build();

    // ===== USE ===== //
    // Event dependency
    void srcDependency(VkCommandBuffer &cmd);
    void dstDependency(VkCommandBuffer &cmd);
    void combineDstDependencies(SyncEvent &with);
    void multiDstDependency(VkCommandBuffer &cmd);
    void resetDependency(VkCommandBuffer &cmd, VkPipelineStageFlags2KHR stage);
    bool isSet();
    // Pipeline barrier
    void placeBarrier(VkCommandBuffer &cmd);
    static void setupPFN(VkInstance instance);
private:
    static PFN_vkCmdSetEvent2KHR ptr_vkCmdSetEvent2KHR;
    static PFN_vkCmdWaitEvents2KHR ptr_vkCmdWaitEvents2KHR;
    static PFN_vkCmdResetEvent2KHR ptr_vkCmdResetEvent2KHR;
    static PFN_vkCmdPipelineBarrier2KHR ptr_vkCmdPipelineBarrier2KHR;
    VulkanMgr *master = nullptr;
    VkEvent event = VK_NULL_HANDLE;
    VkDependencyInfoKHR dep;
    std::vector<VkMemoryBarrier2KHR> global;
    std::vector<VkBufferMemoryBarrier2KHR> buffers;
    std::vector<VkImageMemoryBarrier2KHR> image;
    std::vector<VkEvent> multiEvent;
    std::vector<VkDependencyInfoKHR> multiDep;
};

#endif /* SYNCEVENT_HPP_ */
