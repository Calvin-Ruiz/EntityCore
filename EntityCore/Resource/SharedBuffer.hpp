#ifndef SHAREDBUFFER_HPP_
#define SHAREDBUFFER_HPP_

#include "EntityCore/Core/BufferMgr.hpp"

template <typename T>
class SharedBuffer {
public:
    SharedBuffer(BufferMgr &mgr) : mgr(mgr), buffer(mgr.acquireBuffer(sizeof(T))), ptr(*(T *) mgr.getPtr(buffer)), allocSize(buffer.size) {
        buffer.size = sizeof(T);
    }
    ~SharedBuffer() {
        buffer.size = allocSize;
        mgr.releaseBuffer(buffer);
    }
    inline SharedBuffer &operator=(const T &value) {
        ptr = value;
        return *this;
    }
    inline T &operator*() {
        return ptr;
    }
    inline T *operator->() {
        return &ptr;
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

#endif /* end of include guard: SHAREDBUFFER_HPP_ */
