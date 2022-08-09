#ifndef ALLOC_TRACER_HPP_
#define ALLOC_TRACER_HPP_

#include "SaveData.hpp"
#include <cstdlib>
#include <mutex>
#include <map>
#include <set>

// Uncomment if you want to trace the allocations done by stbi
// #define TRACE_STBI_ALLOCATION

struct AllocRegion {
    int current = 0;
    int maximal = 0;
    std::set<size_t> promotions;
};

class AllocTracer {
public:
    static void *malloc(size_t size);
    static void free(void *ptr);
    static void *realloc(void *ptr, size_t size);

    // Mutex which must be locked when accessing other variables
    static std::mutex mtx;
    static std::map<void *, size_t> allocSizes;
    static std::map<size_t, AllocRegion> allocTypes;
};

#ifdef TRACE_STBI_ALLOCATION
#define STBI_MALLOC(sz) AllocTracer::malloc(sz)
#define STBI_FREE(p) AllocTracer::free(p)
#define STBI_REALLOC(p, newsz) AllocTracer::realloc(p, newsz)
#endif

#endif /* end of include guard: ALLOC_TRACER_HPP_ */
