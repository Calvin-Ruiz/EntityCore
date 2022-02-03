#define ENABLE_CONCURRENCY_TRACKING
#include "ConcurrencyTracker.hpp"
#include <iostream>

int TrackPoint::fences[NB_THREAD_FENCES] {}

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
    int owner = fences[fence].exchange(std::this_thread::get_id());
    if (owner)
        concurrencyAlert(fence, owner);
}

void TrackPoint::end(int fence)
{
    fences[fence] = 0;
}

void TrackPoint::concurrencyAlert(int fence, int owner)
{
    std::cerr << "Access to fence " << fence << " by thread " << std::this_thread::get_id() << " while in use by thread " << owner << "\n";
}
