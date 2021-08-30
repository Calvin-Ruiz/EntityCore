#include "EntityCore/Core/VulkanMgr.hpp"
#include "EntityCore/Core/BufferMgr.hpp"
#include "EntityCore/Core/MemoryManager.hpp"
#include "Texture.hpp"
#include "PipelineLayout.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

std::string Texture::textureDir = "./";

Texture::Texture(VulkanMgr &master, int width, int height, VkSampleCountFlagBits sampleCount, const std::string &name, VkImageUsageFlags usage, VkFormat format, VkImageAspectFlags aspect) : master(master), mgr(nullptr), aspect(aspect), name(name)
{
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent = {(uint32_t) width, (uint32_t) height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.samples = sampleCount;
}

Texture::Texture(VulkanMgr &master, BufferMgr &mgr, VkImageUsageFlags usage, const std::string &name, VkFormat format, VkImageType type) : master(master), mgr(&mgr), name(name)
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
    info.samples = VK_SAMPLE_COUNT_1_BIT;
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

bool Texture::init(int width, int height, void *content, bool mipmap, int _nbChannels, int _elemSize, VkImageAspectFlags _aspect, VkSampleCountFlagBits sampleCount, int depth)
{
    aspect = _aspect;
    nbChannels = _nbChannels;
    elemSize = _elemSize;
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = depth;
    info.samples = sampleCount;
    info.mipLevels = static_cast<uint32_t>(std::log2(std::max(width, height))) + 1;
    widthSplit = 1 << ((static_cast<int>(std::log2(depth)) - 1) / 2);
    if (mipmap)
        info.mipLevels = static_cast<uint32_t>(std::log2(std::max(width, height))) + 1;
    if (content) {
        VkDeviceSize size = width * height * depth * nbChannels * elemSize;
        staging = mgr->acquireBuffer(size);
        onCPU = true;
        long *src = (long *) content;
        long *dst = (long *) mgr->getPtr(staging);
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
    memory = (memDedicated.prefersDedicatedAllocation) ?
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
    return mgr->getPtr(staging);
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

void Texture::createSurface()
{
    if (!onCPU) {
        VkDeviceSize size = info.extent.width * info.extent.height * nbChannels * elemSize;
        staging = mgr->acquireBuffer(size);
        onCPU = true;
    }
}

SDL_Surface *Texture::createSDLSurface()
{
    createSurface();
    if (onCPU) {
        #if SDL_BYTEORDER == SDL_BIG_ENDIAN
            const unsigned int rmask = 0xff000000;
            const unsigned int gmask = 0x00ff0000;
            const unsigned int bmask = 0x0000ff00;
            const unsigned int amask = 0x000000ff;
        #else // little endian, like x86
            const unsigned int rmask = 0x000000ff;
            const unsigned int gmask = 0x0000ff00;
            const unsigned int bmask = 0x00ff0000;
            const unsigned int amask = 0xff000000;
        #endif
        SDL_Surface *ret = SDL_CreateRGBSurfaceFrom(mgr->getPtr(staging), info.extent.width, info.extent.height, 32, 4*info.extent.width, rmask, gmask, bmask, amask);
        sdlSurface = true;
        return ret;
    } else {
        return nullptr;
    }
}

void Texture::detach()
{
    if (onCPU) {
        mgr->releaseBuffer(staging);
        onCPU = false;
    }
}

bool Texture::use(VkCommandBuffer cmd, bool includeTransition)
{
    bool includeFirstTransition = includeTransition;
    if (!onGPU) {
        if (createImage()) {
            master.putLog("Created image support for '" + name + "'", LogType::INFO);
            includeFirstTransition = true;
        } else {
            return false;
        }
    }
    if (cmd == VK_NULL_HANDLE) {
        if (onCPU)
            master.putLog("No cmd given for '" + name + "', don't upload anything", LogType::INFO);
        return true;
    }
    if (!onCPU) {
        master.putLog("Can't upload '" + name + "' datas : not stored in RAM", LogType::WARNING);
        return true;
    }
    if (sdlSurface) {
        VkImageMemoryBarrier barrier {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, 0, info.mipLevels, 0, info.arrayLayers}};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    VkImageMemoryBarrier barrier[2] {
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, 0, info.mipLevels, 0, info.arrayLayers}},
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, info.mipLevels - 1, 1, 0, info.arrayLayers}}};
    if (includeFirstTransition)
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, barrier);
    VkBufferImageCopy region {(VkDeviceSize) staging.offset, info.extent.width * widthSplit, 0, {aspect, 0, 0, info.arrayLayers}, {0, 0, 0}, info.extent};
    if (info.extent.depth == 1) {
        vkCmdCopyBufferToImage(cmd, staging.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    } else {
        std::vector<VkBufferImageCopy> regions;
        regions.reserve(widthSplit);
        region.imageExtent.depth /= widthSplit;
        for (int i = 0; i < widthSplit; ++i) {

            regions.push_back(region);
            region.bufferOffset += info.extent.width * nbChannels * elemSize;
            region.imageOffset.z += region.imageExtent.depth;
        }
        vkCmdCopyBufferToImage(cmd, staging.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.size(), regions.data());
    }
    if (includeTransition) {
        VkImageBlit iregion {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, info.arrayLayers}, {{0, 0, 0}, {0, 0, 1}}, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, info.arrayLayers}, {{0, 0, 0}, {(int) info.extent.width, (int) info.extent.height, 1}}};
        if (info.mipLevels > 1) {
            barrier->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier->dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier->oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier->newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier->subresourceRange.levelCount = 1;
            for (int i = 1; i < info.mipLevels; ++i) {
                iregion.srcSubresource.mipLevel = iregion.dstSubresource.mipLevel++;
                iregion.srcOffsets[1].x = iregion.dstOffsets[1].x;
                iregion.srcOffsets[1].y = iregion.dstOffsets[1].y;
                if (iregion.dstOffsets[1].x > 1)
                    iregion.dstOffsets[1].x /= 2;
                if (iregion.dstOffsets[1].y > 1)
                    iregion.dstOffsets[1].y /= 2;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, barrier);
                vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &iregion, VK_FILTER_LINEAR);
                ++barrier->subresourceRange.baseMipLevel;
            }
            barrier->srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier->dstAccessMask = 0;
            barrier->oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier->newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier->subresourceRange.baseMipLevel = 0;
            barrier->subresourceRange.levelCount = info.mipLevels - 1;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, barrier);
        } else {
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, barrier + 1);
        }
    }
    if (sdlSurface) {
        VkImageMemoryBarrier barrier {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, 0, info.mipLevels, 0, info.arrayLayers}};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    return true;
}
