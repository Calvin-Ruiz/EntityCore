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
#include <vulkan/vulkan.h>

class VulkanMgr;
class Texture;
class BufferMgr;
class RenderMgr;
class FrameMgr;

struct EntityData;
struct QueueFamily;

#define WIN_SIZE_SCALING 1.5
#define WINDOWLESS true

class EntityLib {
public:
    EntityLib(const char *AppName, uint32_t appVersion, int width = 600, int height = 600, bool enableDebugLayers = true, bool drawLogs = true, bool saveLogs = false);
    virtual ~EntityLib();
    EntityLib(const EntityLib &cpy) = delete;
    EntityLib &operator=(const EntityLib &src) = delete;

    void loadFragment(int idx, int texX1, int texY1, int texX2, int texY2, int shield, int damage, int width, int height, unsigned char flag = 0);
    EntityData &getFragment(int idx);
    EntityData &getFragment(int idx, int x, int y, int velX, int velY = 0);
    EntityData &dropFragment(int idx, float x, float y, float velX, float velY = 0);
    void setFragmentPos(EntityData &entity, int x, int y);

    BufferMgr &getLocalBuffer() {return *localBuffer;}
    Texture &getEntityMap() {return *entityMap;}
    RenderMgr &getRender() {return *renderMgr;}
    std::vector<std::unique_ptr<FrameMgr>> &getFrames() {return frames;}
    std::vector<std::unique_ptr<Texture>> frameDraw;
    std::vector<unsigned char> flags;
    const float worldScaleX;
    const float worldScaleY;

    static std::string toText(long nbr);
    VkQueue graphicQueue;
    const struct QueueFamily *graphicQueueFamily;
private:
    std::vector<EntityData> fragments;
    std::unique_ptr<BufferMgr> localBuffer;
    std::unique_ptr<Texture> entityMap;
    std::unique_ptr<RenderMgr> renderMgr;
    std::vector<std::unique_ptr<FrameMgr>> frames;
    SDL_Window *window;
    int mapWidth, mapHeight;
    float mapScaleX;
    float mapScaleY;
    std::unique_ptr<VulkanMgr> master;
};

#endif /* ENTITYLIB_HPP_ */
