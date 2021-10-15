#include "EntityCore/Core/VulkanMgr.hpp"
#include "VertexArray.hpp"
#include "VertexBuffer.hpp"

static int getFormatSize(VkFormat format)
{
    switch (format) {
        case VK_FORMAT_R32_SFLOAT:
        case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32_UINT: return 4;
        case VK_FORMAT_R32G32_SFLOAT:
        case VK_FORMAT_R32G32_SINT:
        case VK_FORMAT_R32G32_UINT: return 8;
        case VK_FORMAT_R32G32B32_SFLOAT:
        case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32_UINT: return 12;
        case VK_FORMAT_R32G32B32A32_SFLOAT:
        case VK_FORMAT_R32G32B32A32_SINT:
        case VK_FORMAT_R32G32B32A32_UINT: return 16;
        default: assert(false); return 0;
    }
}

VertexArray::VertexArray(VulkanMgr &master, int alignment) : alignment(alignment), master(master)
{
}

bool VertexArray::createBindingEntry(uint32_t stride, VkVertexInputRate type)
{
    if (alignment % stride) {
        master.putLog("Alignment with value " + std::to_string(alignment) + " must be a multiple of stride with value " + std::to_string(stride), LogType::WARNING);
        return false;
    }
    bindingDesc.push_back({++binding, stride, type});
    size = stride;
    location = 0;
    offset = 0;
    return true;
}

bool VertexArray::addInput(VkFormat format, bool used)
{
    const int subSize = getFormatSize(format);
    if (size < subSize) {
        master.putLog("Unsufficient stride for binding " + std::to_string(binding), LogType::WARNING);
        return false;
    }
    size -= subSize;
    if (used)
        attributeDesc.push_back({location++, binding, format, offset});
    offset += subSize;
    return true;
}

std::unique_ptr<VertexBuffer> VertexArray::createBuffer(int binding, int vertexCount, BufferMgr *vertexMgr, BufferMgr *instanceMgr)
{
    const int _alignment = (alignment) ? alignment : bindingDesc[binding].stride;
    switch (bindingDesc[binding].inputRate) {
        case VK_VERTEX_INPUT_RATE_VERTEX:
            return std::make_unique<VertexBuffer>(*vertexMgr, vertexCount, bindingDesc[binding].stride, _alignment, binding);
        case VK_VERTEX_INPUT_RATE_INSTANCE:
            return std::make_unique<VertexBuffer>(*instanceMgr, vertexCount, bindingDesc[binding].stride, _alignment, binding);
        default:
            return nullptr;
    }
}

void VertexArray::bind(VkCommandBuffer &cmd, const std::vector<VertexBuffer *> &vertex, int firstBinding)
{
    const uint32_t sz = vertex.size();
    VkBuffer *buffers = reinterpret_cast<VkBuffer *>(alloca((sizeof(VkDeviceSize) + sizeof(VkBuffer)) * sz));
    VkDeviceSize *offsets = reinterpret_cast<VkDeviceSize *>(buffers + sz);
    for (int i = 0; i < sz; ++i) {
        SubBuffer &b = vertex[i]->get();
        buffers[i] = b.buffer;
        offsets[i] = b.offset;
    }
    vkCmdBindVertexBuffers(cmd, firstBinding, sz, buffers, offsets);
}
