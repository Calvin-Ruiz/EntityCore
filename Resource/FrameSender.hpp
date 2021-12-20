#ifndef FRAME_SENDER_HPP_
#define FRAME_SENDER_HPP_

#include "EntityCore/SubBuffer.hpp"
#include <vulkan/vulkan.h>
#include <thread>
#include <memory>
#include <vector>
class VulkanMgr;
class Texture;
class BufferMgr;

class FrameSender {
public:
    FrameSender(VulkanMgr &vkmgr, std::vector<std::unique_ptr<Texture>> &frames, VkFence *fences);
    virtual ~FrameSender();
    void setupReadback(VkCommandBuffer cmd, unsigned char frameIdx);
    void acquireFrame(uint32_t &frameIdx);
    void presentFrame(uint32_t frameIdx);
private:
    virtual void submitFrame(void *data) = 0;
    void mainloop();
    VulkanMgr &vkmgr;
    std::vector<std::unique_ptr<Texture>> &frames;
    std::thread thread;
    std::unique_ptr<BufferMgr> readbackMgr;
    std::vector<SubBuffer> readback;
    std::vector<void *> readbackPtr;
    VkFence *fences;
    int width;
    int height;
    bool alive = true;
    unsigned char pendingFrameIdx = 2; // Frame waiting to be presented
    unsigned char blockedFrameIdx = 2; // Frame currently presented
    unsigned char nextFrameIdx = 0; // Next frame to acquire
};

#endif /* end of include guard: FRAME_SENDER_HPP_ */
