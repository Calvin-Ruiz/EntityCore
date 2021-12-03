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
#include <thread>
#include "EntityCore/Tools/SafeQueue.hpp"

class VulkanMgr;
class RenderMgr;
class Texture;
class SyncEvent;

/*
** Manage frame-local resources
** CommandPool must came from here
*/
class FrameMgr {
public:
    // if submitFunc is not null, it is used
    FrameMgr(VulkanMgr &master, RenderMgr &renderer, int id, uint32_t width, uint32_t height, const std::string &name = "Default", void (*submitFunc)(void *data, int id) = nullptr, void *data = nullptr);
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
    bool build(uint32_t queueFamily = UINT32_MAX, bool alwaysRecord = false, bool useSecondary = false, bool staticSecondary = true);

    // ===== USE ===== //
    // Create count subCommands and return the id of the first newly created subCommand
    int create(uint32_t count);
    // Start recording subCommand for use in layerIdx layer
    void begin(VkCommandBuffer cmd, int layerIdx);
    // Slower than begin, this can be called concurrently to begin and beginAsync calls
    void beginAsync(VkCommandBuffer cmd, int layerIdx);
    // Start recording subCommand for use in layerIdx layer
    VkCommandBuffer &begin(int idx, int layerIdx);
    // Compile subCommand
    void compile(int idx = -1);
    // Compile subCommand
    void compile(VkCommandBuffer &cmd);
    // Set name to command
    void setName(int idx, const std::string &name);
    // Set name to command
    void setName(VkCommandBuffer &cmd, const std::string &name);
    // Reset every command previously recorded
    void discardRecord();
    // Get handle of subCommand
    VkCommandBuffer getHandle(int idx) {return cmds[idx];}

    // --- main command --- //
    // Create a primary CommandBuffer which will be resetted when starting recording the main command
    VkCommandBuffer createMain();
    // Start recording
    VkCommandBuffer &begin(VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE, int nbTextures = 0, Texture **textures = nullptr, SyncEvent *sync = nullptr);
    // Start recording, without starting render pass, can be used instead of begin
    VkCommandBuffer preBegin();
    // Start render pass, must be called after preBegin
    void postBegin(VkSubpassContents content = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
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

    // ===== Thread helper mechanism ===== //
    // Start thread helper, allow use of the functions below
    // Note : When using a helper, don't use any function of the "main command" outside of the submitFunc, except begin()
    static void startHelper();
    static void stopHelper();
    inline void toExecute(int idx) {
        batches[batch].push_back(cmds[idx]);
    }
    inline void toExecute(int idx, int layerIdx) {
        batches[layerIdx].push_back(cmds[idx]);
    }
    inline void toExecute(VkCommandBuffer cmd, int layerIdx) {
        batches[layerIdx].push_back(cmd);
    }
    // Cancel execution of one or several commands
    // Must be called before submit() or submitInline()
    // The commands must have all been pre-submitted with toExecute, and must be sorted in layer order, then in pre-submission order
    void cancelExecution(std::vector<VkCommandBuffer> &cmds);
    // Next subpass which is VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS
    inline void nextPass() {
        ++batch;
    }
    // Submit command, apply every actions and call submitFunc in the helper thread
    // Your submitFunc must call either compileMain or call vkCmdEndRenderPass and vkEndCommandBuffer in the main command
    void submit();
    // Like submit, but execution is executed in this thread instead of the helper thread
    void submitInline();
    // Return true if the last submission has completed, don't call begin() on the main command while it return false
    inline bool isDone() const {
        return submitted;
    }
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

    // Execution builder helper
    static void helperMainloop();
    std::vector<std::vector<VkCommandBuffer>> batches;
    void (*submitFunc)(void *data, int id);
    void *data;
    static std::thread helper;
    static WorkQueue<FrameMgr *, 7> queue;
    int batch = 0;
    bool submitted = true;
};

#endif /* FRAMEMGR_HPP_ */
