/*
** EPITECH PROJECT, 2020
** Vulkan-Engine
** File description:
** EntityLib.cpp
*/
#include "EntityLib.hpp"
#include "EntityLib/ComputeLoop/GPUEntityMgr.hpp"
#include "EntityCore/Core/VulkanMgr.hpp"
#include "EntityCore/Core/BufferMgr.hpp"
#include "EntityCore/Resource/Texture.hpp"

EntityLib::EntityLib(const char *AppName, uint32_t appVersion, int width, int height, bool enableDebugLayers, bool drawLogs, bool saveLogs) : worldScaleX(2.f / width), worldScaleY(2.f / height)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);
    window = SDL_CreateWindow(AppName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_VULKAN|SDL_WINDOW_SHOWN);
    master = std::make_unique<VulkanMgr>(AppName, appVersion, window, width, height, 64, enableDebugLayers, drawLogs, saveLogs, "cache/");
    localBuffer = std::make_unique<BufferMgr>(*master, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0, 256*1024);
    Texture::setTextureDir("./Textures/");
    entityMap = std::make_unique<Texture>(*master, *localBuffer, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, "entityMap.png");
    entityMap->init();
    entityMap->getDimensions(mapWidth, mapHeight);
    mapScaleX = 1.f / mapWidth;
    mapScaleY = 1.f / mapHeight;
}

EntityLib::~EntityLib()
{
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void EntityLib::loadFragment(int idx, int texX1, int texY1, int texX2, int texY2, int shield, int damage, int width, int height, unsigned char flag)
{
    if (idx >= fragments.size()) {
        fragments.resize(idx + 1);
    }
    auto &tmp = fragments[idx];
    tmp.health = shield;
    tmp.damage = -damage;
    tmp.sizeX = width * worldScaleX / 2;
    tmp.sizeY = height * worldScaleY / 2;
    tmp.texX1 = texX1 * mapScaleX + 0.0001f;
    tmp.texY1 = texY1 * mapScaleY + 0.0001f;
    tmp.texX2 = texX2 * mapScaleX - 0.0002f;
    tmp.texY2 = texY2 * mapScaleY - 0.0002f;
    tmp.aliveNow = VK_FALSE;
    tmp.newlyInserted = VK_TRUE;
}

EntityData &EntityLib::getFragment(int idx, int x, int y, int velX, int velY)
{
    auto &tmp = fragments[idx];

    tmp.posX = x * worldScaleX - 1;
    tmp.posY = y * worldScaleY - 1;
    tmp.velX = velX * worldScaleX;
    tmp.velY = velY * worldScaleY;
    return tmp;
}
