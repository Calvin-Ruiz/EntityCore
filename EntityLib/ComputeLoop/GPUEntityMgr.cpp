/*
** EPITECH PROJECT, 2020
** Vulkan-Engine
** File description:
** GPUEntityMgr.cpp
*/
#include "EntityCore/Core/VulkanMgr.hpp"
#include "EntityCore/Core/BufferMgr.hpp"
#include "EntityCore/Resources/BufferMgr.hpp"
#include "GPUEntityMgr.hpp"
#include <cstring>
#include <chrono>

GPUEntityMgr::GPUEntityMgr(std::shared_ptr<EntityLib> master, BufferMgr &localBuffer) : vkmgr(*VulkanMgr::instance), localBuffer(localBuffer), barrier(VK_DEPENDENCY_BY_REGION_BIT), master(master)
{
    entityMgr = std::make_unique<BufferMgr>(master, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, sizeof(EntityData) * 1024);
    vertexMgr = std::make_unique<BufferMgr>(master, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, sizeof(EntityVertexGroup) * 1024);
    readbackMgr = std::make_unique<BufferMgr>(master, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_CACHED_BIT, sizeof(EntityState) * 1024);
    entityPushBuffer = localBuffer.acquireBuffer(sizeof(EntityData) * 1024);
    readbackBuffer = readbackMgr->acquireBuffer(sizeof(EntityState) * 1024);
    gpuEntities = entityMgr->acquireBuffer(sizeof(EntityData) * 1024);
    gpuVertices = vertexMgr->acquireBuffer(sizeof(EntityVertexGroup) * 1024);
    entityPush = localBuffer.getPtr(entityPushBuffer);
    readback = readbackMgr->getPtr(readbackBuffer);
    computeQueue = vkmgr.getComputeQueues[0];

    syncExt = new SyncEvent[2] {{&vkmgr, false}, {&vkmgr, false}};
    syncInt = new SyncEvent[4] {{&vkmgr}, {&vkmgr}, {&vkmgr}, {&vkmgr}};
    syncExt[0].bufferBarrier(gpuEntities, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR);
    syncExt[1].bufferBarrier(gpuEntities, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR);
    syncInt[0].bufferBarrier(gpuEntities, VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR);
    syncInt[1].bufferBarrier(gpuEntities, VK_PIPELINE_STAGE_2_COPY_BIT_KHR, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR);
    syncInt[2].bufferBarrier(gpuEntities, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR);
    syncInt[3].bufferBarrier(gpuEntities, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT_KHR, VK_ACCESS_2_SHADER_STORAGE_READ_BIT_KHR);

    VkCommandPoolCreateInfo poolInfo {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, (uint32_t) master.getComputeQueueFamilyIndex()[0]};
    vkCreateCommandPool(vkmgr.refDevice, &poolInfo, nullptr, &transferPool);
    poolInfo.flags = 0;
    vkCreateCommandPool(vkmgr.refDevice, &poolInfo, nullptr, &computePool);
    VkCommandBufferAllocateInfo allocInfo {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, transferPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    vkAllocateCommandBuffers(master.refDevice, &allocInfo, &cmds);
    vkAllocateCommandBuffers(master.refDevice, &allocInfo, &cmds + 2);
    allocInfo.commandPool = computePool;
    vkAllocateCommandBuffers(master.refDevice, &allocInfo, &cmds + 1);
    vkAllocateCommandBuffers(master.refDevice, &allocInfo, &cmds + 3);
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
    //
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
    VkBufferCopy tmp = {0, 0, 1024*sizeof(EntityData)};
    vkCmdCopyBuffer(cmd, entityPushBuffer.buffer, gpuEntities.buffer, 1, &tmp);
    syncInt[frameparity].srcDependency(cmd);
    vkEndCommandBuffer(cmd);
    vkQueueSubmit(computeQueue, 1, sinfo + frameparity, VK_NULL_HANDLE);
    frameparity = !frameparity;
    regionsBase.clear();
}

void GPUEntityMgr::start(void (*update)(void *, GPUEntityMgr &), void (*updatePlayer)(void *, GPUEntityMgr &), void *data)
{
    if (!alive) {
        alive = true;
        updater = std::make_unique<std::thread>(&GPUEntityMgr::mainloop, this, update, data);
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

EntityData &GPUEntityMgr::pushPlayerShoot(unsigned char flag = 0)
{
    while (psidx < BEG_CANDY_SHOOT) {
        if (attachment[psidx].alive) {
            ++psidx;
            continue;
        }
        attachment[psidx].flag = flag
        attachment[psidx].alive = true;
        return entityPush[psidx++];
    }
    vkmgr.putLog("Failed to insert player shoot", LogType::WARNING);
    return default;
}

EntityData &GPUEntityMgr::pushCandyShoot(unsigned char flag = 0)
{
    while (csidx < BEG_BONUS) {
        if (attachment[csidx].alive) {
            ++csidx;
            continue;
        }
        attachment[csidx].flag = flag
        attachment[csidx].alive = true;
        return entityPush[csidx++];
    }
    vkmgr.putLog("Failed to insert candy shoot", LogType::WARNING);
    return default;
}

EntityData &GPUEntityMgr::pushBonus(unsigned char flag = 0)
{
    while (bidx < BEG_CANDY) {
        if (attachment[bidx].alive) {
            ++bidx;
            continue;
        }
        attachment[bidx].flag = flag
        attachment[bidx].alive = true;
        return entityPush[bidx++];
    }
    vkmgr.putLog("Failed to insert bonus or special candy shoot", LogType::WARNING);
    return default;
}

EntityData &GPUEntityMgr::pushCandy(unsigned char flag = 0)
{
    while (cidx < END_ALL) {
        if (attachment[cidx].alive) {
            ++cidx;
            continue;
        }
        attachment[cidx].flag = flag
        attachment[cidx].alive = true;
        return entityPush[cidx++];
    }
    vkmgr.putLog("Failed to insert candy", LogType::WARNING);
    return default;
}

EntityData &GPUEntityMgr::pushPlayer(int idx)
{
    if (!(pidx & (1 << idx))) {
        pidx |= (1 << idx);
        regionsBase.push_back({sizeof(EntityData) * idx, sizeof(EntityData) * idx, 5 * sizeof(VkFloat32)});
        regions.push_back({sizeof(EntityData) * idx, sizeof(EntityData) * idx, sizeof(EntityData)});
    }
    return entityPush[idx];
}

EntityState &GPUEntityMgr::readEntity(int idx)
{
    return readback[idx];
}
