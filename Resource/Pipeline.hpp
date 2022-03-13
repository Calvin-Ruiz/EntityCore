#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <forward_list>

class VulkanMgr;
class VertexBuffer;
class VertexArray;
class PipelineLayout;
class RenderMgr;

const VkPipelineColorBlendAttachmentState BLEND_NONE {VK_FALSE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
const VkPipelineColorBlendAttachmentState BLEND_SRC_ALPHA {VK_TRUE, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
const VkPipelineColorBlendAttachmentState BLEND_DST_ALPHA {VK_TRUE, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
const VkPipelineColorBlendAttachmentState BLEND_SATURATE_ALPHA {VK_TRUE, VK_BLEND_FACTOR_SRC_ALPHA_SATURATE, VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
const VkPipelineColorBlendAttachmentState BLEND_ADD {VK_TRUE, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};

/**
*   \brief Handle the whole draw context
*/
class Pipeline {
public:
    Pipeline(VulkanMgr &master, RenderMgr &render, int subpass, PipelineLayout *layout, std::vector<VkDynamicState> _dynamicStates = {});
    ~Pipeline();
    //! Define which shader must be used
    void bindShader(const std::string &filename, const std::string entry = "main");
    void bindShader(const std::string &filename, VkShaderStageFlagBits stage, const std::string entry = "main");
    //! Set specialized constant value to previously binded shader
    void setSpecializedConstant(uint32_t constantID, void *data, size_t size);
    //! Define VertexArray layout (registered vertex and instance entry)
    void bindVertex(VertexArray &vertex);
    //! Remove vertex/instance location, so that it won't be send to the vertex shader
    void removeVertexEntry(uint32_t location);
    //! Set cull mode (default : false)
    void setCullMode(bool enable) {rasterizer.cullMode = enable ? VK_CULL_MODE_BACK_BIT : 0;}
    //! Set front face (default : false)
    void setFrontFace(bool clockwise = true) {rasterizer.frontFace = clockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;}
    //! Set draw topology (default : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, no breaks)
    void setTopology(VkPrimitiveTopology state, bool enableStripBreaks = false);
    //! Set blend mode (default : blend src alpha)
    void setBlendMode(const VkPipelineColorBlendAttachmentState &blendMode);
    //! Set depth test mode (default : enabled)
    void setDepthStencilMode(VkBool32 enableDepthTest = VK_FALSE, VkBool32 enableDepthWrite = VK_FALSE, VkCompareOp compare = VK_COMPARE_OP_LESS_OR_EQUAL, VkBool32 enableDepthBiais = VK_FALSE, float depthBiasClamp = 0.0f, float depthBiasSlopeFactor = 1.0f);
    //! @brief Set tessellation state
    //! @param patchControlPoints number of control points per patch
    void setTessellationState(uint32_t patchControlPoints = 32);
    //! Set line width (default : value of defaultLineWidth when created)
    void setLineWidth(float lineWidth);
    //! Set which renderPass types this pipeline will be used with
    void disableSampleShading() {multisampling.sampleShadingEnable = VK_FALSE;}
    //! Set polygon mode, default is VK_POLYGON_MODE_FILL
    void setPolygonMode(VkPolygonMode polygonMode) {rasterizer.polygonMode = polygonMode;}
    //! Set custom viewport, which MUSTN'T be destroyed until this pipeline was build();
    void setViewportState(VkPipelineViewportStateCreateInfo *viewport);
    //! Clone unbuild pipeline, the methods bindShader, setSpecializedConstant, build and clone mustn't be used on cloned pipeline. A cloned pipeline is build when calling build() on the parent pipeline.
    Pipeline *clone(const std::string &customName = "\0");
    //! Build pipeline for use, built pipeline allow calling bind() and get(), but disallow every other methods
    void build(const std::string &customName = "\0");
    //! Bind pipeline in command buffer
    inline void bind(VkCommandBuffer &cmd) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    }
    //! For internal use only
    VkPipeline &get() {return graphicsPipeline;}
    //! Set shader path
    static void setShaderDir(const std::string &_shaderDir) {shaderDir = _shaderDir;}
    //! Set default line width for all pipelines created after this call (default : 1.0f)
    static void setDefaultLineWidth(float _defaultLineWidth) {defaultLineWidth = _defaultLineWidth;}
private:
    VkGraphicsPipelineCreateInfo &preBuild(const std::string &customName = "\0");
    void postBuild();
    void initPtr();
    Pipeline(Pipeline *parent);
    static std::string shaderDir;
    static float defaultLineWidth;
    VulkanMgr &master;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    bool isOk = true;
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    VkPipelineDynamicStateCreateInfo dynamicState{}; // UNUSED
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    VkPipelineTessellationStateCreateInfo tessellation{};
    VkPipelineMultisampleStateCreateInfo multisampling{};
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};

    struct SpecializationInfo {
        VkSpecializationInfo info;
        std::vector<VkSpecializationMapEntry> entry;
        std::vector<char> data;
    };
    std::string name;
    std::forward_list<std::string> pNames;
    std::forward_list<SpecializationInfo> specializationInfo;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    std::vector<Pipeline *> childs;
};

#endif /* end of include guard: PIPELINE_HPP */
