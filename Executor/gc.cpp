#include "gc.hpp"
#include <thread>

unsigned int gc::preservationCycles = 2;
unsigned int gc::cycle = 256;
unsigned int gc::collectedCycle = 255;
std::chrono::milliseconds gc::collectionDeltaTime(500);
std::chrono::milliseconds gc::loadingDeltaTime(50);

static bool active = false;
static std::thread thread;
static GCObject *loadingObj[256]; // Objects to load
static GCObject *loadedObj[65536]; // Objects loaded
static unsigned char readIdx = 0;
static unsigned char writeIdx = 0;

static void mainloop() {
    auto nextCycle = std::chrono::steady_clock::now() + gc::loadingDeltaTime;
    auto nextCollection = nextCycle + gc::collectionDeltaTime;
    unsigned short loadedCount = 0;
    while (active) {
        // Process loading cycle
        while (readIdx != writeIdx) {
            GCObject *obj = loadingObj[readIdx++];
            obj->load();
            obj->loaded = true;
            loadedObj[loadedCount++] = obj;
        }
        // Process collection cycle
        if (nextCycle > nextCollection) {
            const unsigned int unloadCycle = ++gc::cycle - gc::preservationCycles;
            gc::collectedCycle = unloadCycle;
            GCObject **src = loadedObj;
            GCObject **dst = src;
            unsigned short i = loadedCount;
            while (i--) {
                if ((**src).lastUse <= unloadCycle) {
                    (**src).loaded = false;
                    (**src).unload();
                    if ((**src).deletable) {
                        delete *src;
                    } else {
                        (**src).deletable = true;
                    }
                } else {
                    *(dst++) = *src;
                }
                ++src;
            }
            loadedCount = dst - loadedObj;
            nextCollection = nextCycle + gc::collectionDeltaTime; // Avoid cycle spaced by less than collectionDeltaTime
        }
        // Wait for next cycle
        auto now = std::chrono::steady_clock::now();
        if (now < nextCycle) {
            std::this_thread::sleep_until(nextCycle);
            nextCycle += gc::loadingDeltaTime;
        } else {
            nextCycle = now + gc::loadingDeltaTime; // Avoid cycle spaced by less than loadingDeltaTime
        }
    }
    // Final collection cycle - unload everything
    while (readIdx != writeIdx) {
        GCObject *obj = loadingObj[readIdx++];
        if (obj->deletable) {
            delete obj;
        } else {
            obj->deletable = true;
        }
    }
    for (GCObject **src = loadedObj; loadedCount--; ++src) {
        (**src).unload();
        if ((**src).deletable) {
            delete *src;
        } else {
            (**src).deletable = true;
        }
    }
}

void gc::start() {
    if (!active) {
        active = true;
        if (thread.joinable())
            thread.join();
        thread = std::thread(mainloop);
    }
}

void gc::stop() {
    active = false;
}

void gc::waitStopped() {
    if (thread.joinable()) {
        thread.join();
    }
}

void gc::load(GCObject *obj) {
    static unsigned char writingIdx = 255;
    loadingObj[++writingIdx] = obj;
    ++writeIdx;
}
