/*
** EPITECH PROJECT, 2020
** EntityCore
** File description:
** SyncEvent.hpp
*/

#ifndef SYNCEVENT_HPP_
#define SYNCEVENT_HPP_

#include <vulkan/vulkan.h>
#include "EntityCore/SubMemory.hpp"
#include "EntityCore/SubBuffer.hpp"
#include <vector>

class VulkanMgr;
class Texture;
class BufferMgr;

/*
* Simplify use of VkEvent and add compatibility system for advanced synchronisation
* Note that VkEvent is a synchronisation mechanism internal to a VkQueue
* Synchronisation between VkQueue must be done using VkSemaphore
*/
class SyncEvent {
public:
    // Create SyncEvent for event dependency (and pipeline barrier)
    SyncEvent(VulkanMgr *master, bool deviceOnly = true);
    // Create SyncEvent for pipeline barrier
    SyncEvent(VkDependencyFlags dependencies = 0);
    virtual ~SyncEvent();
    SyncEvent(const SyncEvent &cpy) = delete;
    SyncEvent &operator=(const SyncEvent &src) = delete;

    // ===== SETUP ===== //
    void globalBarrier(VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess);
    void bufferBarrier(SubBuffer &buffer, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess);
    void bufferBarrier(VkBuffer buffer, uint32_t offset, uint32_t size, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess);
    void bufferBarrier(BufferMgr &buffer, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess);
    void imageBarrier(Texture &texture, VkImageLayout srcLayout, VkImageLayout dstLayout, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess, uint32_t miplevel = 0, uint32_t miplevelcount = VK_REMAINING_MIP_LEVELS);
    void imageBarrier(VkImage image, VkImageAspectFlags aspect, VkImageLayout srcLayout, VkImageLayout dstLayout, VkPipelineStageFlags2KHR srcStage, VkPipelineStageFlags2KHR dstStage, VkAccessFlags2KHR srcAccess, VkAccessFlags2KHR dstAccess, uint32_t miplevel = 0, uint32_t miplevelcount = VK_REMAINING_MIP_LEVELS);
    // Assemble every previously defined barriers for any use
    // Re-build is neither allowed between srcDependency and dstDependency nor after any combineDstDependency involving this SyncEvent
    void build();

    // ===== USE ===== //
    // Event dependency
    void srcDependency(VkCommandBuffer cmd);
    void dstDependency(VkCommandBuffer cmd);
    void combineDstDependencies(SyncEvent &with);
    void multiDstDependency(VkCommandBuffer cmd);
    void resetDependency(VkCommandBuffer cmd, VkPipelineStageFlags2KHR stage);
    bool isSet();
    // Pipeline barrier
    void placeBarrier(VkCommandBuffer cmd);
    bool hasMultiDstDependency() const {return !multiEvent.empty();}
    static void setupPFN(VkInstance instance);
    static void enable() {enabled = true;}
    //! Return true if VK_KHR_synchronization2 is supported and enabled
    static bool useSynchronization2() {return enabled;}
    VkPipelineStageFlags compatConvStage(VkPipelineStageFlags2KHR stage);
    VkAccessFlags compatConvAccess(VkAccessFlags2KHR access);
    static PFN_vkQueueSubmit2KHR ptr_vkQueueSubmit2KHR;
private:
    static PFN_vkCmdSetEvent2KHR ptr_vkCmdSetEvent2KHR;
    static PFN_vkCmdWaitEvents2KHR ptr_vkCmdWaitEvents2KHR;
    static PFN_vkCmdResetEvent2KHR ptr_vkCmdResetEvent2KHR;
    static PFN_vkCmdPipelineBarrier2KHR ptr_vkCmdPipelineBarrier2KHR;
    static bool enabled;
    VulkanMgr *master = nullptr;
    VkEvent event = VK_NULL_HANDLE;
    VkDependencyInfoKHR dep;
    std::vector<VkMemoryBarrier2KHR> global;
    std::vector<VkBufferMemoryBarrier2KHR> buffers;
    std::vector<VkImageMemoryBarrier2KHR> image;
    std::vector<VkEvent> multiEvent;
    std::vector<VkDependencyInfoKHR> multiDep;

    VkPipelineStageFlags compatSrc = 0;
    VkPipelineStageFlags compatDst = 0;
    std::vector<VkMemoryBarrier> compatGlobal;
    std::vector<VkBufferMemoryBarrier> compatBuffers;
    std::vector<VkImageMemoryBarrier> compatImage;
    std::vector<SyncEvent *> compatMulti;
};

#endif /* SYNCEVENT_HPP_ */
