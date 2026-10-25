#pragma once

#ifdef HAVE_TASKFLOW
    #include <taskflow/taskflow.hpp>
#endif

#include <functional>
#include <vector>
#include <chrono>

namespace bendcore::parallel {

/**
 * Task performance metrics
 */
struct TaskMetrics {
    std::string task_name;
    std::chrono::milliseconds duration;
    size_t worker_id = 0;
};

/**
 * Parallel Execution wrapper around Taskflow library
 * Provides modern task graph-based parallel computing
 */
class ParallelExecutor {
public:
    using Task = std::function<void()>;
    using TaskWithId = std::function<void(size_t)>;

    ParallelExecutor();
    ~ParallelExecutor();

    /**
     * Set number of worker threads
     */
    void set_num_workers(size_t num_workers);

    /**
     * Get number of worker threads
     */
    size_t get_num_workers() const;

    /**
     * Execute tasks in parallel
     */
    void execute_parallel(const std::vector<Task>& tasks);

    /**
     * Execute task for each item in a range
     */
    void execute_for_each(size_t start, size_t end, TaskWithId task);

    /**
     * Create a task graph for complex dependencies
     */
    void create_task_graph(const std::function<void(void*)>& graph_builder);

    /**
     * Execute task graph
     */
    void execute_task_graph();

    /**
     * Wait for all tasks to complete
     */
    void wait_all();

    /**
     * Execute with performance monitoring
     */
    std::vector<TaskMetrics> execute_with_metrics(const std::vector<Task>& tasks);

    /**
     * Parallel reduce operation
     */
    template<typename T>
    T parallel_reduce(const std::vector<T>& data,
                      std::function<T(T, T)> reduce_fn);

    /**
     * Parallel map operation
     */
    template<typename T, typename R>
    std::vector<R> parallel_map(const std::vector<T>& data,
                                std::function<R(const T&)> map_fn);

    /**
     * Get total execution time
     */
    std::chrono::milliseconds get_execution_time() const;

    /**
     * Print execution statistics
     */
    void print_statistics() const;

private:
    #ifdef HAVE_TASKFLOW
    std::unique_ptr<tf::Executor> executor_;
    tf::Taskflow taskflow_;
    #else
    void* executor_ = nullptr;
    void* taskflow_ = nullptr;
    #endif

    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
    std::chrono::time_point<std::chrono::high_resolution_clock> end_time_;
};

/**
 * Parallel computation helpers
 */
namespace utils {
    /**
     * Calculate optimal chunk size for parallel processing
     */
    size_t calculate_chunk_size(size_t total_items, size_t num_workers);

    /**
     * Get optimal number of threads for system
     */
    size_t get_optimal_threads();

    /**
     * Parallel dot product
     */
    double parallel_dot_product(
        const std::vector<double>& a,
        const std::vector<double>& b);

    /**
     * Parallel matrix multiplication
     */
    std::vector<std::vector<double>> parallel_matmul(
        const std::vector<std::vector<double>>& A,
        const std::vector<std::vector<double>>& B);
}

} // namespace bendcore::parallel
