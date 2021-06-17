/*
** EPITECH PROJECT, 2020
** EntityCore
** File description:
** SubmitEngine.hpp
*/

#ifndef SUBMITENGINE_HPP_
#define SUBMITENGINE_HPP_

// Transfer 1 (single-use) :
// Wait event 4
// Push new entities
// Signal event 1 after writing updates

// Compute 1 (reused) :
// Wait event 1 before reading entries or transfering data
// Reset event 1
// Querry player datas
// Dispatch collision (bullet/player look at block to damage)
// Barrier : wait health update before updating state
// Dispatch update (except collision test)
// Reset event 4
// Signal event 2 after writing player and output

// Transfer 2 (single-use) :
// Wait event 2
// Push new entities
// Signal event 3 after writing updates

// Compute 2 (reused) :
// Wait event 3 before reading entries or transfering data
// Reset event 3
// Querry player datas
// Dispatch collision (bullet/player look at block to damage)
// Barrier : wait health update before updating state
// Dispatch update (except collision test)
// Reset event 2 on compute stage
// Signal event 4 after writing player and output

// Local :
// Update game
// Record transfer due to tic update
// Wait event 4/2
// Read back changes
// Update player shield
// Write player changes
// Record transfer due to GPU event
// Submit transfer 1/2 and compute 1/2

#include <vulkan/vulkan.h>
#include <string>
#include <thread>

enum class RequestType : unsigned short {
    MULTI_BUFFER_CPY,
    BUFFER_CPY,
    BUFFER_TO_IMG,
    IMG_TO_BUFFER,
    SUBMIT
};

typedef union {
    RequestType type;
    struct {
        RequestType type;
        MultiBufferCpy *data;
    } multiBuffCpy;
    struct {
        RequestType type;
        BufferCpy *data;
    } buffCpy;
    struct {
        RequestType type;
        ImageCpy *data;
    } imgCpy;
} Request;

/*
** Handle frame submission
*/
class SubmitEngine {
public:
    SubmitEngine();
    virtual ~SubmitEngine();
    SubmitEngine(const SubmitEngine &cpy) = delete;
    SubmitEngine &operator=(const SubmitEngine &src) = delete;


private:
};

#endif /* SUBMITENGINE_HPP_ */
