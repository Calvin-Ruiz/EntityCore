#ifndef ASYNC_BUILDER_HPP_
#define ASYNC_BUILDER_HPP_

#include "AsyncBase.hpp"
#include <atomic>

class AsyncBuilder {
public:
    AsyncBuilder(LoadPriority priority) :
        priority(priority)
    {
    }
    virtual ~AsyncBuilder() = default;

    virtual void asyncLoad() = 0;
    virtual void postLoad() = 0;
    inline void detach() noexcept {
        if (useCount.fetch_sub(1U, std::memory_order_relaxed) == 1U)
            delete this;
    }

    std::atomic<LoadPriority> priority;
    std::atomic<uint32_t> useCount{1U};
};

#endif /* end of include guard: ASYNC_BUILDER_HPP_ */
