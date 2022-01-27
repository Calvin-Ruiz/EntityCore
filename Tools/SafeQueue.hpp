/*
** EPITECH PROJECT, 2020
** G-JAM-001-MAR-0-2-jam-killian.moudery
** File description:
** SafeQueue.hpp
*/

#ifndef SAFEQUEUE_HPP_
#define SAFEQUEUE_HPP_

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <cstring>

// For thread-safe queue for insertion, the following data race might occur :
// If an insertion operation start
// A second insertion operation start and complete
// Before the first insertion operation complete, a pop operation start
// Then, the pop operation will move the non-inserted or partially-inserted element of the first insertion operation

// Thread-safe queue for insertion with blocking pop operation (maximal capacity of 65535 elements)
template<class T, unsigned short capacity = 255>
class WorkQueue {
public:
    WorkQueue() : writeIdx(0), count(0), vcount(0) {}
    ~WorkQueue() = default;
    // Insert element to thread-safe queue, return true on success
    bool push(T &data) {
        if (++vcount > capacity) {
            --vcount;
            return false;
        }
        datas[++writeIdx % (capacity + 1)] = std::move(data);
        if (!count++)
            cv.notify_one();
        return true;
    }
    bool emplace(const T &data) {
        if (++vcount > capacity) {
            --vcount;
            return false;
        }
        datas[++writeIdx % (capacity + 1)] = data;
        if (!count++)
            cv.notify_one();
        return true;
    }
    bool pushRaw(const void *data) {
        if (++vcount > capacity) {
            --vcount;
            return false;
        }
        std::memcpy(datas[++writeIdx % (capacity + 1)], data, sizeof(T));
        if (!count++)
            cv.notify_one();
        return true;
    }
    // Extract element from queue, return true on success, or false on failure if non-blocking
    bool pop(T &data) {
        while (blocking) {
            if (count)
                goto EXTRACT;
            cv.wait(lock);
        }
        if (!count)
            return false;
        EXTRACT:
        data = std::move(datas[++readIdx % (capacity + 1)]);
        --vcount;
        --count;
        return true;
    }
    // Extract element address from queue valid until next call to popRaw
    // return true on success, or false on failure if non-blocking
    bool popRaw(void *&data) {
        while (blocking) {
            if (count)
                goto EXTRACT;
            cv.wait(lock);
        }
        if (!count)
            return false;
        EXTRACT:
        data = &datas[++readIdx % (capacity + 1)];
        --vcount;
        --count;
        return true;
    }
    unsigned char size() const {
        return count;
    }
    bool empty() const {
        return count == 0;
    }
    // Acquire ownership of this queue by this thread for pop operations
    void acquire() {
        lock = std::unique_lock<std::mutex>(mtx);
        blocking = true;
    }
    // Release ownership of this queue by this thread for pop operations
    void release() {
        lock.release();
        mtx.unlock();
        blocking = false;
    }
    // Enfore the worker thread to check for work
    void flush() {
        cv.notify_one();
    }
    // Make the pop call non-blocking, mostly usefull to close a worker thread
    void close() {
        blocking = false;
        cv.notify_one();
    }
    // Wait for the queue to complete operations, or wait for .release()
    void waitIdle() {
        mtx.lock();
        mtx.unlock();
    }
    // Interrupt operations on this worker thread
    // Return once the worker thread has completed all his tasks
    void interrupt() {
        mtx.lock();
    }
    // Resume operations on this worker thread
    void resume() {
        mtx.unlock();
    }
    // Abort every pending execution. Mustn't be called concurrently to push/pop operations
    void abortPending() {
        count = 0;
        vcount = 0;
        readIdx = writeIdx;
    }
    static unsigned char *trace(void *data, unsigned char *buffer) {
        return reinterpret_cast<WorkQueue *>(data)->traceInternal(buffer);
    }
    unsigned char *traceInternal(unsigned char *buffer) {
        memcpy(buffer, "{size:", 6);
        buffer += 6;
        traceNbr(count, buffer);
        *(buffer++) = '/';
        traceNbr(capacity, buffer);
        memcpy(buffer, ", rd:", 5);
        buffer += 5;
        traceNbr(readIdx, buffer);
        memcpy(buffer, ", wr:", 5);
        buffer += 5;
        traceNbr(writeIdx, buffer);
        *(buffer++) = '}';
        return buffer;
    }
    // Call this function at least once every 65535 push operations if (capacity + 1) is not a power-of-two
    void fixNonPowerOfTwo() {
        readIdx -= readIdx / (capacity + nbWorker) * (capacity + nbWorker);
        writeIdx -= writeIdx / (capacity + nbWorker) * (capacity + nbWorker);
    }
private:
    void traceNbr(unsigned short value, unsigned char *&buffer) {
        if (value > 9)
            traceNbr(value / 10, buffer);
        *(buffer++) = '0' + value % 10;
    }
    T datas[capacity + 1];
    unsigned short readIdx = 0;
    std::atomic<unsigned short> writeIdx;
    std::atomic<unsigned short> count;
    std::atomic<unsigned int> vcount;
    bool blocking = false;
    std::condition_variable cv;
    std::mutex mtx;
    std::unique_lock<std::mutex> lock;
};

