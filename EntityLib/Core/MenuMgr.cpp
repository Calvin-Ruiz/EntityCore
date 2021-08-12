#include "MenuMgr.hpp"
#include "EntityLib.hpp"
#include "MenuBase.hpp"
#include "EntityCore/Core/VulkanMgr.hpp"
#include "EntityCore/Core/FrameMgr.hpp"
#include "EntityCore/Core/RenderMgr.hpp"
#include "EntityCore/Core/BufferMgr.hpp"
#include "EntityCore/Resource/Texture.hpp"
#include "EntityCore/Resource/Set.hpp"
#include "EntityCore/Resource/SetMgr.hpp"
#include "EntityCore/Resource/Pipeline.hpp"
#include "EntityCore/Resource/PipelineLayout.hpp"
#include <thread>
#include <chrono>

#define SELPAGE(rel, player) if (selectPage) selectPage(data, rel, player)

MenuMgr::MenuMgr(std::shared_ptr<EntityLib> master, void *data, void (*selectPage)(void *data, int pageShift, int player)) : vkmgr(*VulkanMgr::instance),renderMgr(master->getRender()), frames(master->getFrames()), swapchain(vkmgr.getSwapchain()), master(master), data(data), selectPage(selectPage)
{
    VkSemaphoreCreateInfo semInfo {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};
    vkCreateSemaphore(vkmgr.refDevice, &semInfo, nullptr, semaphores);
    vkCreateSemaphore(vkmgr.refDevice, &semInfo, nullptr, semaphores + 1);
    vkCreateSemaphore(vkmgr.refDevice, &semInfo, nullptr, semaphores + 2);
    graphicQueue = master->graphicQueue;
    myFont = TTF_OpenFont("textures/font.ttf", 32);

    uniBufferMgr = std::make_unique<BufferMgr>(vkmgr, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0, 1920*1080*4*sizeof(float));
    surfaceTexture = new Texture(vkmgr, *uniBufferMgr, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, "Menu SDL Surface");
    surfaceTexture->init(1920, 1080);
    surface = surfaceTexture->createSDLSurface();
    pLayout = std::make_unique<PipelineLayout>(vkmgr);
    pLayout->setTextureLocation(0, &PipelineLayout::DEFAULT_SAMPLER);
    pLayout->buildLayout();
    pLayout->build();

    setMgr = std::make_unique<SetMgr>(vkmgr, 1, 0, 1);
    menuSet = std::make_unique<Set>(vkmgr, *setMgr, pLayout.get());

    pipeline = std::make_unique<Pipeline>(vkmgr, renderMgr, 0, pLayout.get());
    pipeline->setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
    pipeline->bindShader("screen.vert.spv");
    pipeline->bindShader("image.frag.spv");
    pipeline->build("MenuMgr");
    menuSet->bindTexture(*surfaceTexture, 0);
    for (auto &f : frames) {
        VkCommandBuffer &cmd = f->begin(VK_SUBPASS_CONTENTS_INLINE, 1, &surfaceTexture);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->get());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pLayout->getPipelineLayout(), 0, 1, menuSet->get(), 0, nullptr);
        vkCmdDraw(cmd, 4, 1, 0, 0);
        f->compileMain();
    }
}

MenuMgr::~MenuMgr()
{
    while (!elements.empty())
        delete elements.front();
    vkQueueWaitIdle(graphicQueue);
    TTF_CloseFont(myFont);
    vkDestroySemaphore(vkmgr.refDevice, semaphores[0], nullptr);
    vkDestroySemaphore(vkmgr.refDevice, semaphores[1], nullptr);
    vkDestroySemaphore(vkmgr.refDevice, semaphores[2], nullptr);
    SDL_FreeSurface(surface);
    delete surfaceTexture;
}

