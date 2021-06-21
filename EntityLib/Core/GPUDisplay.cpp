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
#include <chrono>

GPUDisplay::GPUDisplay(std::shared_ptr<EntityLib> master, GPUEntityMgr &entityMgr) : vkmgr(*VulkanMgr::instance), localBuffer(master->getLocalBuffer()), entityMap(master->getEntityMap()), renderMgr(master->getRender()), frames(master->getFrames()), swapchain(vkmgr.getSwapchain()), master(master)
{
    entityVertexBuffer = entityMgr.getVertexBuffer();
    VkCommandPoolCreateInfo poolInfo {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0, (uint32_t) vkmgr.getGraphicQueueFamilyIndex()[0]};
    vkCreateCommandPool(vkmgr.refDevice, &poolInfo, nullptr, &cmdPool);
    VkCommandBufferAllocateInfo allocInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 3};
    vkAllocateCommandBuffers(vkmgr.refDevice, &allocInfo, cmds);
    graphicQueue = vkmgr.getGraphicQueues()[0];
    VkSemaphoreCreateInfo semInfo {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0};
    for (int i = 0; i < 6; ++i)
        vkCreateSemaphore(vkmgr.refDevice, &semInfo, nullptr, semaphores + i);

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
    imageVertexArray = std::make_unique<VertexArray>(vkmgr, sizeof(JaugeVertex));
    imageVertexArray->createBindingEntry(sizeof(ImageVertex));
    imageVertexArray->addInput(VK_FORMAT_R32G32_SFLOAT);
    imageVertexArray->addInput(VK_FORMAT_R32G32_SFLOAT);
    jaugeVertexArray = std::make_unique<VertexArray>(vkmgr, sizeof(JaugeVertex));
    jaugeVertexArray->createBindingEntry(sizeof(JaugeVertex));
    jaugeVertexArray->addInput(VK_FORMAT_R32G32B32A32_SFLOAT);
    jaugeVertexArray->addInput(VK_FORMAT_R32G32B32A32_SFLOAT);
    vertexBufferMgr = std::make_unique<BufferMgr>(vkmgr, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, sizeof(ImageVertex)*6*3 + sizeof(JaugeVertex)*(4*((1+1)*2+1)+6));
    vertexBufferMgr->setName("Graphic objects");
    imageVertexBuffer = std::unique_ptr<VertexBuffer>(imageVertexArray->createBuffer(0, 6*3, vertexBufferMgr.get()));
    jaugeVertexBuffer = std::unique_ptr<VertexBuffer>(jaugeVertexArray->createBuffer(0, 4*((1+1)*2+1)+6, vertexBufferMgr.get()));
    indexBufferMgr = std::make_unique<BufferMgr>(vkmgr, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, 2*6*(1024+10));
    indexBufferMgr->setName("Index buffer");
    jaugeIndexBuffer = indexBufferMgr->acquireBuffer(2*6*10);
    entityIndexBuffer = indexBufferMgr->acquireBuffer(2*6*1024);

    TTF_Init();
    submitResources();
    initCommands();
    VkFenceCreateInfo fenceInfo {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
    vkCreateFence(vkmgr.refDevice, &fenceInfo, nullptr, fences);
    vkCreateFence(vkmgr.refDevice, &fenceInfo, nullptr, fences + 1);
    vkCreateFence(vkmgr.refDevice, &fenceInfo, nullptr, fences + 2);
}

GPUDisplay::~GPUDisplay()
{
    stop();
    updater->join();
    TTF_CloseFont(myFont);
    TTF_Quit();
    SDL_FreeSurface(scoreboardSurface);
    vkDestroyFence(vkmgr.refDevice, fences[0], nullptr);
    vkDestroyFence(vkmgr.refDevice, fences[1], nullptr);
    vkDestroyFence(vkmgr.refDevice, fences[2], nullptr);
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
    shield.lock();
    active = false;
}

void GPUDisplay::unpause()
{
    active = true;
    shield.unlock();
}

