/*
** EPITECH PROJECT, 2020
** EntityCore
** File description:
** RenderMgr.hpp
*/

#ifndef RENDERMGR_HPP_
#define RENDERMGR_HPP_

#include <vulkan/vulkan.h>
#include <vector>

class VulkanMgr;
class Texture;

#define MAX_FB_BIND 3

class RenderMgr {
public:
    RenderMgr(VulkanMgr &master);
    virtual ~RenderMgr();
    RenderMgr(const RenderMgr &cpy) = delete;
    RenderMgr &operator=(const RenderMgr &src) = delete;

    // ===== SETUP ===== //
    // Attach a resources and return his attachment id
    int attach(VkFormat format, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED);
    // Color attachment
    void setupClear(int id, VkClearColorValue color);
    // Depth buffer
    void setupClear(int id, float value);
    // Add dependency between actual layer and next layer
    void addDependency(VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage, VkAccessFlagBits srcAccess, VkAccessFlagBits dstAccess, bool framebufferLocal = true);
    // Add dependency inside this layer
    void addSelfDependency(VkPipelineStageFlagBits srcStage, VkPipelineStageFlagBits dstStage, VkAccessFlagBits srcAccess, VkAccessFlagBits dstAccess);
    // Bind input attachment to use
    void bindInput(int id, VkImageLayout layout);
    // Bind color attachment to use
    void bindColor(int id, VkImageLayout layout);
    // Bind depth attachment to use
    void bindDepth(int id, VkImageLayout layout);
    // Bind color attachment destination for multisample resolve
    void bindResolveDst(int id, VkImageLayout layout);
    // Preserve attachment content accross this drawing layer
    void bindPreserve(int id);
    // Create a subpass with previously defined characteristics
    void pushLayer(VkPipelineBindPoint type = VK_PIPELINE_BIND_POINT_GRAPHICS);
    // Build rendering pattern
    bool build();

    // ===== USE ===== //
    void bind(int bindID, VkFramebuffer frameBuffer, VkRect2D renderArea);
    inline void begin(int bindID, VkCommandBuffer cmd, VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE) {
        vkCmdBeginRenderPass(cmd, infos + bindID, content);
    }
    inline void next(int, VkCommandBuffer cmd, VkSubpassContents content = VK_SUBPASS_CONTENTS_INLINE) {
        vkCmdNextSubpass(cmd, content);
    }
    //! Don't use it outside of EntityCore/Core
    VkRenderPass renderPass;
private:
    struct Layer {
        std::vector<VkAttachmentReference> inputAttachment;
        std::vector<VkAttachmentReference> colorAttachment;
        std::vector<VkAttachmentReference> depthAttachment;
        std::vector<VkAttachmentReference> resolveAttachment;
        std::vector<uint32_t> preserveAttachment;
    };

    bool builded = false;
    VulkanMgr &master;
    VkRenderPassCreateInfo info {};
    std::vector<VkAttachmentDescription> attachment;
    std::vector<VkSubpassDescription> subpasses;
    std::vector<VkSubpassDependency> dep;
    std::vector<Layer> layers;
    std::vector<VkClearValue> clears;
    VkRenderPassBeginInfo infos[MAX_FB_BIND];
    int subpass = -1;
};

#endif /* RENDERMGR_HPP_ */
