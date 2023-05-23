#ifndef ASYNC_LOADER_HPP_
#define ASYNC_LOADER_HPP_

#include <filesystem>

class SaveData;

#ifdef __linux__
#define AL_FILE int
#define AL_READ(file, buffer, size) read(file, buffer, size)
#else
#define AL_FILE std::ifstream*
#define AL_READ(file, buffer, size) file->read(buffer, size)
#endif

enum class LoadPriority : unsigned char {
    DONE, // Either completed or cancelled, it should be removed from the list
    COMPLETED, // It has completed loading
    LOADING, // It is currently loading
    LAZY, // Load in case it is used later
    BACKGROUND, // It may be needed quickly
    PRELOAD, // It will be needed rather quickly
    NOW, // It is currently needed
};

class AsyncLoader {
public:
    AsyncLoader(const std::filesystem::path &source, LoadPriority priority);
    virtual ~AsyncLoader();

    // Asynchronously generate cache from source file
    virtual void generateCache(SaveData &cache) = 0;
    // Asynchronously load datas from cache
    virtual void loadCache(SaveData &cache, AL_FILE binCache) = 0;
    // Synchronously finalize loading
    virtual void postLoad() {}

    const std::filesystem::path source;
    LoadPriority priority;
    bool once = true; // True if this file will be read only once with large reads
};

#endif /* end of include guard: ASYNC_LOADER_HPP_ */
