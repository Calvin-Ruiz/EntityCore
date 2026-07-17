#pragma once

#include <atomic>

class Taskable;
// A unit of work for a Taskable chain. start() returning does not imply completion:
// the task is done when Taskable::endTask(this) is called, from any thread.
// See the Taskable contract for ordering, lifetime and memory-validity promises.
class Task {
    friend class Taskable;
public:
    virtual void start(Taskable *target) = 0;
private:
    std::atomic<Task *> next = nullptr;
};
