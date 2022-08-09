#ifndef EXECUTOR_GRAPH_HPP_
#define EXECUTOR_GRAPH_HPP_

#include <atomic>
#include <semaphore>
class ExecutorNode;

// Note : must be one of (uint8_t, uint16_t), as uint32_t would need 32GB
#define PENDING_NODES_INDEX_TYPE uint16_t
#define MAX_PENDING_NODES (1L << (8 * sizeof(PENDING_NODES_INDEX_TYPE)))

class ExecutorGraph {
public:
    // Enfore executing the node given as argument, this ma
    void executeNode(ExecutorNode *node);
    // Execute the graph using current thread, only return when the graph is stopped
    void executeGraph();

    // There is always a unique instance of this executor
    ExecutorGraph instance;
private:
    ExecutorGraph();
    std::counting_semaphore<MAX_PENDING_NODES-1> semaphore;
    std::atomic<PENDING_NODES_INDEX_TYPE> readIdx;
    std::atomic<PENDING_NODES_INDEX_TYPE> writeIdx;
    ExecutorNode *pending[MAX_PENDING_NODES];
};

#endif /* end of include guard: EXECUTOR_GRAPH_HPP_ */
