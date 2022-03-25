#ifndef SHARED_ARRAY_HPP_
#define SHARED_ARRAY_HPP_

#include "EntityCore/Core/BufferMgr.hpp"

template <typename T>
class SharedArray {
public:
    SharedArray(BufferMgr &mgr, int arraySize) : mgr(mgr), buffer(mgr.acquireBuffer(sizeof(T) * arraySize)), ptr((T *) mgr.getPtr(buffer)), allocSize(buffer.size) {
        buffer.size = sizeof(T) * arraySize;
    }
    ~SharedArray() {
        buffer.size = allocSize;
        mgr.releaseBuffer(buffer);
    }
    inline T &operator[](int index) {
        return ptr[index];
    }
    T *get() {return ptr;}
    SubBuffer &getBuffer() {return buffer;}
    int getOffset() {return buffer.offset;}
    operator T*() {return ptr;}
    operator SubBuffer&() {return buffer;}
private:
    BufferMgr &mgr;
    SubBuffer buffer;
    T *ptr;
    const int allocSize;
};

#endif /* end of include guard: SHARED_ARRAY_HPP_ */
