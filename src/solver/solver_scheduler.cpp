// Solver Scheduler Implementation
// M7 Phase: Real-time Solver (GAP-030)

#include "bendcore/solver/solver_scheduler.hpp"
#include "bendcore/solver/real_time_solver.hpp"
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace bendcore::solver {

// ============================================================================
// Adaptive Quality Controller
// ============================================================================

QualityLevel AdaptiveQualityController::select_quality_level(
    const std::vector<SolverTask>& pending_tasks,
    double available_time_ms) {

    int32_t num_tasks = pending_tasks.size();

    if (available_time_ms > target_frame_time_ms_ * 1.5) {
        current_quality_ = QualityLevel::HighPrecision;
    } else if (available_time_ms > target_frame_time_ms_) {
        current_quality_ = QualityLevel::Fine;
    } else if (available_time_ms > target_frame_time_ms_ * 0.75) {
        current_quality_ = QualityLevel::Normal;
    } else if (available_time_ms > target_frame_time_ms_ * 0.5) {
        current_quality_ = QualityLevel::Fast;
    } else {
        current_quality_ = QualityLevel::Draft;
    }

    return current_quality_;
}

void AdaptiveQualityController::adjust_quality_based_on_performance(
    double actual_frame_time_ms,
    double target_frame_time_ms) {

    double ratio = actual_frame_time_ms / target_frame_time_ms;

    if (ratio > 1.1 && current_quality_ > QualityLevel::Draft) {
        // Frame took too long, reduce quality
        current_quality_ = static_cast<QualityLevel>(
            static_cast<int32_t>(current_quality_) - 1);
        quality_adjust_counter_ = 0;
    } else if (ratio < 0.8 && current_quality_ < QualityLevel::HighPrecision) {
        // Frame finished early, maybe increase quality
        ++quality_adjust_counter_;
        if (quality_adjust_counter_ > 2) {
            current_quality_ = static_cast<QualityLevel>(
                static_cast<int32_t>(current_quality_) + 1);
            quality_adjust_counter_ = 0;
        }
    }
}

AdaptiveQualityController::SolverParameters
AdaptiveQualityController::get_parameters_for_quality(QualityLevel level) const {

    switch (level) {
        case QualityLevel::Draft:
            return {10, 1e-2, 5};
        case QualityLevel::Fast:
            return {25, 1e-3, 8};
        case QualityLevel::Normal:
            return {100, 1e-4, 10};
        case QualityLevel::Fine:
            return {250, 1e-5, 15};
        case QualityLevel::HighPrecision:
            return {1000, 1e-6, 20};
        default:
            return {100, 1e-4, 10};
    }
}

// ============================================================================
// Time Budget Allocator
// ============================================================================

TimeBudgetAllocator::TimeBudgetAllocator(double total_budget_ms)
    : total_budget_ms_(total_budget_ms),
      remaining_budget_ms_(total_budget_ms) {}

double TimeBudgetAllocator::allocate_time(
    int32_t task_id,
    int32_t priority,
    int32_t num_pending_tasks) {

    double allocated = 0.0;

    if (num_pending_tasks > 0) {
        double base_allocation = remaining_budget_ms_ / num_pending_tasks;
        double priority_factor = 1.0 + 0.1 * priority;
        allocated = base_allocation * priority_factor;
        allocated = std::min(allocated, remaining_budget_ms_);
    }

    allocations_.push_back({task_id, allocated});
    return allocated;
}

void TimeBudgetAllocator::reset_budget(double total_budget_ms) {
    total_budget_ms_ = total_budget_ms;
    remaining_budget_ms_ = total_budget_ms;
    allocations_.clear();
}

TimeBudgetAllocator::AllocationStats TimeBudgetAllocator::get_statistics() const {
    double allocated = 0.0;
    for (const auto& [task_id, time] : allocations_) {
        allocated += time;
    }

    return AllocationStats{
        total_budget_ms_,
        allocated,
        remaining_budget_ms_,
        static_cast<int32_t>(allocations_.size())
    };
}

// ============================================================================
// Solver Scheduler
// ============================================================================

SolverScheduler::SolverScheduler(double frame_time_budget_ms)
    : frame_time_budget_ms_(frame_time_budget_ms),
      task_queue_([](const SolverTask& a, const SolverTask& b) {
          return a.priority < b.priority;
      }) {

    quality_controller_ = std::make_unique<AdaptiveQualityController>();
    budget_allocator_ = std::make_unique<TimeBudgetAllocator>(frame_time_budget_ms);
    quality_controller_->set_target_frame_time_ms(frame_time_budget_ms);
}

int32_t SolverScheduler::submit_task(
    const std::function<void()>& work,
    int32_t priority,
    double time_budget_ms,
    bool is_real_time) {

    int32_t task_id = next_task_id_++;
    auto now = std::chrono::steady_clock::now();
    auto deadline = now + std::chrono::milliseconds(static_cast<int32_t>(time_budget_ms));

    SolverTask task{
        task_id,
        priority,
        time_budget_ms,
        work,
        deadline,
        is_real_time
    };

    task_queue_.push(task);
    return task_id;
}

SchedulingResult SolverScheduler::execute_frame() {
    auto frame_start = std::chrono::steady_clock::now();

    int32_t completed = 0;
    int32_t pending = task_queue_.size();

    while (!task_queue_.empty()) {
        auto now = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(
            now - frame_start).count();

        if (elapsed_ms > frame_time_budget_ms_) {
            break;
        }

        SolverTask task = task_queue_.top();
        task_queue_.pop();

        try {
            task.work();
            completed_tasks_.push_back(task);
            ++completed;
        } catch (const std::exception& e) {
            std::cerr << "Task " << task.task_id << " failed: " << e.what() << "\n";
        }
    }

    auto frame_end = std::chrono::steady_clock::now();
    auto frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        frame_end - frame_start);

    frame_times_ms_.push_back(frame_time.count());
    total_completed_ += completed;

    double avg_utilization = 0.0;
    if (!frame_times_ms_.empty()) {
        double avg_time = 0.0;
        for (double t : frame_times_ms_) {
            avg_time += t;
        }
        avg_time /= frame_times_ms_.size();
        avg_utilization = std::min(100.0, 100.0 * avg_time / frame_time_budget_ms_);
    }

    return SchedulingResult{
        task_queue_.empty(),
        completed,
        static_cast<int32_t>(task_queue_.size()),
        frame_time,
        avg_utilization
    };
}

