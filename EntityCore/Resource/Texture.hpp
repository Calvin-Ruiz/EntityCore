#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <vulkan/vulkan.h>
#include "EntityCore/SubBuffer.hpp"
#include "EntityCore/SubMemory.hpp"
#include <string>
#include <SDL2/SDL.h>

class VulkanMgr;
class BufferMgr;

/**
*   \brief Manage texture, including mipmapping, writing, reading and sample
*/
class Texture {
public:
    // Create a framebuffer attachment
    Texture(VulkanMgr &master, int width, int height, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT, const std::string &name = "depth attachment", VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VkFormat format = VK_FORMAT_D24_UNORM_S8_UINT, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT);
    // Create a texture
    Texture(VulkanMgr &master, BufferMgr &mgr, VkImageUsageFlags usage, const std::string &name = "unnamed", VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, VkImageType type = VK_IMAGE_TYPE_2D);
    virtual ~Texture();
    // load texture using name as filename, return true on success
    bool init(int nbChannels = 4, bool mipmap = false);
    // load texture with custom datas
    bool init(int width, int height, void *content = nullptr, bool mipmap = false, int nbChannels = 4, int elemSize = 1, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT);
    //! Export texture to GPU, return true on success
    //! note : if the texture is already on GPU, assume the texture layout is TRANSFER_DST and don't have mipmap
    //! includeTransition : include layout transition to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, also build mipmap if any (disabled if the texture is already on GPU)
    bool use(VkCommandBuffer cmd = VK_NULL_HANDLE, bool includeTransition = false);
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
    //! Internal use only
    VkImage getImage() {return image;}
    //! Internal use only
    VkImageView getView() {return view;}
    //! Write texture size in width and height arguments
    void getDimensions(int &width, int &height) const {width=info.extent.width;height=info.extent.height;}
    //! Write texture size in width, height and depth arguments
    void getDimensions(int &width, int &height, int &depth) const {width=info.extent.width;height=info.extent.height;depth=info.extent.depth;}
    int getMipmapCount() const {return info.mipLevels;}
    VkImageAspectFlags getAspect() const {return aspect;}
private:
    //! Create image on GPU
    bool createImage();
    static std::string textureDir;
    VulkanMgr &master;
    BufferMgr *mgr; // Staging memory manager
    VkImage image;
    VkImageView view;
    SubMemory memory;
    SubBuffer staging;
    VkImageCreateInfo info;
    VkImageAspectFlags aspect;
    int nbChannels;
    int elemSize;
    std::string name;
    bool onCPU = false;
    bool onGPU = false;
    bool sdlSurface = false;
};

#endif /* end of include guard: TEXTURE_HPP */
