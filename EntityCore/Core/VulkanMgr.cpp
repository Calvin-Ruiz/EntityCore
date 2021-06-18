#include "VulkanMgr.hpp"
#include "BufferMgr.hpp"
#include "MemoryManager.hpp"
#include "EntityCore/Resource/SyncEvent.hpp"
#include <SDL2/SDL_vulkan.h>
#include <iostream>
#include <set>
#include <algorithm>
#include <chrono>
#include <thread>
#include <sstream>

bool VulkanMgr::isAlive = false;
VulkanMgr *VulkanMgr::instance = nullptr;

VulkanMgr::VulkanMgr(const char *_AppName, uint32_t appVersion, SDL_Window *window, int width, int height, int chunkSize, bool enableDebugLayers, bool drawLogs, bool saveLogs, std::string _cachePath) :
    refDevice(device), drawLogs(drawLogs), saveLogs(saveLogs)
{
    assert(!isAlive); // There must be only one VulkanMgr instance
    instance = this;
    cachePath = _cachePath;
    isAlive = true;
    if (saveLogs) {
        logs.open("EntityCore-logs.txt", std::ofstream::out | std::ofstream::trunc);
        saveLogs = logs.is_open();
    }
    uint32_t sdl2ExtensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdl2ExtensionCount, nullptr))
        std::runtime_error("Fatal : Failed to found Vulkan extension for SDL2.");

    size_t initialSize = instanceExtension.size();
    instanceExtension.resize(initialSize + sdl2ExtensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window, &sdl2ExtensionCount, instanceExtension.data() + initialSize))
        std::runtime_error("Fatal : Failed to found Vulkan extension for SDL2.");

    initDevice(_AppName, appVersion, window, enableDebugLayers);
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
    displayPhysicalDeviceInfo(physicalDeviceProperties);
    initQueues(1);
    initSwapchain(width, height);
    createImageViews();
    memoryManager = new MemoryManager(*this, chunkSize*1024*1024);
    BufferMgr::setUniformOffsetAlignment(physicalDeviceProperties.limits.minUniformBufferOffsetAlignment);
    SyncEvent::setupPFN(vkinstance.get());

    // Déformation de l'image
    viewport.width = (float) swapChainExtent.width;
    viewport.height = swapChainExtent.height;
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Découpage de l'image
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;

    // Viewport
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineCacheCreateInfo cacheCreateInfo;
    cacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheCreateInfo.pNext = nullptr;
    cacheCreateInfo.flags = 0;
    cacheCreateInfo.initialDataSize = 0;
    cacheCreateInfo.pInitialData = nullptr;
    std::vector<char> cacheContent;
    if (!cachePath.empty()) {
        std::ifstream t(cachePath + "pipelineCache.dat", std::ifstream::binary);
        if (t) {
            t.seekg(0, t.end);
            cacheContent.resize(t.tellg());
            t.seekg(0, t.beg);
            t.read(cacheContent.data(), cacheContent.size());
            cacheCreateInfo.initialDataSize = cacheContent.size();
            cacheCreateInfo.pInitialData = cacheContent.data();
            t.close();
        }
    }
    if (vkCreatePipelineCache(device, &cacheCreateInfo, nullptr, &pipelineCache) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline cache.";
    }
}

VulkanMgr::~VulkanMgr()
{
    isAlive = false;
    vkDeviceWaitIdle(device);
    std::cout << "Release resources\n";
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(device, swapChain, nullptr);
    vkDestroySurfaceKHR(vkinstance.get(), surface, nullptr);
    if (!cachePath.empty()) {
        std::vector<char> cacheContent;
        std::vector<char> previousContent;
        size_t size;
        vkGetPipelineCacheData(device, pipelineCache, &size, nullptr);
        cacheContent.resize(size);
        vkGetPipelineCacheData(device, pipelineCache, &size, cacheContent.data());
        // Check if there is effective changes
        {
            std::ifstream t(cachePath + "pipelineCache.dat", std::ofstream::in | std::ofstream::binary);
            if (t) {
                t.seekg(0, t.end);
                previousContent.resize(t.tellg());
                t.seekg(0, t.beg);
                t.read(previousContent.data(), previousContent.size());
                t.close();
            }
        }
        if (cacheContent.size() == previousContent.size() && memcmp(cacheContent.data(), previousContent.data(), cacheContent.size()) == 0) {
            std::ofstream t(cachePath + "pipelineCache.dat", std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
            t.write(cacheContent.data(), size);
            t.close();
            putLog("Changes detected in the pipelineCache, store them", LogType::INFO);
        } else {
            putLog("No changes in the pipelineCache", LogType::INFO);
        }
    }
    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    delete memoryManager;
    vkDestroyDevice(device, nullptr);
    if (hasLayer)
        destroyDebug();
    instance = nullptr;
}

void VulkanMgr::waitIdle()
{
    vkDeviceWaitIdle(device);
}

SwapChainSupportDetails VulkanMgr::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

bool VulkanMgr::checkDeviceExtensionSupport(VkPhysicalDevice pDevice) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(pDevice, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(pDevice, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtension.begin(), deviceExtension.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}

bool VulkanMgr::isDeviceSuitable(VkPhysicalDevice pDevice) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pDevice, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0)
        return false;

    bool extensionsSupported = checkDeviceExtensionSupport(pDevice);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(pDevice);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return extensionsSupported && swapChainAdequate;
}

