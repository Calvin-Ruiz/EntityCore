#ifndef TILEMAP_HPP
#define TILEMAP_HPP

#include "Texture.hpp"
#include "EntityCore/SubTexture.hpp"

enum class Implicit : uint8_t
{
    NOTHING = 0x00,
    SRC_LAYOUT = 0x01,
    DST_LAYOUT = 0x02,
    LAYOUT = SRC_LAYOUT | DST_LAYOUT,
};

/**
*   \brief Dynamic creation and usage of tile map
*/
class TileMap : protected Texture {
public:
    // chunkSize : size of a chunk, every allocation are rounded up to a multiple of this value
    TileMap(VulkanMgr &master, BufferMgr &mgr, const std::string &name = "unnamed", uint8_t chunkSize = 4, VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);
    virtual ~TileMap();

    bool createMap(uint32_t width, uint32_t height);
    SubTexture acquireSurface(uint32_t width, uint32_t height);
    void releaseSurface(SubTexture &surface);
    void writeSurface(SubTexture &surface, const void *data);
    void writeInSurface(SubTexture &surface, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const void *data);
    // Create a SubTexture at specific coordinates. You are responsible for the validity of the area acquired by this way.
    SubTexture makeSubTexture(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
    void uploadChanges(VkCommandBuffer cmd, Implicit implicit = Implicit::LAYOUT, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    // For set binding
    inline Texture &operator*() {return *(Texture *) this;}
private:
    void reserveSpace(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void releaseSpace(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void writeAt(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const int *data);
    inline void propagate(uint16_t *ptr) {
        while (*--ptr)
            *ptr = ptr[1] + 1;
    }
    inline void propagate(uint16_t *ptr, uint16_t forced) {
        while (forced--) {
            --ptr;
            *ptr = ptr[1] + 1;
        }
        while (*--ptr)
            *ptr = ptr[1] + 1;
    }
    std::vector<VkBufferImageCopy> writes;
    int *ptr;
    uint16_t *map = nullptr;
    uint16_t *negmap = nullptr;
    uint16_t lineSize = 0;
    uint16_t *end;
    const uint8_t CHUNK_SIZE;
    bool loaded = false;

};

#endif /* end of include guard: TILEMAP_HPP */
