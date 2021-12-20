#ifndef COMPUTE_PIPELINE_HPP
#define COMPUTE_PIPELINE_HPP

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class VulkanMgr;
class PipelineLayout;

/**
*   \brief Handle the whole compute context
*/
class ComputePipeline {
public:
    ComputePipeline(VulkanMgr &master, PipelineLayout *layout, VkPipelineCreateFlags flags = 0);
    ~ComputePipeline();
    //! Define which shader must be used
    void bindShader(const std::string &filename, const std::string entry = "main");
    //! Set specialized constant to shader
    void setSpecializedConstant(uint32_t constantID, void *data, size_t size);
    //! Build pipeline for use
    void build();
    //! Bind pipeline in command buffer
    inline void bind(VkCommandBuffer &cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    }
    //! For internal use only
    VkPipeline &get() {return computePipeline;}
    //! Set shader path
    static void setShaderDir(const std::string &_shaderDir) {shaderDir = _shaderDir;}
private:
    static std::string shaderDir;
    VulkanMgr &master;
    VkPipeline computePipeline = VK_NULL_HANDLE;
    bool isOk = true;
    struct SpecializationInfo {
        VkSpecializationInfo info{0, nullptr, 0, nullptr};
        std::vector<VkSpecializationMapEntry> entry;
        std::vector<char> data;
    } specializationInfo;

    VkComputePipelineCreateInfo pipelineInfo{.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .pNext=nullptr, .flags=0, .stage{.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext=nullptr, .flags=0, .stage=VK_SHADER_STAGE_COMPUTE_BIT, .module=VK_NULL_HANDLE, .pName=nullptr, .pSpecializationInfo = &specializationInfo.info}, .layout=VK_NULL_HANDLE, .basePipelineHandle = VK_NULL_HANDLE, .basePipelineIndex=-1};

    std::string name;
    std::string entryName;
};

#endif /* end of include guard: COMPUTE_PIPELINE_HPP */
