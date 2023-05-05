#include "EntityCore/Core/VulkanMgr.hpp"
#include "EntityCore/Core/BufferMgr.hpp"
#include "EntityCore/Core/MemoryManager.hpp"
#include "Texture.hpp"
#include "PipelineLayout.hpp"
#include "EntityCore/Tools/AllocTracer.hpp"
#include <SDL2/SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

std::string Texture::textureDir = "./";

Texture::Texture(VulkanMgr &master, const TextureInfo &texInfo) : master(master), mgr(texInfo.mgr), name(texInfo.name)
{
    int texChannels = -1;
    int height = texInfo.height;
    bool flipHorizontal;
    void *content = texInfo.content;
    if (height < 0) {
        height = -height;
        flipHorizontal = true;
    } else
        flipHorizontal = false;
    nbChannels = texInfo.nbChannels;
    elemSize = texInfo.channelSize;
    aspect = texInfo.aspect;
    widthSplit = (texInfo.depthColumn) ? texInfo.depthColumn : (1 << (static_cast<int>(std::log2(texInfo.depth)) / 2));
    memoryBatch = texInfo.memoryBatch;
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.imageType = texInfo.type;
    info.format = texInfo.format;
    info.extent = {(uint32_t) texInfo.width, (uint32_t) height, (uint32_t) texInfo.depth};
    if (!texInfo.width)
        content = stbi_load((textureDir + name).c_str(), (int *) &info.extent.width, (int *) &info.extent.height, &texChannels, nbChannels);
    info.mipLevels = (texInfo.mipmap) ? static_cast<uint32_t>(std::log2(std::max(info.extent.width, info.extent.height))) + 1 : 1;
    info.arrayLayers = texInfo.arrayLayers;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = texInfo.usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    info.samples = texInfo.sampleCount;
    if (content) {
        VkDeviceSize size = info.extent.width * info.extent.height * info.extent.depth * nbChannels * elemSize;
        // Note : if size if rounded up to the nearest multiple of 8
        if (size % sizeof(uint64_t))
            size += sizeof(uint64_t) - (size % sizeof(uint64_t));
        staging = mgr->acquireBuffer(size);
        onCPU = true;
        if (flipHorizontal) {
            // Note : we can't round up with horizontal flipping
            assert(info.extent.width * nbChannels * elemSize % sizeof(uint64_t) == 0);
            const int lineSize = info.extent.width * nbChannels * elemSize / sizeof(uint64_t);
            uint64_t *src = ((uint64_t *) content) + lineSize * (info.extent.height + 1);
            uint64_t *dst = (uint64_t *) mgr->getPtr(staging);
            for (int i = info.extent.height; i--;) {
                src -= lineSize * 2;
                for (int j = lineSize; j--;)
                    *(dst++) = *(src++);
            }
        } else {
            uint64_t *src = (uint64_t *) content;
            uint64_t *dst = (uint64_t *) mgr->getPtr(staging);
            for (int i = size / sizeof(uint64_t); i > 0; --i)
                *(dst++) = *(src++);
        }
    }
    if (texChannels != -1)
        stbi_image_free(content);
}

Texture::Texture(VulkanMgr &master, int width, int height, VkSampleCountFlagBits sampleCount, const std::string &name, VkImageUsageFlags usage, VkFormat format, VkImageAspectFlags aspect, bool mipmap) : master(master), mgr(nullptr), aspect(aspect), name(name)
{
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.extent = {(uint32_t) width, (uint32_t) height, 1};
    info.mipLevels = (mipmap) ? static_cast<uint32_t>(std::log2(std::max(width, height))) + 1 : 1;
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
    bool flipHorizontal;
    if (height < 0) {
        height = -height;
        flipHorizontal = true;
    } else
        flipHorizontal = false;
    info.extent.height = height;
    info.extent.depth = depth;
    info.samples = sampleCount;
    widthSplit = 1 << (static_cast<int>(std::log2(depth)) / 2);
    info.mipLevels = (mipmap) ? static_cast<uint32_t>(std::log2(std::max(width, height))) + 1 : 1;
    if (content) {
        VkDeviceSize size = width * height * depth * nbChannels * elemSize;
        staging = mgr->acquireBuffer(size);
        onCPU = true;
        if (flipHorizontal) {
            const int lineSize = width * nbChannels * elemSize / sizeof(uint64_t); // Only work for multiple of 8
            uint64_t *src = ((uint64_t *) content) + lineSize * (height + 1);
            uint64_t *dst = (uint64_t *) mgr->getPtr(staging);
            for (int i = height; i--;) {
                src -= lineSize * 2;
                for (int j = lineSize; j--;)
                    *(dst++) = *(src++);
            }
        } else {
            uint64_t *src = (uint64_t *) content;
            uint64_t *dst = (uint64_t *) mgr->getPtr(staging);
            for (int i = size / sizeof(uint64_t); i > 0; --i)
                *(dst++) = *(src++);
        }
    } else
        return createImage();
    return true;
}

