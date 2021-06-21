/*
** EPITECH PROJECT, 2020
** Vulkan-Engine
** File description:
** GPUDisplay.hpp
*/

#ifndef GPUDISPLAY_HPP_
#define GPUDISPLAY_HPP_

#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "EntityCore/SubBuffer.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define JB1 0.5,  0.5, 0.5,  0.125
#define JB2 0.25, 1,   0.25, 0.125

#define J1 1, 0.5, 0, 0.25
#define J2 0, 1,   1, 0.25
#define J3 0, 0,   1, 0.25
#define J4 1, 0,   0, 0.25

#define JD 0, 0, 0, 1
#define JRZ 0, 0, 0, 0
#define JR 0, 0

#define JPUT(pos, fill, down) \
*(jaugeIdxTmpPtr++) = pos; \
*(jaugeIdxTmpPtr++) = fill; \
*(jaugeIdxTmpPtr++) = down; \
*(jaugeIdxTmpPtr++) = pos; \
*(jaugeIdxTmpPtr++) = down; \
*(jaugeIdxTmpPtr++) = fill

class EntityLib;
class GPUEntityMgr;

class VulkanMgr;
class BufferMgr;
class FrameMgr;
class RenderMgr;

class SetMgr;
class Set;
class PipelineLayout;
class Pipeline;
class SyncEvent;
class Texture;
class VertexArray;
class VertexBuffer;

struct JaugeVertex {
    float x;
    float y;
    float z;
    float w;
    float r;
    float g;
    float b;
    float a;
};

struct ImageVertex {
    float x;
    float y;
    float texX;
    float texY;
};

class GPUDisplay {
public:
    GPUDisplay(std::shared_ptr<EntityLib> master, GPUEntityMgr &entityMgr);
    virtual ~GPUDisplay();
    GPUDisplay(const GPUDisplay &cpy) = delete;
    GPUDisplay &operator=(const GPUDisplay &src) = delete;

    void start();
    void pause(); // To switch to another graphical api
    void unpause();
    void stop() {alive = false; active = true;}

    JaugeVertex *getJaugePtr() {return jaugePtr;}

    // No time for better
    std::string header1 = "Score ";
    std::string header2 = "Level ";
    char sep1 = '/';
    char sep2 = '|';
    bool section2 = false;
    long score1 = 0;
    long score1Max = 0;
    long score2 = 0;
    long score2Max = 0;
    int level1 = 0;
    int level1Max = 0;

private:
    void mainloop();
    void submitResources();
    void initCommands();
    VulkanMgr &vkmgr;
    BufferMgr &localBuffer;
    Texture &entityMap;
    RenderMgr &renderMgr;
    std::vector<std::unique_ptr<FrameMgr>> &frames;
    VkSwapchainKHR &swapchain;
    SubBuffer entityVertexBuffer;
    std::mutex shield;
    std::unique_ptr<std::thread> updater;
    std::unique_ptr<SetMgr> setMgr;
    std::unique_ptr<Set> bgSet;
    std::unique_ptr<Set> entitySet;
    std::unique_ptr<Set> sbSet;
    std::unique_ptr<VertexArray> imageVertexArray;
    std::unique_ptr<VertexArray> jaugeVertexArray;
    std::unique_ptr<VertexArray> entityVertexArray;
    std::unique_ptr<PipelineLayout> imagePLayout;
    std::unique_ptr<PipelineLayout> jaugePLayout;
    std::unique_ptr<Pipeline> imagePipeline;
    std::unique_ptr<Pipeline> entityPipeline;
    std::unique_ptr<Pipeline> jaugePipeline;
    std::unique_ptr<VertexBuffer> imageVertexBuffer;
    std::unique_ptr<VertexBuffer> jaugeVertexBuffer;
    std::unique_ptr<BufferMgr> indexBufferMgr;
    std::unique_ptr<BufferMgr> vertexBufferMgr;
    std::unique_ptr<Texture> background;
    std::unique_ptr<Texture> scoreboard; // 68x240

    SubBuffer jaugeIndexBuffer;
    SubBuffer entityIndexBuffer;
    SubBuffer jaugeStaging;
    JaugeVertex *jaugePtr;
    VkQueue graphicQueue;
    VkSemaphore semaphores[6];
    VkFence fences[3];
    VkCommandPool cmdPool;
    VkCommandBuffer cmds[3];
    TTF_Font *myFont;
    SDL_Surface *scoreboardSurface;

    uint32_t imageIdx = 0;
    const VkPipelineStageFlags semaphoreStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo sinfo[3] {
        {VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, nullptr, &semaphoreStage, 1, cmds, 1, semaphores + 3},
        {VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, nullptr, &semaphoreStage, 1, cmds + 1, 1, semaphores + 4},
        {VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, nullptr, &semaphoreStage, 1, cmds + 2, 1, semaphores + 5}
    };
    VkPresentInfoKHR presentInfo[3] {
        {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1, semaphores + 3, 1, &swapchain, &imageIdx, nullptr},
        {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1, semaphores + 4, 1, &swapchain, &imageIdx, nullptr},
        {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1, semaphores + 5, 1, &swapchain, &imageIdx, nullptr}
    };

    std::shared_ptr<EntityLib> master;
    int switcher = 0; // out of 3
    bool alive = false;
    bool active = true;
};

#endif /* GPUDISPLAY_HPP_ */
