/*
** EPITECH PROJECT, 2020
** Vulkan-Engine
** File description:
** GPUDisplay.cpp
*/
#include "EntityLib.hpp"
#include "GPUDisplay.hpp"
#include "GPUEntityMgr.hpp"
#include "EntityCore/Core/VulkanMgr.hpp"
#include "EntityCore/Core/BufferMgr.hpp"
#include "EntityCore/Core/FrameMgr.hpp"
#include "EntityCore/Core/RenderMgr.hpp"
#include "EntityCore/Resource/Pipeline.hpp"
#include "EntityCore/Resource/PipelineLayout.hpp"
#include "EntityCore/Resource/Set.hpp"
#include "EntityCore/Resource/SetMgr.hpp"
#include "EntityCore/Resource/SyncEvent.hpp"
#include "EntityCore/Resource/Texture.hpp"
#include "EntityCore/Resource/VertexArray.hpp"
#include "EntityCore/Resource/VertexBuffer.hpp"
#include "EntityCore/Resource/FrameSender.hpp"
#include <chrono>

GPUDisplay::GPUDisplay(std::shared_ptr<EntityLib> master, GPUEntityMgr &entityMgr) : vkmgr(*VulkanMgr::instance), localBuffer(master->getLocalBuffer()), entityMap(master->getEntityMap()), renderMgr(master->getRender()), frames(master->getFrames()), swapchain(vkmgr.getSwapchain()), master(master)
{
    entityVertexBuffer = entityMgr.getVertexBuffer();
    VkCommandPoolCreateInfo poolInfo {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0, master->graphicQueueFamily->id};
    vkCreateCommandPool(vkmgr.refDevice, &poolInfo, nullptr, &cmdPool);
    VkCommandBufferAllocateInfo allocInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 3};
    vkAllocateCommandBuffers(vkmgr.refDevice, &allocInfo, cmds);
    graphicQueue = master->graphicQueue;
    if (WINDOWLESS) {

    } else {
        VkSemaphoreCreateInfo semInfo {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};
        for (int i = 0; i < 6; ++i)
            vkCreateSemaphore(vkmgr.refDevice, &semInfo, nullptr, semaphores + i);
    }

    imagePLayout = std::make_unique<PipelineLayout>(vkmgr);
    imagePLayout->setTextureLocation(0, &PipelineLayout::DEFAULT_SAMPLER);
    imagePLayout->buildLayout();
    imagePLayout->build();
    jaugePLayout = std::make_unique<PipelineLayout>(vkmgr);
    jaugePLayout->build();
    setMgr = std::make_unique<SetMgr>(vkmgr, 3, 0, 3);
    bgSet = std::make_unique<Set>(vkmgr, *setMgr, imagePLayout.get());
    entitySet = std::make_unique<Set>(vkmgr, *setMgr, imagePLayout.get());
    sbSet = std::make_unique<Set>(vkmgr, *setMgr, imagePLayout.get());

    entityVertexArray = std::make_unique<VertexArray>(vkmgr, sizeof(float) * 6);
    entityVertexArray->createBindingEntry(sizeof(float) * 6);
    entityVertexArray->addInput(VK_FORMAT_R32G32B32A32_SFLOAT);
    entityVertexArray->addInput(VK_FORMAT_R32G32_SFLOAT);
    imageVertexArray = std::make_unique<VertexArray>(vkmgr, sizeof(ImageVertex)); // no need for alignment because bound undependently
    imageVertexArray->createBindingEntry(sizeof(ImageVertex));
    imageVertexArray->addInput(VK_FORMAT_R32G32_SFLOAT);
    imageVertexArray->addInput(VK_FORMAT_R32G32_SFLOAT);
    jaugeVertexArray = std::make_unique<VertexArray>(vkmgr, sizeof(JaugeVertex));// no need for alignment because bound undependently
    jaugeVertexArray->createBindingEntry(sizeof(JaugeVertex));
    jaugeVertexArray->addInput(VK_FORMAT_R32G32_SFLOAT);
    jaugeVertexArray->addInput(VK_FORMAT_R32G32B32A32_SFLOAT);
    vertexBufferMgr = std::make_unique<BufferMgr>(vkmgr, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, sizeof(ImageVertex)*6*3 + sizeof(JaugeVertex)*(4*10));
    vertexBufferMgr->setName("Graphic objects");
    imageVertexBuffer = imageVertexArray->createBuffer(0, 6*3, vertexBufferMgr.get());
    jaugeVertexBuffer = jaugeVertexArray->createBuffer(0, 4*10, vertexBufferMgr.get());
    indexBufferMgr = std::make_unique<BufferMgr>(vkmgr, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, 2*6*END_ALL);
    indexBufferMgr->setName("Index buffer");
    entityIndexBuffer = indexBufferMgr->acquireBuffer(2*6*END_ALL);

    TTF_Init();
    submitResources();
    VkFenceCreateInfo fenceInfo {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, WINDOWLESS ? 0u : VK_FENCE_CREATE_SIGNALED_BIT};
    for (int i = 0; i < 3; ++i)
        vkCreateFence(vkmgr.refDevice, &fenceInfo, nullptr, fences + i);
    if (WINDOWLESS) {
        sender = std::make_unique<FrameSender>(vkmgr, master->frameDraw, fences);
    }
    initCommands();
}

