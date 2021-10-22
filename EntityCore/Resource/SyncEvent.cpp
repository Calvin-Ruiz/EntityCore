/*
** EPITECH PROJECT, 2020
** EntityCore
** File description:
** SyncEvent.cpp
*/
#include "SyncEvent.hpp"
#include "EntityCore/Core/VulkanMgr.hpp"
#include "EntityCore/Core/BufferMgr.hpp"
#include "Texture.hpp"

#define STAGE_TRANSFER (VK_PIPELINE_STAGE_2_COPY_BIT_KHR | VK_PIPELINE_STAGE_2_BLIT_BIT_KHR | VK_PIPELINE_STAGE_2_RESOLVE_BIT_KHR | VK_PIPELINE_STAGE_2_CLEAR_BIT_KHR)

PFN_vkCmdSetEvent2KHR SyncEvent::ptr_vkCmdSetEvent2KHR = nullptr;
PFN_vkCmdWaitEvents2KHR SyncEvent::ptr_vkCmdWaitEvents2KHR = nullptr;
PFN_vkCmdResetEvent2KHR SyncEvent::ptr_vkCmdResetEvent2KHR = nullptr;
PFN_vkCmdPipelineBarrier2KHR SyncEvent::ptr_vkCmdPipelineBarrier2KHR = nullptr;
bool SyncEvent::enabled = false;

void SyncEvent::setupPFN(VkInstance instance)
{
    if (enabled) {
        SETUP_PFN(PFN_vkCmdSetEvent2KHR, ptr_vkCmdSetEvent2KHR, "vkCmdSetEvent2KHR");
        SETUP_PFN(PFN_vkCmdWaitEvents2KHR, ptr_vkCmdWaitEvents2KHR, "vkCmdWaitEvents2KHR");
        SETUP_PFN(PFN_vkCmdResetEvent2KHR, ptr_vkCmdResetEvent2KHR, "vkCmdResetEvent2KHR");
        SETUP_PFN(PFN_vkCmdPipelineBarrier2KHR, ptr_vkCmdPipelineBarrier2KHR, "vkCmdPipelineBarrier2KHR");
        VulkanMgr::instance->putLog("Advanced event enabled", LogType::DEBUG);
    } else {
        VulkanMgr::instance->putLog("Advanced event not supported, using compatibility mode", LogType::WARNING);
    }
}

