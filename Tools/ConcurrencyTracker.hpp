/*
** EntityCore
** C++ Tools - ConcurrencyTracker
** File description:
** Tool designed to detect concurrent access to one or several section(s) of code which shouldn't be accessed concurently
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/
#ifndef CONCURRENCY_TRACKER_HPP
#define CONCURRENCY_TRACKER_HPP

#ifdef ENABLE_CONCURRENCY_TRACKING
#define NB_THREAD_FENCES 16

#include <thread>
#include <atomic>

class TrackPoint {
public:
    TrackPoint(int fence);
    ~TrackPoint();

    static void begin(int fence);
    static void end(int fence);
private:
    const int fence;
    static void concurrencyAlert(int fence, std::thread::id owner);
    static std::atomic<std::thread::id> fences[NB_THREAD_FENCES];
    static std::atomic<int> useCount[NB_THREAD_FENCES];
};

#define TRACK_HERE(fence) TrackPoint _tp(fence)
#define TRACK_BEG(fence) TrackPoint::begin(fence)
#define TRACK_END(fence) TrackPoint::end(fence)

#else
#define TRACK_HERE(fence)
#define TRACK_BEG(fence)
#define TRACK_END(fence)

#endif

#endif /* end of include guard: CONCURRENCY_TRACKER_HPP */
