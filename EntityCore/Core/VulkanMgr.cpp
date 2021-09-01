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

VulkanMgr::VulkanMgr(const char *_AppName, uint32_t appVersion, SDL_Window *window, int width, int height, const QueueRequirement &queueRequest, const VkPhysicalDeviceFeatures &requiredFeatures, const VkPhysicalDeviceFeatures &preferedFeatures, int chunkSize, bool enableDebugLayers, bool drawLogs, bool saveLogs, std::string _cachePath, VkImageUsageFlags swapchainUsage) :
    refDevice(device), drawLogs(drawLogs), saveLogs(saveLogs), presenting(window != nullptr)
{
    assert(!isAlive); // There must be only one VulkanMgr instance
    instance = this;
    cachePath = _cachePath;
    isAlive = true;
    if (saveLogs) {
        logs.open(_cachePath + "EntityCore-logs.txt", std::ofstream::out | std::ofstream::trunc);
        saveLogs = logs.is_open();
    }
    if (presenting) {
        uint32_t sdl2ExtensionCount = 0;
        if (!SDL_Vulkan_GetInstanceExtensions(window, &sdl2ExtensionCount, nullptr))
            std::runtime_error("Fatal : Failed to found Vulkan extension for SDL2.");

        size_t initialSize = instanceExtension.size();
        instanceExtension.resize(initialSize + sdl2ExtensionCount);
        if (!SDL_Vulkan_GetInstanceExtensions(window, &sdl2ExtensionCount, instanceExtension.data() + initialSize))
            std::runtime_error("Fatal : Failed to found Vulkan extension for SDL2.");
    }

    initVulkan(_AppName, appVersion, window, enableDebugLayers);
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
    displayPhysicalDeviceInfo(physicalDeviceProperties);
    initQueues(queueRequest);
    initDevice(requiredFeatures, preferedFeatures);
    if (presenting) {
        initSwapchain(width, abs(height), swapchainUsage);
        createImageViews();
    } else {
        swapChainExtent.width = width;
        swapChainExtent.height = abs(height);
    }
    memoryManager = new MemoryManager(*this, chunkSize*1024*1024);
    BufferMgr::setUniformOffsetAlignment(physicalDeviceProperties.limits.minUniformBufferOffsetAlignment);
    SyncEvent::setupPFN(vkinstance.get());

    // Déformation de l'image
    viewport.width = width;
    viewport.height = height;
    viewport.x = (swapChainExtent.width - width) / 2.f;
    viewport.y = (swapChainExtent.height - height) / 2.f;
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
    putLog("Release resources", LogType::INFO);
    for (auto &tmp : samplers)
        vkDestroySampler(device, tmp, nullptr);
    if (presenting) {
        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(device, swapChain, nullptr);
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
        swapChainAdequate = (!swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty()) || !presenting;
    }

    return extensionsSupported && swapChainAdequate;
}

