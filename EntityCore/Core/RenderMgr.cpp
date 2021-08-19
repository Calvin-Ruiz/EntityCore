/*
** EPITECH PROJECT, 2020
** EntityCore
** File description:
** RenderMgr.cpp
*/
#include "RenderMgr.hpp"
#include "VulkanMgr.hpp"

RenderMgr::RenderMgr(VulkanMgr &master) : master(master)
{
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    layers.resize(1);
}

RenderMgr::~RenderMgr()
{
    if (builded) {
        vkDestroyRenderPass(master.refDevice, renderPass, nullptr);
    }
}

int RenderMgr::attach(VkFormat format, VkSampleCountFlagBits samples, VkImageLayout initialLayout, VkImageLayout finalLayout)
{
    attachment.push_back({0, format, samples,
        (initialLayout == VK_IMAGE_LAYOUT_UNDEFINED) ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_LOAD,
        (finalLayout == VK_IMAGE_LAYOUT_UNDEFINED) ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, initialLayout, finalLayout});
    return (attachment.size() - 1);
}

void RenderMgr::setupClear(int id, VkClearColorValue color)
{
    attachment[id].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    if (clears.size() <= id)
        clears.resize(id + 1);
    clears[id].color = color;
}

void RenderMgr::setupClear(int id, float value)
{
    attachment[id].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    if (clears.size() <= id)
        clears.resize(id + 1);
    clears[id].depthStencil = {value, 0};
}

void RenderMgr::addDependencyFrom(int id, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess, bool framebufferLocal)
{
    dep.push_back({(id == -1) ? VK_SUBPASS_EXTERNAL : id, (uint32_t) (subpass + 1), srcStage, dstStage, srcAccess, dstAccess, (framebufferLocal) ? VK_DEPENDENCY_BY_REGION_BIT : (VkDependencyFlags) 0});
}

void RenderMgr::addDependency(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess, bool framebufferLocal)
{
    dep.push_back({(subpass == -1) ? VK_SUBPASS_EXTERNAL : subpass, (uint32_t) (subpass + 1), srcStage, dstStage, srcAccess, dstAccess, (framebufferLocal) ? VK_DEPENDENCY_BY_REGION_BIT : (VkDependencyFlags) 0});
}

void RenderMgr::addSelfDependency(VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess)
{
    dep.push_back({(uint32_t) subpass, (uint32_t) subpass, srcStage, dstStage, srcAccess, dstAccess, VK_DEPENDENCY_BY_REGION_BIT});
}

void RenderMgr::bindInput(int id, VkImageLayout layout)
{
    layers.back().inputAttachment.push_back({(uint32_t) id, layout});
}

void RenderMgr::bindColor(int id, VkImageLayout layout)
{
    layers.back().colorAttachment.push_back({(uint32_t) id, layout});
}

void RenderMgr::bindDepth(int id, VkImageLayout layout)
{
    layers.back().depthAttachment.push_back({(uint32_t) id, layout});
}

void RenderMgr::bindResolveDst(int id, VkImageLayout layout)
{
    layers.back().resolveAttachment.push_back({(uint32_t) id, layout});
}

void RenderMgr::bindPreserve(int id)
{
    layers.back().preserveAttachment.push_back(id);
}

void RenderMgr::pushLayer(VkPipelineBindPoint type)
{
    auto &tmp = layers.back();
    subpasses.push_back({0, type, (uint32_t) tmp.inputAttachment.size(), tmp.inputAttachment.data(),
        (uint32_t) tmp.colorAttachment.size(), tmp.colorAttachment.data(),
        ((tmp.resolveAttachment.empty()) ? nullptr : tmp.resolveAttachment.data()),
        ((tmp.depthAttachment.empty()) ? nullptr : tmp.depthAttachment.data()),
        (uint32_t) tmp.preserveAttachment.size(), tmp.preserveAttachment.data()});
    layers.resize(++subpass + 2);
}

void RenderMgr::bind(int bindID, VkFramebuffer frameBuffer, VkRect2D renderArea)
{
    infos[bindID] = VkRenderPassBeginInfo({VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, renderPass, frameBuffer, renderArea, (uint32_t) clears.size(), clears.data()});
}

bool RenderMgr::build(int maxFrameBuffer)
{
    {
        ++subpass;
        const auto end = dep.rend();
        for (auto it = dep.rbegin(); it != end; ++it) {
            if (it->dstSubpass != subpass)
                break;
            it->dstSubpass = VK_SUBPASS_EXTERNAL;
        }
    }
    info.attachmentCount = attachment.size();
    info.pAttachments = attachment.data();
    info.subpassCount = subpasses.size();
    info.pSubpasses = subpasses.data();
    info.dependencyCount = dep.size();
    info.pDependencies = dep.data();
    if (vkCreateRenderPass(master.refDevice, &info, nullptr, &renderPass) != VK_SUCCESS) {
        master.putLog("Failed to build RenderPass", LogType::ERROR);
        return false;
    }
    builded = true;
    layers.clear();
    layers.shrink_to_fit();
    attachment.clear();
    attachment.shrink_to_fit();
    subpasses.clear();
    subpasses.shrink_to_fit();
    dep.clear();
    dep.shrink_to_fit();
    infos.resize(maxFrameBuffer);
    return true;
}