// Mostly thread-safe queue for insertion (maximal capacity of 65535 elements)
template<class T, unsigned short capacity = 255>
class PushQueue {
public:
    PushQueue() : writeIdx(0), count(0), vcount(0) {}
    ~PushQueue() = default;
    // Insert element to thread-safe queue, return true on success
    bool push(T &data) {
        if (++vcount > capacity) {
            --vcount;
            return false;
        }
        datas[++writeIdx % (capacity + 1)] = std::move(data);
        ++count;
        return true;
    }
    bool pushRaw(const void *data) {
        if (++vcount > capacity) {
            --vcount;
            return false;
        }
        std::memcpy(datas[++writeIdx % (capacity + 1)], data, sizeof(T));
        ++count;
        return true;
    }
    bool emplace(T data) {
        return push(data);
    }
    // Extract element from queue, return true on success
    bool pop(T &data) {
        if (count) {
            data = std::move(datas[++readIdx % (capacity + 1)]);
            --count;
            --vcount;
            return true;
        }
        return false;
    }
    // Extract element address from queue valid until next call to popRaw
    // return true on success, false on failure
    bool popRaw(void *&data) {
        if (count) {
            data = &datas[++readIdx % (capacity + 1)];
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
    static unsigned char *trace(void *data, unsigned char *buffer) {
        return reinterpret_cast<WorkQueue<T, capacity> *>(data)->traceInternal(buffer);
    }
    unsigned char *traceInternal(unsigned char *buffer) {
        memcpy(buffer, "{size:", 6);
        buffer += 6;
        traceNbr(count, buffer);
        *(buffer++) = '/';
        traceNbr(capacity, buffer);
        memcpy(buffer, ", rd:", 5);
        buffer += 5;
        traceNbr(readIdx, buffer);
        memcpy(buffer, ", wr:", 5);
        buffer += 5;
        traceNbr(writeIdx, buffer);
        *(buffer++) = '}';
        return buffer;
    }
    // Call this function at least once every 65535 push operations if (capacity + 1) is not a power-of-two
    void fixNonPowerOfTwo() {
        readIdx -= readIdx / (capacity + nbWorker) * (capacity + nbWorker);
        writeIdx -= writeIdx / (capacity + nbWorker) * (capacity + nbWorker);
    }
private:
    void traceNbr(unsigned short value, unsigned char *&buffer) {
        if (value > 9)
            traceNbr(value / 10, buffer);
        *(buffer++) = '0' + value % 10;
    }
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
        datas[++writeIdx % (capacity + 1)] = std::move(data);
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
            data = std::move(datas[++readIdx % (capacity + 1)]);
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
    static unsigned char *trace(void *data, unsigned char *buffer) {
        return reinterpret_cast<WorkQueue<T, capacity> *>(data)->traceInternal(buffer);
    }
    unsigned char *traceInternal(unsigned char *buffer) {
        memcpy(buffer, "{size:", 6);
        buffer += 6;
        traceNbr(count, buffer);
        *(buffer++) = '/';
        traceNbr(capacity, buffer);
        memcpy(buffer, ", rd:", 5);
        buffer += 5;
        traceNbr(readIdx, buffer);
        memcpy(buffer, ", wr:", 5);
        buffer += 5;
        traceNbr(writeIdx, buffer);
        *(buffer++) = '}';
        return buffer;
    }
    // Call this function at least once every 65535 push operations if (capacity + 1) is not a power-of-two
    void fixNonPowerOfTwo() {
        readIdx -= readIdx / (capacity + nbWorker) * (capacity + nbWorker);
        writeIdx -= writeIdx / (capacity + nbWorker) * (capacity + nbWorker);
    }
private:
    void traceNbr(unsigned short value, unsigned char *&buffer) {
        if (value > 9)
            traceNbr(value / 10, buffer);
        *(buffer++) = '0' + value % 10;
    }
    T datas[capacity + 1];
    std::atomic<unsigned short> readIdx;
    unsigned short writeIdx;
    std::atomic<unsigned short> count;
    std::atomic<int> vcount;
};

template<class T, unsigned char nbWorker, unsigned short capacity>
class DispatchOutQueue;


// Thread-safe blocking-pop queue for work dispatch (maximal capacity of 65536 - nbWorker elements)
template<class T, unsigned char nbWorker = 1, unsigned short capacity = 255>
class DispatchInQueue {
    friend class DispatchOutQueue<T, nbWorker, capacity>;
public:
    DispatchInQueue() : readIdx(0), writeIdx(0), count(0), vcount(0) {}
    ~DispatchInQueue() = default;
    // Insert element to queue, return true on success
    bool push(T &data) {
        if (count >= capacity)
            return false;
        datas[++writeIdx % (capacity + nbWorker)] = std::move(data);
        ++count;
        ++vcount;
        cv.notify_one();
        return true;
    }
    // Insert element to queue, return true on success
    bool emplace(const T &data) {
        if (count >= capacity)
            return false;
        datas[++writeIdx % (capacity + nbWorker)] = data;
        ++count;
        ++vcount;
        cv.notify_one();
        return true;
    }
    unsigned char size() const {
        return count;
    }
    bool empty() const {
        return count == 0;
    }
    void close() {
        blocking = false;
        cv.notify_all();
    }
    void reopen() {
        blocking = true;
    }
    void flush() {
        cv.notify_all();
    }
    static unsigned char *trace(void *data, unsigned char *buffer) {
        return reinterpret_cast<WorkQueue<T, capacity> *>(data)->traceInternal(buffer);
    }
    unsigned char *traceInternal(unsigned char *buffer) {
        memcpy(buffer, "{size:", 6);
        buffer += 6;
        traceNbr(count, buffer);
        *(buffer++) = '/';
        traceNbr(capacity, buffer);
        memcpy(buffer, ", rd:", 5);
        buffer += 5;
        traceNbr(readIdx, buffer);
        memcpy(buffer, ", wr:", 5);
        buffer += 5;
        traceNbr(writeIdx, buffer);
        *(buffer++) = '}';
        return buffer;
    }
    // Call this function at least once every 65535 push operations if (capacity + nbWorker) is not a power-of-two
    void fixNonPowerOfTwo() {
        readIdx -= readIdx / (capacity + nbWorker) * (capacity + nbWorker);
        writeIdx -= writeIdx / (capacity + nbWorker) * (capacity + nbWorker);
    }
private:
    void traceNbr(unsigned short value, unsigned char *&buffer) {
        if (value > 9)
            traceNbr(value / 10, buffer);
        *(buffer++) = '0' + value % 10;
    }
    T datas[capacity + nbWorker];
    std::atomic<unsigned short> readIdx;
    unsigned short writeIdx;
    std::atomic<unsigned short> count;
    std::atomic<int> vcount;
    std::condition_variable cv;
    bool blocking = true;
};

// Thread-safe blocking-pop queue for work dispatch (worker thread) (maximal capacity of 65536 - nbWorker elements)
template<class T, unsigned char nbWorker = 1, unsigned short capacity = 255>
class DispatchOutQueue {
public:
    DispatchOutQueue(DispatchInQueue<T, nbWorker, capacity> &master) : master(master) {}
    ~DispatchOutQueue() = default;
    // Extract element from queue, return true on success, or false on failure if non-blocking
    bool pop(T &data) {
        BEGIN:
        if (--master.vcount >= 0) {
            data = std::move(master.datas[++master.readIdx % (capacity + nbWorker)]);
            --master.count;
            return true;
        }
        ++master.vcount;
        if (!master.blocking)
            return false;
        master.cv.wait(lock);
        goto BEGIN;
    }
    // Wait for the queue to complete operations, or wait for .release()
    void waitIdle() {
        mtx.lock();
        mtx.unlock();
    }
    // Acquire ownership of this queue by this thread for pop operations
    void acquire() {
        lock = std::unique_lock<std::mutex>(mtx);
    }
    // Release ownership of this queue by this thread for pop operations
    void release() {
        lock.release();
        mtx.unlock();
    }
    // Interrupt operations on this worker thread
    // Return once the worker thread has completed all his tasks
    void interrupt() {
        mtx.lock();
    }
    // Resume operations on this worker thread
    void resume() {
        mtx.unlock();
    }
private:
    DispatchInQueue<T, nbWorker, capacity> &master;
    std::mutex mtx;
    std::unique_lock<std::mutex> lock;
};

#endif /* SAFEQUEUE_HPP_ */
