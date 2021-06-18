/*
** EPITECH PROJECT, 2020
** EntityCore
** File description:
** FrameMgr.cpp
*/
#include "VulkanMgr.hpp"
#include "FrameMgr.hpp"
#include "RenderMgr.hpp"
#include "EntityCore/Resource/Texture.hpp"

FrameMgr::FrameMgr(VulkanMgr &master, RenderMgr &renderer, int id, uint32_t width, uint32_t height, const std::string &name) :
    master(master), renderer(renderer), id(id), name(name)
{
    info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    info.width = width;
    info.height = height;
    info.layers = 1;
}

FrameMgr::~FrameMgr()
{
    if (builded) {
        vkDestroyFramebuffer(master.refDevice, framebuffer, nullptr);
        vkDestroyCommandPool(master.refDevice, graphicPool, nullptr);
        if (secondaryPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(master.refDevice, secondaryPool, nullptr);
    }
}

void FrameMgr::bind(int id, Texture &texture)
{
    if (id >= views.size())
        views.resize(id + 1);
    views[id] = texture.getView();
}

bool FrameMgr::build(bool alwaysRecord, bool useSecondary, bool staticSecondary)
{
    info.attachmentCount = views.size();
    info.pAttachments = views.data();
    info.renderPass = renderer.renderPass;
    if (vkCreateFramebuffer(master.refDevice, &info, nullptr, &framebuffer) != VK_SUCCESS) {
        master.putLog("Failed to build FrameBuffer '" + name + "'", LogType::ERROR);
        return false;
    }
    builded = true;
    VkCommandPoolCreateInfo poolInfo {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, (alwaysRecord ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : (VkCommandPoolCreateFlags) 0), (uint32_t) master.getGraphicQueueFamilyIndex()[0]};
    vkCreateCommandPool(master.refDevice, &poolInfo, nullptr, &graphicPool);
    VkCommandBufferAllocateInfo allocInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, graphicPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    vkAllocateCommandBuffers(master.refDevice, &allocInfo, &mainCmd);
    master.setObjectName(mainCmd, VK_OBJECT_TYPE_COMMAND_BUFFER, "mainCmd of " + name);
    cmdInfo.flags = (alwaysRecord) ? VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT : 0;
    if (useSecondary) {
        poolInfo.flags = (staticSecondary) ? 0 : VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCreateCommandPool(master.refDevice, &poolInfo, nullptr, &secondaryPool);
        inheritance.renderPass = renderer.renderPass;
        inheritance.framebuffer = framebuffer;
    }
    views.clear();
    views.shrink_to_fit();
    master.setObjectName(framebuffer, VK_OBJECT_TYPE_FRAMEBUFFER, name);
    return true;
}

int FrameMgr::create(uint32_t count)
{
    uint32_t first = cmds.size();
    VkCommandBufferAllocateInfo allocInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, secondaryPool, VK_COMMAND_BUFFER_LEVEL_SECONDARY, count};

    cmds.resize(first + count);
    if (vkAllocateCommandBuffers(master.refDevice, &allocInfo, cmds.data() + first) != VK_SUCCESS) {
        master.putLog("Failed to allocate command buffer", LogType::ERROR);
    }
    return (first);
}

void FrameMgr::begin(int idx, int layerIdx)
{
    inheritance.subpass = layerIdx;
    actual = cmds[idx];
    vkBeginCommandBuffer(actual, &tmpCmdInfo);
}

void FrameMgr::compile()
{
    vkEndCommandBuffer(actual);
    actual = VK_NULL_HANDLE;
}

void FrameMgr::setName(int id, const std::string &name)
{
    master.setObjectName(cmds[id], VK_OBJECT_TYPE_COMMAND_BUFFER, name);
}

void FrameMgr::discardRecord()
{
    if (secondaryPool != VK_NULL_HANDLE)
        vkResetCommandPool(master.refDevice, secondaryPool, 0);
}

VkCommandBuffer &FrameMgr::begin(VkSubpassContents content)
{
    vkResetCommandPool(master.refDevice, graphicPool, 0);
    vkBeginCommandBuffer(mainCmd, &cmdInfo);
    renderer.begin(id, mainCmd, content);
    switch (content) {
        case VK_SUBPASS_CONTENTS_INLINE:
            actual = mainCmd;
            break;
        case VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS:
            if (actual == mainCmd)
                actual = VK_NULL_HANDLE;
            break;
        default:;
    }
    return mainCmd;
}

void FrameMgr::next(VkSubpassContents content)
{
    vkCmdNextSubpass(mainCmd, content);
    switch (content) {
        case VK_SUBPASS_CONTENTS_INLINE:
            actual = mainCmd;
            break;
        case VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS:
            if (actual == mainCmd)
                actual = VK_NULL_HANDLE;
            break;
        default:;
    }
}

void FrameMgr::compileMain()
{
    vkCmdEndRenderPass(mainCmd);
    vkEndCommandBuffer(mainCmd);
    if (actual == mainCmd)
        actual = VK_NULL_HANDLE;
}
