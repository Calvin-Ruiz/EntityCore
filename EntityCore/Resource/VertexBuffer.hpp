#ifndef VERTEX_BUFFER_HPP
#define VERTEX_BUFFER_HPP

#include <vulkan/vulkan.h>
#include "EntityCore/SubBuffer.hpp"

class BufferMgr;

/*
* A VertexBuffer created from a VertexArray
* The size of the SubBuffer is always a multiple of the alignment of the VertexBuffer
* Because of this, if only VertexBuffer with the same alignment are created from a BufferMgr,
* it is possible to bind the beginning of the BufferMgr as VertexBuffer and use getOffset
* as the vertexOffset of the draw, allowing dynamic change of model3D by changing the offset of the indirect draw arguments
*/
class VertexBuffer {
public:
    // Note : stride MUST be a multiple of alignment
    VertexBuffer(BufferMgr &mgr, int size, int stride, int alignment);
    ~VertexBuffer();
    // Return the SubBuffer matching this VertexBuffer
    SubBuffer &get() {return vertexBuffer;}
    //! Get the firstVertex/vertexOffset for the draw command
    int getOffset() const {return offset;}
    //! Get the vertexCount for the draw command
    int getVertexCount() const {return size;}
private:
    BufferMgr &mgr;
    SubBuffer vertexBuffer;
    int offset;
    const int size;
};

#endif /* end of include guard: VERTEX_BUFFER_HPP */
