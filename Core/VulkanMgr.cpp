#include "VulkanMgr.hpp"
#include "BufferMgr.hpp"
#include "MemoryManager.hpp"
#include "EntityCore/Resource/SyncEvent.hpp"
#include "EntityCore/Resource/Set.hpp"
#include "EntityCore/Resource/Texture.hpp"
#include <SDL2/SDL_vulkan.h>
#include <iostream>
#include <set>
#include <algorithm>
#include <chrono>
#include <thread>
#include <sstream>
#include <cstdlib>
#include <filesystem>

#ifdef WIN32
// Note : comment the line below if you use a unix-like command prompt like sh
#define NO_DISPLAY_COLOR
#endif

VulkanMgr *VulkanMgr::instance = nullptr;

VulkanMgr::VulkanMgr(const char *AppName, uint32_t appVersion, SDL_Window *window, int width, int height, const QueueRequirement &queueRequest, const VkPhysicalDeviceFeatures &requiredFeatures, const VkPhysicalDeviceFeatures &preferedFeatures, int chunkSize, bool enableDebugLayers, bool drawLogs, bool saveLogs, std::string cachePath, int forceSwapchainCount, VkImageUsageFlags swapchainUsage, bool usePushSet, logger_t redirectLog) :
    VulkanMgr({.AppName=AppName, .appVersion=appVersion, .window=window, .width=width, .height=height,
        .queueRequest=queueRequest, .requiredFeatures={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, nullptr, requiredFeatures}, .preferedFeatures={VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, nullptr, preferedFeatures},
        .requiredExtensions=(usePushSet ? std::vector<const char *>{"VK_KHR_push_descriptor"} : std::vector<const char*>{}),
        .redirectLog=redirectLog, .cachePath=cachePath, .logPath=cachePath, .swapchainUsage = swapchainUsage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, .chunkSize=chunkSize, .forceSwapchainCount=forceSwapchainCount,
        .enableDebugLayers=enableDebugLayers, .drawLogs=drawLogs, .saveLogs=saveLogs})
{
}

