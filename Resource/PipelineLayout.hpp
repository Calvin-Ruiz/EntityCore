#ifndef PIPELINE_LAYOUT_HPP
#define PIPELINE_LAYOUT_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <list>
#include <array>

class VulkanMgr;
class Set;

/**
*   \brief Handle shared resources (uniform, uniform texture, buffer, push constant)
*   Layout
*   For set and push constant reusability, see https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#descriptorsets-compatibility
*/
class PipelineLayout {
public:
    PipelineLayout(VulkanMgr &master);
    ~PipelineLayout();
    //! @brief Set uniform location to this PipelineLayout
    //! @param stages combination of flags describing the types of shader accessing it (vertex, fragment, etc.)
    void setUniformLocation(VkShaderStageFlags stage, uint32_t binding, uint32_t arraySize = 1, bool isDynamic = false);
    void setTextureLocation(uint32_t binding, const VkSamplerCreateInfo *samplerInfo = nullptr, VkShaderStageFlags stage = VK_SHADER_STAGE_FRAGMENT_BIT, VkSampler *sampler = nullptr);
    void setTextureArrayLocation(uint32_t binding, uint32_t textureCount, const VkSamplerCreateInfo *samplerInfo = nullptr, VkShaderStageFlags stage = VK_SHADER_STAGE_FRAGMENT_BIT, VkSampler *samplers = nullptr);
    //! @brief Set storage image location to this PipelineLayout
    void setImageLocation(uint32_t binding, VkShaderStageFlags stage = VK_SHADER_STAGE_COMPUTE_BIT);
    //! Like setUniformLocation for storage buffer (which are writable from shader)
    void setStorageBufferLocation(VkShaderStageFlags stage, uint32_t binding, uint32_t arraySize = 1);
    //! Define push constant range for one stage
    void setPushConstant(VkShaderStageFlags stage, uint32_t offset, uint32_t size);
    //! Build pipelineLayout
    void build();
    //! Build set emplacement
    void buildLayout(VkDescriptorSetLayoutCreateFlags flags = 0);
    //! Use set emplacement of another pipelineLayout (default : first owned set emplacement)
    void setGlobalPipelineLayout(PipelineLayout *pl, int index = -1);
    //! Bind descriptor set
    void bindSet(VkCommandBuffer &cmd, Set &set, int binding = 0, VkPipelineBindPoint bp = VK_PIPELINE_BIND_POINT_GRAPHICS);
    //! Bind multiple descriptor set
    void bindSets(VkCommandBuffer &cmd, const std::vector<VkDescriptorSet> &sets, int firstBinding = 0, VkPipelineBindPoint bp = VK_PIPELINE_BIND_POINT_GRAPHICS);
    //! Bind multiple descriptor set of which one has dynamic offset
    void bindSets(VkCommandBuffer &cmd, const std::vector<VkDescriptorSet> &sets, Set &dynamicOffset, int firstBinding = 0, VkPipelineBindPoint bp = VK_PIPELINE_BIND_POINT_GRAPHICS);
    //! Push constants, the index start to 0 and match the setPushConstant call order
    inline void pushConstant(VkCommandBuffer &cmd, int idx, const void *pValues) {
        VkPushConstantRange &info = pushConstants[idx];
        vkCmdPushConstants(cmd, pipelineLayout, info.stageFlags, info.offset, info.size, pValues);
    }
    //! Push constants, the index start to 0 and match the setPushConstant call order
    inline void pushConstant(VkCommandBuffer &cmd, int idx, const void *pValues, int offset, int size) {
        VkPushConstantRange &info = pushConstants[idx];
        vkCmdPushConstants(cmd, pipelineLayout, info.stageFlags, info.offset + offset, size, pValues);
    }
    //! Return pipeline layout
    VkPipelineLayout &getPipelineLayout() {return pipelineLayout;}
    //! Return layout for Set
    VkDescriptorSetLayout &getDescriptorLayout(int index = -1) {return descriptor[(index != -1) ? index : descriptorPos.front()];}
    static VkSamplerCreateInfo DEFAULT_SAMPLER;

private:
    VulkanMgr &master;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    bool isOk = true;
    std::vector<VkDescriptorSetLayoutBinding> uniformsLayout;
    std::vector<VkDescriptorSetLayout> descriptor;
    std::vector<VkPushConstantRange> pushConstants;
    std::vector<std::vector<VkSampler>> cachedSamplers;
    //! Inform which descriptor is owned by this PipelineLayout
    std::vector<int> descriptorPos;

    bool builded = false;
};

#endif /* end of include guard: PIPELINE_LAYOUT_HPP */
