/*
** EPITECH PROJECT, 2020
** Vulkan-Engine
** File description:
** GPUEntityMgr.cpp
*/
#include "EntityCore/Core/VulkanMgr.hpp"
#include "EntityCore/Core/BufferMgr.hpp"
#include "EntityCore/Resource/SetMgr.hpp"
#include "EntityCore/Resource/Set.hpp"
#include "EntityCore/Resource/PipelineLayout.hpp"
#include "EntityCore/Resource/ComputePipeline.hpp"
#include "EntityCore/Resource/SyncEvent.hpp"
#include "EntityLib/Core/EntityLib.hpp"
#include "GPUEntityMgr.hpp"
#include <cstring>
#include <chrono>

GPUEntityMgr::GPUEntityMgr(std::shared_ptr<EntityLib> master) : vkmgr(*VulkanMgr::instance), localBuffer(master->getLocalBuffer()), master(master)
{
    entityMgr = std::make_unique<BufferMgr>(vkmgr, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, sizeof(EntityData) * END_ALL);
    vertexMgr = std::make_unique<BufferMgr>(vkmgr, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, sizeof(EntityVertexGroup) * END_ALL);
    readbackMgr = std::make_unique<BufferMgr>(vkmgr, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT, sizeof(EntityState) * END_ALL);
    entityPushBuffer = localBuffer.acquireBuffer(sizeof(EntityData) * 1024);
    readbackBuffer = readbackMgr->acquireBuffer(sizeof(EntityState) * 1024);
    gpuEntities = entityMgr->acquireBuffer(sizeof(EntityData) * 1024);
    gpuVertices = vertexMgr->acquireBuffer(sizeof(EntityVertexGroup) * 1024);
    entityPush = (EntityData *) localBuffer.getPtr(entityPushBuffer);
    readback = (EntityState *) readbackMgr->getPtr(readbackBuffer);
    if (vkmgr.getComputeQueues().size() > 0) {
        computeQueue = vkmgr.getComputeQueues()[0];
    } else if (vkmgr.getGraphicQueues().size() > 1) {
        vkmgr.putLog("No dedicated compute queue found, assume graphic queue support compute", LogType::WARNING);
        computeQueue = vkmgr.getGraphicQueues()[1];
    } else {
        vkmgr.putLog("No dedicated compute queue found, assume graphic queue support compute", LogType::WARNING);
        vkmgr.putLog("Only one graphic & compute queue is available", LogType::WARNING);
        vkmgr.putLog("Queue submission may conflict", LogType::WARNING);
        computeQueue = vkmgr.getGraphicQueues()[0];
    }

    syncExt = new SyncEvent[2] {{&vkmgr, false}, {&vkmgr, false}};
    syncInt = new SyncEvent[4] {{&vkmgr}, {&vkmgr}, {&vkmgr}, {&vkmgr}};
    syncExt[0].bufferBarrier(gpuEntities, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR);
    syncExt[1].bufferBarrier(gpuEntities, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR);
    syncInt[0].bufferBarrier(gpuEntities, VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR);
    syncInt[1].bufferBarrier(gpuEntities, VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR);
    syncInt[2].bufferBarrier(gpuEntities, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR);
    syncInt[3].bufferBarrier(gpuEntities, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR);
    syncInt[0].combineDstDependencies(syncInt[3]);
    syncInt[1].combineDstDependencies(syncInt[2]);

    VkCommandPoolCreateInfo poolInfo {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, (uint32_t) vkmgr.getComputeQueueFamilyIndex()[0]};
    vkCreateCommandPool(vkmgr.refDevice, &poolInfo, nullptr, &transferPool);
    poolInfo.flags = 0;
    vkCreateCommandPool(vkmgr.refDevice, &poolInfo, nullptr, &computePool);
    VkCommandBufferAllocateInfo allocInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, transferPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    vkAllocateCommandBuffers(vkmgr.refDevice, &allocInfo, cmds);
    vkAllocateCommandBuffers(vkmgr.refDevice, &allocInfo, cmds + 2);
    allocInfo.commandPool = computePool;
    vkAllocateCommandBuffers(vkmgr.refDevice, &allocInfo, cmds + 1);
    vkAllocateCommandBuffers(vkmgr.refDevice, &allocInfo, cmds + 3);
    buildCompute();
}

GPUEntityMgr::~GPUEntityMgr()
{
    stop();
    updater->join();
    delete[] syncExt;
    delete[] syncInt;
}

