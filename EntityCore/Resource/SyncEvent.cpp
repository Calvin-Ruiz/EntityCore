/*
** EPITECH PROJECT, 2020
** EntityCore
** File description:
** SyncEvent.cpp
*/
#include "SyncEvent.hpp"
#include "EntityCore/Core/VulkanMgr.hpp"
#include "Texture.hpp"

PFN_vkCmdSetEvent2KHR SyncEvent::ptr_vkCmdSetEvent2KHR = nullptr;
PFN_vkCmdWaitEvents2KHR SyncEvent::ptr_vkCmdWaitEvents2KHR = nullptr;
PFN_vkCmdResetEvent2KHR SyncEvent::ptr_vkCmdResetEvent2KHR = nullptr;
PFN_vkCmdPipelineBarrier2KHR SyncEvent::ptr_vkCmdPipelineBarrier2KHR = nullptr;

void SyncEvent::setupPFN(VkInstance instance)
{
    SETUP_PFN(PFN_vkCmdSetEvent2KHR, ptr_vkCmdSetEvent2KHR, "vkCmdSetEvent2KHR");
    SETUP_PFN(PFN_vkCmdWaitEvents2KHR, ptr_vkCmdWaitEvents2KHR, "vkCmdWaitEvents2KHR");
    SETUP_PFN(PFN_vkCmdResetEvent2KHR, ptr_vkCmdResetEvent2KHR, "vkCmdResetEvent2KHR");
    SETUP_PFN(PFN_vkCmdPipelineBarrier2KHR, ptr_vkCmdPipelineBarrier2KHR, "vkCmdPipelineBarrier2KHR");
}

SyncEvent::SyncEvent(VulkanMgr *master, bool deviceOnly) : master(master)
{
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dep.pNext = nullptr;
    dep.dependencyFlags = 0;
    VkEventCreateInfo info {VK_STRUCTURE_TYPE_EVENT_CREATE_INFO, nullptr, (deviceOnly) ? VK_EVENT_CREATE_DEVICE_ONLY_BIT_KHR : (VkEventCreateFlags) 0};
    vkCreateEvent(master->refDevice, &info, nullptr, &event);
}

SyncEvent::SyncEvent(VkDependencyFlags dependencies)
{
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dep.pNext = nullptr;
    dep.dependencyFlags = dependencies;
}

SyncEvent::~SyncEvent()
{
    if (event != VK_NULL_HANDLE)
        vkDestroyEvent(master->refDevice, event, nullptr);
}

void SyncEvent::globalBarrier(VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess)
{
    global.push_back({VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR, nullptr, srcStage, srcAccess, dstStage, dstAccess});
}

void SyncEvent::bufferBarrier(SubBuffer &buffer, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess)
{
    buffers.push_back({VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR, nullptr, srcStage, srcAccess, dstStage, dstAccess, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, buffer.buffer, (VkDeviceSize) buffer.offset, (VkDeviceSize) buffer.size});
}

void SyncEvent::bufferBarrier(VkBuffer buffer, uint32_t offset, uint32_t size, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess)
{
    buffers.push_back({VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR, nullptr, srcStage, srcAccess, dstStage, dstAccess, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, buffer, offset, size});
}

void SyncEvent::imageBarrier(Texture &texture, VkImageLayout srcLayout, VkImageLayout dstLayout, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess, uint32_t miplevel, uint32_t miplevelcount)
{
    image.push_back({VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR, nullptr, srcStage, srcAccess, dstStage, dstAccess, srcLayout, dstLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, texture.getImage(), {texture.getAspect(), miplevel, miplevelcount, 0, 1}});
}

void SyncEvent::build()
{
    dep.memoryBarrierCount = global.size();
    dep.pMemoryBarriers = global.data();
    dep.bufferMemoryBarrierCount = buffers.size();
    dep.pBufferMemoryBarriers = buffers.data();
    dep.imageMemoryBarrierCount = image.size();
    dep.pImageMemoryBarriers = image.data();
}

void SyncEvent::srcDependency(VkCommandBuffer &cmd)
{
    ptr_vkCmdSetEvent2KHR(cmd, event, &dep);
}

void SyncEvent::dstDependency(VkCommandBuffer &cmd)
{
    ptr_vkCmdWaitEvents2KHR(cmd, 1, &event, &dep);
}

void SyncEvent::resetDependency(VkCommandBuffer &cmd, VkPipelineStageFlags2KHR stage)
{
    ptr_vkCmdResetEvent2KHR(cmd, event, stage);
}

void SyncEvent::placeBarrier(VkCommandBuffer &cmd)
{
    ptr_vkCmdPipelineBarrier2KHR(cmd, &dep);
}

bool SyncEvent::isSet()
{
    return ((vkGetEventStatus(master->refDevice, event) == VK_EVENT_SET) ? true : false);
}
