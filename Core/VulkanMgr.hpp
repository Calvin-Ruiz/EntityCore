#ifndef VULKAN_MGR_HPP_
#define VULKAN_MGR_HPP_

#include <vulkan/vulkan.hpp>
#include "EntityCore/SubMemory.hpp"
#include <string>
#include <fstream>
#include <list>
#include <mutex>

struct SDL_Window;
class MemoryManager;

#ifndef MAX_FRAMES_IN_FLIGHT
#define MAX_FRAMES_IN_FLIGHT 2
#endif

struct VulkanWalker {
    VkStructureType sType;
    VulkanWalker *pNext;
};

// Remove windows macro disturbing everything
#undef ERROR

// pattern : SETUP_PFN(PFN_#, ptr_#, "#");
#define SETUP_PFN(pfn, ptr, str) ptr = reinterpret_cast<pfn>(vkGetInstanceProcAddr(instance, str))

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

enum class LogType : unsigned char {
    DEBUG,
    INFO,
    LAYER,
    WARNING,
    ERROR
};

struct QueueFamily {
    uint32_t id;
    unsigned char capacity;
    unsigned char size; // Number of unused queues which can be acquired
    bool graphic;
    bool compute;
    bool transfer;
    bool present;
    // Determine how many of each type of queue can be extracted
    unsigned char dedicatedGraphicCount;
    unsigned char dedicatedComputeCount;
    unsigned char dedicatedGraphicAndComputeCount;
    unsigned char dedicatedTransferCount;
};

struct QueueRequirement {
    unsigned char transfer;
    unsigned char dedicatedGraphic;
    unsigned char dedicatedCompute;
    unsigned char dedicatedGraphicAndCompute; // If not available, querry pair of graphic and compute queues
    unsigned char dedicatedTransfer;
};

typedef void (*logger_t)(const std::string &str, LogType type);

struct VulkanMgrCreateInfo {
    const char *AppName = nullptr;
    uint32_t appVersion = 1;
    SDL_Window *window = nullptr;
    uint32_t vulkanVersion = VK_API_VERSION_1_1; // Mustn't be a lower version than this one
    int width = 600;
    int height = 600;
    QueueRequirement queueRequest = {1, 1, 0, 0, 0};
    // Note : Version features extensions MUST be chained in increasing order and include every subversion
    // e.g. VkPhysicalDeviceFeatures2 -> VkPhysicalDeviceVulkan11Features -> VkPhysicalDeviceVulkan12Features
    // pNext chains other than version features and are ensured to :
    // - When coming from requiredFeatures, be passed to vkCreateDevice
    // - When coming from preferedFeatures, be passed to vkGetPhysicalDeviceFeatures2 then vkCreateDevice
    // - Be chained to deviceFeatures after version features
    // - Possibly have his pNext value modified
    VkPhysicalDeviceFeatures2 requiredFeatures = {};
    VkPhysicalDeviceFeatures2 preferedFeatures = {};
    std::vector<const char *> requiredExtensions = {};
    logger_t redirectLog = nullptr;
    std::string cachePath = "\0";
    std::string logPath = "\0";
    VkImageUsageFlags swapchainUsage = 0;
    VkPresentModeKHR preferedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    int chunkSize = 64;
    int memoryBatchCount = 0; // Number of memory batch
    int forceSwapchainCount = 0;
    bool enableDebugLayers = true;
    bool drawLogs = true;
    bool saveLogs = false;
    bool preferIntegrated = false;
    bool colorSpaceSRGB = false;
    LogType minLogPrintLevel = LogType::INFO;
    LogType minLogWriteLevel = LogType::INFO;
    void (*customReleaseMemory)() = nullptr;
};

