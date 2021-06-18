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

class VulkanMgr;

class EntityLib {
public:
    EntityLib(const char *AppName, uint32_t appVersion, int width = 600, int height = 600, bool enableDebugLayers = true, bool drawLogs = true, bool saveLogs = false);
    virtual ~EntityLib();
    EntityLib(const EntityLib &cpy) = delete;
    EntityLib &operator=(const EntityLib &src) = delete;
private:
    std::unique_ptr<VulkanMgr> master;
};

#endif /* ENTITYLIB_HPP_ */
