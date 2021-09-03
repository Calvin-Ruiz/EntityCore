#ifndef UNIFORM_HPP_
#define UNIFORM_HPP_

#include "EntityCore/Core/BufferMgr.hpp"

template <typename T>
class Uniform {
public:
    Uniform(BufferMgr &mgr) : mgr(mgr), buffer(mgr.acquireBuffer(sizeof(T))), ptr(*mgr.getPtr(buffer)), allocSize(buffer.size) {
        buffer.size = sizeof(T);
    }
    ~Uniform() {
        buffer.size = allocSize;
        mgr.releaseBuffer(buffer);
    }
    T &get() {return ptr;}
    SubBuffer &getBuffer() {return buffer;}
    int getOffset() {return buffer.offset;}
    operator T&() {return ptr;}
    operator SubBuffer&() {return buffer;}
private:
    BufferMgr &mgr;
    SubBuffer buffer;
    T &ptr;
    const int allocSize;
};

#endif /* end of include guard: UNIFORM_HPP_ */