VulkanMgr::VulkanMgr(const VulkanMgrCreateInfo &createInfo) :
    refDevice(device), drawLogs(createInfo.drawLogs), saveLogs(createInfo.saveLogs), redirectLog(createInfo.redirectLog),
    customReleaseMemory(createInfo.customReleaseMemory), forceSwapchainCount(createInfo.forceSwapchainCount), presenting(createInfo.window != nullptr),
    minLogPrintLevel(createInfo.minLogPrintLevel), minLogWriteLevel(createInfo.minLogWriteLevel)
{
    assert(!instance); // There must be only one VulkanMgr instance
    instance = this;
    cachePath = createInfo.cachePath;
    std::error_code ec;
    if (!std::filesystem::exists(cachePath, ec) && ec)
        std::filesystem::create_directories(cachePath);
    if (!std::filesystem::exists(createInfo.logPath, ec) && ec)
        std::filesystem::create_directories(createInfo.logPath);
    auto swapchainUsage = createInfo.swapchainUsage;
    if (!createInfo.requiredExtensions.empty())
        deviceExtension.insert(deviceExtension.end(), createInfo.requiredExtensions.begin(), createInfo.requiredExtensions.end());
    if (saveLogs) {
        logs.open(createInfo.logPath + "EntityCore-logs.txt", std::ofstream::out | std::ofstream::trunc);
        saveLogs = logs.is_open();
    }
    if (presenting) {
        uint32_t sdl2ExtensionCount = 0;
        if (!SDL_Vulkan_GetInstanceExtensions(createInfo.window, &sdl2ExtensionCount, nullptr))
            std::runtime_error("Fatal : Failed to find Vulkan extension for SDL2.");

        size_t initialSize = instanceExtension.size();
        instanceExtension.resize(initialSize + sdl2ExtensionCount);
        if (!SDL_Vulkan_GetInstanceExtensions(createInfo.window, &sdl2ExtensionCount, instanceExtension.data() + initialSize))
            std::runtime_error("Fatal : Failed to find Vulkan extension for SDL2.");
    } else {
        swapchainUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    initVulkan(createInfo.AppName, createInfo.appVersion, createInfo.vulkanVersion, createInfo.window, createInfo.enableDebugLayers, createInfo.preferIntegrated);
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
    displayPhysicalDeviceInfo(physicalDeviceProperties);
    initQueues(createInfo.queueRequest);
    initDevice(createInfo);
    if (presenting) {
        initSwapchain(createInfo.width, abs(createInfo.height), swapchainUsage, createInfo.preferedPresentMode, !createInfo.colorSpaceSRGB);
        if (swapchainUsage & ALL_IMAGE_VIEW_USAGE)
            createImageViews();
    } else {
        swapchainExtent.width = createInfo.width;
        swapchainExtent.height = abs(createInfo.height);
    }
    memoryManager = new MemoryManager(*this, (createInfo.chunkSize < 256*1024) ? createInfo.chunkSize*1024*1024 : createInfo.chunkSize, createInfo.memoryBatchCount);
    BufferMgr::setUniformOffsetAlignment(physicalDeviceProperties.limits.minUniformBufferOffsetAlignment);
    SyncEvent::setupPFN(vkinstance.get());
    Set::setupPFN(vkinstance.get());

    // Déformation de l'image
    viewport.width = createInfo.width;
    viewport.height = createInfo.height;
    viewport.x = (swapchainExtent.width - createInfo.width) / 2.f;
    viewport.y = (swapchainExtent.height - createInfo.height) / 2.f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Découpage de l'image
    if (createInfo.height > 0) {
        scissor.offset = {(int) (viewport.x + 0.001f), (int) (viewport.y + 0.001f)};
        scissor.extent = {(uint32_t) createInfo.width, (uint32_t) createInfo.height};
    } else {
        scissor.offset = {(int) (viewport.x + 0.001f), (int) (viewport.y + viewport.height + 0.001f)};
        scissor.extent = {(uint32_t) createInfo.width, (uint32_t) -createInfo.height};
    }

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
    vkDeviceWaitIdle(device);
    putLog("Release resources", LogType::INFO);
    cleanupOldSwapchain();
    for (auto &tmp : samplers)
        vkDestroySampler(device, tmp, nullptr);
    if (presenting) {
        for (auto imageView : swapchainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroySurfaceKHR(vkinstance.get(), surface, nullptr);
    }
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
            putLog("No changes in the pipelineCache", LogType::DEBUG);
        } else {
            std::ofstream t(cachePath + "pipelineCache.dat", std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
            t.write(cacheContent.data(), size);
            t.close();
            putLog("Changes detected in the pipelineCache, store them", LogType::DEBUG);
        }
    }
    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    delete memoryManager;
    vkDestroyDevice(device, nullptr);
    if (hasLayer)
        destroyDebug();
    instance = nullptr;
    for (auto ia : internalAllocations)
        ::free(ia);
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

    bool swapchainAdequate = !presenting;
    if (extensionsSupported && !swapchainAdequate) {
        SwapChainSupportDetails swapchainSupport = querySwapChainSupport(pDevice);
        swapchainAdequate = (!swapchainSupport.formats.empty() && !swapchainSupport.presentModes.empty()) || !presenting;
    }

    return extensionsSupported && swapchainAdequate;
}

void VulkanMgr::initVulkan(const char *AppName, uint32_t appVersion, uint32_t vulkanVersion, SDL_Window *window, bool _hasLayer, bool preferIntegrated)
{
    hasLayer = _hasLayer;

    // initialize the vk::ApplicationInfo structure
    vk::ApplicationInfo applicationInfo(AppName, appVersion, "EntityCore", 1, vulkanVersion);

    // initialize the vk::InstanceCreateInfo
    vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo, hasLayer ? validationLayers.size() : 0, validationLayers.data(), instanceExtension.size(), instanceExtension.data());

    if (hasLayer)
        initDebug(&instanceCreateInfo);

    // create a UniqueInstance
    try {
        vkinstance = vk::createInstanceUnique(instanceCreateInfo);
    } catch (vk::LayerNotPresentError &e) {
        instanceCreateInfo.enabledLayerCount = 0;
        vkinstance = vk::createInstanceUnique(instanceCreateInfo);
    }

    if (hasLayer)
        startDebug();

    if (window)
        initWindow(window);

    putLog("Reading GPU(s) properties", LogType::INFO);
    // get a physicalDevice
    bool oneHasBeenSelected = false;
    bool suboptimalSelected = false;
    const auto preferredGPUType = (preferIntegrated) ? vk::PhysicalDeviceType::eIntegratedGpu : vk::PhysicalDeviceType::eDiscreteGpu;
    const auto suboptimalGPUType = (preferIntegrated) ? vk::PhysicalDeviceType::eDiscreteGpu : vk::PhysicalDeviceType::eIntegratedGpu;
    for (const auto &pDevice : vkinstance->enumeratePhysicalDevices()) {
        if (isDeviceSuitable(pDevice)) {
            if (!suboptimalSelected && pDevice.getProperties().deviceType == suboptimalGPUType) {
                physicalDevice = pDevice;
                suboptimalSelected = true;
                oneHasBeenSelected = true;
            } else if (!oneHasBeenSelected) {
                oneHasBeenSelected = true;
                physicalDevice = pDevice;
            }
            if (pDevice.getProperties().deviceType == preferredGPUType) {
                physicalDevice = pDevice;
                break;
            }
        }
    }
    if (oneHasBeenSelected) {
        putLog("GPU selected", LogType::INFO);
    } else {
        putLog("No valid GPU detected", LogType::ERROR);
        std::cerr << "FATAL : No GPU match requirements\n";
        std::this_thread::sleep_for(std::chrono::seconds(3));
        exit(-1);
    }
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());
    for (const auto& extension : availableExtensions) {
        if (extension.extensionName == std::string("VK_KHR_synchronization2")) {
            canSynchronization2 = true;
            break;
        }
    }
}