void GPUDisplay::mainloop()
{
    auto delay = std::chrono::duration<int, std::ratio<1,1000000>>(1000000/150); // Limit to x3 base compute time
    shield.lock();
    auto clock = std::chrono::system_clock::now() + delay;
    while (alive) {
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

        std::string s = header1 + EntityLib::toText(score1);
        s.push_back(sep1);
        s += EntityLib::toText(score1Max);
        if (section2) {
            s.push_back(sep2);
            s += EntityLib::toText(score2Max);
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

        sinfo[imageIdx].pWaitSemaphores = semaphores + switcher;
        vkWaitForFences(vkmgr.refDevice, 1, fences + imageIdx, VK_TRUE, UINT32_MAX);
        vkResetFences(vkmgr.refDevice, 1, fences + imageIdx);
        vkQueueSubmit(graphicQueue, 1, sinfo + imageIdx, fences[imageIdx]);
        vkQueuePresentKHR(graphicQueue, presentInfo + imageIdx);
        if (active) {
            std::this_thread::sleep_until(clock);
            clock = std::chrono::system_clock::now() + delay;
        } else {
            shield.unlock();
            std::this_thread::yield();
            shield.lock();
        }
        switcher = (switcher + 1) % 3;
    }
    shield.unlock();
}

void GPUDisplay::submitResources()
{
    auto tmpMgr = std::make_unique<BufferMgr>(vkmgr, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0, 1024*1024);
    tmpMgr->setName("Temporary staging buffer");
    background = std::make_unique<Texture>(vkmgr, *tmpMgr, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "background.png");
    background->init();
    VkCommandBufferBeginInfo beginInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};
    vkBeginCommandBuffer(cmds[0], &beginInfo);
    entityMap.use(cmds[0], true);
    background->use(cmds[0], true);
    SubBuffer jaugeTmp = tmpMgr->acquireBuffer(sizeof(JaugeVertex)*(4*((1+1)*2+1)+6));
    JaugeVertex *jaugeTmpPtr = (JaugeVertex *) tmpMgr->getPtr(jaugeTmp);
    {
        int i = 0; // -0.025 x -0.0333333 y | 0.05 for 100% .00416666666666 for 4-pixel
        jaugeTmpPtr[i++] = JaugeVertex({JD, JB1});
        jaugeTmpPtr[i++] = JaugeVertex({JD, J1});
        jaugeTmpPtr[i++] = JaugeVertex({JD, J2});
        jaugeTmpPtr[i++] = JaugeVertex({JD, J3});
        jaugeTmpPtr[i++] = JaugeVertex({JD, J4});
        jaugeTmpPtr[i++] = JaugeVertex({JRZ, J1});
        jaugeTmpPtr[i++] = JaugeVertex({JRZ, J2});
        jaugeTmpPtr[i++] = JaugeVertex({JRZ, J3});
        jaugeTmpPtr[i++] = JaugeVertex({JRZ, J4});

        jaugeTmpPtr[i++] = JaugeVertex({JD, JB2});
        jaugeTmpPtr[i++] = JaugeVertex({JD, J1});
        jaugeTmpPtr[i++] = JaugeVertex({JD, J2});
        jaugeTmpPtr[i++] = JaugeVertex({JD, J3});
        jaugeTmpPtr[i++] = JaugeVertex({JD, J4});
        jaugeTmpPtr[i++] = JaugeVertex({JRZ, J1});
        jaugeTmpPtr[i++] = JaugeVertex({JRZ, J2});
        jaugeTmpPtr[i++] = JaugeVertex({JRZ, J3});
        jaugeTmpPtr[i++] = JaugeVertex({JRZ, J4});

        jaugeTmpPtr[i++] = JaugeVertex({0, .004166666666666666666666, JR, J1});
        jaugeTmpPtr[i++] = JaugeVertex({0, .004166666666666666666666, JR, J2});
        jaugeTmpPtr[i++] = JaugeVertex({0, .004166666666666666666666, JR, J3});
        jaugeTmpPtr[i++] = JaugeVertex({0, .004166666666666666666666, JR, J4});
        jaugeTmpPtr[i++] = JaugeVertex({0, .016666666666666666666666, JR, JB1});
        jaugeTmpPtr[i++] = JaugeVertex({0, .016666666666666666666666, JR, JB2});
        jaugeTmpPtr[i++] = JaugeVertex({0.05, 0, JR, JB1});
        jaugeTmpPtr[i++] = JaugeVertex({0.05, 0, JR, JB2});
    }
    BufferMgr::copy(cmds[0], jaugeTmp, jaugeVertexBuffer->get());
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
    SubBuffer jaugeIdxTmp = tmpMgr->acquireBuffer(2*6*10);
    unsigned short *jaugeIdxTmpPtr = (unsigned short *) tmpMgr->getPtr(jaugeIdxTmp);
    {
        // Jauges : posBG pos1 pos2 pos3 pos4 fill1 fill2 fill3 fill4 [same p2] down1 down2 down3 down4 downBG1 downBG2 fillBG1 fillBG2
        JPUT(0, 24, 22);
        JPUT(9, 25, 23);
        JPUT(1, 5, 18);
        JPUT(2, 6, 19);
        JPUT(3, 7, 20);
        JPUT(4, 8, 21);
        JPUT(10, 14, 18);
        JPUT(11, 15, 19);
        JPUT(12, 16, 20);
        JPUT(13, 17, 21);
    }
    BufferMgr::copy(cmds[0], jaugeIdxTmp, jaugeIndexBuffer);
    SubBuffer entityIdxTmp = tmpMgr->acquireBuffer(2*6*1024);
    unsigned short *entityIdxTmpPtr = (unsigned short *) tmpMgr->getPtr(entityIdxTmp);
    for (int i = 0; i < 1024*4; i += 4) {
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
    jaugeStaging = localBuffer.acquireBuffer(sizeof(JaugeVertex)*9*2);
    jaugePtr = (JaugeVertex *) localBuffer.getPtr(jaugeStaging);
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
    VkDeviceSize tmpOff;
    for (int i = 0; i < 3; ++i) {
        VkCommandBuffer cmd = cmds[i];
        vkBeginCommandBuffer(cmd, &beginInfo);
        tmpSync2.placeBarrier(cmd);
        BufferMgr::copy(cmd, jaugeStaging, jaugeVertexBuffer->get());
        tmpSync.placeBarrier(cmd);
        scoreboard->use(cmd);
        renderMgr.begin(i, cmd);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imagePipeline->get());
        tmpOff = imageVertexBuffer->get().offset;
        vkCmdBindVertexBuffers(cmd, 0, 1, &imageVertexBuffer->get().buffer, &tmpOff);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imagePLayout->getPipelineLayout(), 0, 1, bgSet->get(), 0, nullptr);
        vkCmdDraw(cmd, 6, 1, 0, 0);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imagePLayout->getPipelineLayout(), 0, 1, sbSet->get(), 0, nullptr);
        vkCmdDraw(cmd, 6, 1, 6, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, entityPipeline->get());
        tmpOff = entityVertexBuffer.offset;
        vkCmdBindVertexBuffers(cmd, 0, 1, &entityVertexBuffer.buffer, &tmpOff);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, imagePLayout->getPipelineLayout(), 0, 1, entitySet->get(), 0, nullptr);
        vkCmdBindIndexBuffer(cmd, entityIndexBuffer.buffer, entityIndexBuffer.offset, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd, 6*1024, 1, 0, 0, 0);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, jaugePipeline->get());
        tmpOff = jaugeVertexBuffer->get().offset;
        vkCmdBindVertexBuffers(cmd, 0, 1, &jaugeVertexBuffer->get().buffer, &tmpOff);
        vkCmdBindIndexBuffer(cmd, jaugeIndexBuffer.buffer, jaugeIndexBuffer.offset, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd, 6*10, 1, 0, 0, 0);
        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);
    }
}
