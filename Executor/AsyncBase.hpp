#ifndef ASYNC_BASE_HPP_
#define ASYNC_BASE_HPP_

// Fused state+priority encoding: one atomic LoadPriority carries the whole task
// lifecycle. The first three values are STATES (a task holding one of them is no
// longer schedulable), the remaining ones are PRIORITIES (higher value = served
// first). This fusion is deliberate: a single atomic load answers both "is it
// done/loading?" and "how urgent is it?", and a single atomic store transitions
// state and deschedules at once. Comparisons rely on the declaration order:
// state values sort below every priority, so "priority > minPriority" naturally
// skips completed/loading entries.
// Lifecycle: {LAZY..ACTIVE} -> LOADING -> COMPLETED -> DONE.
// Re-prioritization is a plain atomic store while still in {LAZY..ACTIVE}.
enum class LoadPriority : unsigned char {
    DONE, // Either completed or cancelled, it should be removed from the list
    COMPLETED, // It has completed loading
    LOADING, // It is currently loading
    LAZY, // Load in case it is used later
    BACKGROUND, // It may be needed quickly
    PRELOAD, // It will be needed rather quickly
    ACTIVE, // It is currently needed (formerly NOW - renamed for ladder unification with ResourcePriority)
};

#endif /* end of include guard: ASYNC_BASE_HPP_ */
