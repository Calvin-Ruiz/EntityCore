#define ENABLE_CONCURRENCY_TRACKING
#include "ConcurrencyTracker.hpp"
#include <iostream>

std::atomic<std::thread::id> TrackPoint::fences[NB_THREAD_FENCES] {};
std::atomic<int> TrackPoint::useCount[NB_THREAD_FENCES] {};

TrackPoint::TrackPoint(int fence) : fence(fence)
{
    begin(fence);
}

TrackPoint::~TrackPoint()
{
    end(fence);
}

void TrackPoint::begin(int fence)
{
    const std::thread::id me = std::this_thread::get_id();
    std::thread::id owner = fences[fence].exchange(me);
    if (useCount[fence]++ && owner != me)
        concurrencyAlert(fence, owner);
}

void TrackPoint::end(int fence)
{
    --useCount[fence];
}

void TrackPoint::concurrencyAlert(int fence, std::thread::id owner)
{
    std::cerr << "Access to fence " << fence << " by thread " << std::this_thread::get_id() << " while in use by thread " << owner << "\n";
}