void GPUEntityMgr::buildCompute()
{
    setMgr = std::make_unique<SetMgr>(vkmgr, 2, 0, 0, 3);
    updatePLayout = std::make_unique<PipelineLayout>(vkmgr);
    collidePLayout = std::make_unique<PipelineLayout>(vkmgr);
    updatePLayout->setStorageBufferLocation(VK_SHADER_STAGE_COMPUTE_BIT, 0);
    updatePLayout->buildLayout();
    updatePLayout->setStorageBufferLocation(VK_SHADER_STAGE_COMPUTE_BIT, 0);
    updatePLayout->setStorageBufferLocation(VK_SHADER_STAGE_COMPUTE_BIT, 1);
    updatePLayout->buildLayout();
    updatePLayout->build();
    collidePLayout->setGlobalPipelineLayout(updatePLayout.get(), 0);
    collidePLayout->build();
    globalSet = std::make_unique<Set>(vkmgr, *setMgr, updatePLayout.get(), 0);
    updateSet = std::make_unique<Set>(vkmgr, *setMgr, updatePLayout.get(), 1);
    globalSet->bindStorageBuffer(gpuEntities, 0, sizeof(EntityData) * END_ALL);
    updateSet->bindStorageBuffer(gpuVertices, 0, sizeof(EntityVertexGroup) * END_ALL);
    updateSet->bindStorageBuffer(readbackBuffer, 1, sizeof(EntityState) * END_ALL);
    ComputePipeline::setShaderDir("./shader/");
    updatePipeline = std::make_unique<ComputePipeline>(vkmgr, updatePLayout.get());
    updatePipeline->bindShader("update.comp.spv");
    updatePipeline->build();
    collidePipeline = std::make_unique<ComputePipeline>(vkmgr, collidePLayout.get());
    collidePipeline->bindShader("collide.comp.spv");
    collidePipeline->build();
    pcollidePipeline = std::make_unique<ComputePipeline>(vkmgr, collidePLayout.get());
    pcollidePipeline->bindShader("pcollide.comp.spv");
    pcollidePipeline->build();
    VkDescriptorSet sets[2] {*globalSet->get(), *updateSet->get()};
    VkCommandBufferBeginInfo tmp {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};
    for (int i = 0; i < 2; ++i) {
        cmd = cmds[i * 2 | 1];
        vkBeginCommandBuffer(cmd, &tmp);
        syncInt[i].multiDstDependency(cmd);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updatePipeline->get());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, updatePLayout->getPipelineLayout(), 0, 2, sets, 0, nullptr);
        vkCmdDispatch(cmd, 1, 1, 1);
        syncInt[i].resetDependency(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR);
        syncInt[3 - i].resetDependency(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR);
        syncExt[!i].resetDependency(cmd, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR);
        syncExt[i].srcDependency(cmd);
        syncInt[2].placeBarrier(cmd);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, collidePipeline->get());
        vkCmdDispatchBase(cmd, 128, 0, 0, 128, 1, 1);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pcollidePipeline->get());
        vkCmdDispatch(cmd, 1, 1, 1);
        syncInt[i | 2].srcDependency(cmd);
        vkEndCommandBuffer(cmd);
    }
}

void GPUEntityMgr::resetAll()
{
    psidx = BEG_PLAYER_SHOOT;
    csidx = BEG_CANDY_SHOOT;
    bidx = BEG_BONUS;
    cidx = BEG_CANDY;
    for (int i = 0; i < END_ALL; ++i) {
        attachment[i].alive = false;
        entityPush[i].health = -1;
        entityPush[i].aliveNow = VK_TRUE;
        entityPush[i].newlyInserted = VK_FALSE;
    }
    // Record a transfer command which reset everything
    cmd = cmds[frameparity << 1];
    vkBeginCommandBuffer(cmd, &begInfo);
    VkBufferCopy tmp = {(VkDeviceSize) entityPushBuffer.offset, 0, END_ALL*sizeof(EntityData)};
    vkCmdCopyBuffer(cmd, entityPushBuffer.buffer, gpuEntities.buffer, 1, &tmp);
    syncInt[frameparity].srcDependency(cmd);
    if (initDep) {
        syncInt[3].srcDependency(cmd); // This could fail because the compute stage is unused
        initDep = false;
    }
    vkEndCommandBuffer(cmd);
    vkQueueSubmit(computeQueue, 1, sinfo + frameparity, VK_NULL_HANDLE);
    frameparity = !frameparity;
    regionsBase.clear();
}

void GPUEntityMgr::start(void (*update)(void *, GPUEntityMgr &), void (*updatePlayer)(void *, GPUEntityMgr &), void *data)
{
    if (!alive) {
        alive = true;
        updater = std::make_unique<std::thread>(&GPUEntityMgr::mainloop, this, update, updatePlayer, data);
    }
}