void SolverScheduler::cancel_task(int32_t task_id) {
    // Simplified: would need to reimplement priority queue
    // to support removal
}

void SolverScheduler::set_quality_level(QualityLevel level) {
    quality_controller_->set_current_quality(level);
}

QualityLevel SolverScheduler::get_quality_level() const {
    return quality_controller_->get_current_quality();
}

void SolverScheduler::set_target_frame_time_ms(double ms) {
    frame_time_budget_ms_ = ms;
    quality_controller_->set_target_frame_time_ms(ms);
    budget_allocator_->reset_budget(ms);
}

SolverScheduler::SchedulerStats SolverScheduler::get_statistics() const {
    double avg_frame_time = 0.0;
    if (!frame_times_ms_.empty()) {
        for (double t : frame_times_ms_) {
            avg_frame_time += t;
        }
        avg_frame_time /= frame_times_ms_.size();
    }

    double cpu_util = std::min(100.0, 100.0 * avg_frame_time / frame_time_budget_ms_);

    return SchedulerStats{
        total_completed_ + static_cast<int32_t>(task_queue_.size()),
        total_completed_,
        static_cast<int32_t>(task_queue_.size()),
        avg_frame_time,
        cpu_util,
        quality_controller_->get_current_quality()
    };
}

void SolverScheduler::adjust_for_workload(int32_t num_solvers) {
    // Adjust quality based on number of concurrent solvers
    if (num_solvers > 4) {
        set_quality_level(QualityLevel::Fast);
    } else if (num_solvers > 2) {
        set_quality_level(QualityLevel::Normal);
    } else {
        set_quality_level(QualityLevel::Fine);
    }
}

// ============================================================================
// Priority Task Executor
// ============================================================================

void PriorityTaskExecutor::add_task(
    int32_t priority,
    const std::function<void()>& task,
    double time_budget_ms) {

    pending_tasks_.push(PrioritizedTask{priority, task, time_budget_ms});
}

int32_t PriorityTaskExecutor::execute_pending(double time_available_ms) {
    int32_t executed = 0;
    auto start_time = std::chrono::steady_clock::now();

    while (!pending_tasks_.empty()) {
        auto now = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(
            now - start_time).count();

        if (elapsed_ms > time_available_ms) {
            break;
        }

        auto task = pending_tasks_.top();
        pending_tasks_.pop();

        try {
            task.work();
            ++executed;
        } catch (const std::exception& e) {
            // Log error
        }
    }

    return executed;
}

void PriorityTaskExecutor::clear() {
    while (!pending_tasks_.empty()) {
        pending_tasks_.pop();
    }
}

// ============================================================================
// Incremental Solver Executor
// ============================================================================

IncrementalSolverExecutor::IncrementalSolverExecutor(RealTimeSolver& solver)
    : solver_(solver) {}

void IncrementalSolverExecutor::start_solve(
    const std::vector<double>& target_load,
    double time_budget_per_frame_ms) {

    target_load_ = target_load;
    current_solution_.resize(target_load.size(), 0.0);
    total_iterations_ = 0;
    current_iteration_ = 0;
    solve_complete_ = false;
    start_time_ = std::chrono::steady_clock::now();
}

bool IncrementalSolverExecutor::execute_iteration(double time_available_ms) {
    if (solve_complete_) {
        return true;
    }

    auto start_time = std::chrono::steady_clock::now();

    // Execute one step
    std::vector<double> delta_load(target_load_.size());
    for (size_t i = 0; i < target_load_.size(); ++i) {
        delta_load[i] = target_load_[i] / 10.0;  // 10 steps
    }

    auto result = solver_.solve_increment(delta_load, &current_solution_);
    current_solution_ = result.displacement;
    ++current_iteration_;

    if (current_iteration_ >= 10 || result.converged) {
        solve_complete_ = true;
        total_iterations_ = current_iteration_;
        return true;
    }

    auto elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start_time).count();

    return elapsed < time_available_ms;
}

const std::vector<double>& IncrementalSolverExecutor::get_intermediate_solution() const {
    return current_solution_;
}

void IncrementalSolverExecutor::cancel() {
    solve_complete_ = true;
}

IncrementalSolverExecutor::ExecutionStats IncrementalSolverExecutor::get_statistics() const {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_);

    double percentage = total_iterations_ > 0 ?
        100.0 * current_iteration_ / total_iterations_ : 0.0;

    return ExecutionStats{
        total_iterations_,
        current_iteration_,
        percentage,
        elapsed
    };
}

}  // namespace bendcore::solver
