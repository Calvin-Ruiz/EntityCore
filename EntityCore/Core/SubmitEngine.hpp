/*
** EPITECH PROJECT, 2020
** EntityCore
** File description:
** SubmitEngine.hpp
*/

#ifndef SUBMITENGINE_HPP_
#define SUBMITENGINE_HPP_

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