void VulkanMgr::initDevice(const char *AppName, uint32_t appVersion, SDL_Window *window, bool _hasLayer)
{
    hasLayer = _hasLayer;

    // initialize the vk::ApplicationInfo structure
    vk::ApplicationInfo applicationInfo( AppName, appVersion, "EntityCore", 1, VK_API_VERSION_1_2);

    // initialize the vk::InstanceCreateInfo
    vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo, hasLayer ? validationLayers.size() : 0, validationLayers.data(), instanceExtension.size(), instanceExtension.data());

    if (hasLayer)
        initDebug(&instanceCreateInfo);

    // create a UniqueInstance
    vkinstance = vk::createInstanceUnique(instanceCreateInfo);

    if (hasLayer)
        startDebug();

    initWindow(window);

    // get a physicalDevice
    for (const auto &pDevice : vkinstance->enumeratePhysicalDevices()) {
        if (isDeviceSuitable(pDevice)) {
            physicalDevice = pDevice;
            if (pDevice.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                break;
            }
        }
    }
}

void VulkanMgr::initQueues(uint32_t nbQueues)
{
    // get the QueueFamilyProperties of PhysicalDevice
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

    // {TO CHECK} register all index into queueFamiliyProperties which supports graphics, present, or both
    size_t i = 0;
    std::vector<float> queuePriority(nbQueues, 0.0f);
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pQueuePriorities = queuePriority.data();
    queueCreateInfo.pNext = nullptr;

    for (auto &qfp : queueFamilyProperties) {
        VkBool32 presentSupport = true;
        queueCreateInfo.queueCount = std::min(qfp.queueCount, nbQueues);
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (qfp.queueFlags & vk::QueueFlagBits::eGraphics) {
            if (presentSupport)
                graphicsAndPresentQueueFamilyIndex.push_back(i);
            else
                graphicsQueueFamilyIndex.push_back(i);
        } else if (qfp.queueFlags & vk::QueueFlagBits::eCompute)
            computeQueueFamilyIndex.push_back(i);
        else if (presentSupport)
            presentQueueFamilyIndex.push_back(i);
        else if (qfp.queueFlags & vk::QueueFlagBits::eTransfer)
            transferQueueFamilyIndex.push_back(i);
        else {
            i++;
            continue;
        }
        // Build createInfo for this familyQueue
        queueCreateInfo.queueFamilyIndex = i;
        queueCreateInfos.push_back(queueCreateInfo);
        i++;
    }
    assert(graphicsQueueFamilyIndex.size() + graphicsAndPresentQueueFamilyIndex.size() > 0);
    assert(presentQueueFamilyIndex.size() + graphicsAndPresentQueueFamilyIndex.size() > 0);
    //assert(transferQueueFamilyIndex.size() > 0);

    VkPhysicalDeviceFeatures supportedDeviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &supportedDeviceFeatures);

    deviceFeatures.geometryShader = supportedDeviceFeatures.geometryShader;
    deviceFeatures.tessellationShader = supportedDeviceFeatures.tessellationShader;
    deviceFeatures.samplerAnisotropy = supportedDeviceFeatures.samplerAnisotropy;
    deviceFeatures.sampleRateShading = supportedDeviceFeatures.sampleRateShading;
    deviceFeatures.multiDrawIndirect = supportedDeviceFeatures.multiDrawIndirect;
    deviceFeatures.wideLines = supportedDeviceFeatures.wideLines;
    deviceFeatures.shaderFloat64 = supportedDeviceFeatures.shaderFloat64;
    deviceFeatures.vertexPipelineStoresAndAtomics = supportedDeviceFeatures.vertexPipelineStoresAndAtomics;
    displayEnabledFeaturesInfo();

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = deviceExtension.size();
    createInfo.ppEnabledExtensionNames = deviceExtension.data();
    createInfo.enabledLayerCount = 0;

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }

    int maxQueues;
    VkQueue tmpQueue;
    for (auto index : graphicsAndPresentQueueFamilyIndex) {
        maxQueues = std::min(queueFamilyProperties[index].queueCount, nbQueues);
        putLog("Create " + std::to_string(maxQueues) + " graphic queues with present ability", LogType::INFO);
        for (int j = 0; j < maxQueues; j++) {
            vkGetDeviceQueue(device, index, j, &tmpQueue);
            graphicsAndPresentQueues.push_back(tmpQueue);
        }
    }
    for (auto index : graphicsQueueFamilyIndex) {
        maxQueues = std::min(queueFamilyProperties[index].queueCount, nbQueues);
        for (int j = 0; j < maxQueues; j++) {
            std::cout << "Create graphicQueue\n";
            vkGetDeviceQueue(device, index, j, &tmpQueue);
            graphicsQueues.push_back(tmpQueue);
        }
    }
    for (auto index : computeQueueFamilyIndex) {
        maxQueues = std::min(queueFamilyProperties[index].queueCount, nbQueues);
        putLog("Create " + std::to_string(maxQueues) + " compute queues", LogType::INFO);
        for (int j = 0; j < maxQueues; j++) {
            vkGetDeviceQueue(device, index, j, &tmpQueue);
            computeQueues.push_back(tmpQueue);
        }
    }
    for (auto index : presentQueueFamilyIndex) {
        maxQueues = std::min(queueFamilyProperties[index].queueCount, nbQueues);
        for (int j = 0; j < maxQueues; j++) {
            std::cout << "Create presentQueue\n";
            vkGetDeviceQueue(device, index, j, &tmpQueue);
            presentQueues.push_back(tmpQueue);
        }
    }
    return; // We don't want dedicated transfer queue for laser bonbon
    for (auto index : transferQueueFamilyIndex) {
        maxQueues = std::min(queueFamilyProperties[index].queueCount, nbQueues);
        putLog("Create " + std::to_string(maxQueues) + " transfer queues", LogType::INFO);
        for (int j = 0; j < maxQueues; j++) {
            vkGetDeviceQueue(device, index, j, &tmpQueue);
            transferQueues.push_back(tmpQueue);
        }
    }
}

