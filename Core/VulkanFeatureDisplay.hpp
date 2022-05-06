#ifndef VULKAN_FEATURE_DISPLAY_HPP_
#define VULKAN_FEATURE_DISPLAY_HPP_

#ifdef __linux__
#define FEATURE_STATE(name) \
if (requestedFeatures.name) { \
    out << #name << " : "; \
    if (enabledFeatures.name) \
        out << "\e[92;1menabled\e[0m\n"; \
    else if (requiredFeatures.name) { \
        out << "\e[91;1munsupported\e[0m\n"; \
        requirementMet = false; \
    } else \
        out << "\e[93munavailable\e[0m\n"; \
}
#else
#define FEATURE_STATE(name) \
if (requestedFeatures.name) { \
    out << #name << " : "; \
    if (enabledFeatures.name) \
        out << "enabled\n"; \
    else if (requiredFeatures.name) { \
        out << "unsupported\n"; \
        requirementMet = false; \
    } else \
        out << "unavailable\n"; \
}
#endif

static bool displayEnabledFeatures10(std::ostream &out, const VkPhysicalDeviceFeatures &enabledFeatures, const VkPhysicalDeviceFeatures &requestedFeatures, const VkPhysicalDeviceFeatures &requiredFeatures)
{
    out << "--- Vulkan 1.0 features ---\n";
    bool requirementMet = true;
    FEATURE_STATE(fullDrawIndexUint32)
    FEATURE_STATE(imageCubeArray)
    FEATURE_STATE(independentBlend)
    FEATURE_STATE(geometryShader)
    FEATURE_STATE(tessellationShader)
    FEATURE_STATE(sampleRateShading)
    FEATURE_STATE(logicOp)
    FEATURE_STATE(multiDrawIndirect)
    FEATURE_STATE(drawIndirectFirstInstance)
    FEATURE_STATE(depthClamp)
    FEATURE_STATE(depthBiasClamp)
    FEATURE_STATE(fillModeNonSolid)
    FEATURE_STATE(depthBounds)
    FEATURE_STATE(wideLines)
    FEATURE_STATE(largePoints)
    FEATURE_STATE(alphaToOne)
    FEATURE_STATE(multiViewport)
    FEATURE_STATE(samplerAnisotropy)
    FEATURE_STATE(textureCompressionETC2)
    FEATURE_STATE(textureCompressionASTC_LDR)
    FEATURE_STATE(textureCompressionBC)
    FEATURE_STATE(pipelineStatisticsQuery)
    FEATURE_STATE(vertexPipelineStoresAndAtomics)
    FEATURE_STATE(fragmentStoresAndAtomics)
    FEATURE_STATE(shaderTessellationAndGeometryPointSize)
    FEATURE_STATE(shaderImageGatherExtended)
    FEATURE_STATE(shaderStorageImageExtendedFormats)
    FEATURE_STATE(shaderStorageImageMultisample)
    FEATURE_STATE(shaderStorageImageReadWithoutFormat)
    FEATURE_STATE(shaderUniformBufferArrayDynamicIndexing)
    FEATURE_STATE(shaderSampledImageArrayDynamicIndexing)
    FEATURE_STATE(shaderStorageBufferArrayDynamicIndexing)
    FEATURE_STATE(shaderStorageImageArrayDynamicIndexing)
    FEATURE_STATE(shaderClipDistance)
    FEATURE_STATE(shaderCullDistance)
    FEATURE_STATE(shaderFloat64)
    FEATURE_STATE(shaderInt64)
    FEATURE_STATE(shaderInt16)
    FEATURE_STATE(shaderResourceResidency)
    FEATURE_STATE(shaderResourceMinLod)
    FEATURE_STATE(sparseBinding)
    FEATURE_STATE(variableMultisampleRate)
    FEATURE_STATE(inheritedQueries)
    return requirementMet;
}

