#include "EntityCore/Core/VulkanMgr.hpp"
#include "EntityCore/Core/BufferMgr.hpp"
#include "EntityCore/Core/MemoryManager.hpp"
#include "Texture.hpp"
#include "PipelineLayout.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

std::string Texture::textureDir = "./";

Texture::Texture(VulkanMgr &master, BufferMgr &mgr, VkImageUsageFlags usage, const std::string &name, VkFormat format, VkImageType type) : master(master), mgr(mgr), name(name)
{
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.imageType = type;
    info.format = format;
    info.extent = {0, 0, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

bool Texture::init(int nbChannels, bool mipmap)
{
    int texChannels;
    stbi_uc *data = stbi_load((textureDir + name).c_str(), (int *) &info.extent.width, (int *) &info.extent.height, &texChannels, nbChannels);

    if (!data)
        return false;
    const bool res = init(info.extent.width, info.extent.height, data, mipmap, nbChannels);
    stbi_image_free(data);
    return res;
}

bool Texture::init(int width, int height, void *content, bool mipmap, int _nbChannels, int _elemSize, VkImageAspectFlags _aspect, VkSampleCountFlagBits sampleCount)
{
    aspect = _aspect;
    nbChannels = _nbChannels;
    elemSize = _elemSize;
    info.extent.width = width;
    info.extent.height = height;
    info.samples = sampleCount;
    if (mipmap)
        info.mipLevels = static_cast<uint32_t>(std::log2(std::max(width, height))) + 1;
    if (content) {
        VkDeviceSize size = width * height * nbChannels * elemSize;
        staging = mgr.acquireBuffer(size);
        onCPU = true;
        long *src = (long *) content;
        long *dst = (long *) mgr.getPtr(staging);
        for (int i = size / sizeof(long); i > 0; --i)
            *(dst++) = *(src++);
    } else
        return createImage();
    return true;
}

bool Texture::createImage()
{
    if (vkCreateImage(master.refDevice, &info, nullptr, &image) != VK_SUCCESS) {
        master.putLog("Failed to create image support for '" + name + "'", LogType::ERROR);
        return false;
    }
    // Allocate image memory
    VkMemoryDedicatedRequirements memDedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, nullptr, 0, 0};
    VkMemoryRequirements2 memRequirements;
    memRequirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    memRequirements.pNext = &memDedicated;
    VkImageMemoryRequirementsInfo2 memImageInfo{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, nullptr, image};
    vkGetImageMemoryRequirements2(master.refDevice, &memImageInfo, &memRequirements);
    SubMemory memory = (memDedicated.prefersDedicatedAllocation) ?
        master.getMemoryManager()->dmalloc(memRequirements.memoryRequirements, image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT):
        master.getMemoryManager()->malloc(memRequirements.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory.memory == VK_NULL_HANDLE) {
        vkDestroyImage(master.refDevice, image, nullptr);
        return false;
    }
    // Bind image memory
    if (vkBindImageMemory(master.refDevice, image, memory.memory, memory.offset) != VK_SUCCESS) {
        master.putLog("Faild to bind memory to '" + name + "'", LogType::ERROR);
        vkDestroyImage(master.refDevice, image, nullptr);
        master.free(memory);
        return false;
    }
    // Create view
    VkImageViewCreateInfo viewInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, image, (VkImageViewType) info.imageType, info.format, {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY}, {aspect, 0, info.mipLevels, 0, info.arrayLayers}};
    if (vkCreateImageView(master.refDevice, &viewInfo, nullptr, &view) != VK_SUCCESS) {
        master.putLog("Failed to create view for '" + name + "'", LogType::ERROR);
        vkDestroyImage(master.refDevice, image, nullptr);
        master.free(memory);
        return false;
    }
    // Set name and flag onGPU
    onGPU = true;
    master.setObjectName(image, VK_OBJECT_TYPE_IMAGE, name);
    master.setObjectName(view, VK_OBJECT_TYPE_IMAGE_VIEW, name);
    return true;
}

Texture::~Texture()
{
    if (onCPU)
        detach();
    if (onGPU)
        unuse();
}

void *Texture::acquireStagingMemoryPtr()
{
    return mgr.getPtr(staging);
}

void Texture::unuse()
{
    if (onGPU) {
        master.free(memory);
        vkDestroyImageView(master.refDevice, view, nullptr);
        vkDestroyImage(master.refDevice, image, nullptr);
        onGPU = false;
    }
}

void Texture::detach()
{
    if (onCPU) {
        mgr.releaseBuffer(staging);
        onCPU = false;
    }
}

bool Texture::use(VkCommandBuffer cmd, bool includeTransition)
{
    if (!onGPU) {
        if (createImage()) {
            master.putLog("Created image support for '" + name + "'", LogType::DEBUG);
        } else {
            return false;
        }
    } else if (cmd != VK_NULL_HANDLE && onCPU) {
        VkImageMemoryBarrier barrier {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, 0, info.mipLevels, 0, info.arrayLayers}};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkBufferImageCopy region {(VkDeviceSize) staging.offset, 0, 0, {aspect, 0, 0, info.arrayLayers}, {0, 0, 0}, info.extent};
        vkCmdCopyBufferToImage(cmd, staging.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        if (includeTransition) {
            barrier.srcAccessMask = barrier.dstAccessMask;
            barrier.oldLayout = barrier.newLayout;
            barrier.dstAccessMask = 0;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        }
        return true;
    }
    if (cmd == VK_NULL_HANDLE) {
        master.putLog("No cmd given for '" + name + "', don't upload anything", LogType::DEBUG);
        return true;
    }
    if (!onCPU) {
        master.putLog("Can't upload '" + name + "' datas : not stored in RAM", LogType::WARNING);
        return true;
    }
    VkBufferImageCopy region {(VkDeviceSize) staging.offset, 0, 0, {aspect, 0, 0, info.arrayLayers}, {0, 0, 0}, info.extent};
    vkCmdCopyBufferToImage(cmd, staging.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    return true;
}
