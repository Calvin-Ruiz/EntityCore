#include "EntityCore/Core/VulkanMgr.hpp"
#include "EntityCore/Core/RenderMgr.hpp"
#include "PipelineLayout.hpp"
#include "Pipeline.hpp"
#include "VertexArray.hpp"
#include <fstream>
#include <algorithm>

std::string Pipeline::shaderDir = "./";
float Pipeline::defaultLineWidth = 1.0f;

Pipeline::Pipeline(VulkanMgr &master, RenderMgr &render, int subpass, PipelineLayout *layout, std::vector<VkDynamicState> _dynamicStates) : master(master)
{
    colorBlendAttachment = BLEND_SRC_ALPHA;

    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optionnel
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optionnel
    colorBlending.blendConstants[1] = 0.0f; // Optionnel
    colorBlending.blendConstants[2] = 0.0f; // Optionnel
    colorBlending.blendConstants[3] = 0.0f; // Optionnel

    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE; // Tout élément trop loin ou trop près est ramené à la couche la plus loin ou la plus proche
    rasterizer.rasterizerDiscardEnable = VK_FALSE; // Désactiver la géométrie
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = defaultLineWidth;
    rasterizer.cullMode = 0;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optionnel
    rasterizer.depthBiasClamp = 0.0f; // Optionnel
    rasterizer.depthBiasSlopeFactor = 1.0f; // Optionnel

    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Possibilité d'interrompre les liaisons entre les vertices pour les modes _STRIP
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pViewportState = &master.getViewportState();

    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pDepthStencilState = &depthStencil; // Optionnel
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr; // Optionnel
    pipelineInfo.layout = layout->getPipelineLayout();
    pipelineInfo.renderPass = render.renderPass;
    pipelineInfo.subpass = subpass;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optionnel
    pipelineInfo.basePipelineIndex = -1; // Optionnel

    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // Optionnel
    depthStencil.maxDepthBounds = 1.0f; // Optionnel
    depthStencil.stencilTestEnable = VK_FALSE;

    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = master.getDeviceFeatures().sampleRateShading;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = .2f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    isOk = (pipelineInfo.layout != VK_NULL_HANDLE);
}

Pipeline::~Pipeline()
{
    if (graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(master.refDevice, graphicsPipeline, nullptr);
    }
}

void Pipeline::bindShader(const std::string &filename, const std::string entry)
{
    size_t first = filename.find_first_of(".") + 1;
    size_t size = filename.find_last_of(".") - first;
    std::string shaderType = filename.substr(first, size);
    if (shaderType.compare("vert") == 0) {
        bindShader(filename, VK_SHADER_STAGE_VERTEX_BIT, entry);
        return;
    }
    if (shaderType.compare("frag") == 0) {
        bindShader(filename, VK_SHADER_STAGE_FRAGMENT_BIT, entry);
        return;
    }
    if (shaderType.compare("geom") == 0) {
        bindShader(filename, VK_SHADER_STAGE_GEOMETRY_BIT, entry);
        return;
    }
    if (shaderType.compare("tesc") == 0) {
        bindShader(filename, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, entry);
        return;
    }
    if (shaderType.compare("tese") == 0) {
        bindShader(filename, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, entry);
        return;
    }
}

void Pipeline::bindShader(const std::string &filename, VkShaderStageFlagBits stage, const std::string entry)
{
    if (stage & (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) && master.getDeviceFeatures().tessellationShader == VK_FALSE)
        return;

    std::ifstream file(shaderDir + filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        master.putLog("Failed to open file '" + filename + "'", LogType::ERROR);
        isOk = false;
        return;
    }

    size_t fileSize = (size_t) file.tellg();
    char *buffer = new char [fileSize + sizeof(uint32_t)];

    file.seekg(0);
    file.read(buffer, fileSize);

    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = reinterpret_cast<uint32_t*>(buffer);

    VkPipelineShaderStageCreateInfo tmp{};
    tmp.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    tmp.stage = stage;
    tmp.pSpecializationInfo = nullptr; // define constant values if any

    pNames.push_front(entry);
    tmp.pName = pNames.front().c_str();

    SpecializationInfo specInfo;
    specInfo.info.mapEntryCount = 0;
    specInfo.info.dataSize = 0;
    specializationInfo.push_front(specInfo);
    tmp.pSpecializationInfo = &specializationInfo.front().info;

    if (vkCreateShaderModule(master.refDevice, &createInfo, nullptr, &tmp.module) != VK_SUCCESS) {
        master.putLog("Failed to create shader module from file '" + filename + "'", LogType::ERROR);
        isOk = false;
        return;
    }
    delete[] buffer;
    shaderStages.push_back(tmp);
    master.setObjectName(tmp.module, VK_OBJECT_TYPE_SHADER_MODULE, filename);
    name += " " + filename;
}