bool MenuMgr::mainloop(bool _allowExit)
{
    auto delay = std::chrono::duration<int, std::ratio<1,1000000>>(1000000/20);
    auto clock = std::chrono::system_clock::now() + delay;
    allowExit = _allowExit;
    alive = true;
    bool hasChanges = true;
    unsigned char frameparity = 0;
    SELPAGE(0, 0);
    SELPAGE(0, 1);
    clicmask = 0;
    while (alive) {
        auto res = vkAcquireNextImageKHR(vkmgr.refDevice, swapchain, UINT32_MAX, semaphores[frameparity], VK_NULL_HANDLE, &imageIdx);
        switch (res) {
            case VK_SUCCESS:
                break;
            case VK_SUBOPTIMAL_KHR:
                vkmgr.putLog("Suboptimal swapchain", LogType::WARNING);
                break;
            case VK_TIMEOUT:
                vkmgr.putLog("Timeout for swapchain acquire", LogType::ERROR);
                continue;
            default:
                vkmgr.putLog("Invalid swapchain", LogType::ERROR);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    SDL_PushEvent(&event);
                    return false;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_F4:
                            SDL_PushEvent(&event);
                            return false;
                        case SDL_SCANCODE_ESCAPE:
                            if (allowExit)
                                return false;
                            break;
                        case SDL_SCANCODE_A:
                            SELPAGE(-1, 0);
                            break;
                        case SDL_SCANCODE_D:
                            SELPAGE(1, 0);
                            break;
                        case SDL_SCANCODE_W:
                            if (--p1select < 0)
                                p1select = lastHoverID - 1;
                            break;
                        case SDL_SCANCODE_S:
                            if (++p1select == lastHoverID)
                                p1select = 0;
                            break;
                        case SDL_SCANCODE_KP_4:
                            SELPAGE(-1, 1);
                            break;
                        case SDL_SCANCODE_KP_6:
                            SELPAGE(1, 1);
                            break;
                        case SDL_SCANCODE_KP_8:
                            if (--p2select < 0)
                                p2select = lastHoverID - 1;
                            break;
                        case SDL_SCANCODE_KP_5:
                            if (++p2select == lastHoverID)
                                p2select = 0;
                            break;
                        case SDL_SCANCODE_SPACE:
                            clicmask |= 2;
                            break;
                        case SDL_SCANCODE_KP_0:
                            clicmask |= 4;
                            break;
                        default:;
                    }
                    break;
                case SDL_KEYUP:
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_SPACE:
                            clicmask &= ~2;
                            break;
                        case SDL_SCANCODE_KP_0:
                            clicmask &= ~4;
                            break;
                        default:;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    switch (event.button.button) {
                        case SDL_BUTTON_LEFT:
                            clicmask |= 1;
                            break;
                        default:;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    switch (event.button.button) {
                        case SDL_BUTTON_LEFT:
                            clicmask &= ~1;
                            break;
                        default:;
                    }
                    break;
                default:;
            }
        }
        int mx, my;
        clicmask |= (SDL_GetMouseState(&mx, &my) & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        mouseX = mx / (float) width;
        mouseY = my / (float) height;
        // There is changes if one of the updates has returned something
        for (auto &e : elements)
            hasChanges |= e->update();
        if (hasChanges) {
            SDL_FillRect(surface, NULL, 0);
            for (auto &e : elements)
                e->draw();
        }
        VkCommandBuffer &cmd = frames[imageIdx]->begin(VK_SUBPASS_CONTENTS_INLINE, (int) hasChanges, &surfaceTexture);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->get());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pLayout->getPipelineLayout(), 0, 1, menuSet->get(), 0, nullptr);
        vkCmdDraw(cmd, 4, 1, 0, 0);
        frames[imageIdx]->compileMain();
        sInfo.pCommandBuffers = &cmd;
        sInfo.pWaitSemaphores = semaphores + frameparity;
        hasChanges = false;
        vkQueueWaitIdle(graphicQueue);
        vkQueueSubmit(graphicQueue, 1, &sInfo, VK_NULL_HANDLE);
        vkQueuePresentKHR(graphicQueue, &pInfo);
        std::this_thread::sleep_until(clock);
        clock = std::chrono::system_clock::now() + delay;
        frameparity = !frameparity;
    }
    return true;
}

std::list<MenuBase *>::iterator MenuMgr::attach(MenuBase *element)
{
    elements.push_back(element);
    return --elements.end();
}

void MenuMgr::detach(std::list<MenuBase *>::iterator &element)
{
    elements.erase(element);
}

unsigned char MenuMgr::getHoverMask(int hoverID, float x1, float y1, float x2, float y2) const
{
    return ((y1 <= mouseY && y2 >= mouseY && x1 <= mouseX && x2 >= mouseX) | ((p1select == hoverID) << 1) | ((p2select == hoverID) << 2));
}
