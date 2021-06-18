/*
** EPITECH PROJECT, 2020
** Vulkan-Engine
** File description:
** EntityLib.hpp
*/

#ifndef ENTITYLIB_HPP_
#define ENTITYLIB_HPP_

#include <string>
#include <memory>
#include <vector>
#include <SDL2/SDL.h>

class VulkanMgr;
class Texture;
class BufferMgr;
struct EntityData;

class EntityLib {
public:
    EntityLib(const char *AppName, uint32_t appVersion, int width = 600, int height = 600, bool enableDebugLayers = true, bool drawLogs = true, bool saveLogs = false);
    virtual ~EntityLib();
    EntityLib(const EntityLib &cpy) = delete;
    EntityLib &operator=(const EntityLib &src) = delete;

    BufferMgr &getLocalBuffer() const {return *localBuffer;}
    void loadFragment(int idx, int texX1, int texY1, int texX2, int texY2, int shield, int damage, int width, int height, unsigned char flag = 0);
    EntityData &getFragment(int idx, int x, int y, int velX, int velY);
private:
    std::vector<EntityData> fragments;
    std::unique_ptr<BufferMgr> localBuffer;
    std::unique_ptr<Texture> entityMap;
    std::unique_ptr<VulkanMgr> master;
    SDL_Window *window;
    int mapWidth, mapHeight;
    const float worldScaleX;
    const float worldScaleY;
    float mapScaleX;
    float mapScaleY;
};

#endif /* ENTITYLIB_HPP_ */
