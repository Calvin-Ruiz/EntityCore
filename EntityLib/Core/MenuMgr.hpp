#ifndef MENUMGR_HPP_
#define MENUMGR_HPP_

class VulkanMgr;
class FrameMgr;
class RenderMgr;
class BufferMgr;
class VertexArray;
class VertexBuffer;
class Texture;
class MenuBase;
class Set;
class SetMgr;
class Pipeline;
class PipelineLayout;

class EntityLib;

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <memory>
#include <vector>
#include <list>
#include "EntityCore/SubBuffer.hpp"

class MenuMgr {
public:
    MenuMgr(std::shared_ptr<EntityLib> master, void *data, void (*selectPage)(void *data, int pageShift, int player));
    virtual ~MenuMgr();
    MenuMgr(const MenuMgr &cpy) = delete;
    MenuMgr &operator=(const MenuMgr &src) = delete;

    bool mainloop(bool _allowExit = true);
    void close() {alive = false;}

    // For internal use only
    std::list<MenuBase *>::iterator attach(MenuBase *element);
    void detach(std::list<MenuBase *>::iterator &element);
    unsigned char getHoverMask(int hoverID, float x1, float y1, float x2, float y2) const;

    TTF_Font *myFont;
    SDL_Surface *surface;
    const int width = 1920;
    const int height = 1080;
    int lastHoverID = 0;
    unsigned char clicmask;
private:
    VulkanMgr &vkmgr;
    RenderMgr &renderMgr;
    std::vector<std::unique_ptr<FrameMgr>> &frames;
    VkSwapchainKHR &swapchain;
    std::shared_ptr<EntityLib> master;
    std::unique_ptr<BufferMgr> uniBufferMgr;
    std::unique_ptr<SetMgr> setMgr;
    std::unique_ptr<Set> menuSet;
    std::unique_ptr<Pipeline> pipeline;
    std::unique_ptr<PipelineLayout> pLayout;
    std::list<MenuBase *> elements;
    void *data;
    void (*selectPage)(void *data, int pageShift, int player);
    Texture *surfaceTexture;
    VkSemaphore semaphores[3];
    const VkPipelineStageFlags semaphoreStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    uint32_t imageIdx;
    VkSubmitInfo sInfo {VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, semaphores, &semaphoreStage, 1, nullptr, 1, semaphores + 2};
    VkPresentInfoKHR pInfo {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1, semaphores + 2, 1, &swapchain, &imageIdx, nullptr};
    VkQueue graphicQueue;
    bool allowExit = false;
    bool alive = false;
    float mouseX = 0;
    float mouseY = 0;
    int p1select = -1;
    int p2select = -1;
};

#endif