bool Texture::preCreateImage()
{
    if (!image) {
        if (vkCreateImage(master.refDevice, &info, nullptr, &image) != VK_SUCCESS) {
            master.putLog("Failed to create image support for '" + name + "'", LogType::ERROR);
            return false;
        }
        master.setObjectName(image, VK_OBJECT_TYPE_IMAGE, name);
    }
    return true;
}

bool Texture::createImage()
{
    if (!preCreateImage())
        return false;
    // Allocate image memory
    VkMemoryDedicatedRequirements memDedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, nullptr, 0, 0};
    VkMemoryRequirements2 memRequirements;
    memRequirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    memRequirements.pNext = &memDedicated;
    VkImageMemoryRequirementsInfo2 memImageInfo{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, nullptr, image};
    vkGetImageMemoryRequirements2(master.refDevice, &memImageInfo, &memRequirements);
    memory = (memDedicated.prefersDedicatedAllocation) ?
        master.getMemoryManager()->dmalloc(memRequirements.memoryRequirements, {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, nullptr, image, VK_NULL_HANDLE}, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT):
        master.getMemoryManager()->malloc(memRequirements.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, memoryBatch);
    if (memory.memory == VK_NULL_HANDLE)
        return false;
    // Bind image memory
    if (vkBindImageMemory(master.refDevice, image, memory.memory, memory.offset) != VK_SUCCESS) {
        master.putLog("Faild to bind memory to '" + name + "'", LogType::ERROR);
        master.free(memory);
        return false;
    }
    if (info.usage & ALL_IMAGE_VIEW_USAGE) {
        // Create view
        VkImageViewCreateInfo viewInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, image, static_cast<VkImageViewType>(info.imageType + VK_IMAGE_VIEW_TYPE_1D_ARRAY * (info.arrayLayers > 1)), info.format, {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY}, {aspect, 0, info.mipLevels, 0, VK_REMAINING_ARRAY_LAYERS}};
        if (vkCreateImageView(master.refDevice, &viewInfo, nullptr, &view) != VK_SUCCESS) {
            master.putLog("Failed to create view for '" + name + "'", LogType::ERROR);
            vkDestroyImage(master.refDevice, image, nullptr);
            master.free(memory);
            return false;
        }
        master.setObjectName(view, VK_OBJECT_TYPE_IMAGE_VIEW, name);
    }
    sizeInMemory = memRequirements.memoryRequirements.size;
    onGPU = true;
    return true;
}

Texture::~Texture()
{
    if (onCPU)
        detach();
    if (onGPU)
        unuse();
    if (image)
        vkDestroyImage(master.refDevice, image, nullptr);
}

VkImageView Texture::createView(uint32_t baseMipLevel, uint32_t mipLevels, uint32_t baseArrayLevel, uint32_t arrayLevels)
{
    VkImageView v;
    VkImageViewCreateInfo viewInfo {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, image, (VkImageViewType) info.imageType, info.format, {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY}, {aspect, baseMipLevel, mipLevels, baseArrayLevel, arrayLevels}};
    if (vkCreateImageView(master.refDevice, &viewInfo, nullptr, &v) != VK_SUCCESS) {
        master.putLog("Failed to create view on '" + name + "'", LogType::ERROR);
        return VK_NULL_HANDLE;
    }
    return v;
}

void *Texture::acquireStagingMemoryPtr()
{
    return mgr->getPtr(staging);
}

void Texture::unuse()
{
    if (onGPU) {
        master.free(memory);
        if (view)
            vkDestroyImageView(master.refDevice, view, nullptr);
        vkDestroyImage(master.refDevice, image, nullptr);
        onGPU = false;
        image = VK_NULL_HANDLE;
    }
}

