#include "EntityCore/Core/VulkanMgr.hpp"
#include "ComputePipeline.hpp"
#include "PipelineLayout.hpp"
#include <fstream>
#include <cstring>

std::string ComputePipeline::shaderDir = "./";

ComputePipeline::ComputePipeline(VulkanMgr &master, PipelineLayout *layout, VkPipelineCreateFlags flags) : master(master)
{
    pipelineInfo.layout = layout->getPipelineLayout();
    isOk = (pipelineInfo.layout != VK_NULL_HANDLE);
    pipelineInfo.flags = flags;
}

ComputePipeline::~ComputePipeline()
{
    if (computePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(master.refDevice, computePipeline, nullptr);
    }
}

void ComputePipeline::bindShader(const std::string &filename, const std::string entry)
{
    entryName = entry;
    pipelineInfo.stage.pName = entryName.c_str();
    std::ifstream file(shaderDir + filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        master.putLog("Failed to open file '" + filename + "'", LogType::ERROR);
        isOk = false;
        entryName.clear();
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
    if (vkCreateShaderModule(master.refDevice, &createInfo, nullptr, &pipelineInfo.stage.module) != VK_SUCCESS) {
        master.putLog("Failed to create shader module from file '" + filename + "'", LogType::ERROR);
        isOk = false;
        return;
    }
    delete[] buffer;
    master.setObjectName(pipelineInfo.stage.module, VK_OBJECT_TYPE_SHADER_MODULE, filename);
    name = "Use " + filename;
}

void ComputePipeline::setSpecializedConstant(uint32_t constantID, const void *data, size_t size)
{
    specializationInfo.entry.push_back({constantID, static_cast<uint32_t>(specializationInfo.data.size()), size});
    specializationInfo.data.resize(specializationInfo.data.size() + size);
    memcpy(specializationInfo.data.data() + specializationInfo.entry.back().offset, data, size);
    specializationInfo.info.mapEntryCount = specializationInfo.entry.size();
    specializationInfo.info.pMapEntries = specializationInfo.entry.data();
    specializationInfo.info.dataSize = specializationInfo.data.size();
    specializationInfo.info.pData = reinterpret_cast<void *>(specializationInfo.data.data());
}

void ComputePipeline::build()
{
    if (!isOk || entryName.empty()) {
        master.putLog("Can't build invalid Pipeline", LogType::ERROR);
        if (!entryName.empty())
            vkDestroyShaderModule(master.refDevice, pipelineInfo.stage.module, nullptr);
        return;
    }
    if (vkCreateComputePipelines(master.refDevice, master.getPipelineCache(), 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
        master.putLog("Faild to create compute Pipeline", LogType::ERROR);
        computePipeline = VK_NULL_HANDLE;
    } else {
        master.setObjectName(computePipeline, VK_OBJECT_TYPE_PIPELINE, name);
    }
    vkDestroyShaderModule(master.refDevice, pipelineInfo.stage.module, nullptr);
}