GPUDisplay::~GPUDisplay()
{
    stop();
    if (updater->joinable())
        updater->join();
    vkQueueWaitIdle(graphicQueue);
    sender = nullptr;
    TTF_CloseFont(myFont);
    TTF_Quit();
    SDL_FreeSurface(scoreboardSurface);
    vkDestroyCommandPool(vkmgr.refDevice, cmdPool, nullptr);
    for (int i = 0; i < 3; ++i)
        vkDestroyFence(vkmgr.refDevice, fences[i], nullptr);
    if (!WINDOWLESS) {
        for (int i = 0; i < 6; ++i)
            vkDestroySemaphore(vkmgr.refDevice, semaphores[i], nullptr);
    }
    imageVertexBuffer.reset();
    jaugeVertexBuffer.reset();
}

void GPUDisplay::stop()
{
    if (alive) {
        alive = false;
        active = true;
        updater->join();
    }
}

void GPUDisplay::start()
{
    if (!alive) {
        alive = true;
        updater = std::make_unique<std::thread>(&GPUDisplay::mainloop, this);
    }
}

void GPUDisplay::pause()
{
    active = false;
    shield.lock();
    active = true;
}

void GPUDisplay::unpause()
{
    shield.unlock();
}

void GPUDisplay::mainloop()
{
    auto delay = std::chrono::duration<int, std::ratio<1,1000000>>(1000000/200); // Limit to x2 base compute time
    shield.lock();
    auto clock = std::chrono::system_clock::now() + delay;
    while (alive) {
        if (WINDOWLESS) {
            sender->acquireFrame(imageIdx);
        } else {
            auto res = vkAcquireNextImageKHR(vkmgr.refDevice, swapchain, UINT32_MAX, semaphores[switcher], VK_NULL_HANDLE, &imageIdx);
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
        }

        std::string s = header1 + EntityLib::toText(score1);
        s.push_back(sep1);
        s += EntityLib::toText(score1Max);
        if (section2) {
            s += sep2 + EntityLib::toText(score2);
            s.push_back(sep1);
            s += EntityLib::toText(score2Max);
        }
        SDL_FillRect(scoreboardSurface, NULL, 0);
        SDL_Color color={240,240,240,0};
    	SDL_Surface *text = TTF_RenderUTF8_Blended(myFont, s.c_str(), color);
        SDL_Rect tmp;
        tmp.x=2;
        tmp.y=1;
        tmp.w=text->w;
        tmp.h=text->h;
        SDL_BlitSurface(text, NULL, scoreboardSurface, &tmp);
        SDL_FreeSurface(text);
        s = header2 + std::to_string(level1);
        s.push_back(sep1);
        s += std::to_string(level1Max);
        text = TTF_RenderUTF8_Blended(myFont, s.c_str(), color);
        tmp.y+=tmp.h;
        tmp.w=text->w;
        tmp.h=text->h;
        SDL_BlitSurface(text, NULL, scoreboardSurface, &tmp);
        SDL_FreeSurface(text);

        if (WINDOWLESS) {
            vkQueueSubmit(graphicQueue, 1, sinfo + imageIdx, fences[imageIdx]);
            sender->presentFrame(imageIdx);
        } else {
            sinfo[imageIdx].pWaitSemaphores = semaphores + switcher;
            vkWaitForFences(vkmgr.refDevice, 1, fences + imageIdx, VK_TRUE, UINT32_MAX);
            // sumbit this frame right now !
            vkResetFences(vkmgr.refDevice, 1, fences + imageIdx);
            vkQueueSubmit(graphicQueue, 1, sinfo + imageIdx, fences[imageIdx]);
            vkQueuePresentKHR(graphicQueue, presentInfo + imageIdx);
        }
        if (active) {
            std::this_thread::sleep_until(clock);
            clock = std::chrono::system_clock::now() + delay;
        } else {
            shield.unlock();
            while (!active)
                std::this_thread::sleep_for(std::chrono::microseconds(400));
            shield.lock();
        }
        switcher = (switcher + 1) % 3;
    }
    shield.unlock();
}

