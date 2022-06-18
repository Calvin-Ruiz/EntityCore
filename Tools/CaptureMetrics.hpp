/*
** EntityCore
** C++ Tools - CaptureMetrics
** File description:
** Light performance capture tool, allow complete performance analysis
** Designed to help tracking performance issues with as many informations as possible at a very low performance cost
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/
#ifndef CAPTURE_METRICS_HPP_
#define CAPTURE_METRICS_HPP_

#include <chrono>
#include <fstream>
#include <atomic>
#include <vector>

// How many capture points must be backed together in writes. Must be a power-of-two.
#define CAPTURE_METRICS_BUFFER_SIZE 2048
#define CAPTURE_METRICS_MASK (CAPTURE_METRICS_BUFFER_SIZE * 2 - 1)
#define CAPTURE_METRICS_SUBMASK (CAPTURE_METRICS_BUFFER_SIZE - 1)

// Note : The first type is considered as the frame start delimiter
class CaptureMetrics {
public:
    CaptureMetrics(const std::string &filename, const std::vector<std::string> &names);
    ~CaptureMetrics();

    // ========== Capture metrics ========== //
    // Initialize/resume capture
    void startCapture();
    template <class EnumType>
    inline void capture(EnumType type) {capture((int) type);}
    // Capture a point, type is the position of the action description in the names vector
    // This have no effect if startCapture have not been called
    // The first and only the first capture of a frame MUST have a type of 0
    void capture(int type);
    // Interrupt capture
    void stopCapture();

    // ========== Internal structures ========== //
    struct TimePoint {
        int type;
        float datetime;
    };
    struct FrameMetric {
        TimePoint *ptr;
        int size;
    };
    struct SequenceMetric {
        std::vector<int> types;
        FrameMetric *ptr;
        int size;
    };
    struct MarkStatistics {
        int type = -1;
        float min = 1000;
        float max = 0;
        float avr = 0;
        float minRel = 1000;
        float maxRel = 0;
        float avrRel = 0;
    };
    struct SequenceStatistics {
        SequenceMetric *metrics;
        std::vector<MarkStatistics> markStat;
    };

    // ========== Analyze metrics ========== //
    // Note : Any content obtained from this class became undefined when this class is destroyed.
    // Perform an analyze.
    void analyze();
    // Get the frame metrics
    std::vector<FrameMetric> &getFrameMetrics() {return frames;}
    // Return the sequences statistics
    std::vector<SequenceStatistics> &getStatistics() {return statistics;}
    // Display statistics
    void display();
private:
    const std::string filename;
    const std::vector<std::string> names;

    // Capture metrics
    std::ofstream file;
    std::chrono::steady_clock::time_point origin;
    TimePoint buffer[CAPTURE_METRICS_BUFFER_SIZE * 2];
    std::atomic<uint16_t> pos;
    bool active = false;

    // Analyze metrics
    std::vector<TimePoint> points;
    std::vector<FrameMetric> frames;
    std::vector<SequenceMetric> sequences;
    std::vector<SequenceStatistics> statistics;
};

#endif /* end of include guard: CAPTURE_METRICS_HPP_ */
