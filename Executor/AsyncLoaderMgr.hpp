#ifndef ASYNC_LOADER_MGR_HPP_
#define ASYNC_LOADER_MGR_HPP_

#include "AsyncLoader.hpp"
#include "EntityCore/Tools/BigSave.hpp"
#include <filesystem>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <fstream>
#include <list>

class AsyncLoaderMgr {
public:
    AsyncLoaderMgr(const std::filesystem::path &dataPath, const std::filesystem::path &cachePath);
    ~AsyncLoaderMgr();

    void addTask(AsyncLoader *task);
    void update();
    inline void flush() {
        if (paused)
            cv.notify_one();
    }

    SaveData &getCache(const std::filesystem::path &source) {
        SaveData *ret = &sd;
        for (auto &p : source.lexically_relative(dataPath)) {
            ret = &((*ret)[p.string()]);
        }
        return *ret;
    }

    // Store binary datas to a binary cache associated to this cache entry
    std::ofstream setBinCache(SaveData &cache);

    // Highest priority of AsyncLoader to ignore
    LoadPriority minPriority = LoadPriority::BACKGROUND;
    static AsyncLoaderMgr *instance;
private:
    void threadloop();
    std::thread thread;
    std::list<AsyncLoader *> loaders;
    std::atomic_flag loadersLock; // Ensure the list is not modified while it is accessed
    const std::filesystem::path dataPath;
    const std::filesystem::path cachePath;
    std::condition_variable cv;
    BigSave sd;
    bool alive = true;
    bool paused = false;
};

#endif /* end of include guard: ASYNC_LOADER_MGR_HPP_ */
