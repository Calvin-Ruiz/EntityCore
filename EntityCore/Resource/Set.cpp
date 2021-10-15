#include "EntityCore/Core/VulkanMgr.hpp"
#include "SetMgr.hpp"
#include "PipelineLayout.hpp"
#include "Texture.hpp"
#include "Set.hpp"

PFN_vkCmdPushDescriptorSetKHR Set::pushSet;

Set::Set(VulkanMgr &master, SetMgr &mgr, PipelineLayout *_layout, int setBinding, bool initialize, bool temporary) : master(master), mgr(mgr), temporary(temporary)
{
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = mgr.getDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &_layout->getDescriptorLayout(setBinding);
    if (initialize) {
        init();
    }
}

Set::~Set()
{
    uninit();
}

void Set::init()
{
    if (initialized)
        return;
    initialized = true;
    if (*allocInfo.pSetLayouts == VK_NULL_HANDLE) {
        master.putLog("Can't create Set from invalid Layout", LogType::WARNING);
        return;
    }
    if (!createDescriptorSet()) {
        master.putLog("Failed to create Set from this SetMgr", LogType::ERROR);
        set = VK_NULL_HANDLE;
    }
    for (auto &ws : writeSet) {
        ws.dstSet = set;
    }
}

void Set::uninit()
{
    if (temporary && initialized) {
        mgr.destroySet(set);
        initialized = false;
    }
}

bool Set::createDescriptorSet()
{
    switch (vkAllocateDescriptorSets(master.refDevice, &allocInfo, &set)) {
        case VK_SUCCESS:
            return true;
        case VK_ERROR_FRAGMENTED_POOL:
            master.putLog("Not enough continuous space in SetMgr to allocate this Set", LogType::WARNING);
            break;
        case VK_ERROR_OUT_OF_POOL_MEMORY:
            master.putLog("Not enough space in SetMgr to allocate this Set", LogType::WARNING);
            break;
        case VK_ERROR_OUT_OF_HOST_MEMORY:
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            master.putLog("Not enough memory for Set", LogType::WARNING);
            return false;
        default:
            master.putLog("Invalid Set", LogType::WARNING);
            set = VK_NULL_HANDLE;
            return true;
    }
    allocInfo.descriptorPool = mgr.getDescriptorPool();
    return false;
}

void Set::bindUniform(SubBuffer &buffer, uint32_t binding, uint32_t range, int offset)
{
    bufferInfo.push_front({buffer.buffer, (VkDeviceSize) (buffer.offset + offset), (range == UINT32_MAX) ? buffer.size : range});
    writeSet.emplace_back(VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, binding, 0, 1,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        nullptr, &bufferInfo.front(), nullptr});
}

void Set::bindTexture(Texture &texture, uint32_t binding, VkSampler sampler, VkImageLayout layout)
{
    imageInfo.push_front({sampler, texture.getView(), layout});
    writeSet.emplace_back(VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, binding, 0, 1,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        &imageInfo.front(), nullptr, nullptr});
}

void Set::bindStorageBuffer(SubBuffer &buffer, uint32_t binding, uint32_t range, int offset)
{
    VkDescriptorBufferInfo buffInfo{};
    buffInfo.buffer = buffer.buffer;
    buffInfo.offset = buffer.offset + offset;
    buffInfo.range = range;
    bufferInfo.push_front(buffInfo);
    writeSet.emplace_back(VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, binding, 0, 1,
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        nullptr, &bufferInfo.front(), nullptr});
}

int Set::bindVirtualUniform(SubBuffer &buffer, uint32_t binding, uint32_t range, uint32_t arraySize)
{
    VkDescriptorBufferInfo buffInfo{};
    buffInfo.buffer = buffer.buffer;
    buffInfo.offset = buffer.offset;
    buffInfo.range = range;
    bufferInfo.push_front(buffInfo);
    dynamicOffsets.push_back(buffer.offset);
    writeSet.emplace_back(VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, binding, 0, arraySize,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        nullptr, &bufferInfo.front(), nullptr});
    return dynamicOffsets.size() - 1;
}

int Set::bindVirtualUniform(VkBuffer buffer, uint32_t binding, uint32_t range, uint32_t arraySize)
{
    VkDescriptorBufferInfo buffInfo{};
    buffInfo.buffer = buffer;
    buffInfo.offset = 0;
    buffInfo.range = range;
    bufferInfo.push_front(buffInfo);
    dynamicOffsets.push_back(0);
    writeSet.emplace_back(VkWriteDescriptorSet{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, binding, 0, arraySize,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
        nullptr, &bufferInfo.front(), nullptr});
    return dynamicOffsets.size() - 1;
}

void Set::setVirtualUniform(int offset, int virtualUniformID)
{
    dynamicOffsets[virtualUniformID] = offset;
}

void Set::update()
{
    init();
    if (set) {
        vkUpdateDescriptorSets(master.refDevice, writeSet.size(), writeSet.data(), 0, nullptr);
        writeSet.clear();
        bufferInfo.clear();
        imageInfo.clear();
    }
}

VkDescriptorSet *Set::get()
{
    if (!writeSet.empty())
        update();
    return &set;
}

void Set::push(VkCommandBuffer &cmd, PipelineLayout &layout, int binding, VkPipelineBindPoint bindPoint)
{
    pushSet(cmd, bindPoint, layout.getPipelineLayout(), binding, writeSet.size(), writeSet.data());
}

void Set::setupPFN(VkInstance instance)
{
    pushSet = (PFN_vkCmdPushDescriptorSetKHR) vkGetInstanceProcAddr(instance, "vkCmdPushDescriptorSetKHR");
}