void VulkanMgr::initVulkan(const char *AppName, uint32_t appVersion, SDL_Window *window, bool _hasLayer)
{
    hasLayer = _hasLayer;

    // initialize the vk::ApplicationInfo structure
    vk::ApplicationInfo applicationInfo(AppName, appVersion, "EntityCore", 1, VK_API_VERSION_1_1);

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

    putLog("Reading GPU(s) properties", LogType::DEBUG);
    // get a physicalDevice
    bool oneHasBeenSelected = false;
    bool suboptimalSelected = false;
    for (const auto &pDevice : vkinstance->enumeratePhysicalDevices()) {
        if (isDeviceSuitable(pDevice)) {
            if (!suboptimalSelected && pDevice.getProperties().deviceType == vk::PhysicalDeviceType::eIntegratedGpu) {
                suboptimalSelected = true;
                oneHasBeenSelected = false;
            }
            if (!oneHasBeenSelected) {
                oneHasBeenSelected = true;
                physicalDevice = pDevice;
            }
            if (pDevice.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
                physicalDevice = pDevice;
                break;
            }
        }
    }
    if (oneHasBeenSelected) {
        putLog("GPU selected", LogType::DEBUG);
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
            SyncEvent::enable();
            deviceExtension.push_back("VK_KHR_synchronization2");
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

void VulkanMgr::initDevice(const VkPhysicalDeviceFeatures &requiredFeatures, VkPhysicalDeviceFeatures preferedFeatures)
{
    VkPhysicalDeviceFeatures supportedDeviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &supportedDeviceFeatures);

    const VkBool32 *src = reinterpret_cast<const VkBool32 *>(&requiredFeatures);
    VkBool32 *dst = reinterpret_cast<VkBool32 *>(&preferedFeatures);
    constexpr int size = sizeof(requiredFeatures) / sizeof(VkBool32);
    for (int i = 0; i < size; ++i) {
        *(dst++) |= *(src++);
    }
    src = reinterpret_cast<const VkBool32 *>(&preferedFeatures);
    const VkBool32 *src2 = reinterpret_cast<const VkBool32 *>(&supportedDeviceFeatures);
    dst = reinterpret_cast<VkBool32 *>(&deviceFeatures);
    for (int i = 0; i < size; ++i) {
        *(dst++) = *(src++) & *(src2++);
    }
    displayEnabledFeaturesInfo(preferedFeatures, requiredFeatures);

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

void VulkanMgr::initSwapchain(int width, int height, VkImageUsageFlags swapchainUsage)
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
    createInfo.imageUsage = swapchainUsage;
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
    vkGetSwapchainImagesKHR(device, swapChain, &finalImageCount, swapChainImages.data());
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

#define FEATURE_STATE(name, str) if (requestedFeatures.name) { \
    ss << str; \
    if (deviceFeatures.name) { \
        ss << " : enabled\n"; \
    } else { \
        requirementMet |= (!requiredFeatures.name); \
        ss << " : unavailable\n"; \
    } \
} \

void VulkanMgr::displayEnabledFeaturesInfo(const VkPhysicalDeviceFeatures &requestedFeatures, const VkPhysicalDeviceFeatures &requiredFeatures)
{
    bool requirementMet = true;
    std::ostringstream ss;
    ss << "===== Used device features =====\n";
    FEATURE_STATE(fullDrawIndexUint32, "fullDrawIndexUint32")
    FEATURE_STATE(imageCubeArray, "imageCubeArray")
    FEATURE_STATE(independentBlend, "independentBlend")
    FEATURE_STATE(geometryShader, "geometryShader")
    FEATURE_STATE(tessellationShader, "tessellationShader")
    FEATURE_STATE(sampleRateShading, "sampleRateShading")
    FEATURE_STATE(logicOp, "logicOp")
    FEATURE_STATE(multiDrawIndirect, "multiDrawIndirect")
    FEATURE_STATE(drawIndirectFirstInstance, "drawIndirectFirstInstance")
    FEATURE_STATE(depthClamp, "depthClamp")
    FEATURE_STATE(depthBiasClamp, "depthBiasClamp")
    FEATURE_STATE(fillModeNonSolid, "fillModeNonSolid")
    FEATURE_STATE(depthBounds, "depthBounds")
    FEATURE_STATE(wideLines, "wideLines")
    FEATURE_STATE(largePoints, "largePoints")
    FEATURE_STATE(alphaToOne, "alphaToOne")
    FEATURE_STATE(multiViewport, "multiViewport")
    FEATURE_STATE(samplerAnisotropy, "samplerAnisotropy")
    FEATURE_STATE(textureCompressionETC2, "textureCompressionETC2")
    FEATURE_STATE(textureCompressionASTC_LDR, "textureCompressionASTC_LDR")
    FEATURE_STATE(textureCompressionBC, "textureCompressionBC")
    FEATURE_STATE(pipelineStatisticsQuery, "pipelineStatisticsQuery")
    FEATURE_STATE(vertexPipelineStoresAndAtomics, "vertexPipelineStoresAndAtomics")
    FEATURE_STATE(fragmentStoresAndAtomics, "fragmentStoresAndAtomics")
    FEATURE_STATE(shaderTessellationAndGeometryPointSize, "shaderTessellationAndGeometryPointSize")
    FEATURE_STATE(shaderImageGatherExtended, "shaderImageGatherExtended")
    FEATURE_STATE(shaderStorageImageExtendedFormats, "shaderStorageImageExtendedFormats")
    FEATURE_STATE(shaderStorageImageMultisample, "shaderStorageImageMultisample")
    FEATURE_STATE(shaderStorageImageReadWithoutFormat, "shaderStorageImageReadWithoutFormat")
    FEATURE_STATE(shaderUniformBufferArrayDynamicIndexing, "shaderUniformBufferArrayDynamicIndexing")
    FEATURE_STATE(shaderSampledImageArrayDynamicIndexing, "shaderSampledImageArrayDynamicIndexing")
    FEATURE_STATE(shaderStorageBufferArrayDynamicIndexing, "shaderStorageBufferArrayDynamicIndexing")
    FEATURE_STATE(shaderStorageImageArrayDynamicIndexing, "shaderStorageImageArrayDynamicIndexing")
    FEATURE_STATE(shaderClipDistance, "shaderClipDistance")
    FEATURE_STATE(shaderCullDistance, "shaderCullDistance")
    FEATURE_STATE(shaderFloat64, "shaderFloat64")
    FEATURE_STATE(shaderInt64, "shaderInt64")
    FEATURE_STATE(shaderInt16, "shaderInt16")
    FEATURE_STATE(shaderResourceResidency, "shaderResourceResidency")
    FEATURE_STATE(shaderResourceMinLod, "shaderResourceMinLod")
    FEATURE_STATE(sparseBinding, "sparseBinding")
    FEATURE_STATE(variableMultisampleRate, "variableMultisampleRate")
    FEATURE_STATE(inheritedQueries, "inheritedQueries")
    putLog(ss.str(), LogType::LAYER);
    if (!requirementMet) {
        putLog("One or more mandatory feature is not available", LogType::ERROR);
        std::this_thread::sleep_for(std::chrono::seconds(3));
        exit(-1);
    }
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
