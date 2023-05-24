#ifndef ASYNC_LOADER_MGR_HPP_
#define ASYNC_LOADER_MGR_HPP_

#include "AsyncBase.hpp"
#include "EntityCore/Tools/BigSave.hpp"
#include <filesystem>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <fstream>
#include <list>

class AsyncLoader;
class AsyncBuilder;

class AsyncLoaderMgr {
public:
    AsyncLoaderMgr(const std::filesystem::path &dataPath, const std::filesystem::path &cachePath);
    ~AsyncLoaderMgr();

    void startBuilders(int count) {
        alive = true;
        while (count--)
            threads.push_back(std::thread(&AsyncLoaderMgr::builderThreadLoop, this));
    }
    void startLoader() {
        alive = true;
        thread = std::thread(&AsyncLoaderMgr::threadloop, this);
    }
    void stop();

    void addLoad(AsyncLoader *task);
    void addBuild(AsyncBuilder *task);

    void update();
    inline void flush() {
        if (paused)
            cv.notify_one();
        cvBuilder.notify_all();
    }

    inline bool isLoaderIdle() const {
        return paused;
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
    void builderThreadLoop();
    std::thread thread; // Loader thread
    std::vector<std::thread> threads; // Builder threads
    std::list<AsyncLoader *> loaders;
    std::atomic_flag loadersLock; // Ensure the list is not modified while it is accessed
    std::condition_variable cv;
    const std::filesystem::path dataPath;
    const std::filesystem::path cachePath;
    std::list<AsyncBuilder *> builders;
    std::atomic_flag buildersLock; // Ensure the list is not modified while it is accessed
    std::condition_variable cvBuilder;
    BigSave sd;
    bool alive = true;
    bool paused = false;
};

#endif /* end of include guard: ASYNC_LOADER_MGR_HPP_ */