SyncEvent::SyncEvent(VulkanMgr *master, bool deviceOnly) : master(master)
{
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;
    dep.pNext = nullptr;
    dep.dependencyFlags = 0;
    VkEventCreateInfo info {VK_STRUCTURE_TYPE_EVENT_CREATE_INFO, nullptr, (deviceOnly && enabled) ? VK_EVENT_CREATE_DEVICE_ONLY_BIT_KHR : (VkEventCreateFlagBits) 0};
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

void SyncEvent::bufferBarrier(BufferMgr &buffer, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess)
{
    buffers.push_back({VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR, nullptr, srcStage, srcAccess, dstStage, dstAccess, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, buffer.getBuffer(), 0, VK_WHOLE_SIZE});
}

void SyncEvent::imageBarrier(Texture &texture, VkImageLayout srcLayout, VkImageLayout dstLayout, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess, uint32_t miplevel, uint32_t miplevelcount)
{
    image.push_back({VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR, nullptr, srcStage, srcAccess, dstStage, dstAccess, srcLayout, dstLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, texture.getImage(), {texture.getAspect(), miplevel, miplevelcount, 0, 1}});
}

void SyncEvent::build()
{
    if (enabled) {
        dep.memoryBarrierCount = global.size();
        dep.pMemoryBarriers = global.data();
        dep.bufferMemoryBarrierCount = buffers.size();
        dep.pBufferMemoryBarriers = buffers.data();
        dep.imageMemoryBarrierCount = image.size();
        dep.pImageMemoryBarriers = image.data();
    } else {
        // compatibility mode
        for (auto &b : global) {
            compatGlobal.push_back({VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, compatConvAccess(b.srcAccessMask), compatConvAccess(b.dstAccessMask)});
            compatSrc |= compatConvStage(b.srcStageMask);
            compatDst |= compatConvStage(b.dstStageMask);
        }
        for (auto &b : buffers) {
            compatBuffers.push_back({VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, compatConvAccess(b.srcAccessMask), compatConvAccess(b.dstAccessMask), b.srcQueueFamilyIndex, b.dstQueueFamilyIndex, b.buffer, b.offset, b.size});
            compatSrc |= compatConvStage(b.srcStageMask);
            compatDst |= compatConvStage(b.dstStageMask);
        }
        for (auto &b : image) {
            compatImage.push_back({VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, compatConvAccess(b.srcAccessMask), compatConvAccess(b.dstAccessMask), b.oldLayout, b.newLayout, b.srcQueueFamilyIndex, b.dstQueueFamilyIndex, b.image, b.subresourceRange});
            compatSrc |= compatConvStage(b.srcStageMask);
            compatDst |= compatConvStage(b.dstStageMask);
        }
        if (!compatSrc)
            compatSrc = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if (!compatDst)
            compatDst = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
}

void SyncEvent::srcDependency(VkCommandBuffer &cmd)
{
    if (enabled)
        ptr_vkCmdSetEvent2KHR(cmd, event, &dep);
    else {
        vkCmdSetEvent(cmd, event, compatSrc);
    }
}

void SyncEvent::dstDependency(VkCommandBuffer &cmd)
{
    if (enabled)
        ptr_vkCmdWaitEvents2KHR(cmd, 1, &event, &dep);
    else {
        vkCmdWaitEvents(cmd, 1, &event, compatSrc, compatDst, compatGlobal.size(), compatGlobal.data(), compatBuffers.size(), compatBuffers.data(), compatImage.size(), compatImage.data());
    }
}

void SyncEvent::multiDstDependency(VkCommandBuffer &cmd)
{
    if (enabled)
        ptr_vkCmdWaitEvents2KHR(cmd, multiEvent.size(), multiEvent.data(), multiDep.data());
    else {
        dstDependency(cmd);
        for (auto &s : compatMulti)
            s->dstDependency(cmd);
    }
}

void SyncEvent::combineDstDependencies(SyncEvent &with)
{
    if (enabled) {
        if (multiDep.empty()) {
            multiEvent.push_back(event);
            multiDep.push_back(dep);
        }
        multiEvent.push_back(with.event);
        multiDep.push_back(with.dep);
    } else {
        compatMulti.push_back(&with);
    }
}

void SyncEvent::resetDependency(VkCommandBuffer &cmd, VkPipelineStageFlags2KHR stage)
{
    if (enabled)
        ptr_vkCmdResetEvent2KHR(cmd, event, stage);
    else
        vkCmdResetEvent(cmd, event, compatConvStage(stage));
}

void SyncEvent::placeBarrier(VkCommandBuffer &cmd)
{
    if (enabled)
        ptr_vkCmdPipelineBarrier2KHR(cmd, &dep);
    else {
        vkCmdPipelineBarrier(cmd, compatSrc, compatDst, dep.dependencyFlags, compatGlobal.size(), compatGlobal.data(), compatBuffers.size(), compatBuffers.data(), compatImage.size(), compatImage.data());
    }
}

bool SyncEvent::isSet()
{
    return (vkGetEventStatus(master->refDevice, event) == VK_EVENT_SET);
}

VkPipelineStageFlags SyncEvent::compatConvStage(VkPipelineStageFlags2KHR stage)
{
    VkPipelineStageFlags ret = stage & 0x0001ffff;

    if (stage & STAGE_TRANSFER)
        ret |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    if (stage & (VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR | VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR))
        ret |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    if (stage & ~(STAGE_TRANSFER | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR | VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR | 0x0001ffff))
        master->putLog("Unhandled pipeline stage compatibility conversion", LogType::ERROR);
    return ret;
}

VkAccessFlags SyncEvent::compatConvAccess(VkAccessFlags2KHR access)
{
    VkAccessFlags ret = access & 0x0001ffff;
    if (access & (VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR | VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR))
        ret |= VK_ACCESS_SHADER_READ_BIT;
    if (access & VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR)
        ret |= VK_ACCESS_SHADER_WRITE_BIT;
    if (access & ~(VK_ACCESS_2_SHADER_SAMPLED_READ_BIT_KHR | VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR | 0x0001ffff))
        master->putLog("Unhandled access compatibility conversion", LogType::ERROR);
    return ret;
}
