/*
** EntityCore
** C++ Tools - LinuxExecutor
** File description:
** Linux-specific executor, simplify execution of a program and interactions with it
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/
#include "LinuxExecutor.hpp"
#include <cassert>

LinuxExecutor *LinuxExecutor::instance = nullptr;

#ifdef __linux__

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <alloca.h>
#include "StrPack.hpp"
#include "StaticLog.hpp"

// In case it wasn't defined
#ifndef PIPE_BUF
#define PIPE_BUF 4096
#endif

// Define alias
#define forkMutex requestMutex

LinuxExecutor::LinuxExecutor(int maxInputPipes, int maxOutputPipes)
{
    assert(maxInputPipes == 0);
    assert(maxOutputPipes == 0);
}

LinuxExecutor::~LinuxExecutor()
{
    if (instance && pid) {
        int status;
        if (!properlyClosed)
            kill(pid, SIGTERM);
        waitpid(pid, &status, 0);
        instance = nullptr;
    }
}

void LinuxExecutor::start(bool fortifyCurrent)
{
    assert(instance == nullptr);
    alive = true;
    int pipes[4];
    if (pipe(pipes) != 0)
        throw std::system_error(errno, std::system_category(), "pipe");
    if (pipe(pipes + 2) != 0)
        throw std::system_error(errno, std::system_category(), "pipe");
    processCount = 0;
    pid = fork();
    if (pid == 0) {
        // Internal side
        pipeResponse = pipes[0];
        ::close(pipes[1]);
        ::close(pipes[2]);
        pipeRequest = pipes[3];
        mainloop();
        exit(0);
    }
    // External side
    ::close(pipes[0]);
    pipeRequest = pipes[1];
    pipeResponse = pipes[2];
    ::close(pipes[3]);
    instance = this;
    if (fortifyCurrent) {
        for (int fortifyPid = fork(); fortifyPid; fortifyPid = fork()) {
            int status;
            waitpid(fortifyPid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                exit(0);
        }
    }
    thread = std::thread(&LinuxExecutor::mainloopExt, this);
}

void LinuxExecutor::compileContext(const ExecutorInfo &info, std::vector<char> &compiled)
{
    SpawnHead head;
    StrPack<unsigned short> pack(compiled);
    head.nbArgs = 0;
    head.nbEnvs = 0;
    while (info.args[head.nbArgs] != nullptr)
        ++head.nbArgs;
    if (info.env) {
        while (info.env[head.nbEnvs] != nullptr)
            ++head.nbEnvs;
    }
    head.pipeInput = info.pipeInput;
    head.pipeOutput = info.pipeOutput;
    head.pushInput = info.pushInput;
    head.saveOutput = info.saveOutput;
    pack.push((char *) &head, sizeof(head));
    for (int i = 0; i < (int)head.nbArgs; ++i) {
        pack.push(info.args[i], strlen(info.args[i]) + 1);
    }
    for (int j = 0; j < (int)head.nbEnvs; ++j) {
        pack.push(info.env[j], strlen(info.env[j]) + 1);
    }
    if (info.pushInput)
        pack.push(info.pushedInput.str, info.pushedInput.size);
}

ExecutorContext *LinuxExecutor::create(const ExecutorInfo &info)
{
    auto context = new ExecutorContext;
    compileContext(info, context->initializer);
    context->pipeInput = info.pipeInput;
    context->pipeOutput = info.pipeOutput;
    return context;
}

void LinuxExecutor::rewriteContext(ExecutorContext *context, const ExecutorInfo &info)
{
    context->initializer.clear();
    compileContext(info, context->initializer);
    context->pipeInput = info.pipeInput;
    context->pipeOutput = info.pipeOutput;
}

ExecutorInstance *LinuxExecutor::spawnInstance(const ExecutorInfo &info, std::vector<char> &cache)
{
    requestMutex.lock();
    cache.clear();
    compileContext(info, cache);
    instances.emplace_front();
    auto &ei = instances.front();
    ei.it = instances.begin();
    *(void **) (cache.data() + sizeof(unsigned short)) = &ei;
    request(LEFlag::SPAWN, cache, false);
    requestMutex.unlock();
    return &ei;
}

ExecutorInstance *LinuxExecutor::spawnInstance(ExecutorContext *context)
{
    requestMutex.lock();
    instances.emplace_front();
    auto &ei = instances.front();
    ei.it = instances.begin();
    *(void **) (context->initializer.data() + sizeof(unsigned short)) = &ei;
    request(LEFlag::SPAWN, context->initializer, false);
    requestMutex.unlock();
    return &ei;
}

ExecutorInstance *LinuxExecutor::spawnInstance(ExecutorContext *context, const VString &pushInput)
{
    requestMutex.lock();
    StrPack<unsigned short> pack(context->initializer);
    pack.scan();
    pack.rewrite(pushInput.str, pushInput.size, pack.size() - 1);
    instances.emplace_front();
    auto &ei = instances.front();
    ei.it = instances.begin();
    *(void **) (context->initializer.data() + sizeof(unsigned short)) = &ei;
    request(LEFlag::SPAWN, context->initializer, false);
    requestMutex.unlock();
    return &ei;
}

void LinuxExecutor::waitInstance(ExecutorInstance *instance)
{
    while (!instance->started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Keep enough reactivity
    }
    instance->mtx.lock();
    instance->mtx.unlock();
}

void LinuxExecutor::closeInstance(ExecutorInstance *instance)
{
    if (instance->running) {
        instance->joinable = false;
        return;
    }
    if (instance->stdin != -1)
        ::close(instance->stdin);
    if (instance->stdout != -1)
        ::close(instance->stdout);
    instances.erase(instance->it);
}

void LinuxExecutor::discard(ExecutorContext *context)
{
    delete context;
}

void LinuxExecutor::close()
{
    request(LEFlag::CLOSE);
}

bool LinuxExecutor::closed()
{
    if (properlyClosed) {
        if (thread.joinable())
            thread.join();
        if (instance && pid) {
            int status;
            waitpid(pid, &status, 0);
            instance = nullptr;
        }
        return true;
    }
    return false;
}

void LinuxExecutor::execute(std::vector<char> &datas)
{
    StrPack<unsigned short> pack(datas);
    SpawnHead *head; // uninitialized
    ExecutorInstanceInternal instance;
    char **args;
    char **env = nullptr;
    int pipes[4];

    pack.pop((char *&) head); // -Wstrict-aliasing with head var
    args = (char **) alloca((head->nbArgs + 1) * sizeof(char *));
    for (int i = 0; i < (int)head->nbArgs; ++i) {
        pack.pop(args[i]);
    }
    args[head->nbArgs] = nullptr;
    if (head->nbEnvs) {
        env = (char **) alloca((head->nbEnvs + 1) * sizeof(char *));
        for (int i = 0; i < (int)head->nbEnvs; ++i) {
            pack.pop(env[i]);
        }
        env[head->nbEnvs] = nullptr;
    }
    instance.id = head->id;
    if (head->saveOutput) {
        if (pipe(pipes) != 0)
            throw std::system_error(errno, std::system_category(), "pipe");
        instance.stdout = pipes[0];
    }
    if (head->pushInput) {
        if (pipe(pipes + 2) != 0)
            throw std::system_error(errno, std::system_category(), "pipe");
        VString pushedData;
        pushedData.size = pack.pop(pushedData.str);
        if (pushedData.size > 0)
            if (write(pipes[3], pushedData.str, pushedData.size) == -1)
                throw std::system_error(errno, std::system_category(), "write");
        ::close(pipes[3]);
    }
    forkMutex.lock();
    instance.pid = fork();
    if (instance.pid) {
        // Forker side
        if (head->saveOutput)
            ::close(pipes[1]);
        if (head->pushInput)
            ::close(pipes[2]);
        instancesInternal.push_back(instance);
    } else {
        // Forked side
        if (head->saveOutput) {
            ::close(pipes[0]);
            dup2(pipes[1], 1);
        }
        if (head->pushInput) {
            dup2(pipes[2], 0);
        }
        ::close(pipeRequest);
        ::close(pipeResponse);
        alive = false;
        forkMutex.unlock();
        if (thread.joinable())
            thread.join();
        if (env) {
            execve(args[0], args, env);
        } else {
            execv(args[0], args);
        }
        exit(-2);
    }
    if (processCount == 0) {
        if (thread.joinable())
            thread.join();
        ++processCount;
        thread = std::thread(&LinuxExecutor::instanceMainloop, this);
    } else {
        ++processCount;
    }
    head = (SpawnHead *) head->id;
    datas.resize(sizeof(void *));
    *(void **) datas.data() = head;
    request(LEFlag::SPAWN, datas, false);
    forkMutex.unlock();
}

void LinuxExecutor::mainloopExt()
{
    std::vector<char> datas;
    while (alive) {
        switch (getRequest(datas)) {
            case LEFlag::SPAWN: {
                ExecutorInstance *ei = *(ExecutorInstance **) datas.data();
                ei->mtx.lock();
                ei->started = true;
                break;
            }
            case LEFlag::EXITED: {
                ExitData *data = (ExitData *) datas.data();
                if (!data->instance->joinable) {
                    data->instance->mtx.unlock();
                    if (data->instance->stdin != -1)
                        ::close(data->instance->stdin);
                    if (data->instance->stdout != -1)
                        ::close(data->instance->stdout);
                    instances.erase(data->instance->it);
                    break;
                }
                if (data->size) {
                    data->instance->output.resize(data->size);
                    std::memcpy(data->instance->output.data(), data->data, data->size);
                }
                data->instance->exitCode = data->exitCode;
                data->instance->exited = data->exited;
                data->instance->running = false;
                data->instance->mtx.unlock();
                break;
            }
            case LEFlag::CLOSE:
            case LEFlag::CLOSED:
                alive = false;
                properlyClosed = true;
                break;
            default:;
        }
    }
}

void LinuxExecutor::mainloop()
{
    std::vector<char> datas;
    while (alive) {
        switch (getRequest(datas)) {
            case LEFlag::SPAWN:
                execute(datas);
                break;
            case LEFlag::CLOSE:
                alive = false;
                break;
            case LEFlag::TERMINATE:
                return;
            default:;
        }
    }
    if (thread.joinable())
        thread.join();
    request(LEFlag::CLOSED);
}

void LinuxExecutor::instanceMainloop()
{
    int status;

    do {
        int pidHit = waitpid(-1, &status, 0);
        forkMutex.lock();
        if (alive == false) {
            forkMutex.unlock();
            return;
        }
        if (pidHit == pid) {
            // The fortified process has exited
        } else {
            // A child process has existed
            instancesInternal.remove_if([this, pidHit, status](auto &instance){
                if (instance.pid != pidHit)
                    return false;
                std::vector<char> datas(offsetof(ExitData, data));
                ExitData *edata = (ExitData *) datas.data();
                edata->instance = instance.id;
                edata->exited = WIFEXITED(status);
                edata->exitCode = (edata->exited) ? WEXITSTATUS(status) : WTERMSIG(status);
                if (instance.stdout != -1) {
                    int currentSize = datas.size();
                    int size;
                    do {
                        datas.resize(currentSize + PIPE_BUF);
                        size = read(instance.stdout, datas.data() + currentSize, PIPE_BUF);
                        currentSize += size;
                    } while (size == PIPE_BUF);
                    datas.resize(currentSize);
                    ((ExitData *) datas.data())->size = datas.size() - offsetof(ExitData, data);
                } else
                    edata->size = 0;
                request(LEFlag::EXITED, datas, false);
                return true;
            });
        }
        forkMutex.unlock();
    } while (--processCount);
}

void LinuxExecutor::request(LEFlag request, const std::vector<char> &datas, bool requestShouldLock)
{
    ExecutorMsgHead head = {datas.size(), request};

    if (requestShouldLock) {
        requestMutex.lock();
        (void) write(pipeRequest, (char *) &head, sizeof(head));
        (void) write(pipeRequest, datas.data(), head.size);
        requestMutex.unlock();
    } else {
        (void) write(pipeRequest, (char *) &head, sizeof(head));
        (void) write(pipeRequest, datas.data(), head.size);
    }
}

LEFlag LinuxExecutor::getRequest(std::vector<char> &datas)
{
    ExecutorMsgHead head;
    if (read(pipeResponse, (char *) &head, sizeof(head)) <= 0) {
        return LEFlag::CLOSE;
    }
    datas.resize(head.size);
    unsigned int readsize = read(pipeResponse, datas.data(), head.size);
    while (readsize < head.size) {
        readsize += read(pipeResponse, datas.data() + readsize, head.size - readsize);
    }
    return head.flag;
}

#else

#include <iostream>

LinuxExecutor::LinuxExecutor(int maxInputPipes, int maxOutputPipes) {(void) maxInputPipes; (void) maxOutputPipes;}
LinuxExecutor::~LinuxExecutor() {instance = nullptr;}
void LinuxExecutor::start(bool fortifyCurrent) {
    std::cerr << "LinuxExecutor only work under linux\n";
    instance = this;
}
ExecutorContext *LinuxExecutor::create(const ExecutorInfo &info) {return nullptr;}
void LinuxExecutor::rewriteContext(ExecutorContext *context, const ExecutorInfo &info) {}
ExecutorInstance *LinuxExecutor::spawnInstance(const ExecutorInfo &info, std::vector<char> &cache) {return new ExecutorInstance();}
ExecutorInstance *LinuxExecutor::spawnInstance(ExecutorContext *context) {return new ExecutorInstance();}
ExecutorInstance *LinuxExecutor::spawnInstance(ExecutorContext *context, const VString &pushInput) {return new ExecutorInstance();}
void LinuxExecutor::waitInstance(ExecutorInstance *instance) {}
void LinuxExecutor::closeInstance(ExecutorInstance *instance) {delete instance;}
void LinuxExecutor::discard(ExecutorContext *context) {}
void LinuxExecutor::close() {}
bool LinuxExecutor::closed() {return true;}
void LinuxExecutor::compileContext(const ExecutorInfo &info, std::vector<char> &compiled) {}

#endif