void Texture::createSurface()
{
    if (!onCPU) {
        VkDeviceSize size = info.extent.width * info.extent.height * info.extent.depth * nbChannels * elemSize;
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
            master.putLog("Created image support for '" + name + "'", LogType::DEBUG);
            includeFirstTransition = true;
        } else {
            return false;
        }
    }
    if (cmd == VK_NULL_HANDLE) {
        if (onCPU)
            master.putLog("No cmd given for '" + name + "', don't upload anything", LogType::DEBUG);
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
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, 0, includeTransition ? info.mipLevels : 1, 0, info.arrayLayers}},
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, info.mipLevels - 1, 1, 0, info.arrayLayers}}};
    if (includeFirstTransition && !sdlSurface)
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
    if (sdlSurface) {
        VkImageMemoryBarrier barrier {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, 0, info.mipLevels, 0, info.arrayLayers}};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &barrier);
        return true;
    }
    if (includeTransition) {
        VkImageBlit iregion {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, info.arrayLayers}, {{0, 0, 0}, {0, 0, 0}}, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, info.arrayLayers}, {{0, 0, 0}, {(int) info.extent.width, (int) info.extent.height, (int) info.extent.depth}}};
        if (info.mipLevels > 1) {
            barrier->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier->dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier->oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier->newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier->subresourceRange.levelCount = 1;
            for (unsigned int i = 1; i < info.mipLevels; ++i) {
                iregion.srcSubresource.mipLevel = iregion.dstSubresource.mipLevel++;
                iregion.srcOffsets[1].x = iregion.dstOffsets[1].x;
                iregion.srcOffsets[1].y = iregion.dstOffsets[1].y;
                iregion.srcOffsets[1].z = iregion.dstOffsets[1].z;
                if (iregion.dstOffsets[1].x > 1)
                    iregion.dstOffsets[1].x /= 2;
                if (iregion.dstOffsets[1].y > 1)
                    iregion.dstOffsets[1].y /= 2;
                if (iregion.dstOffsets[1].z > 1)
                    iregion.dstOffsets[1].z /= 2;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, barrier);
                vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &iregion, VK_FILTER_LINEAR);
                ++barrier->subresourceRange.baseMipLevel;
            }
            barrier->srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier->dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier->oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier->newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier->subresourceRange.baseMipLevel = 0;
            barrier->subresourceRange.levelCount = info.mipLevels - 1;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, barrier);
        } else {
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, barrier + 1);
        }
    }
    return true;
}

void Texture::implicitBarrier(VkImageLayout layout, VkAccessFlags &access, VkPipelineStageFlags &stage)
{
    switch (layout) {
        case VK_IMAGE_LAYOUT_GENERAL:
            access = VK_ACCESS_SHADER_READ_BIT;
            stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            access = VK_ACCESS_SHADER_READ_BIT;
            stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            access = VK_ACCESS_TRANSFER_READ_BIT;
            stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            access = VK_ACCESS_TRANSFER_WRITE_BIT;
            stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            access = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            stage = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        default:
            access = 0;
    }
}

