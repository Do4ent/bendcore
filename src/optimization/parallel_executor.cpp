// CAD-CORE-052: ParallelExecutor Implementation
#include "bendcore/optimization/parallel_executor.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <numeric>

namespace bendcore::optimization {

// TaskGraph implementation
void TaskGraph::addTask(const std::shared_ptr<Task>& task) {
    tasks_[task->id] = task;
    if (adjacencyList_.find(task->id) == adjacencyList_.end()) {
        adjacencyList_[task->id] = {};
    }
}

void TaskGraph::addDependency(const std::string& taskId, const std::string& dependsOnId) {
    if (adjacencyList_.find(taskId) != adjacencyList_.end()) {
        adjacencyList_[taskId].push_back(dependsOnId);
    }
}

std::vector<std::string> TaskGraph::topologicalSort() const {
    std::map<std::string, int> inDegree;
    std::map<std::string, std::vector<std::string>> adj;

    for (const auto& task : tasks_) {
        inDegree[task.first] = 0;
    }

    for (const auto& entry : adjacencyList_) {
        for (const auto& dep : entry.second) {
            adj[dep].push_back(entry.first);
            inDegree[entry.first]++;
        }
    }

    std::vector<std::string> result;
    std::queue<std::string> queue;

    for (const auto& entry : inDegree) {
        if (entry.second == 0) {
            queue.push(entry.first);
        }
    }

    while (!queue.empty()) {
        std::string node = queue.front();
        queue.pop();
        result.push_back(node);

        for (const auto& neighbor : adj[node]) {
            inDegree[neighbor]--;
            if (inDegree[neighbor] == 0) {
                queue.push(neighbor);
            }
        }
    }

    return result;
}

std::vector<std::string> TaskGraph::getReadyTasks() const {
    std::vector<std::string> ready;

    for (const auto& task : tasks_) {
        bool allDepsReady = true;
        for (const auto& dep : task.second->dependencies) {
            auto it = tasks_.find(dep);
            if (it == tasks_.end() || !it->second->isCompleted) {
                allDepsReady = false;
                break;
            }
        }

        if (allDepsReady && !task.second->isCompleted) {
            ready.push_back(task.first);
        }
    }

    return ready;
}

std::vector<std::string> TaskGraph::getDependencies(const std::string& taskId) const {
    auto it = adjacencyList_.find(taskId);
    if (it != adjacencyList_.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> TaskGraph::getDependents(const std::string& taskId) const {
    std::vector<std::string> dependents;

    for (const auto& entry : adjacencyList_) {
        for (const auto& dep : entry.second) {
            if (dep == taskId) {
                dependents.push_back(entry.first);
                break;
            }
        }
    }

    return dependents;
}

bool TaskGraph::validateDAG() const {
    std::map<std::string, int> visited;  // 0: unvisited, 1: visiting, 2: visited
    std::map<std::string, int> recStack;

    std::function<bool(const std::string&)> hasCycle = [&](const std::string& node) -> bool {
        if (visited[node] == 1) return true;  // Back edge found
        if (visited[node] == 2) return false;  // Already processed

        visited[node] = 1;
        recStack[node] = 1;

        auto it = adjacencyList_.find(node);
        if (it != adjacencyList_.end()) {
            for (const auto& neighbor : it->second) {
                if (hasCycle(neighbor)) return true;
            }
        }

        visited[node] = 2;
        recStack[node] = 0;
        return false;
    };

    for (const auto& task : tasks_) {
        if (visited[task.first] == 0) {
            if (hasCycle(task.first)) return false;
        }
    }

    return true;
}

size_t TaskGraph::getTaskCount() const {
    return tasks_.size();
}

// ParallelExecutor implementation
ParallelExecutor::ParallelExecutor(uint32_t threadCount)
    : threadCount_(threadCount == 0 ? std::thread::hardware_concurrency() : threadCount) {

    for (uint32_t i = 0; i < threadCount_; ++i) {
        threadPool_.emplace_back(&ParallelExecutor::workerThread, this);
    }
}

ParallelExecutor::~ParallelExecutor() {
    shouldStop_ = true;
    queueCV_.notify_all();

    for (auto& thread : threadPool_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

std::string ParallelExecutor::submitTask(
    const std::string& taskName,
    std::function<std::vector<uint8_t>()> computation,
    int32_t priority,
    const std::vector<std::string>& dependencies) {

    static int taskCounter = 0;
    std::string taskId = "task_" + std::to_string(taskCounter++);

    auto task = std::make_shared<Task>();
    task->id = taskId;
    task->name = taskName;
    task->computation = computation;
    task->dependencies = dependencies;
    task->priority = priority;
    task->isCompleted = false;

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        allTasks_[taskId] = task;
        taskQueue_.push(task);
    }

    queueCV_.notify_one();
    return taskId;
}

void ParallelExecutor::submitTaskGraph(const TaskGraph& graph) {
    // Submit all tasks in topological order
    auto sorted = graph.topologicalSort();

    for (const auto& taskId : sorted) {
        // Tasks would already be in the graph, just submit them
    }
}

void ParallelExecutor::executeAll() {
    executeUntilComplete();
}

void ParallelExecutor::executeUntilComplete() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        completionCV_.wait(lock, [this]() {
            return taskQueue_.empty() && completedTasks_ + failedTasks_ == allTasks_.size();
        });
    }
}

bool ParallelExecutor::waitForTask(const std::string& taskId, int64_t timeoutMs) {
    auto startTime = std::chrono::high_resolution_clock::now();

    while (true) {
        {
            std::lock_guard<std::mutex> lock(resultsMutex_);
            if (results_.find(taskId) != results_.end()) {
                return results_[taskId]->successful;
            }
        }

        if (timeoutMs > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - startTime).count();
            if (elapsed >= timeoutMs) {
                return false;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void ParallelExecutor::cancelTask(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    auto it = allTasks_.find(taskId);
    if (it != allTasks_.end() && !it->second->isCompleted) {
        it->second->isCompleted = true;
    }
}

void ParallelExecutor::cancelAll() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    for (auto& task : allTasks_) {
        task.second->isCompleted = true;
    }
    std::queue<std::shared_ptr<Task>> empty;
    std::swap(taskQueue_, empty);
}

void ParallelExecutor::pauseExecution() {
    paused_ = true;
}

void ParallelExecutor::resumeExecution() {
    paused_ = false;
    queueCV_.notify_all();
}

std::unique_ptr<TaskResult> ParallelExecutor::getTaskResult(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    auto it = results_.find(taskId);
    if (it != results_.end()) {
        auto result = std::make_unique<TaskResult>();
        *result = *it->second;
        return result;
    }
    return nullptr;
}

std::vector<TaskResult> ParallelExecutor::getAllResults() {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    std::vector<TaskResult> allResults;

    for (const auto& result : results_) {
        allResults.push_back(*result.second);
    }

    return allResults;
}

std::vector<TaskResult> ParallelExecutor::getFailedResults() {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    std::vector<TaskResult> failedResults;

    for (const auto& result : results_) {
        if (!result.second->successful) {
            failedResults.push_back(*result.second);
        }
    }

    return failedResults;
}

ParallelExecutor::ExecutorStats ParallelExecutor::getStatistics() const {
    ExecutorStats stats;
    stats.threadCount = threadCount_;
    stats.activeThreads = 0;  // Would need thread-local state to track accurately
    stats.totalTasksSubmitted = allTasks_.size();
    stats.completedTasks = completedTasks_;
    stats.failedTasks = failedTasks_;
    stats.averageTaskTimeMs = stats.completedTasks > 0
        ? totalExecutionTimeMs_ / stats.completedTasks
        : 0.0;
    stats.totalExecutionTimeMs = totalExecutionTimeMs_;
    stats.parallelEfficiency = 1.0;  // Simplified

    return stats;
}

void ParallelExecutor::setThreadCount(uint32_t count) {
    if (count != threadCount_) {
        threadCount_ = count;
        // Would need to rebuild thread pool
    }
}

uint32_t ParallelExecutor::getThreadCount() const {
    return threadCount_;
}

uint32_t ParallelExecutor::getActiveThreadCount() const {
    return static_cast<uint32_t>(threadPool_.size());
}

void ParallelExecutor::enableProfiling() {
    profilingEnabled_ = true;
}

void ParallelExecutor::disableProfiling() {
    profilingEnabled_ = false;
}

bool ParallelExecutor::isProfilingEnabled() const {
    return profilingEnabled_;
}

std::string ParallelExecutor::generateExecutionReport() const {
    std::ostringstream oss;
    auto stats = getStatistics();

    oss << "=== Parallel Execution Report ===\n";
    oss << "Thread Count: " << stats.threadCount << "\n";
    oss << "Total Tasks Submitted: " << stats.totalTasksSubmitted << "\n";
    oss << "Completed Tasks: " << stats.completedTasks << "\n";
    oss << "Failed Tasks: " << stats.failedTasks << "\n";
    oss << "Average Task Time: " << stats.averageTaskTimeMs << " ms\n";
    oss << "Total Execution Time: " << stats.totalExecutionTimeMs << " ms\n";

    return oss.str();
}

void ParallelExecutor::clearCompleted() {
    std::lock_guard<std::mutex> lock(resultsMutex_);
    results_.clear();
}

void ParallelExecutor::reset() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    std::queue<std::shared_ptr<Task>> empty;
    std::swap(taskQueue_, empty);
    allTasks_.clear();
    completedTasks_ = 0;
    failedTasks_ = 0;
    totalExecutionTimeMs_ = 0;

    {
        std::lock_guard<std::mutex> resultLock(resultsMutex_);
        results_.clear();
    }
}

void ParallelExecutor::workerThread() {
    while (!shouldStop_) {
        std::shared_ptr<Task> task;

        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCV_.wait(lock, [this]() { return !taskQueue_.empty() || shouldStop_; });

            if (shouldStop_) break;

            while (paused_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            if (!taskQueue_.empty()) {
                task = taskQueue_.front();
                taskQueue_.pop();
            }
        }

        if (task) {
            executeTask(task);
        }
    }
}

std::shared_ptr<Task> ParallelExecutor::getNextTask() {
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (!taskQueue_.empty()) {
        auto task = taskQueue_.front();
        taskQueue_.pop();
        return task;
    }
    return nullptr;
}

void ParallelExecutor::executeTask(const std::shared_ptr<Task>& task) {
    if (!task || task->isCompleted) return;

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        std::vector<uint8_t> result = task->computation();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        auto taskResult = std::make_unique<TaskResult>();
        taskResult->taskId = task->id;
        taskResult->successful = true;
        taskResult->result = result;
        taskResult->executionTimeMs = duration.count();

        {
            std::lock_guard<std::mutex> lock(resultsMutex_);
            results_[task->id] = std::move(taskResult);
        }

        task->isCompleted = true;
        completedTasks_++;
        totalExecutionTimeMs_ += duration.count();

    } catch (const std::exception& e) {
        auto taskResult = std::make_unique<TaskResult>();
        taskResult->taskId = task->id;
        taskResult->successful = false;
        taskResult->errorMessage = std::string(e.what());

        {
            std::lock_guard<std::mutex> lock(resultsMutex_);
            results_[task->id] = std::move(taskResult);
        }

        task->isCompleted = true;
        failedTasks_++;
    }

    completionCV_.notify_all();
}

}  // namespace bendcore::optimization