void GPUDisplay::submitResources()
{
    auto tmpMgr = std::make_unique<BufferMgr>(vkmgr, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0, 1920*1080*4*sizeof(float)); // Same size and properties than menu buffer => reusable memory for menu
    tmpMgr->setName("Temporary staging buffer");
    background = std::make_unique<Texture>(vkmgr, *tmpMgr, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "background.png");
    background->init();
    VkCommandBufferBeginInfo beginInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
    vkBeginCommandBuffer(cmds[0], &beginInfo);
    entityMap.use(cmds[0], true);
    background->use(cmds[0], true);
    SubBuffer imageTmp = tmpMgr->acquireBuffer(sizeof(ImageVertex)*6*3);
    ImageVertex *imageTmpPtr = (ImageVertex *) tmpMgr->getPtr(imageTmp);
    {
        int i = 0;
        imageTmpPtr[i++] = ImageVertex({-1, -1, 0, 0});
        imageTmpPtr[i++] = ImageVertex({1, -1, 1, 0});
        imageTmpPtr[i++] = ImageVertex({1, 1, 1, 1});
        imageTmpPtr[i++] = ImageVertex({-1, -1, 0, 0});
        imageTmpPtr[i++] = ImageVertex({-1, 1, 0, 1});
        imageTmpPtr[i++] = ImageVertex({1, 1, 1, 1});

        imageTmpPtr[i++] = ImageVertex({-1, -1, 0, 0});
        imageTmpPtr[i++] = ImageVertex({-0.5, -1, 1, 0});
        imageTmpPtr[i++] = ImageVertex({-0.5, -0.6, 1, 1});
        imageTmpPtr[i++] = ImageVertex({-1, -1, 0, 0});
        imageTmpPtr[i++] = ImageVertex({-1, -0.6, 0, 1});
        imageTmpPtr[i++] = ImageVertex({-0.5, -0.6, 1, 1});
    }
    BufferMgr::copy(cmds[0], imageTmp, imageVertexBuffer->get());
    SubBuffer entityIdxTmp = tmpMgr->acquireBuffer(2*6*END_ALL);
    unsigned short *entityIdxTmpPtr = (unsigned short *) tmpMgr->getPtr(entityIdxTmp);
    for (int i = 0; i < END_ALL*4; i += 4) {
        *(entityIdxTmpPtr++) = i;
        *(entityIdxTmpPtr++) = i | 1;
        *(entityIdxTmpPtr++) = i | 2;
        *(entityIdxTmpPtr++) = i;
        *(entityIdxTmpPtr++) = i | 1;
        *(entityIdxTmpPtr++) = i | 3;
    }
    BufferMgr::copy(cmds[0], entityIdxTmp, entityIndexBuffer);
    vkEndCommandBuffer(cmds[0]);
    VkSubmitInfo sInfo {VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, cmds, 0, nullptr};
    vkQueueSubmit(graphicQueue, 1, &sInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicQueue);

    entityMap.detach();
    background->detach();
    jaugeStaging = localBuffer.acquireBuffer(sizeof(JaugeVertex)*4*10);
    jaugePtr = (JaugeVertex *) localBuffer.getPtr(jaugeStaging);
    {
        JSET(0, JaugeVertex({0, 0, JB1}));
        JSET(1, JaugeVertex({0, 0, J1}));
        JSET(2, JaugeVertex({0, 0, J2}));
        JSET(3, JaugeVertex({0, 0, J3}));
        JSET(4, JaugeVertex({0, 0, J4}));
        JSET(5, JaugeVertex({0, 0, JB2}));
        JSET(6, JaugeVertex({0, 0, J1}));
        JSET(7, JaugeVertex({0, 0, J2}));
        JSET(8, JaugeVertex({0, 0, J3}));
        JSET(9, JaugeVertex({0, 0, J4}));
    }
    myFont = TTF_OpenFont("textures/font.ttf", 32);
    scoreboard = std::make_unique<Texture>(vkmgr, localBuffer, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, "scoreboard");
    // load empty surface
    scoreboard->init(240, 68);
    scoreboardSurface = scoreboard->createSDLSurface();
    // SyncEvent tmpEvt;
    // tmpEvt.imageBarrier(*scoreboard, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    // tmpEvt.build();
    // tmpEvt.placeBarrier(cmds[1]);
    // vkEndCommandBuffer(cmds[1]);
    // sInfo.pCommandBuffers = cmds + 1;
    // vkQueueSubmit(graphicQueue, 1, &sInfo, VK_NULL_HANDLE);
    // vkQueueWaitIdle(graphicQueue);
    vkResetCommandPool(vkmgr.refDevice, cmdPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
}

void GPUDisplay::initCommands()
{
    Pipeline::setShaderDir("./shader/");
    bgSet->bindTexture(*background, 0);
    entitySet->bindTexture(entityMap, 0);
    sbSet->bindTexture(*scoreboard, 0);
    entityPipeline = std::make_unique<Pipeline>(vkmgr, renderMgr, 0, imagePLayout.get());
    entityPipeline->bindShader("image.vert.spv");
    entityPipeline->bindShader("image.frag.spv");
    entityPipeline->bindVertex(*entityVertexArray);
    entityPipeline->setCullMode(false);
    entityPipeline->setDepthStencilMode();
    entityPipeline->build();
    imagePipeline = std::make_unique<Pipeline>(vkmgr, renderMgr, 0, imagePLayout.get());
    imagePipeline->bindShader("image.vert.spv");
    imagePipeline->bindShader("image.frag.spv");
    imagePipeline->bindVertex(*imageVertexArray);
    imagePipeline->setCullMode(false);
    imagePipeline->setDepthStencilMode();
    imagePipeline->build();
    jaugePipeline = std::make_unique<Pipeline>(vkmgr, renderMgr, 0, jaugePLayout.get());
    jaugePipeline->bindShader("jauge.vert.spv");
    jaugePipeline->bindShader("jauge.frag.spv");
    jaugePipeline->bindVertex(*jaugeVertexArray);
    jaugePipeline->setCullMode(false);
    jaugePipeline->setDepthStencilMode();
    jaugePipeline->build();

    SyncEvent tmpSync2(VK_DEPENDENCY_BY_REGION_BIT);
    tmpSync2.bufferBarrier(jaugeVertexBuffer->get(), VK_PIPELINE_STAGE_2_HOST_BIT_KHR, VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_ACCESS_2_HOST_WRITE_BIT_KHR, VK_ACCESS_2_TRANSFER_READ_BIT_KHR);
    tmpSync2.build();
    SyncEvent tmpSync(VK_DEPENDENCY_BY_REGION_BIT);
    tmpSync.bufferBarrier(jaugeVertexBuffer->get(), VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT_KHR);
    tmpSync.build();
    VkCommandBufferBeginInfo beginInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};
    for (int i = 0; i < 3; ++i) {
        VkCommandBuffer cmd = cmds[i];
        vkBeginCommandBuffer(cmd, &beginInfo);
        tmpSync2.placeBarrier(cmd);
        BufferMgr::copy(cmd, jaugeStaging, jaugeVertexBuffer->get());
        tmpSync.placeBarrier(cmd);
        scoreboard->use(cmd);
        renderMgr.begin(i, cmd);
        imagePipeline->bind(cmd);
        VertexArray::bind(cmd, imageVertexBuffer->get());
        imagePLayout->bindSet(cmd, *bgSet);
        vkCmdDraw(cmd, 6, 1, 0, 0);
        imagePLayout->bindSet(cmd, *sbSet);
        vkCmdDraw(cmd, 6, 1, 6, 0);

        entityPipeline->bind(cmd);
        VertexArray::bind(cmd, entityVertexBuffer);
        imagePLayout->bindSet(cmd, *entitySet);
        vkCmdBindIndexBuffer(cmd, entityIndexBuffer.buffer, entityIndexBuffer.offset, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd, 6*END_ALL, 1, 0, 0, 0);

        jaugePipeline->bind(cmd);

        VertexArray::bind(cmd, jaugeVertexBuffer->get());
        vkCmdDrawIndexed(cmd, 6*10, 1, 0, 0, 0);
        vkCmdEndRenderPass(cmd);
        if (WINDOWLESS) {
            sender->setupReadback(cmd, i);
        }
        vkEndCommandBuffer(cmd);
    }
}
