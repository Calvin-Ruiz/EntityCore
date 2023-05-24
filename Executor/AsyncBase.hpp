#ifndef ASYNC_BASE_HPP_
#define ASYNC_BASE_HPP_

enum class LoadPriority : unsigned char {
    DONE, // Either completed or cancelled, it should be removed from the list
    COMPLETED, // It has completed loading
    LOADING, // It is currently loading
    LAZY, // Load in case it is used later
    BACKGROUND, // It may be needed quickly
    PRELOAD, // It will be needed rather quickly
    NOW, // It is currently needed
};

#endif /* end of include guard: ASYNC_BASE_HPP_ */