void VulkanMgr::initWindow(SDL_Window *window)
{
    if (SDL_Vulkan_CreateSurface(window, vkinstance.get(), &surface) != SDL_TRUE) {
        throw std::runtime_error("Failed to create window surface");
    }
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM /*&& availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR*/) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
{
    return VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
}

static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height)
{
    // Real size is not always supported
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = {width, height};

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

void VulkanMgr::initSwapchain(int width, int height)
{
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    swapChainExtent = chooseSwapExtent(swapChainSupport.capabilities, width, height);
    swapChainImageFormat = surfaceFormat.format;

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapChainExtent;
    createInfo.imageArrayLayers = 1; // Pas besoin de 3D stéréoscopique
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    //createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT; // Transfert nécessaire pour l'affichage
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Pas de transparence pour le contenu de la fenêtre
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_FALSE; // ne pas calculer les pixels masqués par une autre fenêtre
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("échec de la création de la swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &finalImageCount, nullptr);
    swapChainImages.resize(finalImageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &finalImageCount, swapChainImages.data() + finalImageCount);
}

VkImageView VulkanMgr::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("échec de la creation de la vue sur une image!");
    }

    return imageView;
}

void VulkanMgr::createImageViews()
{
    swapChainImageViews.resize(swapChainImages.size());

    for (uint32_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat);
    }
}

bool VulkanMgr::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, SubMemory& bufferMemory, VkMemoryPropertyFlags preferedProperties)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    bufferMemory = memoryManager->malloc(memRequirements, properties, preferedProperties);

    if (bufferMemory.memory == VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
        return false;
    }
    if (vkBindBufferMemory(device, buffer, bufferMemory.memory, bufferMemory.offset) != VK_SUCCESS)
        std::cerr << "Faild to bind buffer memory.\n";
    return true;
}

void VulkanMgr::free(SubMemory& bufferMemory) {memoryManager->free(bufferMemory);}
void VulkanMgr::mapMemory(SubMemory& bufferMemory, void **data) {memoryManager->mapMemory(bufferMemory, data);}
void VulkanMgr::unmapMemory(SubMemory& bufferMemory) {memoryManager->unmapMemory(bufferMemory);}

void VulkanMgr::releaseUnusedMemory()
{
    putLog("Low GPU memory detected - release unused memory", LogType::WARNING);
}

