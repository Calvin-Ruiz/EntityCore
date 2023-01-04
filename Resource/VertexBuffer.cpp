#include "EntityCore/Core/BufferMgr.hpp"
#include "VertexBuffer.hpp"
#include <cassert>

VertexBuffer::VertexBuffer(BufferMgr &mgr, int size, int stride, int alignment, int binding) : mgr(mgr), size(size), binding(binding), stride(stride/4)
{
    VkDeviceSize bufferSize = ((stride * size - 1) / alignment + 1) * alignment;

    assert(alignment % stride == 0);
    vertexBuffer = mgr.acquireBuffer(bufferSize);
    offset = vertexBuffer.offset / stride;
}

VertexBuffer::~VertexBuffer()
{
    mgr.releaseBuffer(vertexBuffer);
}

void VertexBuffer::bind(VkCommandBuffer cmd)
{
    const VkDeviceSize offset = vertexBuffer.offset;
    vkCmdBindVertexBuffers(cmd, binding, 1, &vertexBuffer.buffer, &offset);
}

void VertexBuffer::bind(VkCommandBuffer cmd, int _offset)
{
    const VkDeviceSize offset = vertexBuffer.offset + _offset;
    vkCmdBindVertexBuffers(cmd, binding, 1, &vertexBuffer.buffer, &offset);
}

void VertexBuffer::fillEntry(unsigned char elemSize, unsigned int count, const float *src, float *dst)
{
    while (count--) {
        for (unsigned char i = 0; i < elemSize; ++i)
            dst[i] = *(src++);
        dst += stride;
    }
}