bool Texture::use(VkCommandBuffer cmd, Implicit implicit, VkImageLayout layout, VkImageLayout srcLayout)
{
    VkImageLayout transferLayout;
    switch (ImplicitFilter(implicit, LAYOUT)) {
        case Implicit::NOTHING:
            srcLayout = layout;
            transferLayout = layout;
            break;
        case Implicit::SRC_LAYOUT:
            transferLayout = layout;
            break;
        case Implicit::DST_LAYOUT:
            transferLayout = srcLayout;
            break;
        case Implicit::LAYOUT:
            transferLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            break;
    }
    VkAccessFlags dstAccess;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    implicitBarrier(layout, dstAccess, dstStage);
    VkAccessFlags srcAccess;
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    implicitBarrier(srcLayout, srcAccess, srcStage);
    if (!onGPU) {
        if (createImage()) {
            master.putLog("Created image support for '" + name + "'", LogType::DEBUG);
            srcLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        } else {
            return false;
        }
    }
    if (!onCPU) {
        if ((uint8_t) implicit & (uint8_t) Implicit::LAYOUT) {
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, srcAccess, dstAccess, srcLayout, layout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, 0, info.mipLevels, 0, info.arrayLayers}};
            vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            return true;
        }
        master.putLog("Can't upload '" + name + "' datas : not stored in RAM", LogType::WARNING);
        return false;
    }
    if ((uint8_t) implicit & (uint8_t)Implicit::SRC_LAYOUT) {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, srcAccess, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, transferLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, 0, info.mipLevels, 0, info.arrayLayers}};
        vkCmdPipelineBarrier(cmd, srcStage, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }
    VkBufferImageCopy region {(VkDeviceSize) staging.offset, info.extent.width * widthSplit, 0, {aspect, 0, 0, info.arrayLayers}, {0, 0, 0}, info.extent};
    if (info.extent.depth == 1) {
        vkCmdCopyBufferToImage(cmd, staging.buffer, image, transferLayout, 1, &region);
    } else {
        std::vector<VkBufferImageCopy> regions;
        regions.reserve(widthSplit);
        region.imageExtent.depth /= widthSplit;
        for (int i = 0; i < widthSplit; ++i) {
            regions.push_back(region);
            region.bufferOffset += info.extent.width * nbChannels * elemSize;
            region.imageOffset.z += region.imageExtent.depth;
        }
        vkCmdCopyBufferToImage(cmd, staging.buffer, image, transferLayout, regions.size(), regions.data());
    }
    if ((uint8_t) implicit & (uint8_t) Implicit::DST_LAYOUT) {
        if (info.mipLevels > 1) {
            VkImageMemoryBarrier barrier[2] {
                {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, srcAccess, VK_ACCESS_TRANSFER_WRITE_BIT, srcLayout, transferLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, 0, info.mipLevels, 0, info.arrayLayers}},
                {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, dstAccess, transferLayout, layout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, info.mipLevels - 1, 1, 0, info.arrayLayers}}};
            VkImageBlit iregion {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, info.arrayLayers}, {{0, 0, 0}, {0, 0, 0}}, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, info.arrayLayers}, {{0, 0, 0}, {(int) info.extent.width, (int) info.extent.height, (int) info.extent.depth}}};
            barrier->srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier->dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier->oldLayout = transferLayout;
            barrier->newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier->subresourceRange.levelCount = 1;
            for (unsigned int i = 1; i < info.mipLevels; ++i) {
                iregion.srcSubresource.mipLevel = iregion.dstSubresource.mipLevel++;
                iregion.srcOffsets[1].x = iregion.dstOffsets[1].x;
                iregion.srcOffsets[1].y = iregion.dstOffsets[1].y;
                iregion.srcOffsets[1].z = iregion.dstOffsets[1].z;
                if (iregion.dstOffsets[1].x > 1)
                    iregion.dstOffsets[1].x /= 2;
                if (iregion.dstOffsets[1].y > 1)
                    iregion.dstOffsets[1].y /= 2;
                if (iregion.dstOffsets[1].z > 1)
                    iregion.dstOffsets[1].z /= 2;
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, barrier);
                vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, transferLayout, 1, &iregion, VK_FILTER_LINEAR);
                ++barrier->subresourceRange.baseMipLevel;
            }
            barrier->srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier->dstAccessMask = dstAccess;
            barrier->oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier->newLayout = layout;
            barrier->subresourceRange.baseMipLevel = 0;
            barrier->subresourceRange.levelCount = info.mipLevels - 1;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage, 0, 0, nullptr, 0, nullptr, 2, barrier);
        } else {
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, dstAccess, transferLayout, layout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, {aspect, 0, info.mipLevels, 0, info.arrayLayers}};
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        }
    }
    return true;
}

VkImage Texture::getImage()
{
    if (image)
        return image;
    master.putLog("Binding of '" + name + "' image implicitly create bindless image (safety fallback)", LogType::INFO);
    preCreateImage();
    return image;
}

VkImageView Texture::getView()
{
    if (onGPU)
        return view;
    master.putLog("Binding of '" + name + "' view implicitly call use() (safety fallback)", LogType::INFO);
    use();
    return view;
}

void Texture::rename(const std::string &_name)
{
    name = _name;
    if (image)
        master.setObjectName(image, VK_OBJECT_TYPE_IMAGE, _name);
    if (view)
        master.setObjectName(view, VK_OBJECT_TYPE_IMAGE_VIEW, _name);
}