// =============== DEBUG =============== //
static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void VulkanMgr::initDebug(vk::InstanceCreateInfo *instanceCreateInfo)
{
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugCreateInfo.pfnUserCallback = debugCallback;
    debugCreateInfo.pUserData = nullptr; // Optionnel

    instanceCreateInfo->setPNext(&debugCreateInfo);
}

void VulkanMgr::startDebug()
{
    std::vector<vk::LayerProperties>     layerProperties     = vk::enumerateInstanceLayerProperties();
    std::vector<vk::ExtensionProperties> extensionProperties = vk::enumerateInstanceExtensionProperties();

    if (CreateDebugUtilsMessengerEXT(vkinstance.get(), &debugCreateInfo, nullptr, &callback) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger");
    }
    ptr_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT) vkGetInstanceProcAddr(vkinstance.get(), "vkSetDebugUtilsObjectNameEXT");
}

void VulkanMgr::destroyDebug()
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(vkinstance.get(), "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(vkinstance.get(), callback, nullptr);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanMgr::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void * /*pUserData*/)
{
    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        && pCallbackData->pMessageIdName == std::string("Loader Message"))
        return VK_FALSE;
    std::ostringstream ss;
    ss << vk::to_string( static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>( messageSeverity ) ) << ": "
        << vk::to_string( static_cast<vk::DebugUtilsMessageTypeFlagsEXT>( messageTypes ) ) << ":\n";
    ss << "\t" << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
    ss << "\t" << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
    ss << "\t" << "message         = <" << pCallbackData->pMessage << ">\n";
    if (0 < pCallbackData->queueLabelCount) {
        ss << "\t" << "Queue Labels:\n";
        for (uint8_t i = 0; i < pCallbackData->queueLabelCount; i++) {
            ss << "\t\t" << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
        }
    }
    if (0 < pCallbackData->cmdBufLabelCount) {
        ss << "\t" << "CommandBuffer Labels:\n";
        for (uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++) {
            ss << "\t\t" << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
        }
    }
    if (0 < pCallbackData->objectCount) {
        ss << "\t" << "Objects:\n";
        for ( uint8_t i = 0; i < pCallbackData->objectCount; i++ )
        {
            ss << "\t\t" << "Object " << (int) i << "\n";
            ss << "\t\t\t" << "objectType   = "
            << vk::to_string( static_cast<vk::ObjectType>( pCallbackData->pObjects[i].objectType ) ) << "\n";
            ss << "\t\t\t" << "objectHandle = " << reinterpret_cast<void *>(pCallbackData->pObjects[i].objectHandle) << "\n"; // reinterpret_cast to have form 0x
            if (pCallbackData->pObjects[i].pObjectName) {
                std::string name = pCallbackData->pObjects[i].pObjectName;
                size_t posAddress = name.find(" at ");
                ss << "\t\t\t" << "objectName   = " << name.substr(0, posAddress) << "\n";
                // if (posAddress != std::string::npos)
                //     printDebug(ss, name.substr(posAddress + 4, std::string::npos));
            }
        }
    }
    instance->putLog(ss.str(), LogType::LAYER);
    return VK_FALSE;
}

// void VulkanMgr::printDebug(std::ostringstream &oss, std::string identifier)
// {
//     long address = std::stol(identifier.substr(1, std::string::npos));
//     switch (identifier[0]) {
//         default:;
//     }
// }

void VulkanMgr::setObjectName(void *handle, VkObjectType type, const std::string &name)
{
    if (hasLayer) {
        VkDebugUtilsObjectNameInfoEXT nameInfo {VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, nullptr, type, reinterpret_cast<uint64_t>(handle), name.c_str()};
        ptr_vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
    }
}

