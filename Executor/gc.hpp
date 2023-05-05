#ifndef GC_HPP_
#define GC_HPP_

#include <chrono>

class GCObject;

// Garbage Collector manager
namespace gc {
    void start();
    void stop();
    void waitStopped();
    void load(GCObject *obj);
    extern unsigned int cycle;
    extern unsigned int collectedCycle;
    extern unsigned int preservationCycles; // Number of collection cycles before unloading
    extern std::chrono::milliseconds collectionDeltaTime; // Time between unloading cycles
    extern std::chrono::milliseconds loadingDeltaTime; // Time between loading cycles
};

// Garbage Collectable object, call load() and unload()
class GCObject {
public:
    virtual ~GCObject() = default;
    virtual void load() = 0;
    virtual void unload() = 0;
    bool used() { // Load or keep this object loaded, return true if this object is currently loaded
        if (lastUse > gc::collectedCycle) {
            lastUse = gc::cycle;
            return loaded;
        }
        if (deletable) {
            lastUse = gc::cycle;
            loaded = false;
            deletable = false;
            gc::load(this);
        }
        return false;
    }
    void unused() { // Unload this object in the next GC cycle
        lastUse = 0;
    }
    void destroy() { // Delete this object
        if (deletable) {
            delete this;
        } else {
            deletable = true;
            lastUse = 0;
        }
    }
    // Internal state - accessed in gc.cpp
    unsigned int lastUse = 0;
    bool loaded = false; // Indicate if this object is currently loaded
    bool deletable = true; // MAYBE switch to atomic_flag
};

// For class deriving from GCObject
template <class T>
class gc_ptr {
public:
    gc_ptr() : ptr(new T()) {
    }
    gc_ptr(T *ptr) : ptr(ptr) {
    }
    ~gc_ptr() {
        ptr->destroy();
    }
    operator bool() const {
        return ptr->used();
    }
    inline T &operator*() {
        return *ptr;
    }
    inline T *operator->() {
        return ptr;
    }
private:
    T *ptr;
};

#endif /* end of include guard: GC_HPP_ */
