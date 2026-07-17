#pragma once
#include "Task.hpp"

//#define TASKABLE_REFERENCE_COUNT
//#define TASKABLE_TASK_KEEP_REFERENCE

// A Taskable is an "effective thread": it serializes the Tasks submitted to it, so the
// state it protects is accessed sequentially-consistently regardless of which physical
// thread executes each task.
//
// Contract (promises the implementation relies on):
// - A submitted Task runs exactly once. Strict FIFO ordering per Taskable exists only
//   within [start(), endTask()); outside that window there is no guarantee at all.
// - start() returning does NOT mean the task is done: endTask(task) being called does.
//   The chain holds (later tasks wait) until then; no thread waits. Tasks compose: a
//   Task may hold another Task which calls the enclosing task's endTask.
// - No thread affinity is promised or required: a task executes on whichever thread
//   submitted it to an idle chain (execute), or completed its predecessor (endTask).
// - A Task must hold nothing with side effects past its endTask().
// - Memory validity: a Task's storage must remain valid until its successor is linked
//   or the chain empties - the successor's producer may write task->next AFTER
//   endTask(task) returned (marker window). Inert after endTask is NOT reclaimable.
//   Guarantee lifetime externally (e.g. Task embedded in a refcounted resource object)
//   or via a dedicated flag (not yet implemented).
// - scheduleExecution(task) == false means the task needs an external start: either the
//   chain was idle, or the finishing consumer passed the marker window. Both mean the
//   same thing: this Taskable must be registered wherever external starts happen
//   (exactly-once per idle->busy transition, by construction).
// - Synchronous completion nests: endTask called inside start() cascades on the stack,
//   so an uninterrupted chain of depth N recurses N deep (unless tail-call optimized).
//   Chains going idle resets this - prefer bounded bursts over never-idle chains.
class Taskable {
public:
    #ifdef TASKABLE_REFERENCE_COUNT
    inline Taskable *acquire() {
        refCount.fetch_add(1, std::memory_order_relaxed);
        return this;
    }
    inline void release() {
        if (refCount-- == 1)
            delete this;
    }
    #endif
    inline void execute(Task *task) {
        if (Task *last = lastTask.exchange(task, std::memory_order_acquire)) {
            if (last->next.exchange(task, std::memory_order_acq_rel))
                task->start(this);
        } else {
            #ifdef TASKABLE_TASK_KEEP_REFERENCE
            refCount.fetch_add(1, std::memory_order_relaxed);
            #endif
            task->start(this);
        }
    }
    inline bool scheduleExecution(Task *task) {
        if (Task *last = lastTask.exchange(task, std::memory_order_acquire)) {
            return (last->next.exchange(task, std::memory_order_acq_rel) == nullptr);
        }
        return false;
    }
    inline void endTask(Task *task) {
        if (lastTask.compare_exchange_strong(task, nullptr, std::memory_order_release, std::memory_order_relaxed)) {
            #ifdef TASKABLE_TASK_KEEP_REFERENCE
            release();
            #endif
        } else {
            if (Task *next = task->next.exchange(task, std::memory_order_acq_rel)) {
                next->start(this);
            }
        }
    }
private:
    std::atomic<Task*> lastTask = nullptr;
    #ifdef TASKABLE_REFERENCE_COUNT
    std::atomic<int> refCount = 0;
    #endif
};
