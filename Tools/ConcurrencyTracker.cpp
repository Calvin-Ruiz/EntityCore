#define ENABLE_CONCURRENCY_TRACKING
#include "ConcurrencyTracker.hpp"
#include <iostream>

std::atomic<int> TrackPoint::fences[NB_THREAD_FENCES] {}
std::atomic<int> TrackPoint::useCount[NB_THREAD_FENCES] {}

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
    const int me = std::this_thread::get_id();
    int owner = fences[fence].exchange(me);
    ++useCount[fence];
    if (owner != 0 && owner != me)
        concurrencyAlert(fence, owner);
}

void TrackPoint::end(int fence)
{
    if (--useCount[fence] == 0)
        fences[fence] = 0;
}

void TrackPoint::concurrencyAlert(int fence, int owner)
{
    std::cerr << "Access to fence " << fence << " by thread " << std::this_thread::get_id() << " while in use by thread " << owner << "\n";
}
