// Solver Scheduling and Priority Management
// M7 Phase: Real-time Solver (GAP-030)
#pragma once

#include <vector>
#include <cstdint>
#include <memory>
#include <chrono>
#include <queue>
#include <functional>

namespace bendcore::solver {

// Forward declarations
class RealTimeSolver;

// Solver task with priority
struct SolverTask {
    int32_t task_id;
    int32_t priority;  // Higher = more important
    double time_budget_ms;
    std::function<void()> work;
    std::chrono::steady_clock::time_point deadline;
    bool is_real_time;
};

// Quality-vs-Speed trade-off levels
enum class QualityLevel {
    Draft = 1,        // Minimal iterations, fast
    Fast = 2,         // Reduced iterations
    Normal = 3,       // Standard quality
    Fine = 4,         // More iterations
    HighPrecision = 5 // Maximum precision
};

// Scheduling result
struct SchedulingResult {
    bool all_completed;
    int32_t num_completed;
    int32_t num_pending;
    std::chrono::milliseconds total_time;
    double average_utilization;
};

// Adaptive quality controller
class AdaptiveQualityController {
public:
    // Set target frame time
    void set_target_frame_time_ms(double ms) { target_frame_time_ms_ = ms; }

    // Select quality level based on workload
    QualityLevel select_quality_level(
        const std::vector<SolverTask>& pending_tasks,
        double available_time_ms);

    // Adjust quality based on previous frame time
    void adjust_quality_based_on_performance(
        double actual_frame_time_ms,
        double target_frame_time_ms);

    // Get current quality level
    QualityLevel get_current_quality() const { return current_quality_; }

    // Set quality level
    void set_current_quality(QualityLevel level) { current_quality_ = level; }

    // Get solver parameters for quality level
    struct SolverParameters {
        int32_t max_iterations;
        double tolerance;
        int32_t rom_basis_size;
    };

    SolverParameters get_parameters_for_quality(QualityLevel level) const;

private:
    QualityLevel current_quality_ = QualityLevel::Normal;
    double target_frame_time_ms_ = 16.0;  // 60 FPS
    int32_t quality_adjust_counter_ = 0;
};

// Time budget allocator
class TimeBudgetAllocator {
public:
    explicit TimeBudgetAllocator(double total_budget_ms);

    // Allocate time to a task
    double allocate_time(
        int32_t task_id,
        int32_t priority,
        int32_t num_pending_tasks);

    // Reset budget for next frame
    void reset_budget(double total_budget_ms);

    // Get remaining time
    double get_remaining_time() const { return remaining_budget_ms_; }

    // Consume time
    void consume_time(double time_ms) { remaining_budget_ms_ -= time_ms; }

    // Statistics
    struct AllocationStats {
        double total_budget;
        double allocated;
        double remaining;
        int32_t num_tasks;
    };

    AllocationStats get_statistics() const;

private:
    double total_budget_ms_;
    double remaining_budget_ms_;
    std::vector<std::pair<int32_t, double>> allocations_;  // task_id, allocated_time
};

// Solver task scheduler
class SolverScheduler {
public:
    explicit SolverScheduler(double frame_time_budget_ms = 16.0);

    // Submit a solver task
    int32_t submit_task(
        const std::function<void()>& work,
        int32_t priority = 0,
        double time_budget_ms = 16.0,
        bool is_real_time = true);

    // Execute all pending tasks within time budget
    SchedulingResult execute_frame();

    // Cancel a task
    void cancel_task(int32_t task_id);

    // Set quality level
    void set_quality_level(QualityLevel level);
    QualityLevel get_quality_level() const;

    // Set target frame time
    void set_target_frame_time_ms(double ms);

    // Get statistics
    struct SchedulerStats {
        int32_t total_tasks;
        int32_t completed_tasks;
        int32_t pending_tasks;
        double average_frame_time_ms;
        double cpu_utilization;
        QualityLevel current_quality;
    };

    SchedulerStats get_statistics() const;

    // Dynamic workload adjustment
    void adjust_for_workload(int32_t num_solvers);

private:
    double frame_time_budget_ms_;
    int32_t next_task_id_ = 0;

    std::priority_queue<SolverTask, std::vector<SolverTask>,
        std::function<bool(const SolverTask&, const SolverTask&)>> task_queue_;

    std::vector<SolverTask> completed_tasks_;

    std::unique_ptr<AdaptiveQualityController> quality_controller_;
    std::unique_ptr<TimeBudgetAllocator> budget_allocator_;

    // Statistics
    std::vector<double> frame_times_ms_;
    int32_t total_completed_ = 0;
};

// Priority-based task executor
class PriorityTaskExecutor {
public:
    // Add task with priority
    void add_task(
        int32_t priority,
        const std::function<void()>& task,
        double time_budget_ms = 16.0);

    // Execute tasks in priority order within time budget
    int32_t execute_pending(double time_available_ms);

    // Get pending task count
    int32_t get_num_pending() const { return static_cast<int32_t>(pending_tasks_.size()); }

    // Clear all tasks
    void clear();

private:
    struct PrioritizedTask {
        int32_t priority;
        std::function<void()> work;
        double time_budget_ms;

        bool operator<(const PrioritizedTask& other) const {
            return priority < other.priority;  // Min-heap, higher priority = smaller priority value
        }
    };

    std::priority_queue<PrioritizedTask> pending_tasks_;
};

// Incremental solver executor
// Executes solver iterations incrementally to maintain frame rate
class IncrementalSolverExecutor {
public:
    explicit IncrementalSolverExecutor(RealTimeSolver& solver);

    // Start incremental solve
    void start_solve(
        const std::vector<double>& target_load,
        double time_budget_per_frame_ms = 16.0);

    // Execute one iteration within time budget
    bool execute_iteration(double time_available_ms);

    // Check if solve is complete
    bool is_complete() const { return solve_complete_; }

    // Get current intermediate solution
    const std::vector<double>& get_intermediate_solution() const;

    // Cancel ongoing solve
    void cancel();

    // Statistics
    struct ExecutionStats {
        int32_t total_iterations;
        int32_t completed_iterations;
        double completion_percentage;
        std::chrono::milliseconds elapsed_time;
    };

    ExecutionStats get_statistics() const;

private:
    RealTimeSolver& solver_;
    std::vector<double> target_load_;
    std::vector<double> current_solution_;
    int32_t current_iteration_ = 0;
    int32_t total_iterations_ = 0;
    bool solve_complete_ = false;
    std::chrono::steady_clock::time_point start_time_;
};

}  // namespace bendcore::solver
