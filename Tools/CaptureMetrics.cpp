/*
** EntityCore
** C++ Tools - CaptureMetrics
** File description:
** Light performance capture tool, allow complete performance analysis
** Designed to help tracking performance issues with as many informations as possible at a very low performance cost
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/
#include "CaptureMetrics.hpp"
#include <iostream>

CaptureMetrics::CaptureMetrics(const std::string &filename, const std::vector<std::string> &names) : filename(filename), names(names), pos(0)
{
}

CaptureMetrics::~CaptureMetrics()
{
    if (pos & CAPTURE_METRICS_SUBMASK)
        file.write((char *) ((pos & CAPTURE_METRICS_BUFFER_SIZE) ? buffer : buffer + CAPTURE_METRICS_BUFFER_SIZE), (pos & CAPTURE_METRICS_SUBMASK) * sizeof(TimePoint));
}

void CaptureMetrics::startCapture()
{
    if (!file.is_open()) {
        file.open(filename, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
        if (file) {
            std::cout << "Starting capture \"" << filename << "\"\n";
        } else {
            std::cout << "Failed to open \"" << filename << "\" to capture metrics\n";
        }
        origin = std::chrono::steady_clock::now();
    }
    active = file.good();
}

void CaptureMetrics::stopCapture()
{
    active = false;
}

void CaptureMetrics::capture(int type)
{
    if (active) {
        auto current = std::chrono::steady_clock::now();
        int p = ++pos & CAPTURE_METRICS_MASK;
        buffer[p - 1] = {type, std::chrono::duration_cast<std::chrono::duration<float>>(current - origin).count()};
        if (type == 0)
            origin = current;
        if (!(p & CAPTURE_METRICS_SUBMASK)) {
            file.write((char *) ((p & CAPTURE_METRICS_BUFFER_SIZE) ? buffer : buffer + CAPTURE_METRICS_BUFFER_SIZE), CAPTURE_METRICS_BUFFER_SIZE * sizeof(TimePoint));
        }
    }
}

void CaptureMetrics::analyze()
{
    std::ifstream file(filename, std::ifstream::binary);
    file.seekg(0, file.end);
    points.resize(file.tellg() / sizeof(TimePoint));
    file.seekg(0, file.beg);
    file.read((char *) points.data(), points.size());
    file.close();
    // Analyze frames
    for (auto &p : points) {
        if (p.type) {
            ++frames.back().size;
            continue;
        }
        frames.push_back({&p, 1});
    }
    // Analyze sequences
    SequenceMetric sequence {{}, &frames.front(), 0};
    for (int i = 0; i < frames.front().size; ++i)
        sequence.types.push_back(frames.front().ptr[i].type);
    for (auto &f : frames) {
        bool diff = sequence.ptr->size != f.size;
        if (!diff) {
            for (int i = 0; i < f.size; ++i)
                diff |= sequence.types[i] != f.ptr[i].type;
        }
        if (diff) {
            sequences.push_back(sequence);
            sequence.types.resize(0);
            sequence.ptr = &f;
            sequence.size = 0;
            for (int i = 0; i < f.size; ++i)
                sequence.types.push_back(f.ptr[i].type);
        }
        ++sequence.size;
    }
    sequences.push_back(sequence);
    // Perform sequence statistics
    for (auto &s : sequences) {
        SequenceStatistics ss;
        ss.metrics = &s;
        ss.markStat.resize(s.types.size());
        ss.markStat[0].minRel = 0;
        for (int i = 0; i < s.size; ++i) {
            float value = s.ptr[i].ptr[0].datetime;
            ss.markStat[0].type = s.types[0];
            ss.markStat[0].avr += value;
            if (ss.markStat[0].max < value)
                ss.markStat[0].max = value;
            if (ss.markStat[0].min > value)
                ss.markStat[0].min = value;
            float previous = 0;
            for (uint32_t j = 1; j < s.types.size(); ++j) {
                value = s.ptr[i].ptr[j].datetime;
                const float valueRel = value - previous;
                previous = value;
                ss.markStat[j].avr += value;
                if (ss.markStat[j].max < value)
                    ss.markStat[j].max = value;
                if (ss.markStat[j].min > value)
                    ss.markStat[j].min = value;
                ss.markStat[j].avrRel += valueRel;
                if (ss.markStat[j].maxRel < valueRel)
                    ss.markStat[j].maxRel = valueRel;
                if (ss.markStat[j].minRel > valueRel)
                    ss.markStat[j].minRel = valueRel;
            }
        }
        for (uint32_t j = 1; j < s.types.size(); ++j) {
            ss.markStat[j].type = s.types[j];
            ss.markStat[j].avr /= s.size;
            ss.markStat[j].avrRel /= s.size;
        }
        statistics.push_back(ss);
    }
}

void CaptureMetrics::display()
{
    std::cout << "========== CAPTURE METRICS ==========";
    for (auto &s : statistics) {
        std::cout << "\nSequence of " << s.metrics->size << " frames\n";
        std::cout << "minRel\tmaxRel\tavrRel\tmin\tmax\tavr\tname\n";
        for (auto &m : s.markStat) {
            std::cout << (int) (m.minRel * 1000000) << '\t' << (int) (m.maxRel * 1000000) << '\t' << (int) (m.avrRel * 1000000) << '\t' << (int) (m.min * 1000000) << '\t' << (int) (m.max * 1000000) << '\t' << (int) (m.avr * 1000000) << '\t' << names[m.type] << '\n';
        }
    }
    std::cout << "=====================================\n";
}