void Pipeline::setSpecializedConstant(uint32_t constantID, void *data, size_t size)
{
    SpecializationInfo &specInfo = specializationInfo.front();
    specInfo.entry.push_back({constantID, static_cast<uint32_t>(specInfo.data.size()), size});
    specInfo.data.resize(specInfo.data.size() + size);
    memcpy(specInfo.data.data() + specInfo.entry.back().offset, data, size);
    specInfo.info.mapEntryCount = specInfo.entry.size();
    specInfo.info.pMapEntries = specInfo.entry.data();
    specInfo.info.dataSize = specInfo.data.size();
    specInfo.info.pData = reinterpret_cast<void *>(specInfo.data.data());
}

void Pipeline::setTopology(VkPrimitiveTopology state, bool enableStripBreaks)
{
    inputAssembly.topology = state;
    inputAssembly.primitiveRestartEnable = enableStripBreaks ? VK_TRUE : VK_FALSE;
}

void Pipeline::setBlendMode(const VkPipelineColorBlendAttachmentState &blendMode)
{
    colorBlendAttachment = blendMode;
}

void Pipeline::setDepthStencilMode(VkBool32 enableDepthTest, VkBool32 enableDepthWrite, VkCompareOp compare, VkBool32 enableDepthBiais, float depthBiasClamp, float depthBiasSlopeFactor)
{
    depthStencil.depthTestEnable = enableDepthTest;
    depthStencil.depthWriteEnable = enableDepthWrite;
    depthStencil.depthCompareOp = compare;
    rasterizer.depthBiasEnable = enableDepthBiais;
    rasterizer.depthBiasClamp = depthBiasClamp;
    rasterizer.depthBiasSlopeFactor = depthBiasSlopeFactor;
}

void Pipeline::setTessellationState(uint32_t patchControlPoints)
{
    tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellation.pNext = nullptr;
    tessellation.flags = 0;
    tessellation.patchControlPoints = patchControlPoints;
    pipelineInfo.pTessellationState = &tessellation;
}

void Pipeline::setLineWidth(float lineWidth)
{
    rasterizer.lineWidth = lineWidth;
}

void Pipeline::setSampleCount(VkSampleCountFlagBits sample)
{
   multisampling.rasterizationSamples = sample;
}

void Pipeline::bindVertex(VertexArray &vertex)
{
    bindingDescriptions = vertex.getVertexBindingDesc();
    attributeDescriptions = vertex.getVertexAttributeDesc();
}

void Pipeline::removeVertexEntry(uint32_t location)
{
    for (auto it = attributeDescriptions.begin(); it != attributeDescriptions.end(); ++it) {
        if (it->location == location) {
            attributeDescriptions.erase(it);
            return;
        }
    }
    master.putLog("Failed to remove entry", LogType::DEBUG);
}

void Pipeline::build(const std::string &customName)
{
    if (!isOk || shaderStages.empty()) {
        master.putLog("Can't build invalid Pipeline", LogType::ERROR);
        for (auto &stage : shaderStages) {
            vkDestroyShaderModule(master.refDevice, stage.module, nullptr);
        }
        return;
    }
    if (bindingDescriptions.empty()) {
        master.putLog("No vertex entry defined for Pipeline \"" + (customName.empty() ? "Use" + name : customName.c_str()) + "\"", LogType::DEBUG);
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    pipelineInfo.stageCount = shaderStages.size();
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optionnel
    pipelineInfo.basePipelineIndex = -1; // Optionnel

    if (vkCreateGraphicsPipelines(master.refDevice, master.getPipelineCache(), 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        master.putLog("Faild to create Pipeline", LogType::ERROR);
        graphicsPipeline = VK_NULL_HANDLE;
    } else {
        master.setObjectName(graphicsPipeline, VK_OBJECT_TYPE_PIPELINE, customName.empty() ? "Use" + name : customName.c_str());
    }
    for (auto &stage : shaderStages) {
        vkDestroyShaderModule(master.refDevice, stage.module, nullptr);
    }
    pNames.clear();
    shaderStages.clear();
}
