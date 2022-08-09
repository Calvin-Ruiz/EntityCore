#include "AllocTracer.hpp"

std::mutex AllocTracer::mtx;
std::map<void *, size_t> AllocTracer::allocSizes;
std::map<size_t, AllocRegion> AllocTracer::allocTypes;

void *AllocTracer::malloc(size_t size)
{
    void *ret = ::malloc(size);
    mtx.lock();
    auto &tmp = allocTypes[size];
    allocSizes[ret] = size;
    if (++tmp.current > tmp.maximal)
        tmp.maximal = tmp.current;
    mtx.unlock();
    return ret;
}

void AllocTracer::free(void *ptr)
{
    mtx.lock();
    --allocTypes[allocSizes[ptr]].current;
    ::free(ptr);
    mtx.unlock();
}

void *AllocTracer::realloc(void *ptr, size_t size)
{
    mtx.lock();
    {
        auto &tmp = allocTypes[allocSizes[ptr]];
        ptr = ::realloc(ptr, size);
        --tmp.current;
        tmp.promotions.insert(size);
    }
    auto &tmp = allocTypes[size];
    allocSizes[ptr] = size;
    if (++tmp.current > tmp.maximal)
        tmp.maximal = tmp.current;
    mtx.unlock();
    return ptr;
}
