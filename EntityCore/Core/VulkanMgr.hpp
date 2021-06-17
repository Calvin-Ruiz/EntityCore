#ifndef VULKAN_MGR_HPP_
#define VULKAN_MGR_HPP_

#include <vulkan/vulkan.hpp>
#include "EntityCore/SubMemory.hpp"
#include <string>
#include <fstream>

class SDL_Window;
class MemoryManager;

#ifndef MAX_FRAMES_IN_FLIGHT
#define MAX_FRAMES_IN_FLIGHT 2
#endif

// pattern : SETUP_PFN(PFN_#, ptr_#, "#");
#define SETUP_PFN(pfn, ptr, str) ptr = reinterpret_cast<pfn>(vkGetInstanceProcAddr(instance, str))

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

enum class LogType {
    INFO,
    DEBUG,
    LAYER,
    WARNING,
    ERROR
};

class VulkanMgr {
public:
    VulkanMgr(const char *_AppName, uint32_t appVersion, SDL_Window *window, int width = 600, int height = 600, int chunkSize = 64, bool enableDebugLayers = true, bool drawLogs = true, bool saveLogs = false, std::string _cachePath = "\0");
    ~VulkanMgr();
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, SubMemory& bufferMemory, VkMemoryPropertyFlags preferedProperties = 0);
    //! for malloc
    MemoryManager *getMemoryManager() {return memoryManager;}
    void free(SubMemory& bufferMemory);
    void mapMemory(SubMemory& bufferMemory, void **data);
    void unmapMemory(SubMemory& bufferMemory);
    void waitIdle();
    VkPipelineViewportStateCreateInfo &getViewportState() {return viewportState;}
    VkSwapchainKHR &getSwapchain() {return swapChain;}
    std::vector<VkImageView> &getSwapchainView() {return swapChainImageViews;}
    VkExtent2D &getSwapChainExtent() {return swapChainExtent;}
    const VkPhysicalDeviceFeatures &getDeviceFeatures() {return deviceFeatures;}
    VkPipelineCache &getPipelineCache() {return pipelineCache;}
    //! Only MemoryManager should use this method
    VkPhysicalDevice getPhysicalDevice() {return physicalDevice;}
    void setObjectName(void *handle, VkObjectType type, const std::string &name);
    //! Called by MemoryManager when getting low of memory,
    void releaseUnusedMemory();

    //! Queues
    const std::vector<VkQueue> &getGraphicQueues() const {return graphicsAndPresentQueues;}
    const std::vector<VkQueue> &getComputeQueues() const {return computeQueues;}
    const std::vector<VkQueue> &getTransferQueues() const {return transferQueues;}
    const std::vector<size_t> &getGraphicQueueFamilyIndex() const {return graphicsAndPresentQueueFamilyIndex;}
    const std::vector<size_t> &getComputeQueueFamilyIndex() const {return computeQueueFamilyIndex;}
    const std::vector<size_t> &getTransferQueueFamilyIndex() const {return transferQueueFamilyIndex;}

    //! Others
    void putLog(const std::string &str, LogType type = LogType::INFO);

    //! getDevice();
    const VkDevice &refDevice;
    static VulkanMgr *instance;
private:
    const bool drawLogs;
    const bool saveLogs;
    std::ofstream logs;
    std::string cachePath;
    MemoryManager *memoryManager;
    VkPipelineViewportStateCreateInfo viewportState{};
    VkViewport viewport{};
    VkRect2D scissor{};
    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    VkPhysicalDeviceFeatures deviceFeatures{};
    VkPipelineCache pipelineCache;

    void initDevice(const char *AppName, uint32_t appVersion, SDL_Window *window, bool _hasLayer = false);
    vk::UniqueInstance vkinstance;
    vk::PhysicalDevice physicalDevice;

    void initWindow(SDL_Window *window);
    VkSurfaceKHR surface;

    void initQueues(uint32_t nbQueues = 1);
    std::vector<size_t> graphicsAndPresentQueueFamilyIndex;
    std::vector<size_t> graphicsQueueFamilyIndex;
    std::vector<size_t> computeQueueFamilyIndex;
    std::vector<size_t> presentQueueFamilyIndex;
    std::vector<size_t> transferQueueFamilyIndex;
    std::vector<VkQueue> graphicsAndPresentQueues;
    std::vector<VkQueue> graphicsQueues;
    std::vector<VkQueue> computeQueues;
    std::vector<VkQueue> presentQueues;
    std::vector<VkQueue> transferQueues;
    VkDevice device;

    void initSwapchain(int width, int height);
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
    // static void printDebug(std::ostringstream &oss, std::string identifier);
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void * /*pUserData*/);
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    VkDebugUtilsMessengerEXT callback;
    void displayPhysicalDeviceInfo(VkPhysicalDeviceProperties &prop);
    void displayEnabledFeaturesInfo();
    PFN_vkSetDebugUtilsObjectNameEXT ptr_vkSetDebugUtilsObjectNameEXT = nullptr;

    bool checkDeviceExtensionSupport(VkPhysicalDevice pDevice);
    bool isDeviceSuitable(VkPhysicalDevice pDevice);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    std::vector<const char *> instanceExtension = {"VK_KHR_surface", VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_EXTENSION_NAME};
    std::vector<const char *> deviceExtension = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_push_descriptor", "VK_EXT_conditional_rendering", "VK_KHR_synchronization2"};
    std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    bool hasLayer;
    static bool isAlive;
    bool isReady = false;
};

#endif
