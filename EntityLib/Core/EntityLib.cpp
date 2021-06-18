/*
** EPITECH PROJECT, 2020
** Vulkan-Engine
** File description:
** EntityLib.cpp
*/
#include "EntityLib.hpp"
#include "EntityCore/Core/VulkanMgr.hpp"

EntityLib::EntityLib(const char *AppName, uint32_t appVersion, int width = 600, int height = 600, bool enableDebugLayers = true, bool drawLogs = true, bool saveLogs = false)
{
    master = std::make_unique<VulkanMgr>(AppName, appVersion, window, width, height, 64, enableDebugLayers, drawLogs, saveLogs, "cache/");
}

EntityLib::~EntityLib()
{
}