void VulkanMgr::initQueues(const QueueRequirement &queueRequest)
{
    // get the QueueFamilyProperties of PhysicalDevice
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
    const float queuePriority[16] = {};
    size_t i = 0;
    queues.reserve(queueFamilyProperties.size());
    for (auto &qfp : queueFamilyProperties) {
        VkBool32 presentSupport = presenting;
        queues.resize(queues.size() + 1);
        queues.back().id = i;
        queues.back().capacity = qfp.queueCount;
        if (presenting) {
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        }
        queues.back().graphic = (qfp.queueFlags & vk::QueueFlagBits::eGraphics) ? 1 : 0;
        queues.back().compute = (qfp.queueFlags & vk::QueueFlagBits::eCompute) ? 1 : 0;
        queues.back().transfer = (qfp.queueFlags & vk::QueueFlagBits::eTransfer) ? 1 : 0;
        queues.back().present = presentSupport;
        ++i;
    }
    unsigned char dedicatedGraphicAndCompute = queueRequest.dedicatedGraphicAndCompute;
    unsigned char dedicatedGraphic = queueRequest.dedicatedGraphic;
    unsigned char dedicatedCompute = queueRequest.dedicatedCompute;
    unsigned char dedicatedTransfer = queueRequest.dedicatedTransfer;
    for (auto &q : queues) {
        if (q.graphic) {
            if (q.compute) {
                q.size = std::min(dedicatedGraphicAndCompute, q.capacity);
                q.dedicatedGraphicAndComputeCount = q.size;
                dedicatedGraphicAndCompute -= q.size;
            } else {
                q.size = std::min(dedicatedGraphic, q.capacity);
                q.dedicatedGraphicCount = q.size;
                dedicatedGraphic -= q.size;
            }
        } else {
            if (q.compute) {
                q.size = std::min(dedicatedCompute, q.capacity);
                q.dedicatedComputeCount = q.size;
                dedicatedCompute -= q.size;
            } else if (q.transfer) {
                q.size = std::min(dedicatedTransfer, q.capacity);
                q.dedicatedTransferCount = q.size;
                dedicatedTransfer -= q.size;
            }
        }
    }
    dedicatedGraphic += dedicatedGraphicAndCompute;
    dedicatedCompute += dedicatedGraphicAndCompute;
    // Look for missing dedicated queues, also split missing dedicated graphic and compute queue
    char transfer = queueRequest.transfer;
    for (auto &q : queues) {
        if (q.capacity > q.size) {
            if (q.graphic) {
                const unsigned char extract = std::min((unsigned char) (q.capacity - q.size), dedicatedGraphic);
                q.size += extract;
                q.dedicatedGraphicCount += extract;
                dedicatedGraphic -= extract;
            }
            if (q.compute) {
                const unsigned char extract = std::min((unsigned char) (q.capacity - q.size), dedicatedCompute);
                q.size += extract;
                q.dedicatedComputeCount += extract;
                dedicatedCompute -= extract;
            }
            if (q.transfer) {
                const unsigned char extract = std::min((unsigned char) (q.capacity - q.size), dedicatedTransfer);
                q.size += extract;
                q.dedicatedTransferCount += extract;
                dedicatedTransfer -= extract;
            }
        }
        if (q.transfer)
            transfer -= q.size;
    }
    if (transfer > 0) {
        // Look for missing transfer queues
        for (auto &q : queues) {
            if (q.transfer) {
                const unsigned char extract = std::min(transfer, (char) (q.capacity - q.size));
                q.size += extract;
                q.dedicatedTransferCount += extract;
                transfer -= extract;
            }
        }
    }
    // Define queue creation
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.pQueuePriorities = queuePriority;
    queueCreateInfo.pNext = nullptr;
    for (auto &q : queues) {
        if (q.size) {
            queueCreateInfo.queueCount = q.size;
            queueCreateInfo.queueFamilyIndex = q.id;
            queueCreateInfos.push_back(queueCreateInfo);
        }
    }
}

