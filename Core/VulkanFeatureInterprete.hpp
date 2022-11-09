#ifndef VULKAN_FEATURE_INTERPRETE_HPP_
#define VULKAN_FEATURE_INTERPRETE_HPP_

struct VulkanFeatureWalker {
    VkStructureType sType;
    VulkanFeatureWalker *pNext;
    VkBool32 flags[];
};

#define NBFLAGSIN(object) (sizeof(object) - offsetof(VulkanFeatureWalker, flags)) / sizeof(VkBool32)

#define COMBINE_LEVEL(object) for (int i = NBFLAGSIN(object); i--; requestedFeatures->flags[i] |= requiredFeatures->flags[i])

#define INTERPRETE_FEATURE_LEVEL(flag, object) \
if (requiredFeatures->pNext && requiredFeatures->pNext->sType == flag) { \
    requiredFeatures = requiredFeatures->pNext; \
    if (!(requestedFeatures->pNext && requestedFeatures->pNext->sType == flag)) { \
        internalAllocations.push_back(malloc(sizeof(object))); \
        ((VulkanFeatureWalker *) internalAllocations.back())->pNext = requestedFeatures->pNext; \
        requestedFeatures->pNext = (VulkanFeatureWalker *) internalAllocations.back(); \
        requestedFeatures->pNext->sType = flag; \
    } \
    requestedFeatures = requestedFeatures->pNext; \
    COMBINE_LEVEL(object);

// This function must :
// 1 - Properly set sType
// 2 - Properly add requiredFeatures to requestedFeatures
// 3 - Properly chain deviceFeatures the same way requestedFeatures is chained
static void interpreteFeatures(VkPhysicalDeviceFeatures2 &_deviceFeatures, VkPhysicalDeviceFeatures2 &_requestedFeatures, VkPhysicalDeviceFeatures2 &_requiredFeatures, std::vector<void *> &internalAllocations)
{
    VulkanFeatureWalker *deviceFeatures = (VulkanFeatureWalker *) &_deviceFeatures;
    VulkanFeatureWalker *requestedFeatures = (VulkanFeatureWalker *) &_requestedFeatures;
    VulkanFeatureWalker *requiredFeatures = (VulkanFeatureWalker *) &_requiredFeatures;

    deviceFeatures->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    requestedFeatures->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    requiredFeatures->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    // Add requiredFeatures to requestedFeatures
    COMBINE_LEVEL(VkPhysicalDeviceFeatures2);
    #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
    INTERPRETE_FEATURE_LEVEL(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, VkPhysicalDeviceVulkan11Features)
        #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
        INTERPRETE_FEATURE_LEVEL(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, VkPhysicalDeviceVulkan12Features)
            #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
            INTERPRETE_FEATURE_LEVEL(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, VkPhysicalDeviceVulkan13Features)
                #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES
                INTERPRETE_FEATURE_LEVEL(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES, VkPhysicalDeviceVulkan14Features)
                }
                #endif
            }
            #endif
        }
        #endif
    }
    #endif

    // Chain deviceFeatures the same way requrestedFeatures is chained
    VulkanFeatureWalker *tmp = deviceFeatures->pNext;
    requestedFeatures = (VulkanFeatureWalker *) &_requestedFeatures;
    #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
    if (requestedFeatures->pNext && requestedFeatures->pNext->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES) {
        requestedFeatures = requestedFeatures->pNext;
        deviceFeatures->pNext = (VulkanFeatureWalker *) malloc(sizeof(VkPhysicalDeviceVulkan11Features));
        deviceFeatures = deviceFeatures->pNext;
        deviceFeatures->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        internalAllocations.push_back(deviceFeatures);
        #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
        if (requestedFeatures->pNext && requestedFeatures->pNext->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
            requestedFeatures = requestedFeatures->pNext;
            deviceFeatures->pNext = (VulkanFeatureWalker *) malloc(sizeof(VkPhysicalDeviceVulkan12Features));
            deviceFeatures = deviceFeatures->pNext;
            deviceFeatures->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            internalAllocations.push_back(deviceFeatures);
            #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
            if (requestedFeatures->pNext && requestedFeatures->pNext->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES) {
                requestedFeatures = requestedFeatures->pNext;
                deviceFeatures->pNext = (VulkanFeatureWalker *) malloc(sizeof(VkPhysicalDeviceVulkan13Features));
                deviceFeatures = deviceFeatures->pNext;
                deviceFeatures->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
                internalAllocations.push_back(deviceFeatures);
                #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES
                if (requestedFeatures->pNext && requestedFeatures->pNext->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES) {
                    requestedFeatures = requestedFeatures->pNext;
                    deviceFeatures->pNext = (VulkanFeatureWalker *) malloc(sizeof(VkPhysicalDeviceVulkan14Features));
                    deviceFeatures = deviceFeatures->pNext;
                    deviceFeatures->sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
                    internalAllocations.push_back(deviceFeatures);
                }
                #endif
            }
            #endif
        }
        #endif
    }
    #endif
    deviceFeatures->pNext = tmp;
    while (deviceFeatures->pNext)
        deviceFeatures = deviceFeatures->pNext;
    deviceFeatures->pNext = requestedFeatures->pNext;
}

static void finalizeFeatures(VkPhysicalDeviceFeatures2 &_deviceFeatures, VkPhysicalDeviceFeatures2 &_requestedFeatures, VkPhysicalDeviceFeatures2 &_requiredFeatures)
{
    VulkanFeatureWalker *deviceFeatures = (VulkanFeatureWalker *) &_deviceFeatures;
    // VulkanFeatureWalker *requestedFeatures = (VulkanFeatureWalker *) &_requestedFeatures;
    VulkanFeatureWalker *requiredFeatures = (VulkanFeatureWalker *) &_requiredFeatures;

    while (deviceFeatures->pNext)
        deviceFeatures = deviceFeatures->pNext;

    #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES
    if (requiredFeatures->pNext && requiredFeatures->pNext->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES) {
        requiredFeatures = requiredFeatures->pNext;
        #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES
        if (requiredFeatures->pNext && requiredFeatures->pNext->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
            requiredFeatures = requiredFeatures->pNext;
            #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
            if (requiredFeatures->pNext && requiredFeatures->pNext->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES) {
                requiredFeatures = requiredFeatures->pNext;
                #ifdef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES
                if (requiredFeatures->pNext && requiredFeatures->pNext->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES) {
                    requiredFeatures = requiredFeatures->pNext;
                }
                #endif
            }
            #endif
        }
        #endif
    }
    #endif
    deviceFeatures->pNext = requiredFeatures->pNext;
}

#undef NBFLAGSIN

#endif /* end of include guard: VULKAN_FEATURE_INTERPRETE_HPP_ */