void VulkanMgr::displayPhysicalDeviceInfo(VkPhysicalDeviceProperties &prop)
{
    std::ostringstream ss;
    ss << "===== Device info =====\n";
    ss << "API Version : " << VK_VERSION_MAJOR(prop.apiVersion) << "." << VK_VERSION_MINOR(prop.apiVersion) << "." << VK_VERSION_PATCH(prop.apiVersion) << std::endl;
    ss << "Driver Version : " << VK_VERSION_MAJOR(prop.driverVersion) << "." << VK_VERSION_MINOR(prop.driverVersion) << "." << VK_VERSION_PATCH(prop.driverVersion) << std::endl;
    ss << "Vendor : ";
    switch (prop.vendorID) {
        case VK_VENDOR_ID_VIV: ss << "VIV"; break;
        case VK_VENDOR_ID_VSI: ss << "VSI"; break;
        case VK_VENDOR_ID_KAZAN: ss << "KAZAN"; break;
        // case VK_VENDOR_ID_CODEPLAY: ss << "CODEPLAY"; break;
        // case VK_VENDOR_ID_MESA: ss << "MESA"; break;
        case 0x1002: ss << "AMD"; break;
        case 0x1010: ss << "ImgTec"; break;
        case 0x10DE: ss << "NVIDIA"; break;
        case 0x13B5: ss << "ARM"; break;
        case 0x5143: ss << "Qualcomm"; break;
        case 0x8086: ss << "INTEL"; break;
        default: ss << "UNKNOWN (id = " << reinterpret_cast<void *>(prop.vendorID) << ")";
    }
    ss << std::endl;
    ss << "Device : " << prop.deviceName << " (id = " << prop.deviceID << ")" << std::endl;
    ss << "Device type : ";
    switch (prop.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER: ss << "Other"; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: ss << "Integrated GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: ss << "Discrete GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: ss << "VIRTUAL GPU"; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: ss << "CPU"; break;
        default: ss << "Unknown";
    }
    putLog(ss.str(), LogType::LAYER);
}

void VulkanMgr::displayEnabledFeaturesInfo()
{
    std::ostringstream ss;
    ss << "===== Used device features =====\n";
    ss << "geometryShader : " << (deviceFeatures.geometryShader == VK_TRUE ? "enabled" : "disabled") << std::endl;
    ss << "tessellationShader : " << (deviceFeatures.tessellationShader == VK_TRUE ? "enabled" : "disabled") << std::endl;
    ss << "samplerAnisotropy : " << (deviceFeatures.samplerAnisotropy == VK_TRUE ? "enabled" : "disabled") << std::endl;
    ss << "sampleRateShading : " << (deviceFeatures.sampleRateShading == VK_TRUE ? "enabled" : "disabled") << std::endl;
    ss << "multiDrawIndirect : " << (deviceFeatures.multiDrawIndirect == VK_TRUE ? "enabled" : "disabled") << std::endl;
    ss << "wideLines : " << (deviceFeatures.wideLines == VK_TRUE ? "enabled" : "disabled") << std::endl;
    ss << "shaderFloat64 : " << (deviceFeatures.shaderFloat64 == VK_TRUE ? "enabled" : "disabled");
    putLog(ss.str(), LogType::LAYER);
}

void VulkanMgr::putLog(const std::string &str, LogType type)
{
    std::string header;

    switch (type) {
        case LogType::INFO:
            header = "(INFO)\t";
            break;
        case LogType::DEBUG:
            header = "(DEBUG)\t";
            break;
        case LogType::LAYER:
            break;
        case LogType::WARNING:
            header = "(WARN)\t";
            break;
        case LogType::ERROR:
            header = "(ERROR)\t";
            break;
    }
    if (drawLogs)
        std::cerr << header << str << std::endl;
    if (saveLogs)
        logs << header << str << std::endl;
}