#include "VulkanFeatureInterprete.hpp"

void VulkanMgr::initDevice(const VulkanMgrCreateInfo &createInfo)
{
    VkPhysicalDeviceFeatures2 requestedFeatures = createInfo.preferedFeatures;
    VkPhysicalDeviceFeatures2 requiredFeatures = createInfo.requiredFeatures;

    deviceFeatures.pNext = (canSynchronization2) ? &sync2 : nullptr;
    interpreteFeatures(deviceFeatures, requestedFeatures, requiredFeatures, internalAllocations);
    vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);
    finalizeFeatures(deviceFeatures, requestedFeatures, requiredFeatures);

    displayEnabledFeaturesInfo(deviceFeatures, requestedFeatures, requiredFeatures);

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &deviceFeatures;

    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

    deviceCreateInfo.pEnabledFeatures = nullptr;

    if (canSynchronization2 && sync2.synchronization2 == VK_TRUE) {
        SyncEvent::enable();
        deviceExtension.push_back("VK_KHR_synchronization2");
    }

    deviceCreateInfo.enabledExtensionCount = deviceExtension.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtension.data();
    deviceCreateInfo.enabledLayerCount = 0;

    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device");
    }
}

void VulkanMgr::initWindow(SDL_Window *window)
{
    if (SDL_Vulkan_CreateSurface(window, vkinstance.get(), &surface) != SDL_TRUE) {
        putLog(SDL_GetError(), LogType::ERROR);
        throw std::runtime_error("Failed to create window surface");
    }
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats, bool expectLinear)
{
    for (const auto& availableFormat : availableFormats) {
        if (expectLinear) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
                return availableFormat;
        } else {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                return availableFormat;
        }
    }

    return availableFormats[0];
}

static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes, VkPresentModeKHR preferedPresentMode)
{
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == preferedPresentMode)
            return availablePresentMode;
    }
    return VK_PRESENT_MODE_FIFO_KHR; // This mode is the only mode always available
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

void VulkanMgr::initSwapchain(int width, int height, VkImageUsageFlags swapchainUsage, VkPresentModeKHR preferedPresentMode, bool expectLinear)
{
    SwapChainSupportDetails swapchainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats, expectLinear);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes, preferedPresentMode);
    swapchainExtent = chooseSwapExtent(swapchainSupport.capabilities, width, height);
    swapchainImageFormat = surfaceFormat.format;

    uint32_t imageCount = (forceSwapchainCount) ? forceSwapchainCount : swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount) {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.minImageCount = imageCount;
    swapchainCreateInfo.imageFormat = surfaceFormat.format;
    swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainCreateInfo.imageExtent = swapchainExtent;
    swapchainCreateInfo.imageArrayLayers = 1; // Pas besoin de 3D stéréoscopique
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.imageUsage = swapchainUsage;
    swapchainCreateInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Pas de transparence pour le contenu de la fenêtre
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.clipped = VK_FALSE; // ne pas calculer les pixels masqués par une autre fenêtre
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("échec de la création de la swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapchain, &finalImageCount, nullptr);
    swapchainImages.resize(finalImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &finalImageCount, swapchainImages.data());
    for (unsigned int i = 0; i < finalImageCount; ++i) {
        setObjectName(swapchainImages[i], VK_OBJECT_TYPE_IMAGE, "Swapchain Image " + std::to_string(i));
    }
}

bool VulkanMgr::regenerateSwapchain(int width, int height)
{
    VkSurfaceCapabilitiesKHR capabilities;
    cleanupOldSwapchain();

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
    swapchainCreateInfo.imageExtent = chooseSwapExtent(capabilities, width, abs(height));
    if (swapchainCreateInfo.imageExtent.width == 0 || swapchainCreateInfo.imageExtent.height == 0) {
        return false;
    }
    swapchainCreateInfo.oldSwapchain = swapchain;
    swapchainExtent = swapchainCreateInfo.imageExtent;

    if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to regenerate the swapchain");
    }

    uint32_t finalImageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &finalImageCount, nullptr);
    if (finalImageCount != swapchainImages.size()) {
        putLog("The swapchain image count has changed to " + std::to_string(finalImageCount), LogType::WARNING);
        swapchainImages.resize(finalImageCount);
        if (finalImageCount == 0) {
            // This swapchain can't be used, destroy it
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = swapchainCreateInfo.oldSwapchain;
            swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
            return false; // Failure
        }
    }
    vkGetSwapchainImagesKHR(device, swapchain, &finalImageCount, swapchainImages.data());
    for (unsigned int i = 0; i < finalImageCount; ++i)
        setObjectName(swapchainImages[i], VK_OBJECT_TYPE_IMAGE, "Swapchain Image " + std::to_string(i));
    swapchainImageViews.swap(pendingDestroySwapchainImageViews);
    if (swapchainCreateInfo.imageUsage & ALL_IMAGE_VIEW_USAGE)
        createImageViews();

    viewport.width = width;
    viewport.height = height;
    viewport.x = (swapchainExtent.width - width) / 2.f;
    viewport.y = (swapchainExtent.height - height) / 2.f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // Découpage de l'image
    if (height > 0) {
        scissor.offset = {(int) (viewport.x + 0.001f), (int) (viewport.y + 0.001f)};
        scissor.extent = {(uint32_t) width, (uint32_t) height};
    } else {
        scissor.offset = {(int) (viewport.x + 0.001f), (int) (viewport.y + viewport.height + 0.001f)};
        scissor.extent = {(uint32_t) width, (uint32_t) -height};
    }
    return true;
}

