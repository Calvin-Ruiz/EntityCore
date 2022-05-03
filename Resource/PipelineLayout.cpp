#include "EntityCore/Core/VulkanMgr.hpp"
#include "PipelineLayout.hpp"
#include "Set.hpp"

VkSamplerCreateInfo PipelineLayout::DEFAULT_SAMPLER = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, nullptr, 0, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, VK_TRUE, 8.0f, VK_FALSE, VK_COMPARE_OP_ALWAYS, 0.0f, VK_LOD_CLAMP_NONE, VK_BORDER_COLOR_INT_OPAQUE_BLACK, VK_FALSE};

PipelineLayout::PipelineLayout(VulkanMgr &master) : master(master)
{
    DEFAULT_SAMPLER.anisotropyEnable = master.getDeviceFeatures().samplerAnisotropy;
}

PipelineLayout::~PipelineLayout()
{
    if (builded) {
        vkDestroyPipelineLayout(master.refDevice, pipelineLayout, nullptr);
    }
    for (int index : descriptorPos) {
        vkDestroyDescriptorSetLayout(master.refDevice, descriptor[index], nullptr);
    }
}

void PipelineLayout::setTextureLocation(uint32_t binding, const VkSamplerCreateInfo *samplerInfo, VkShaderStageFlags stage, VkSampler *pSampler)
{
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = binding;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = samplerInfo ? &master.getSampler(*samplerInfo) : pSampler;
    samplerLayoutBinding.stageFlags = stage;
    uniformsLayout.push_back(samplerLayoutBinding);
}

void PipelineLayout::setTextureArrayLocation(uint32_t binding, uint32_t textureCount, const VkSamplerCreateInfo *samplerInfo, VkShaderStageFlags stage, VkSampler *pSamplers)
{
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = binding;
    samplerLayoutBinding.descriptorCount = textureCount;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    if (samplerInfo) {
        cachedSamplers.push_back({});
        std::vector<VkSampler> &samplers = cachedSamplers.back();
        samplers.resize(textureCount, master.getSampler(*samplerInfo));
        samplerLayoutBinding.pImmutableSamplers = samplers.data();
    } else
        samplerLayoutBinding.pImmutableSamplers = pSamplers;
    samplerLayoutBinding.stageFlags = stage;
    uniformsLayout.push_back(samplerLayoutBinding);
}

void PipelineLayout::setImageLocation(uint32_t binding, VkShaderStageFlags stage)
{
    VkDescriptorSetLayoutBinding imageLayoutBinding{};
    imageLayoutBinding.binding = binding;
    imageLayoutBinding.descriptorCount = 1;
    imageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    imageLayoutBinding.stageFlags = stage;
    uniformsLayout.push_back(imageLayoutBinding);
}

void PipelineLayout::setUniformLocation(VkShaderStageFlags stage, uint32_t binding, uint32_t arraySize, bool isDynamic)
{
    VkDescriptorSetLayoutBinding uniformCollection{};
    uniformCollection.binding = binding;
    uniformCollection.descriptorType = (isDynamic) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformCollection.descriptorCount = arraySize;
    uniformCollection.stageFlags = stage;
    //uniformCollection.pImmutableSamplers = nullptr; // Optionnel
    uniformsLayout.push_back(uniformCollection);
}

void PipelineLayout::setStorageBufferLocation(VkShaderStageFlags stage, uint32_t binding, uint32_t arraySize)
{
    VkDescriptorSetLayoutBinding storageBuffer{};
    storageBuffer.binding = binding;
    storageBuffer.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    storageBuffer.descriptorCount = arraySize;
    storageBuffer.stageFlags = stage;
    uniformsLayout.push_back(storageBuffer);
}

void PipelineLayout::buildLayout(VkDescriptorSetLayoutCreateFlags flags)
{
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = uniformsLayout.size();
    layoutInfo.pBindings = uniformsLayout.data();
    layoutInfo.flags = flags;

    VkDescriptorSetLayout tmp;
    if (vkCreateDescriptorSetLayout(master.refDevice, &layoutInfo, nullptr, &tmp) != VK_SUCCESS) {
        isOk = false;
        master.putLog("Faild to create Layout", LogType::ERROR);
        tmp = VK_NULL_HANDLE;
    }
    descriptorPos.push_back(descriptor.size());
    descriptor.push_back(tmp);
    uniformsLayout.clear();
}

void PipelineLayout::setGlobalPipelineLayout(PipelineLayout *pl, int index)
{
    VkDescriptorSetLayout tmp = pl->getDescriptorLayout(index);
    if (tmp == VK_NULL_HANDLE) {
        isOk = false;
        master.putLog("Use of invalid Layout", LogType::ERROR);
    }
    descriptor.push_back(tmp);
}

void PipelineLayout::build()
{
    if (!isOk) {
        master.putLog("Can't build invalid PipelineLayout", LogType::ERROR);
        return;
    }
    assert(uniformsLayout.empty()); // there mustn't be unbuilded layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = descriptor.size();
    pipelineLayoutInfo.pSetLayouts = descriptor.data();
    pipelineLayoutInfo.pushConstantRangeCount = pushConstants.size();
    pipelineLayoutInfo.pPushConstantRanges = pushConstants.data();

    if (vkCreatePipelineLayout(master.refDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        master.putLog("Faild to create PipelineLayout", LogType::ERROR);
        pipelineLayout = VK_NULL_HANDLE;
        return;
    }
    builded = true;
}

void PipelineLayout::setPushConstant(VkShaderStageFlags stage, uint32_t offset, uint32_t size)
{
    pushConstants.emplace_back(VkPushConstantRange{stage, offset, size});
}

void PipelineLayout::bindSet(VkCommandBuffer &cmd, Set &set, int binding, VkPipelineBindPoint bp)
{
    vkCmdBindDescriptorSets(cmd, bp, pipelineLayout, binding, 1, set.get(), set.getDynamicOffsets().size(), set.getDynamicOffsets().data());
}

void PipelineLayout::bindSets(VkCommandBuffer &cmd, const std::vector<VkDescriptorSet> &sets, int firstBinding, VkPipelineBindPoint bp)
{
    vkCmdBindDescriptorSets(cmd, bp, pipelineLayout, firstBinding, sets.size(), sets.data(), 0, nullptr);
}

void PipelineLayout::bindSets(VkCommandBuffer &cmd, const std::vector<VkDescriptorSet> &sets, Set &dynamicOffset, int firstBinding, VkPipelineBindPoint bp)
{
    vkCmdBindDescriptorSets(cmd, bp, pipelineLayout, firstBinding, sets.size(), sets.data(), dynamicOffset.getDynamicOffsets().size(), dynamicOffset.getDynamicOffsets().data());
}
