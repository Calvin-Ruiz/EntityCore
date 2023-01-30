#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <vulkan/vulkan.h>
#include "EntityCore/SubBuffer.hpp"
#include "EntityCore/SubMemory.hpp"
#include "EntityCore/Globals.hpp"
#include <string>

class VulkanMgr;
class BufferMgr;
struct SDL_Surface;

#define ALL_IMAGE_VIEW_USAGE_EXCEPT_VIDEO VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR | VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT

#ifdef VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR
#define ALL_IMAGE_VIEW_USAGE (ALL_IMAGE_VIEW_USAGE_EXCEPT_VIDEO | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR | VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR | VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR)
#else
#define ALL_IMAGE_VIEW_USAGE (ALL_IMAGE_VIEW_USAGE_EXCEPT_VIDEO)
#endif

struct TextureInfo {
    int width = 0; // width of the texture, or 0 to use name as the filename of the texture to load
    int height = 1; // Negate height to inverse the up and the down of this image at initial upload
    int depth = 1;
    int depthColumn = 0; // Number of column used for the depth, 0 mean pow(2, int(log2(depth) / 2))
    int nbChannels = 4; // Number of channels
    int channelSize = 1; // Size of a channel
    int arrayLayers = 1; // Number of layers in the texture
    uint32_t memoryBatch = 0; // Memory batch the texture is allocated from
    void *content = nullptr; // Raw content to upload
    BufferMgr *mgr = nullptr; // BufferMgr from which staging buffer is created
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImageType type = VK_IMAGE_TYPE_2D;
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    const std::string &name = "unnamed";
    bool mipmap = false;
};

/**
*   \brief Manage texture, including mipmapping, writing, reading and sample
*/
class Texture {
public:
    // Create a texture
    Texture(VulkanMgr &master, const TextureInfo &info);
    // Create a framebuffer attachment
    [[deprecated]] Texture(VulkanMgr &master, int width, int height, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT, const std::string &name = "depth attachment", VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VkFormat format = VK_FORMAT_D24_UNORM_S8_UINT, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT, bool mipmap = false);
    // Create a texture
    [[deprecated]] Texture(VulkanMgr &master, BufferMgr &mgr, VkImageUsageFlags usage, const std::string &name = "unnamed", VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, VkImageType type = VK_IMAGE_TYPE_2D);
    virtual ~Texture();
    // load texture using name as filename, return true on success
    [[deprecated]] bool init(int nbChannels = 4, bool mipmap = false);
    // load texture with custom datas
    [[deprecated]] bool init(int width, int height, void *content = nullptr, bool mipmap = false, int nbChannels = 4, int elemSize = 1, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT, int depth = 1);
    //! Export texture to GPU, return true on success
    //! note : if the texture is already on GPU, assume the texture layout is TRANSFER_DST and don't have mipmap
    //! includeTransition : include layout transition to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL. Also build mipmap, if any.
    bool use(VkCommandBuffer cmd = VK_NULL_HANDLE, bool includeTransition = false);
    //! Export texture to GPU, return true on success
    bool use(VkCommandBuffer cmd, Implicit implicit = Implicit::LAYOUT, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VkImageLayout srcLayout = VK_IMAGE_LAYOUT_UNDEFINED);
    //! Release texture on GPU (may invalidate all previous bindings)
    void unuse();
    //! Create texture on RAM with undefined content
    void createSurface();
    //! Create texture on RAM with undefined content and bind it to an new SDL_Surface
    //! Note : the SDL_Surface must be externally destroyed before detaching or destroying this texture
    SDL_Surface *createSDLSurface();
    //! Release texture on RAM, assuming the last transfer to GPU has complete
    void detach();
    //! Acquire pointer to staging memory
    void *acquireStagingMemoryPtr();
    //! Define texture path
    static void setTextureDir(std::string _textureDir) {textureDir = _textureDir;}
    bool isOnCPU() const {return onCPU;}
    bool isOnGPU() const {return onGPU;}
    //! Return internal VkImage
    VkImage getImage();
    //! Return internal VkImageView
    VkImageView getView();
    operator VkImage() {return getImage();}
    operator VkImageView() {return getView();}
    //! Write texture size in width and height arguments
    void getDimensions(int &width, int &height) const {width=info.extent.width;height=info.extent.height;}
    //! Write texture size in width, height and depth arguments
    void getDimensions(int &width, int &height, int &depth) const {width=info.extent.width;height=info.extent.height;depth=info.extent.depth;}
    int getMipmapCount() const {return info.mipLevels;}
    VkImageAspectFlags getAspect() const {return aspect;}
    //! Create a specific view, usefull when you need a specialized VkImageView
    VkImageView createView(uint32_t baseMipLevel, uint32_t mipLevels = 1, uint32_t baseArrayLevel = 0, uint32_t arrayLevels = VK_REMAINING_ARRAY_LAYERS);
    //! Allow renaming a texture on the fly. Usefull when dynamically reusing a texture to save allocation and binding cost.
    void rename(const std::string &name);
    //! Return the size of this texture in the GPU memory.
    VkDeviceSize getTextureSize() const {return sizeInMemory;}
    //! Set the implicit access and pipeline stage assigned to an image layout
    static void implicitBarrier(VkImageLayout layout, VkAccessFlags &access, VkPipelineStageFlags &stage);
protected:
    //! Pre-create image on GPU
    bool preCreateImage();
    //! Create image on GPU
    bool createImage();
    static std::string textureDir;
    VulkanMgr &master;
    BufferMgr *mgr; // Staging memory manager
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    SubMemory memory;
    SubBuffer staging;
    VkImageCreateInfo info;
    VkImageAspectFlags aspect;
    int nbChannels = 4;
    int elemSize = 1;
    std::string name;
    VkDeviceSize sizeInMemory;
    uint32_t memoryBatch = 0;
    unsigned short widthSplit = 1;
    bool onCPU = false;
    bool onGPU = false;
    bool sdlSurface = false;
};

#endif /* end of include guard: TEXTURE_HPP */
