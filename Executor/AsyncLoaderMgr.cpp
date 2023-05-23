#include "AsyncLoaderMgr.hpp"
#include "AsyncLoader.hpp"

#ifdef __linux__
#include <unistd.h>
#include <fcntl.h>
#endif

AsyncLoaderMgr *AsyncLoaderMgr::instance = nullptr;

AsyncLoaderMgr::AsyncLoaderMgr(const std::filesystem::path &dataPath, const std::filesystem::path &cachePath) :
    dataPath(dataPath), cachePath(cachePath/"data")
{
    if (!std::filesystem::exists(this->cachePath))
        std::filesystem::create_directories(this->cachePath);
    assert(!instance);
    instance = this;
    sd.open(cachePath.string());
    thread = std::thread(&AsyncLoaderMgr::threadloop, this);
}

AsyncLoaderMgr::~AsyncLoaderMgr()
{
    alive = false;
    cv.notify_all();
    thread.join();
    for (auto task : loaders)
        task->priority = LoadPriority::DONE;
    instance = nullptr;
}

void AsyncLoaderMgr::addTask(AsyncLoader *task)
{
    loaders.push_front(task);
    if (paused && task->priority > minPriority)
        cv.notify_one();
}

std::ofstream AsyncLoaderMgr::setBinCache(SaveData &cache)
{
   if (cache.get().size() <= 8) {
       cache.get().resize(16);
       reinterpret_cast<uint64_t*>(cache.get().data())[1] = sd.get<uint64_t>()++;
   }
   std::ofstream file(cachePath/std::to_string(reinterpret_cast<uint64_t *>(cache.get().data())[1]), std::ofstream::binary | std::ofstream::trunc);
   return file;
}

void AsyncLoaderMgr::update()
{
    if (loadersLock.test_and_set()) {
        for (auto task : loaders) {
            if (task->priority == LoadPriority::COMPLETED) {
                task->postLoad();
                task->priority = LoadPriority::DONE;
            }
        }
    } else {
        loaders.remove_if([](AsyncLoader *task){
            switch (task->priority) {
                case LoadPriority::COMPLETED:
                    task->postLoad();
                    task->priority = LoadPriority::DONE;
                    [[fallthrough]];
                case LoadPriority::DONE:
                    return true;
                default:
                    return false;
            }
        });
        loadersLock.clear();
    }
}

void AsyncLoaderMgr::threadloop()
{
    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);

    while (alive) {
        if (loadersLock.test_and_set()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        AsyncLoader *task = nullptr;
        LoadPriority taskPriority = minPriority;
        for (auto t : loaders) {
            if (t->priority > taskPriority) {
                taskPriority = t->priority;
                task = t;
            }
        }
        loadersLock.clear();
        if (task) {
            task->priority = LoadPriority::LOADING;
            auto &cache = getCache(task->source);
            if (cache.checkCache(task->source))
                task->generateCache(cache);
            if (cache.get().size() <= 8) {
                task->loadCache(cache, (AL_FILE) 0);
            } else {
                #ifdef __linux__
                int fd = open((cachePath/std::to_string(reinterpret_cast<uint64_t *>(cache.get().data())[1])).c_str(), task->once ? (O_RDONLY | O_DIRECT) : O_RDONLY, 0660);
                task->loadCache(cache, fd);
                close(fd);
                #else
                std::ifstream file(cachePath/std::to_string(reinterpret_cast<uint64_t *>(cache.get().data())[1]), std::ifstream::binary);
                task->loadCache(cache, &file);
                #endif
            }
            task->priority = LoadPriority::COMPLETED;
        } else {
            paused = true;
            cv.wait(lock);
            paused = false;
        }
    }
}
