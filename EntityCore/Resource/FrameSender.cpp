#include "FrameSender.hpp"
#include "EntityCore/Core/VulkanMgr.hpp"
#include "EntityCore/Core/BufferMgr.hpp"
#include "Texture.hpp"
#include <iostream>

FrameSender::FrameSender(VulkanMgr &vkmgr, std::vector<std::unique_ptr<Texture>> &frames, VkFence *fences) : vkmgr(vkmgr), frames(frames), fences(fences)
{
    frames[0]->getDimensions(width, height);
    readbackMgr = std::make_unique<BufferMgr>(vkmgr, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 0, frames.size()*4*width*height);
    readbackMgr->setName("FrameSender readback buffer");
    for (int i = 0; i < frames.size(); ++i) {
        readback.push_back(readbackMgr->acquireBuffer(4*width*height));
        readbackPtr.push_back(readbackMgr->getPtr(readback.back()));
    }
    pendingFrameIdx = blockedFrameIdx = frames.size() - 1;
    thread = std::thread(&FrameSender::mainloop, this);
}

FrameSender::~FrameSender()
{
    alive = false;
    thread.join();
    for (auto &rb : readback) {
        readbackMgr->releaseBuffer(rb);
    }
    readbackMgr = nullptr;
}

void FrameSender::setupReadback(VkCommandBuffer cmd, unsigned char frameIdx)
{
    VkBufferImageCopy region {(VkDeviceSize) readback[frameIdx].offset, 0, 0, VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {(uint32_t) width, (uint32_t) height, 1}};
    vkCmdCopyImageToBuffer(cmd, frames[frameIdx]->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback[frameIdx].buffer, 1, &region);
}

void FrameSender::mainloop()
{
    while (alive) {
        if (blockedFrameIdx == pendingFrameIdx) {
            std::this_thread::yield();
            continue;
        }
        blockedFrameIdx = (blockedFrameIdx + 1) % readback.size();
        vkWaitForFences(vkmgr.refDevice, 1, fences + blockedFrameIdx, VK_TRUE, UINT32_MAX);
        vkResetFences(vkmgr.refDevice, 1, fences + blockedFrameIdx);
        readbackMgr->invalidate(readback[blockedFrameIdx]);
        submitFrame(readbackPtr[blockedFrameIdx]);
        std::cout << "Submit frame " << (int) blockedFrameIdx << std::endl;
    }
}

void FrameSender::acquireFrame(uint32_t &frameIdx)
{
    while (nextFrameIdx == blockedFrameIdx) {
        std::this_thread::yield();
    }
    frameIdx = nextFrameIdx++;
    nextFrameIdx %= readback.size();
}

void FrameSender::presentFrame(uint32_t frameIdx)
{
    pendingFrameIdx = frameIdx;
}

void FrameSender::submitFrame(void *data)
{
    // Send the frame here
}