#ifdef VK_API_VERSION_1_2
static bool displayEnabledFeatures11(std::ostream &out, const VkPhysicalDeviceVulkan11Features &enabledFeatures, const VkPhysicalDeviceVulkan11Features &requestedFeatures, const VkPhysicalDeviceVulkan11Features &requiredFeatures)
{
    out << "--- Vulkan 1.1 features ---\n";
    bool requirementMet = true;
    FEATURE_STATE(storageBuffer16BitAccess)
    FEATURE_STATE(uniformAndStorageBuffer16BitAccess)
    FEATURE_STATE(storagePushConstant16)
    FEATURE_STATE(storageInputOutput16)
    FEATURE_STATE(multiview)
    FEATURE_STATE(multiviewGeometryShader)
    FEATURE_STATE(multiviewTessellationShader)
    FEATURE_STATE(variablePointersStorageBuffer)
    FEATURE_STATE(variablePointers)
    FEATURE_STATE(protectedMemory)
    FEATURE_STATE(samplerYcbcrConversion)
    FEATURE_STATE(shaderDrawParameters)
    return requirementMet;
}

static bool displayEnabledFeatures12(std::ostream &out, const VkPhysicalDeviceVulkan12Features &enabledFeatures, const VkPhysicalDeviceVulkan12Features &requestedFeatures, const VkPhysicalDeviceVulkan12Features &requiredFeatures)
{
    out << "--- Vulkan 1.2 features ---\n";
    bool requirementMet = true;
    FEATURE_STATE(samplerMirrorClampToEdge)
    FEATURE_STATE(drawIndirectCount)
    FEATURE_STATE(storageBuffer8BitAccess)
    FEATURE_STATE(uniformAndStorageBuffer8BitAccess)
    FEATURE_STATE(storagePushConstant8)
    FEATURE_STATE(shaderBufferInt64Atomics)
    FEATURE_STATE(shaderSharedInt64Atomics)
    FEATURE_STATE(shaderFloat16)
    FEATURE_STATE(shaderInt8)
    FEATURE_STATE(descriptorIndexing)
    FEATURE_STATE(shaderInputAttachmentArrayDynamicIndexing)
    FEATURE_STATE(shaderUniformTexelBufferArrayDynamicIndexing)
    FEATURE_STATE(shaderStorageTexelBufferArrayDynamicIndexing)
    FEATURE_STATE(shaderUniformBufferArrayNonUniformIndexing)
    FEATURE_STATE(shaderSampledImageArrayNonUniformIndexing)
    FEATURE_STATE(shaderStorageBufferArrayNonUniformIndexing)
    FEATURE_STATE(shaderStorageImageArrayNonUniformIndexing)
    FEATURE_STATE(shaderInputAttachmentArrayNonUniformIndexing)
    FEATURE_STATE(shaderUniformTexelBufferArrayNonUniformIndexing)
    FEATURE_STATE(shaderStorageTexelBufferArrayNonUniformIndexing)
    FEATURE_STATE(descriptorBindingUniformBufferUpdateAfterBind)
    FEATURE_STATE(descriptorBindingSampledImageUpdateAfterBind)
    FEATURE_STATE(descriptorBindingStorageImageUpdateAfterBind)
    FEATURE_STATE(descriptorBindingStorageBufferUpdateAfterBind)
    FEATURE_STATE(descriptorBindingUniformTexelBufferUpdateAfterBind)
    FEATURE_STATE(descriptorBindingStorageTexelBufferUpdateAfterBind)
    FEATURE_STATE(descriptorBindingUpdateUnusedWhilePending)
    FEATURE_STATE(descriptorBindingPartiallyBound)
    FEATURE_STATE(descriptorBindingVariableDescriptorCount)
    FEATURE_STATE(runtimeDescriptorArray)
    FEATURE_STATE(samplerFilterMinmax)
    FEATURE_STATE(scalarBlockLayout)
    FEATURE_STATE(imagelessFramebuffer)
    FEATURE_STATE(uniformBufferStandardLayout)
    FEATURE_STATE(shaderSubgroupExtendedTypes)
    FEATURE_STATE(separateDepthStencilLayouts)
    FEATURE_STATE(hostQueryReset)
    FEATURE_STATE(timelineSemaphore)
    FEATURE_STATE(bufferDeviceAddress)
    FEATURE_STATE(bufferDeviceAddressCaptureReplay)
    FEATURE_STATE(bufferDeviceAddressMultiDevice)
    FEATURE_STATE(vulkanMemoryModel)
    FEATURE_STATE(vulkanMemoryModelDeviceScope)
    FEATURE_STATE(vulkanMemoryModelAvailabilityVisibilityChains)
    FEATURE_STATE(shaderOutputViewportIndex)
    FEATURE_STATE(shaderOutputLayer)
    FEATURE_STATE(subgroupBroadcastDynamicId)
    return requirementMet;
}

