// CAD-CORE-052: ParallelExecutor - Parallel Computation Engine
// M5 Phase: Performance Optimization and Caching
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace bendcore::optimization {

/**
 * ParallelExecutor
 *
 * Multi-threaded execution engine for:
 * - Feature computation
 * - B-Rep operations
 * - Boolean operations
 * - Dependency resolution
 *
 * Features:
 * - Thread pool management
 * - Task dependency resolution
 * - Work queue management
 * - Result aggregation
 * - Exception handling
 */

struct Task {
    std::string id;
    std::string name;
    std::function<std::vector<uint8_t>()> computation;
    std::vector<std::string> dependencies;
    int32_t priority;  // 0 (highest) to 100 (lowest)
    int64_t estimatedDurationMs;
    bool isCompleted;
    std::vector<uint8_t> result;
    std::string errorMessage;
};

struct TaskResult {
    std::string taskId;
    bool successful;
    std::vector<uint8_t> result;
    std::string errorMessage;
    int64_t executionTimeMs;
};

class TaskGraph {
public:
    void addTask(const std::shared_ptr<Task>& task);
    void addDependency(const std::string& taskId, const std::string& dependsOnId);

    std::vector<std::string> topologicalSort() const;
    std::vector<std::string> getReadyTasks() const;
    std::vector<std::string> getDependencies(const std::string& taskId) const;
    std::vector<std::string> getDependents(const std::string& taskId) const;

    bool validateDAG() const;
    size_t getTaskCount() const;

private:
    std::map<std::string, std::shared_ptr<Task>> tasks_;
    std::map<std::string, std::vector<std::string>> adjacencyList_;
};

class ParallelExecutor {
public:
    explicit ParallelExecutor(uint32_t threadCount = 0);  // 0 = auto-detect
    ~ParallelExecutor();

    // Task submission
    std::string submitTask(
        const std::string& taskName,
        std::function<std::vector<uint8_t>()> computation,
        int32_t priority = 50,
        const std::vector<std::string>& dependencies = {}
    );

    void submitTaskGraph(const TaskGraph& graph);

    // Execution control
    void executeAll();
    void executeUntilComplete();
    bool waitForTask(const std::string& taskId, int64_t timeoutMs = -1);

    // Task management
    void cancelTask(const std::string& taskId);
    void cancelAll();
    void pauseExecution();
    void resumeExecution();

    // Results
    std::unique_ptr<TaskResult> getTaskResult(const std::string& taskId);
    std::vector<TaskResult> getAllResults();
    std::vector<TaskResult> getFailedResults();

    // Statistics
    struct ExecutorStats {
        uint32_t threadCount;
        uint32_t activeThreads;
        size_t totalTasksSubmitted;
        size_t completedTasks;
        size_t failedTasks;
        double averageTaskTimeMs;
        double totalExecutionTimeMs;
        double parallelEfficiency;
        std::map<std::string, int64_t> taskExecutionTimes;
    };

    ExecutorStats getStatistics() const;

    // Thread pool management
    void setThreadCount(uint32_t count);
    uint32_t getThreadCount() const;
    uint32_t getActiveThreadCount() const;

    // Monitoring
    void enableProfiling();
    void disableProfiling();
    bool isProfilingEnabled() const;

    std::string generateExecutionReport() const;

    // Clear state
    void clearCompleted();
    void reset();

private:
    uint32_t threadCount_;
    std::vector<std::thread> threadPool_;
    std::queue<std::shared_ptr<Task>> taskQueue_;

    std::map<std::string, std::shared_ptr<Task>> allTasks_;
    std::map<std::string, std::unique_ptr<TaskResult>> results_;

    std::mutex queueMutex_;
    std::mutex resultsMutex_;
    std::condition_variable queueCV_;
    std::condition_variable completionCV_;

    bool shouldStop_ = false;
    bool paused_ = false;
    bool profilingEnabled_ = false;

    size_t completedTasks_ = 0;
    size_t failedTasks_ = 0;
    double totalExecutionTimeMs_ = 0;

    void workerThread();
    std::shared_ptr<Task> getNextTask();
    void executeTask(const std::shared_ptr<Task>& task);
};

}  // namespace bendcore::optimization
