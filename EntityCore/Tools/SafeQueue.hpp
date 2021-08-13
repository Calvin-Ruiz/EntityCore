/*
** EPITECH PROJECT, 2020
** G-JAM-001-MAR-0-2-jam-killian.moudery
** File description:
** SafeQueue.hpp
*/

#ifndef SAFEQUEUE_HPP_
#define SAFEQUEUE_HPP_

#include <atomic>

// Thread-safe queue for insertion (maximal capacity of 65535 elements, must be a power-of-two - 1)
template<class T, unsigned short capacity = 255>
class PushQueue {
public:
    PushQueue() : writeIdx(0), count(0), vcount(0) {}
    ~PushQueue() = default;
    // Insert element to thread-safe queue, return true on success
    bool push(T &data) {
        if (++vcount >= capacity) {
            --vcount;
            return false;
        }
        datas[++writeIdx & capacity] = std::move(data);
        ++count;
        return true;
    }
    bool emplace(T data) {
        return push(data);
    }
    // Extract element from queue, return true on success
    bool pop(T &data) {
        if (count) {
            data = std::move(datas[++readIdx & capacity]);
            --count;
            --vcount;
            return true;
        }
        return false;
    }
    unsigned char size() const {
        return count;
    }
    bool empty() const {
        return count == 0;
    }
private:
    T datas[capacity + 1];
    unsigned short readIdx = 0;
    std::atomic<unsigned short> writeIdx;
    std::atomic<unsigned short> count;
    std::atomic<unsigned int> vcount;
};

// Thread-safe queue for extraction (maximal capacity of 65535 elements)
template<class T, unsigned short capacity = 255>
class PopQueue {
public:
    PopQueue() : readIdx(0), writeIdx(0), count(0), vcount(0) {}
    ~PopQueue() = default;
    // Insert element to queue, return true on success
    bool push(T &data) {
        if (count >= capacity)
            return false;
        datas[++writeIdx & capacity] = std::move(data);
        ++count;
        ++vcount;
        return true;
    }
    bool emplace(T data) {
        return push(data);
    }
    // Extract element from thread-safe queue, return true on success
    bool pop(T &data) {
        if (--vcount >= 0) {
            data = std::move(datas[++readIdx & capacity]);
            --count;
            return true;
        }
        ++vcount;
        return false;
    }
    unsigned char size() const {
        return count;
    }
    bool empty() const {
        return count == 0;
    }
private:
    T datas[capacity + 1];
    std::atomic<unsigned short> readIdx;
    unsigned short writeIdx;
    std::atomic<unsigned short> count;
    std::atomic<int> vcount;
};

#endif /* SAFEQUEUE_HPP_ */