void GPUEntityMgr::mainloop(void (*update)(void *, GPUEntityMgr &), void (*updatePlayer)(void *, GPUEntityMgr &), void *data)
{
    resetAll();

    auto delay = std::chrono::duration<int, std::ratio<1,1000000>>(1000000/60);
    bool prevLimit = limit;
    auto clock = std::chrono::system_clock::now();

    while (alive) {
        while (!active) {
            std::this_thread::yield();
            clock = std::chrono::system_clock::now();
        }
        cmd = cmds[frameparity << 1];
        regions.resize(regionsBase.size());
        memcpy(regions.data(), regionsBase.data(), regionsBase.size() * sizeof(VkBufferCopy));
        lastPush = 0;
        vkBeginCommandBuffer(cmd, &begInfo);
        syncExt[!frameparity].dstDependency(cmd);

        update(data, *this); // Update game // Record transfer due to local event

        if (prevLimit != limit) {
            clock = std::chrono::system_clock::now();
            prevLimit = limit;
        }
        if (limit) {
            std::this_thread::sleep_until(clock);
            clock += delay;
        }
        while (!syncExt[frameparity].isSet())
            std::this_thread::yield();

        updateChanges(); // Read back changes
        updatePlayer(data, *this); // Update player shield // Record transfer due to GPU event // Write player changes
        vkCmdCopyBuffer(cmd, entityPushBuffer.buffer, gpuEntities.buffer, regions.size(), regions.data());
        syncInt[frameparity].srcDependency(cmd);
        vkEndCommandBuffer(cmd);
        vkQueueSubmit(computeQueue, 1, sinfo + frameparity, VK_NULL_HANDLE);
        frameparity = !frameparity;
    }
    pidx = BEG_PLAYER;
}

void GPUEntityMgr::updateChanges()
{
    readbackMgr->invalidate(readbackBuffer);
    nbDead = 0;
    for (int i = 0; i < END_ALL; ++i) {
        if (attachment[i].alive && readback[i].health < 0) {
            attachment[i].alive = false;
            deadFlags[nbDead].first = i;
            deadFlags[nbDead++].second = attachment[i].flag;
        }
    }
    psidx = BEG_PLAYER_SHOOT;
    csidx = BEG_CANDY_SHOOT;
    bidx = BEG_BONUS;
    cidx = BEG_CANDY;
}

EntityData &GPUEntityMgr::pushPlayerShoot(unsigned char flag)
{
    while (psidx < BEG_CANDY_SHOOT) {
        if (attachment[psidx].alive) {
            ++psidx;
            continue;
        }
        attachment[psidx].flag = flag;
        attachment[psidx].alive = true;
        pushRegion(psidx);
        return entityPush[psidx++];
    }
    vkmgr.putLog("Failed to insert player shoot", LogType::WARNING);
    return deft;
}

EntityData &GPUEntityMgr::pushCandyShoot(unsigned char flag)
{
    while (csidx < BEG_BONUS) {
        if (attachment[csidx].alive) {
            ++csidx;
            continue;
        }
        attachment[csidx].flag = flag;
        attachment[csidx].alive = true;
        pushRegion(csidx);
        return entityPush[csidx++];
    }
    vkmgr.putLog("Failed to insert candy shoot", LogType::WARNING);
    return deft;
}

EntityData &GPUEntityMgr::pushBonus(unsigned char flag)
{
    while (bidx < BEG_CANDY) {
        if (attachment[bidx].alive) {
            ++bidx;
            continue;
        }
        attachment[bidx].flag = flag;
        attachment[bidx].alive = true;
        pushRegion(bidx);
        return entityPush[bidx++];
    }
    vkmgr.putLog("Failed to insert bonus or special candy shoot", LogType::WARNING);
    return deft;
}

EntityData &GPUEntityMgr::pushCandy(unsigned char flag)
{
    while (cidx < END_ALL) {
        if (attachment[cidx].alive) {
            ++cidx;
            continue;
        }
        attachment[cidx].flag = flag;
        attachment[cidx].alive = true;
        pushRegion(cidx);
        return entityPush[cidx++];
    }
    vkmgr.putLog("Failed to insert candy", LogType::WARNING);
    return deft;
}

EntityData &GPUEntityMgr::pushPlayer(short idx)
{
    if (!(pidx & (1 << idx))) {
        pidx |= (1 << idx);
        regionsBase.push_back({entityPushBuffer.offset + sizeof(EntityData) * idx, sizeof(EntityData) * idx, 5 * sizeof(float)});
        regions.push_back({entityPushBuffer.offset + sizeof(EntityData) * idx, sizeof(EntityData) * idx, sizeof(EntityData)});
    }
    return entityPush[idx];
}

EntityState &GPUEntityMgr::readEntity(short idx)
{
    return readback[idx];
}

void GPUEntityMgr::pushRegion(short idx)
{
    if (lastPush + 1 == idx) {
        regions.back().size += sizeof(EntityData);
    } else {
        regions.push_back({entityPushBuffer.offset + idx * sizeof(EntityData), idx * sizeof(EntityData), sizeof(EntityData)});
    }
    lastPush = idx;
}
