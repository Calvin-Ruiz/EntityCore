#include "EntityCore/Core/VulkanMgr.hpp"
#include "VertexBuffer.hpp"
#include "VertexArray.hpp"

static int getFormatSize(VkFormat format)
{
    switch (format) {
        case VK_FORMAT_R32_SFLOAT:
        case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32_UINT: return 1;
        case VK_FORMAT_R32G32_SFLOAT:
        case VK_FORMAT_R32G32_SINT:
        case VK_FORMAT_R32G32_UINT: return 2;
        case VK_FORMAT_R32G32B32_SFLOAT:
        case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32_UINT: return 3;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
        case VK_FORMAT_R32G32B32A32_SINT:
        case VK_FORMAT_R32G32B32A32_UINT: return 4;
        default: assert(false); return 0;
    }
}

VertexArray::VertexArray(VulkanMgr &master) : master(master)
{
}