/*
void VulkanMgr::createMultisample(VkSampleCountFlagBits sampleCount)
{
    multisampleImage.resize(swapChainImages.size());
    multisampleImageMemory.resize(multisampleImage.size());
    multisampleImageView.resize(multisampleImage.size());
    for (uint32_t i = 0; i < multisampleImage.size(); ++i) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapChainExtent.width;
        imageInfo.extent.height = swapChainExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = swapChainImageFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.samples = sampleCount;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr, &multisampleImage[i]) != VK_SUCCESS) {
            throw std::runtime_error("echec de la creation d'une image");
        }
        setObjectName(multisampleImage[i], VK_OBJECT_TYPE_IMAGE, "Main FBO multisample color attachment");

        VkMemoryDedicatedRequirements memDedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, nullptr, 0, 0};
        VkMemoryRequirements2 memRequirements;
        memRequirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        memRequirements.pNext = &memDedicated;
        VkImageMemoryRequirementsInfo2 memImageInfo{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, nullptr, multisampleImage[i]};
        vkGetImageMemoryRequirements2(device, &memImageInfo, &memRequirements);
        multisampleImageMemory[i] = (memDedicated.prefersDedicatedAllocation) ?
            memoryManager->dmalloc(memRequirements.memoryRequirements, multisampleImage[i], VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT):
            memoryManager->malloc(memRequirements.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkBindImageMemory(device, multisampleImage[i], multisampleImageMemory[i].memory, multisampleImageMemory[i].offset);
        multisampleImageView[i] = createImageView(multisampleImage[i], swapChainImageFormat);
    }
}

void VulkanMgr::createVirtualSurfaces(VkSampleCountFlagBits sampleCount)
{
    bool isThreaded = swapChain.size() > 1; // Assume VirtualSurface were in separate threads
    for (uint32_t i = 0; i < swapChain.size(); i++) {
        virtualSurface.push_back(std::make_unique<VirtualSurface>(this, i, sampleCount, isThreaded));
    }
}

void VulkanMgr::createRenderPass(VkSampleCountFlagBits sampleCount)
{
    VkAttachmentDescription colorAttachment{};
    //colorAttachment.flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = sampleCount;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    //depthAttachment.flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
    depthAttachment.format = VK_FORMAT_D24_UNORM_S8_UINT;
    depthAttachment.samples = sampleCount;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependencies[4]{
        {VK_SUBPASS_EXTERNAL, 0,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT},
        {VK_SUBPASS_EXTERNAL, 0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT},
        {0, VK_SUBPASS_EXTERNAL,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT},
        {0, VK_SUBPASS_EXTERNAL,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, VK_DEPENDENCY_BY_REGION_BIT}
    };
    std::vector<VkAttachmentDescription> attachments{colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies + 1;

    renderPass.resize(static_cast<uint8_t>(renderPassType::SINGLE_SAMPLE_PRESENT) + 1);
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::CLEAR]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::DEFAULT]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::CLEAR_DEPTH_BUFFER_DONT_SAVE]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    renderPassInfo.dependencyCount = 3;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::CLEAR_DEPTH_BUFFER]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    renderPassInfo.dependencyCount = 4;
    renderPassInfo.pDependencies = dependencies;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::USE_DEPTH_BUFFER]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    renderPassInfo.dependencyCount = 3;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::USE_DEPTH_BUFFER_DONT_SAVE]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies + 1;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::PRESENT]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::DRAW_USE]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (sampleCount == VK_SAMPLE_COUNT_1_BIT) {
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::SINGLE_PASS]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create RenderPass");
        }
    }
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::SINGLE_PASS_DRAW_USE]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::DEPTH_BUFFER_SINGLE_PASS_DRAW_USE]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::SINGLE_PASS_DRAW_USE_ADDITIVE]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::DEPTH_BUFFER_SINGLE_PASS_DRAW_USE_ADDITIVE]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create RenderPass");
    }
    if (sampleCount != VK_SAMPLE_COUNT_1_BIT) {
        // There is more than 1 sample, this call build renderPass
        // Reset state of multisample attachments
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[0].initialLayout = attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].storeOp = attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkAttachmentDescription singleSampleColorAttachment{};
        //singleSampleColorAttachment.flags = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT;
        singleSampleColorAttachment.format = swapChainImageFormat;
        singleSampleColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        singleSampleColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        singleSampleColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        singleSampleColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        singleSampleColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        singleSampleColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        singleSampleColorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference singleSampleColorAttachmentRef{};
        singleSampleColorAttachmentRef.attachment = 2;
        singleSampleColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // RESOLVE
        attachments.push_back(singleSampleColorAttachment);
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        subpass.pResolveAttachments = &singleSampleColorAttachmentRef;
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::RESOLVE_DEFAULT]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create RenderPass");
        }
        attachments[2].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::RESOLVE_PRESENT]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create RenderPass");
        }
        // SINGLE_SAMPLE
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &singleSampleColorAttachment;
        subpass.pDepthStencilAttachment = subpass.pResolveAttachments = nullptr;
        singleSampleColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::SINGLE_SAMPLE_CLEAR]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create RenderPass");
        }
        singleSampleColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        singleSampleColorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::SINGLE_SAMPLE_DEFAULT]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create RenderPass");
        }
        singleSampleColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::SINGLE_SAMPLE_PRESENT]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create RenderPass");
        }
        // SINGLE_PASS
        singleSampleColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        singleSampleColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        singleSampleColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass[(uint8_t)renderPassType::SINGLE_PASS]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create RenderPass");
        }
        // build renderPassExternal
        renderPass.swap(renderPassExternal);
        createRenderPass(VK_SAMPLE_COUNT_1_BIT);
        renderPass.swap(renderPassExternal);
    } else {
        // There is 1 sample for the renderPass builded by this call
        renderPass[(uint8_t)renderPassType::SINGLE_SAMPLE_CLEAR] = renderPass[(uint8_t)renderPassType::CLEAR];
        renderPass[(uint8_t)renderPassType::RESOLVE_DEFAULT] = renderPass[(uint8_t)renderPassType::SINGLE_SAMPLE_DEFAULT] = renderPass[(uint8_t)renderPassType::DEFAULT];
        renderPass[(uint8_t)renderPassType::RESOLVE_PRESENT] = renderPass[(uint8_t)renderPassType::SINGLE_SAMPLE_PRESENT] = renderPass[(uint8_t)renderPassType::PRESENT];
        if (renderPassExternal.empty()) {
            // This call has build renderPass, and multisampling is disabled
            renderPassExternal = renderPass;
        } else {
            // This call has build renderPassExternal
            return;
        }
    }
    // For calls which build renderPass on this call :
    renderPassCompatibility.push_back(renderPass[(uint8_t)renderPassType::DEFAULT]);
    renderPassCompatibility.push_back(renderPass[(uint8_t)renderPassType::RESOLVE_DEFAULT]);
    renderPassCompatibility.push_back(renderPass[(uint8_t)renderPassType::SINGLE_SAMPLE_DEFAULT]);
}

void VulkanMgr::createFramebuffer(VkSampleCountFlagBits sampleCount)
{
    swapChainFramebuffers.resize(swapChainImageViews.size());
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {
            ((sampleCount == VK_SAMPLE_COUNT_1_BIT) ? swapChainImageViews[i] : multisampleImageView[i]),
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPassCompatibility.front();
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("échec de la création d'un framebuffer!");
        }
    }
    if (sampleCount == VK_SAMPLE_COUNT_1_BIT) {
        resolveFramebuffers = swapChainFramebuffers;
        singleSampleFramebuffers = swapChainFramebuffers;
        return;
    }
    resolveFramebuffers.resize(swapChainFramebuffers.size());
    singleSampleFramebuffers.resize(swapChainFramebuffers.size());
    for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
        VkImageView attachments[] = {
            multisampleImageView[i],
            depthImageView,
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPassCompatibility[static_cast<uint8_t>(renderPassCompatibility::RESOLVE)];
        framebufferInfo.attachmentCount = 3;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &resolveFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("échec de la création d'un framebuffer!");
        }
        framebufferInfo.renderPass = renderPassCompatibility[static_cast<uint8_t>(renderPassCompatibility::SINGLE_SAMPLE)];
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &swapChainImageViews[i];
        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &singleSampleFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("échec de la création d'un framebuffer!");
        }
    }
}

void VulkanMgr::createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = 0; // Optionel
    poolInfo.queueFamilyIndex = graphicsAndPresentQueueFamilyIndex[0];
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("échec de la création d'une command pool !");
    }
}

void VulkanMgr::sendFrame()
{
    graphicQueueMutex.lock();
    graphicQueue.push(reinterpret_cast<CommandMgr *>(this));
    graphicQueueMutex.unlock();
    memoryManager->endOfFrame();
}

void VulkanMgr::submitFrame()
{
    for (uint32_t i = 0; i < virtualSurface.size(); i++) {
        virtualSurface[i]->getNextFrame();
    }
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    if (interceptNextFrame) {
        submitTransfer(&interceptSubmitInfo[frameIndex]);
        waitTransferQueueIdle(false);
        presentInfo.pWaitSemaphores = &interceptEndSemaphores[frameIndex];
    } else {
        presentInfo.pWaitSemaphores = &presentSemaphores[frameIndex];
    }
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChain.data();
    presentInfo.pImageIndices = &frameIndex;
    presentInfo.pResults = nullptr; // Optionnel
    if (vkQueuePresentKHR(graphicsAndPresentQueues[0], &presentInfo) != VK_SUCCESS) {
        cLog::get()->write("Presentation failed", LOG_TYPE::L_ERROR, LOG_FILE::VULKAN);
    }
    int oldFrameIndex = frameIndex;
    acquireNextFrame();
    if (interceptNextFrame)
        waitTransferQueueIdle(true);
    for (uint32_t i = 0; i < virtualSurface.size(); i++) {
        virtualSurface[i]->releaseFrame();
    }
    if (interceptNextFrame) {
        vkInvalidateMappedMemoryRanges(device, 1, &interceptMemoryRange[oldFrameIndex]);
        interceptor(pUserData, pInterceptBufferData[oldFrameIndex], viewport.width, abs(viewport.height));
        interceptNextFrame = false;
    }
}

void VulkanMgr::acquireNextFrame()
{
    VkResult result = vkAcquireNextImageKHR(device, swapChain[0], UINT64_MAX, imageAvailableSemaphores[switcher], VK_NULL_HANDLE, &frameIndex);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("Failed to acquire next frame.");
    virtualSurface[0]->setTopSemaphore(frameIndex, imageAvailableSemaphores[switcher]);
    switcher = (switcher + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanMgr::createDepthResources(VkSampleCountFlagBits sampleCount)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = swapChainExtent.width;
    imageInfo.extent.height = swapChainExtent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_D24_UNORM_S8_UINT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = sampleCount;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &depthImage) != VK_SUCCESS) {
        throw std::runtime_error("echec de la creation d'une image");
    }
    setObjectName(depthImage, VK_OBJECT_TYPE_IMAGE, "Main FBO depth attachment");

    VkMemoryDedicatedRequirements memDedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, nullptr, 0, 0};
    VkMemoryRequirements2 memRequirements;
    memRequirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    memRequirements.pNext = &memDedicated;
    VkImageMemoryRequirementsInfo2 memImageInfo{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2, nullptr, depthImage};
    vkGetImageMemoryRequirements2(device, &memImageInfo, &memRequirements);
    depthImageMemory = (memDedicated.prefersDedicatedAllocation) ?
        memoryManager->dmalloc(memRequirements.memoryRequirements, depthImage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT):
        memoryManager->malloc(memRequirements.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkBindImageMemory(device, depthImage, depthImageMemory.memory, depthImageMemory.offset);
    depthImageView = createImageView(depthImage, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
}

void VulkanMgr::setupInterceptor(void *_pUserData, void(*_interceptor)(void *pUserData, void *pData, uint32_t width, uint32_t height))
{
    pUserData=_pUserData;
    interceptor=_interceptor;
    createBuffer(viewport.width * abs(viewport.height) * 4 * swapChainImages.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, interceptBuffer, interceptBufferMemory);
    setObjectName(interceptBuffer, VK_OBJECT_TYPE_BUFFER, "Frame interceptor buffer");

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = 0; // Optionel
    poolInfo.queueFamilyIndex = transferQueueFamilyIndex[0];
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &interceptPool) != VK_SUCCESS) {
        throw std::runtime_error("échec de la création d'une command pool !");
    }

    interceptCmdBuffer.resize(swapChainImages.size());
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = interceptCmdBuffer.size();
    allocInfo.commandPool = interceptPool;
    if (vkAllocateCommandBuffers(device, &allocInfo, interceptCmdBuffer.data()) != VK_SUCCESS) {
        throw std::runtime_error("échec de l'allocation de command buffers!");
    }

    void *data;
    memoryManager->mapMemory(interceptBufferMemory, &data);

    interceptEndSemaphores.resize(interceptCmdBuffer.size());
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkMappedMemoryRange range;
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.pNext = nullptr;
    range.memory = interceptBufferMemory.memory;
    range.size = viewport.width * abs(viewport.height) * 4;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkImageMemoryBarrier barrierIn{};
    barrierIn.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierIn.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrierIn.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrierIn.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierIn.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierIn.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrierIn.subresourceRange.baseMipLevel = 0;
    barrierIn.subresourceRange.levelCount = 1;
    barrierIn.subresourceRange.baseArrayLayer = 0;
    barrierIn.subresourceRange.layerCount = 1;
    barrierIn.srcAccessMask = 0;
    barrierIn.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    VkImageMemoryBarrier barrierOut = barrierIn;
    barrierOut.oldLayout = barrierIn.newLayout;
    barrierOut.newLayout = barrierIn.oldLayout;
    barrierOut.srcAccessMask = barrierIn.dstAccessMask;
    barrierOut.dstAccessMask = barrierIn.srcAccessMask;

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {(int32_t) viewport.x, (int32_t) (viewport.height < 0 ? viewport.y + viewport.height : viewport.y), 0};
    region.imageExtent = {(uint32_t) viewport.width, (uint32_t) abs(viewport.height), 1};

    for (uint8_t i = 0; i < interceptCmdBuffer.size(); ++i) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &interceptEndSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphore for previously allocated command buffers.");
        }
        VkSubmitInfo submitInfo;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &presentSemaphores[i];
        submitInfo.pWaitDstStageMask = &interceptStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &interceptCmdBuffer[i];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &interceptEndSemaphores[i];
        interceptSubmitInfo.push_back(submitInfo);
        region.bufferOffset = range.offset = range.size * i;
        interceptMemoryRange.push_back(range);
        pInterceptBufferData.push_back(static_cast<char *>(data) + range.offset);
        vkBeginCommandBuffer(interceptCmdBuffer[i], &beginInfo);
        barrierOut.image = barrierIn.image = swapChainImages[i];
        vkCmdPipelineBarrier(interceptCmdBuffer[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierIn);
        vkCmdCopyImageToBuffer(interceptCmdBuffer[i], barrierIn.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, interceptBuffer, 1, &region);
        vkCmdPipelineBarrier(interceptCmdBuffer[i], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierOut);
        vkEndCommandBuffer(interceptCmdBuffer[i]);
    }
}
*/
