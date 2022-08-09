#ifndef EXECUTOR_NODE_HPP_
#define EXECUTOR_NODE_HPP_

#include <atomic>
#include <semaphore>
class ExecutorGraph;

class ExecutorNode {
    friend class ExecutorGraph; // Because ExecutorGraph handle the whole execution of nodes
public:
    // Add a dependency to the completion of the execution of a given node
    void addDependency(ExecutorNode *dependsOn) {
        dependsOn->dependants.push_back(this);
        ++dependencyCount;
        pendingDependencyCount.fetch_add(1, std::memory_order_relaxed);
    }
    // Like addDependency, excepted that the dependency is not
    void addFutureDependency(ExecutorNode *willDependsOn) {
        willDependsOn->dependants.push_back(this);
        ++dependencyCount;
    }
    // Add a dependency to a signalDependency() call to this node for the next execution only
    void addSingleExternalDependency() {
        pendingDependencyCount.fetch_add(1, std::memory_order_relaxed);
    }
    // Add a dependency to a signalDependency() call to this node
    void addExternalDependency() {
        ++dependencyCount;
        pendingDependencyCount.fetch_add(1, std::memory_order_relaxed);
    }
    void signalDependency() {
        if (pendingDependencyCount.fetch_sub(1, std::memory_order_relaxed) == 1)
            ExecutorGraph::instance->
    }
protected:
    virtual void execute() {}
    uint16_t dependencyCount = 0;
    uint8_t priority
    std::atomic<uint16_t> pendingDependencyCount;
    // List of all dependencies
    std::vector<ExecutorNode *> dependants;
    // If false, execute() will be skipped, but dependants nodes will be signaled as usual
    bool skipExecution;
    // If true, this node enter in the prioritary queue which is executed before this queue
    bool isPrioritary = false;
};

#endif /* end of include guard: EXECUTOR_NODE_HPP_ */