void VulkanMgr::cleanupCurrentSwapchain()
{
    cleanupOldSwapchain();
    if (swapchain) {
        for (auto imageView : swapchainImageViews)
            vkDestroyImageView(device, imageView, nullptr);
        swapchainImageViews.clear();
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

void VulkanMgr::cleanupOldSwapchain()
{
    if (swapchainCreateInfo.oldSwapchain) {
        for (auto imageView : pendingDestroySwapchainImageViews)
            vkDestroyImageView(device, imageView, nullptr);
        pendingDestroySwapchainImageViews.clear();
        vkDestroySwapchainKHR(device, swapchainCreateInfo.oldSwapchain, nullptr);
        swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    }
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
    swapchainImageViews.resize(swapchainImages.size());

    for (uint32_t i = 0; i < swapchainImages.size(); i++) {
        swapchainImageViews[i] = createImageView(swapchainImages[i], swapchainImageFormat);
    }
}

bool VulkanMgr::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, SubMemory& bufferMemory, VkMemoryPropertyFlags preferedProperties, uint32_t batch)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        return false;

    VkMemoryDedicatedRequirements memDedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS, nullptr, 0, 0};
    VkMemoryRequirements2 memRequirements;
    memRequirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    memRequirements.pNext = &memDedicated;
    VkBufferMemoryRequirementsInfo2 memBufferInfo{VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2, nullptr, buffer};
    vkGetBufferMemoryRequirements2(device, &memBufferInfo, &memRequirements);
    bufferMemory = (memDedicated.prefersDedicatedAllocation || batch == -1) ?
        memoryManager->dmalloc(memRequirements.memoryRequirements, {VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, nullptr, VK_NULL_HANDLE, buffer}, properties, preferedProperties) :
        memoryManager->malloc(memRequirements.memoryRequirements, properties, preferedProperties, batch);

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
    memoryManager->releaseUnusedChunks();
    if (customReleaseMemory)
        customReleaseMemory();
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
                if (posAddress != std::string::npos)
                    instance->debugFunc[name[posAddress + 4] & 0x3f](reinterpret_cast<void *>(std::stol(name.substr(posAddress + 5))), ss);
            }
        }
    }
    instance->putLog(ss.str(), LogType::LAYER);
    return VK_FALSE;
}

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

#include "VulkanFeatureDisplay.hpp"

void VulkanMgr::displayEnabledFeaturesInfo(const VkPhysicalDeviceFeatures2 &enabledFeatures, const VkPhysicalDeviceFeatures2 &requestedFeatures, const VkPhysicalDeviceFeatures2 &requiredFeatures)
{
    std::ostringstream ss;
    ss << "===== Used device features =====\n";
    bool requirementMet = displayAllEnabledFeatures(ss, enabledFeatures, requestedFeatures, requiredFeatures);
    putLog(ss.str(), LogType::LAYER);
    if (!requirementMet) {
        putLog("One or more mandatory feature is not available", LogType::ERROR);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        exit(-1);
    }
}