/*
* Core class, manage global ressources
* Used as base to create any vulkan ressource
*/
class VulkanMgr {
public:
    [[deprecated]] VulkanMgr(const char *AppName = nullptr, uint32_t appVersion = 1, SDL_Window *window = nullptr, int width = 600, int height = 600, const QueueRequirement &queueRequest = {1, 1, 0, 0, 0}, const VkPhysicalDeviceFeatures &requiredFeatures = {}, const VkPhysicalDeviceFeatures &preferedFeatures = {}, int chunkSize = 64, bool enableDebugLayers = true, bool drawLogs = true, bool saveLogs = false, std::string _cachePath = "\0", int forceSwapchainCount = 0, VkImageUsageFlags swapchainUsage = 0, bool usePushSet = false, logger_t redirectLog = nullptr);
    VulkanMgr(const VulkanMgrCreateInfo &createInfo);
    ~VulkanMgr();
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, SubMemory& bufferMemory, VkMemoryPropertyFlags preferedProperties = 0, uint32_t batch = 0);
    //! for malloc
    MemoryManager *getMemoryManager() {return memoryManager;}
    void free(SubMemory& bufferMemory);
    void mapMemory(SubMemory& bufferMemory, void **data);
    void unmapMemory(SubMemory& bufferMemory);
    void waitIdle();
    VkPipelineViewportStateCreateInfo &getViewportState() {return viewportState;}
    VkSwapchainKHR &getSwapchain() {return swapChain;}
    std::vector<VkImageView> &getSwapchainView() {return swapChainImageViews;}
    std::vector<VkImage> &getSwapchainImage() {return swapChainImages;}
    VkExtent2D &getSwapChainExtent() {return swapChainExtent;}
    VkRect2D &getScreenRect() {return scissor;}
    // Note : If vulkanVersion is at least VK_API_VERSION_1_2, pNext chain the feature of increasing versions
    const VkPhysicalDeviceFeatures2 &getDeviceFeatures() {return deviceFeatures;}
    VkPipelineCache &getPipelineCache() {return pipelineCache;}
    //! Only MemoryManager should use this method
    VkPhysicalDevice getPhysicalDevice() {return physicalDevice;}
    void setObjectName(void *handle, VkObjectType type, const std::string &name);
    //! Called by MemoryManager when getting low of memory,
    void releaseUnusedMemory();
    //! Define the method called when releaseUnusedMemory is called.
    void setReleaseUnusedMemoryCustomAction(void (*func)()) {
        customReleaseMemory = func;
    }
    //! Call this method once per frame for garbage collector as various optimisations to work
    void update();
    VkInstance getInstance() {
        return vkinstance.get();
    }

    // Load a dedicated graphic, compute, graphic_compute or transfer queue in the queue argument
    // Return the queue family from which the queue was created, or nullptr in case of failure
    // The number of dedicated queues can be lower than expected if they are not available
    // The number of dedicated queues can be higher than expected to fulfill the requirement of queue ability
    // e.g. a request for 1 graphic and 1 compute queue with 1 dedicated graphic may result to 1 dedicated graphic and 1 dedicated compute if there is no graphic and compute queue available.
    enum class QueueType {
        GRAPHIC,
        COMPUTE,
        GRAPHIC_COMPUTE,
        TRANSFER
    };
    const QueueFamily *previewQueueFamily(QueueType type);
    const QueueFamily *acquireQueue(VkQueue &queue, QueueType type, const std::string &name = "\0");

    //! Others
    void putLog(const std::string &str, LogType type = LogType::INFO);
    VkSampler &getSampler(const VkSamplerCreateInfo &createInfo);

    //! getDevice();
    const VkDevice &refDevice;
    static VulkanMgr *instance;
private:
    const bool drawLogs;
    bool saveLogs;
    bool canSynchronization2 = false;
    VkPhysicalDeviceSynchronization2FeaturesKHR sync2 {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR, nullptr, VK_TRUE};
    logger_t redirectLog;
    std::ofstream logs;
    std::string cachePath;
    MemoryManager *memoryManager;
    VkPipelineViewportStateCreateInfo viewportState{};
    VkViewport viewport{};
    VkRect2D scissor{};
    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    VkPhysicalDeviceFeatures2 deviceFeatures{};
    VkPipelineCache pipelineCache;

    void initVulkan(const char *AppName, uint32_t appVersion, uint32_t vulkanVersion, SDL_Window *window, bool _hasLayer = false, bool preferIntegrated = false);
    vk::UniqueInstance vkinstance;
    vk::PhysicalDevice physicalDevice;

    void initWindow(SDL_Window *window);
    VkSurfaceKHR surface;

    void initQueues(const QueueRequirement &queueRequest);
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::vector<QueueFamily> queues;

    void initDevice(const VulkanMgrCreateInfo &createInfo);
    VkDevice device;

    void initSwapchain(int width, int height, VkImageUsageFlags swapchainUsage, VkPresentModeKHR preferedPresentMode, bool expectLinear);
    VkSwapchainKHR swapChain;
    uint32_t finalImageCount;
    std::vector<VkImage> swapChainImages;
    VkExtent2D swapChainExtent;
    VkFormat swapChainImageFormat;

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);
    void createImageViews();
    std::vector<VkImageView> swapChainImageViews;

    // Debug
    void initDebug(vk::InstanceCreateInfo *instanceCreateInfo);
    void startDebug();
    void destroyDebug();
    // Note : identifier ascii code must be included between 0x40 and 0x7e both included
    void setDebugFunction(char identifier, void (*func)(void *self, std::ostringstream &ss)) {
        debugFunc[identifier & 0x3f] = func;
    }
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void * /*pUserData*/);
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    VkDebugUtilsMessengerEXT callback;
    void displayPhysicalDeviceInfo(VkPhysicalDeviceProperties &prop);
    void displayEnabledFeaturesInfo(const VkPhysicalDeviceFeatures2 &enabledFeatures, const VkPhysicalDeviceFeatures2 &requestedFeatures, const VkPhysicalDeviceFeatures2 &requiredFeatures);
    PFN_vkSetDebugUtilsObjectNameEXT ptr_vkSetDebugUtilsObjectNameEXT = nullptr;

    bool checkDeviceExtensionSupport(VkPhysicalDevice pDevice);
    bool isDeviceSuitable(VkPhysicalDevice pDevice);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    std::vector<const char *> instanceExtension = {"VK_KHR_surface", VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_EXTENSION_NAME};
    std::vector<const char *> deviceExtension = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    std::vector<void *> internalAllocations;
    std::list<VkSampler> samplers;
    std::vector<VkSamplerCreateInfo> samplersInfo;
    void (*debugFunc[63])(void *self, std::ostringstream &ss) {};
    void (*customReleaseMemory)();
    std::mutex logMutex;
    int forceSwapchainCount;
    bool hasLayer;
    bool isReady = false;
    bool presenting;
    LogType minLogPrintLevel;
    LogType minLogWriteLevel;
};

#endif
