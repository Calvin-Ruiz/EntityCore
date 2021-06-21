#include "EntityCore/Core/BufferMgr.hpp"
#include "VertexBuffer.hpp"
#include <cassert>

VertexBuffer::VertexBuffer(BufferMgr &mgr, int size, int stride, int alignment) : mgr(mgr)
{
    VkDeviceSize bufferSize = ((stride * size - 1) / alignment + 1) * alignment;

    assert(alignment % stride == 0);
    vertexBuffer = mgr.acquireBuffer(bufferSize, false);
    offset = vertexBuffer.offset / stride;
}

VertexBuffer::~VertexBuffer()
{
    mgr.releaseBuffer(vertexBuffer);
}