void VulkanMgr::putLog(const std::string &str, LogType type)
{
    if (redirectLog)
        return redirectLog(str, type);
    #ifdef NO_DISPLAY_COLOR
        const char *saveHeader;
        #define drawHeader saveHeader
    #else
        const char *saveHeader, *drawHeader;
    #endif

    switch (type) {
        case LogType::INFO:
            drawHeader = "(\x1b[92mINFO\x1b[0m)\t\x1b[3m";
            saveHeader = "(INFO)\t";
            break;
        case LogType::DEBUG:
            drawHeader = "(\x1b[96mDEBUG\x1b[0m)\t";
            saveHeader = "(DEBUG)\t";
            break;
        case LogType::LAYER:
            drawHeader = "\0";
            saveHeader = "\0";
            break;
        case LogType::WARNING:
            drawHeader = "(\x1b[1;93mWARN\x1b[0m)\t\x1b[93m";
            saveHeader = "(WARN)\t";
            break;
        case LogType::ERROR:
            drawHeader = "(\x1b[1;91mERROR\x1b[0m)\t\x1b[1;4;91m";
            saveHeader = "(ERROR)\t";
            break;
    }
    if (drawLogs && type >= minLogPrintLevel) {
        logMutex.lock();
        #ifdef NO_DISPLAY_COLOR
        std::cerr << drawHeader << str << std::endl;
        #else
        std::cerr << drawHeader << str << "\x1b[0m\n";
        #endif
        logMutex.unlock();
    }
    if (saveLogs && type >= minLogWriteLevel) {
        logMutex.lock();
        logs << saveHeader << str << std::endl;
        logMutex.unlock();
    }
}

const QueueFamily *VulkanMgr::previewQueueFamily(VulkanMgr::QueueType type)
{
    switch (type) {
        case QueueType::GRAPHIC:
            for (auto &q : queues) {
                if (q.dedicatedGraphicCount)
                    return &q;
            }
            break;
        case QueueType::COMPUTE:
            for (auto &q : queues) {
                if (q.dedicatedComputeCount)
                    return &q;
            }
            break;
        case QueueType::GRAPHIC_COMPUTE:
            for (auto &q : queues) {
                if (q.dedicatedGraphicAndComputeCount)
                    return &q;
            }
            break;
        case QueueType::TRANSFER:
            for (auto &q : queues) {
                if (q.dedicatedTransferCount)
                    return &q;
            }
            break;
    }
    return nullptr;
}

const QueueFamily *VulkanMgr::acquireQueue(VkQueue &queue, VulkanMgr::QueueType type, const std::string &name)
{
    for (auto &q : queues) {
        switch (type) {
            case QueueType::GRAPHIC:
                if (q.dedicatedGraphicCount) {
                    --q.dedicatedGraphicCount;
                    break;
                }
                continue;
            case QueueType::COMPUTE:
                if (q.dedicatedComputeCount) {
                    --q.dedicatedComputeCount;
                    break;
                }
                continue;
            case QueueType::GRAPHIC_COMPUTE:
                if (q.dedicatedGraphicAndComputeCount) {
                    --q.dedicatedGraphicAndComputeCount;
                    break;
                }
                continue;
            case QueueType::TRANSFER:
                if (q.dedicatedTransferCount) {
                    --q.dedicatedTransferCount;
                    break;
                }
                continue;
        }
        vkGetDeviceQueue(device, q.id, --q.size, &queue);
        if (!name.empty())
            setObjectName(queue, VK_OBJECT_TYPE_QUEUE, name);
        return &q;
    }
    return nullptr;
}

VkSampler &VulkanMgr::getSampler(const VkSamplerCreateInfo &createInfo)
{
    int i = 0;
    for (auto &s : samplers) {
        if (memcmp(&createInfo, &samplersInfo[i++], sizeof(createInfo)) == 0) {
            return s;
        }
    }
    samplers.resize(samplers.size() + 1);
    samplersInfo.push_back(createInfo);
    if (vkCreateSampler(device, &createInfo, nullptr, &samplers.back()) != VK_SUCCESS) {
        throw std::runtime_error("échec de la creation d'un sampler!");
    }
    return samplers.back();
}

void VulkanMgr::update()
{
    // Nothing done yet
}
