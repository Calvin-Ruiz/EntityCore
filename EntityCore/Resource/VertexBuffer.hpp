#ifndef VERTEX_BUFFER_HPP
#define VERTEX_BUFFER_HPP

#include <vulkan/vulkan.h>
#include "EntityCore/SubBuffer.hpp"

class BufferMgr;

class VertexBuffer {
public:
    // Note : stride MUST be a multiple of alignment
    VertexBuffer(BufferMgr &mgr, int size, int stride, int alignment);
    ~VertexBuffer();
    SubBuffer &get() {return vertexBuffer;}
    int getOffset() const {return offset;}
private:
    BufferMgr &mgr;
    SubBuffer vertexBuffer;
    int offset;
};

#endif /* end of include guard: VERTEX_BUFFER_HPP */
