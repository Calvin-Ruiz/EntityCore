#ifndef ASYNC_BUILDER_HPP_
#define ASYNC_BUILDER_HPP_

#include "AsyncBase.hpp"

class AsyncBuilder {
public:
    AsyncBuilder(LoadPriority priority) :
        priority(priority)
    {
    }
    virtual ~AsyncBuilder() = default;

    virtual void asyncLoad() = 0;
    virtual void postLoad() = 0;

    LoadPriority priority;
    bool deletable = true; // Implicitly set to false as long as it is acquired by the AsyncLoaderMgr
    bool autodelete = false;
};

#endif /* end of include guard: ASYNC_BUILDER_HPP_ */