#ifdef VK_API_VERSION_1_3
static bool displayEnabledFeatures13(std::ostream &out, const VkPhysicalDeviceVulkan13Features &enabledFeatures, const VkPhysicalDeviceVulkan13Features &requestedFeatures, const VkPhysicalDeviceVulkan13Features &requiredFeatures)
{
    out << "--- Vulkan 1.3 features ---\n";
    bool requirementMet = true;
    FEATURE_STATE(robustImageAccess)
    FEATURE_STATE(inlineUniformBlock)
    FEATURE_STATE(descriptorBindingInlineUniformBlockUpdateAfterBind)
    FEATURE_STATE(pipelineCreationCacheControl)
    FEATURE_STATE(privateData)
    FEATURE_STATE(shaderDemoteToHelperInvocation)
    FEATURE_STATE(shaderTerminateInvocation)
    FEATURE_STATE(subgroupSizeControl)
    FEATURE_STATE(computeFullSubgroups)
    FEATURE_STATE(synchronization2)
    FEATURE_STATE(textureCompressionASTC_HDR)
    FEATURE_STATE(shaderZeroInitializeWorkgroupMemory)
    FEATURE_STATE(dynamicRendering)
    FEATURE_STATE(shaderIntegerDotProduct)
    FEATURE_STATE(maintenance4)
    return requirementMet;
}

#endif
#endif

static bool displayAllEnabledFeatures(std::ostream &out, const VkPhysicalDeviceFeatures2 &enabledFeatures, const VkPhysicalDeviceFeatures2 &requestedFeatures, const VkPhysicalDeviceFeatures2 &requiredFeatures)
{
    bool requirementMet = displayEnabledFeatures10(out, enabledFeatures.features, requestedFeatures.features, requiredFeatures.features);
    #ifdef VK_API_VERSION_1_2
    VulkanWalker *enabledFeaturesWalker = (VulkanWalker *) enabledFeatures.pNext;
    VulkanWalker *requestedFeaturesWalker = (VulkanWalker *) requestedFeatures.pNext;
    VulkanWalker *requiredFeaturesWalker = (VulkanWalker *) requiredFeatures.pNext;
    if (enabledFeaturesWalker && enabledFeaturesWalker->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES) {
        if (!(requiredFeaturesWalker && requiredFeaturesWalker->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES))
            requiredFeaturesWalker = enabledFeaturesWalker;
        requirementMet &= displayEnabledFeatures11(out, *(VkPhysicalDeviceVulkan11Features *) enabledFeaturesWalker, *(VkPhysicalDeviceVulkan11Features *) requestedFeaturesWalker, *(VkPhysicalDeviceVulkan11Features *) requiredFeaturesWalker);
        enabledFeaturesWalker = enabledFeaturesWalker->pNext;
        requestedFeaturesWalker = requestedFeaturesWalker->pNext;
        requiredFeaturesWalker = requiredFeaturesWalker->pNext;
        if (enabledFeaturesWalker && enabledFeaturesWalker->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
            if (!(requiredFeaturesWalker && requiredFeaturesWalker->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES))
                requiredFeaturesWalker = enabledFeaturesWalker;
            requirementMet &= displayEnabledFeatures12(out, *(VkPhysicalDeviceVulkan12Features *) enabledFeaturesWalker, *(VkPhysicalDeviceVulkan12Features *) requestedFeaturesWalker, *(VkPhysicalDeviceVulkan12Features *) requiredFeaturesWalker);
            #ifdef VK_API_VERSION_1_3
            enabledFeaturesWalker = enabledFeaturesWalker->pNext;
            requestedFeaturesWalker = requestedFeaturesWalker->pNext;
            requiredFeaturesWalker = requiredFeaturesWalker->pNext;
            if (enabledFeaturesWalker && enabledFeaturesWalker->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES) {
                if (!(requiredFeaturesWalker && requiredFeaturesWalker->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES))
                    requiredFeaturesWalker = enabledFeaturesWalker;
                requirementMet &= displayEnabledFeatures13(out, *(VkPhysicalDeviceVulkan13Features *) enabledFeaturesWalker, *(VkPhysicalDeviceVulkan13Features *) requestedFeaturesWalker, *(VkPhysicalDeviceVulkan13Features *) requiredFeaturesWalker);
            }
            #endif
        }
    }
    #endif
    return requirementMet;
}

#endif /* end of include guard: VULKAN_FEATURE_DISPLAY_HPP_ */
