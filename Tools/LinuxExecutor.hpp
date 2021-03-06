/*
** EntityCore
** C++ Tools - LinuxExecutor
** File description:
** Linux-specific executor, simplify execution of a program and interactions with it
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/

#ifndef LINUXEXECUTOR_HPP_
#define LINUXEXECUTOR_HPP_

#include <string>
#include <memory>
#include <list>
#include <vector>
#include <thread>
#include <mutex>
#include "VString.hpp"

// Note : direct pipes (pipeInput/pipeOutput) is not supported yet

enum class LEFlag : unsigned char {
    SPAWN,
    CLOSE,
    TERMINATE,
    EXITED, // A process has exited
    CLOSED,
};

struct ExecutorInfo {
    const char **args = nullptr;
    const char **env = nullptr;
    VString pushedInput;
    bool pipeInput = false;
    bool pipeOutput = false;
    // Push an input as stdin
    bool pushInput = false;
    // Safe output and write it in ExecutorInstance::output when passed to LinuxExecutor::waitInstance
    bool saveOutput = false;
};

struct ExecutorContext {
    // Variables below are implementation-defined
    std::vector<char> initializer;
    bool pipeInput;
    bool pipeOutput;
};

// Patch for Visual Studio under Windows
#undef stdin
#undef stdout

struct ExecutorInstance {
    std::vector<char> output; // Reading this is unsafe and undefined until passed to LinuxExecutor::waitInstance
    int stdin = -1;
    int stdout = -1;
    char exitCode = -1;
    bool exited = false; // True if exited, false if killed
    // Variables below are implementation-defined
    std::list<ExecutorInstance>::iterator it;
    std::mutex mtx;
    bool running = true;
    bool joinable = true;
    bool started = false;
};

class LinuxExecutor {
public:
    LinuxExecutor(int maxInputPipes, int maxOutputPipes);
    ~LinuxExecutor();

    // Start the Linux Executor
    // If fortifyCurrent is true, the child process will execute the code after this call and restart on crash
    // if fortifyCurrent is false, the main process will execute the code after this call
    void start(bool fortifyCurrent);
    static LinuxExecutor *instance;
    // Create an executor context
    ExecutorContext *create(const ExecutorInfo &info);
    // Rewrite an executor context
    void rewriteContext(ExecutorContext *context, const ExecutorInfo &info);
    // Spawn an instance with a context created on the fly, the cache should be preserved across calls
    ExecutorInstance *spawnInstance(const ExecutorInfo &info, std::vector<char> &cache);
    // Spawn an instance of the executor context
    ExecutorInstance *spawnInstance(ExecutorContext *context);
    // Spawn an instance of the executor context with modified push data
    ExecutorInstance *spawnInstance(ExecutorContext *context, const VString &pushInput);
    // Wait for the instance to complete executions
    void waitInstance(ExecutorInstance *instance);
    // Release the resources allocated for this instance
    void closeInstance(ExecutorInstance *instance);
    // Destroy an executor context
    void discard(ExecutorContext *context);
    // Close the Linux Executor
    void close();
    // Tell if the Linux Executor have been successfully closed yet
    bool closed();
private:
    // Compile an execution context for submission
    void compileContext(const ExecutorInfo &info, std::vector<char> &compiled);
    struct ExecutorInstanceInternal {
        ExecutorInstance *id;
        int pid;
        int stdout = -1; // If output must be saved
    };
    struct ExecutorMsgHead {
        uint64_t size;
        LEFlag flag = LEFlag::CLOSED;
    };
    struct SpawnHead {
        ExecutorInstance *id;
        uint64_t nbArgs;
        uint64_t nbEnvs;
        bool pipeInput;
        bool pipeOutput;
        bool pushInput;
        bool saveOutput;
    };
    struct ExitData {
        ExecutorInstance *instance;
        uint64_t size;
        char exitCode;
        bool exited; // true if exited, false if crashed or killed
        char data[];
    };
    int pipeRequest;
    int pipeResponse;
    int pid;
    std::thread thread;
    bool alive = false;
    bool properlyClosed = false;
    // External side only
    void request(LEFlag request, const std::vector<char> &datas = {}, bool requestShouldLock = true);
    LEFlag getRequest(std::vector<char> &datas);
    std::mutex requestMutex;
    std::list<ExecutorInstance> instances;
    void mainloopExt();
    // Internal side only
    void execute(std::vector<char> &datas);
    void mainloop();
    void instanceMainloop();
    std::list<ExecutorInstanceInternal> instancesInternal;
    int processCount;
    //std::mutex forkMutex; // Alias of requestMutex
};

#endif /* LINUXEXECUTOR_HPP_ */
