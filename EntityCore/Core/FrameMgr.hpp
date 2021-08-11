/*
** EPITECH PROJECT, 2020
** EntityCore
** File description:
** FrameMgr.hpp
*/

#ifndef FRAMEMGR_HPP_
#define FRAMEMGR_HPP_

#include <string>
#include <vulkan/vulkan.h>
#include <vector>

class VulkanMgr;
class RenderMgr;
class Texture;

/*
** Manage frame-local resources
** CommandPool must came from here
*/
class FrameMgr {
public:
    FrameMgr(VulkanMgr &master, RenderMgr &renderer, int id, uint32_t width, uint32_t height, const std::string &name = "Default");
    virtual ~FrameMgr();
    FrameMgr(const FrameMgr &cpy) = delete;
    FrameMgr &operator=(const FrameMgr &src) = delete;

    // ===== SETUP ===== //
    // Bind texture at the RenderMgr's id
    void bind(int id, Texture &texture);
    void bind(int id, VkImageView &view);
    // alwaysRecord : record the main command every frame
    // useSecondary : enable use of subCommand
    // staticSecondary : if true, the only way to re-record a subCommand is to call discardRecord and record every subCommand again
    bool build(uint32_t queueFamily, bool alwaysRecord = false, bool useSecondary = false, bool staticSecondary = true);

    // ===== USE ===== //
    // Create count subCommands and return the id of the first newly created subCommand
    int create(uint32_t count);
    // Start recording subCommand for use in layerIdx layer
    void begin(int idx, int layerIdx);
    // Compile subCommand
    void compile();
    // Set name to command
    void setName(int idx, const std::string &name);
    // Reset every command previously recorded
    void discardRecord();
    // Get handle of subCommand
    VkCommandBuffer getHandle(int idx) {return cmds[idx];}

    // Start recording
    VkCommandBuffer &begin(VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE, int nbTextures = 0, Texture **textures = nullptr);
    // Next rendering layer
    void next(VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE);
    // Execute command
    inline void execute(int idx) {
        vkCmdExecuteCommands(mainCmd, 1, &cmds[idx]);
    }
    inline void execute(VkCommandBuffer *cmds, int nbCmds) {
        vkCmdExecuteCommands(mainCmd, nbCmds, cmds);
    }
    // Compile main command
    void compileMain();
    // Get main command
    VkCommandBuffer getMainHandle() {return mainCmd;}
    // Submit command
    void submit();
private:
    VulkanMgr &master;
    RenderMgr &renderer;
    VkCommandBufferBeginInfo cmdInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};
    VkCommandBufferInheritanceInfo inheritance {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, nullptr, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, VK_FALSE, 0, 0};
    VkCommandBufferBeginInfo tmpCmdInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT, &inheritance};
    VkFramebufferCreateInfo info {};
    const int id;
    const std::string name;
    VkFramebuffer framebuffer;
    std::vector<VkImageView> views;
    bool builded = false;
    // Graphic commands
    VkCommandPool graphicPool = VK_NULL_HANDLE;
    VkCommandBuffer mainCmd = VK_NULL_HANDLE;
    VkCommandPool secondaryPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> cmds;
    VkCommandBuffer actual = VK_NULL_HANDLE;
};

#endif /* FRAMEMGR_HPP_ */
