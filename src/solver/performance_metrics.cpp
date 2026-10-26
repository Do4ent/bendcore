// Performance Metrics and Profiling Implementation
// M7 Phase: Real-time Solver (GAP-030)

#include "bendcore/solver/performance_metrics.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace bendcore::solver {

// ============================================================================
// Performance Timer
// ============================================================================

PerformanceTimer::PerformanceTimer(const std::string& name)
    : name_(name), start_time_(std::chrono::steady_clock::now()) {}

PerformanceTimer::~PerformanceTimer() = default;

std::chrono::nanoseconds PerformanceTimer::elapsed() const {
    if (stopped_) {
        return std::chrono::nanoseconds(0);
    }
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_time_);
}

void PerformanceTimer::reset() {
    start_time_ = std::chrono::steady_clock::now();
    stopped_ = false;
}

void PerformanceTimer::stop() {
    stopped_ = true;
}

// ============================================================================
// Hierarchical Timer
// ============================================================================

HierarchicalTimer::HierarchicalTimer(const std::string& name)
    : root_name_(name) {}

HierarchicalTimer::~HierarchicalTimer() = default;

void HierarchicalTimer::push(const std::string& region_name) {
    region_stack_.push_back(region_name);
    time_stack_.push_back(std::chrono::steady_clock::now());
}

void HierarchicalTimer::pop() {
    if (region_stack_.empty()) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    std::string region = region_stack_.back();
    region_stack_.pop_back();

    auto start = time_stack_.back();
    time_stack_.pop_back();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start);

    events_.push_back(PerformanceEvent{region, duration, now,
                                       static_cast<int32_t>(region_stack_.size())});

    region_totals_[region] += duration;
}

std::chrono::nanoseconds HierarchicalTimer::get_region_time(
    const std::string& region_name) const {

    auto it = region_totals_.find(region_name);
    if (it != region_totals_.end()) {
        return it->second;
    }
    return std::chrono::nanoseconds(0);
}

void HierarchicalTimer::print_report() const {
    std::cout << "\n=== Timing Report: " << root_name_ << " ===\n";

    for (const auto& [region, time] : region_totals_) {
        double ms = std::chrono::duration<double, std::milli>(time).count();
        std::cout << "  " << region << ": " << std::fixed << std::setprecision(3)
                  << ms << " ms\n";
    }
}

void HierarchicalTimer::reset() {
    events_.clear();
    region_totals_.clear();
    region_stack_.clear();
    time_stack_.clear();
}

// ============================================================================
// Memory Profiler
// ============================================================================

void MemoryProfiler::record_allocation(int64_t size, const std::string& tag) {
    current_usage_ += size;
    peak_usage_ = std::max(peak_usage_, current_usage_);
    tag_usage_[tag] += size;
    usage_history_.push_back({std::chrono::steady_clock::now(), current_usage_});
}

void MemoryProfiler::record_deallocation(int64_t size, const std::string& tag) {
    current_usage_ -= size;
    if (tag_usage_.count(tag)) {
        tag_usage_[tag] -= size;
    }
    usage_history_.push_back({std::chrono::steady_clock::now(), current_usage_});
}

int64_t MemoryProfiler::get_current_usage() const {
    return current_usage_;
}

int64_t MemoryProfiler::get_peak_usage() const {
    return peak_usage_;
}

int64_t MemoryProfiler::get_usage_by_tag(const std::string& tag) const {
    auto it = tag_usage_.find(tag);
    return it != tag_usage_.end() ? it->second : 0;
}

void MemoryProfiler::print_report() const {
    std::cout << "\n=== Memory Profiling Report ===\n";
    std::cout << "Current Usage: " << current_usage_ / 1024.0 << " KB\n";
    std::cout << "Peak Usage: " << peak_usage_ / 1024.0 << " KB\n";

    std::cout << "\nUsage by Tag:\n";
    for (const auto& [tag, usage] : tag_usage_) {
        std::cout << "  " << tag << ": " << usage / 1024.0 << " KB\n";
    }
}

void MemoryProfiler::reset() {
    current_usage_ = 0;
    peak_usage_ = 0;
    tag_usage_.clear();
    usage_history_.clear();
}

// ============================================================================
// SIMD Counter
// ============================================================================

void SIMDCounter::count_simd_operation(int32_t vector_width, const std::string& operation) {
    operation_counts_[operation]++;
    operation_widths_[operation] = vector_width;
}

int64_t SIMDCounter::get_total_operations() const {
    int64_t total = 0;
    for (const auto& [op, count] : operation_counts_) {
        total += count;
    }
    return total;
}

int64_t SIMDCounter::get_scalar_equivalent() const {
    int64_t total = 0;
    for (const auto& [op, count] : operation_counts_) {
        if (operation_widths_.count(op)) {
            total += count * operation_widths_.at(op);
        } else {
            total += count;
        }
    }
    return total;
}

double SIMDCounter::get_simd_speedup() const {
    int64_t scalar = get_scalar_equivalent();
    int64_t simd = get_total_operations();

    if (simd == 0) return 0.0;
    return static_cast<double>(scalar) / simd;
}

void SIMDCounter::print_report() const {
    std::cout << "\n=== SIMD Performance Report ===\n";
    std::cout << "Total SIMD Operations: " << get_total_operations() << "\n";
    std::cout << "Scalar Equivalent: " << get_scalar_equivalent() << "\n";
    std::cout << "Speedup Factor: " << std::fixed << std::setprecision(2)
              << get_simd_speedup() << "x\n";
}

// ============================================================================
// Performance Analyzer
// ============================================================================

void PerformanceAnalyzer::record_solve_metrics(const SolverMetrics& metrics) {
    metrics_history_.push_back(metrics);
}

SolverMetrics PerformanceAnalyzer::get_average_metrics() const {
    if (metrics_history_.empty()) {
        return SolverMetrics();
    }

    SolverMetrics avg;
    avg.num_iterations = 0;
    avg.initial_residual = 0.0;
    avg.final_residual = 0.0;
    avg.convergence_rate = 0.0;

    for (const auto& m : metrics_history_) {
        avg.num_iterations += m.num_iterations;
        avg.final_residual += m.final_residual;
        avg.convergence_rate += m.convergence_rate;
    }

    int32_t n = metrics_history_.size();
    avg.num_iterations /= n;
    avg.final_residual /= n;
    avg.convergence_rate /= n;

    return avg;
}

std::pair<SolverMetrics, SolverMetrics> PerformanceAnalyzer::get_extrema() const {
    SolverMetrics best = metrics_history_[0];
    SolverMetrics worst = metrics_history_[0];

    for (const auto& m : metrics_history_) {
        if (m.num_iterations < best.num_iterations) {
            best = m;
        }
        if (m.num_iterations > worst.num_iterations) {
            worst = m;
        }
    }

    return {best, worst};
}

int64_t PerformanceAnalyzer::estimate_flops(
    int32_t matrix_size,
    int32_t num_nonzeros,
    int32_t num_iterations) {

    // SpMV: 2 * nnz
    // CG: ~5 SpMVs per iteration
    return 5LL * num_iterations * 2 * num_nonzeros;
}

PerformanceAnalyzer::RooflineModel PerformanceAnalyzer::analyze_roofline(
    const SolverMetrics& metrics) {

    RooflineModel model;
    model.peak_flops = 1e12;  // 1 TFLOP/s typical for CPU
    model.memory_bandwidth_gbps = 100.0;  // 100 GB/s typical
    model.arithmetic_intensity =
        static_cast<double>(metrics.num_flops) / (metrics.total_time.count() * 1e9);

    double flop_bound = model.peak_flops * metrics.total_time.count() * 1e-3;
    double memory_bound = model.memory_bandwidth_gbps * 1e9 * metrics.total_time.count() * 1e-3;

    model.achieved_flops = std::min(flop_bound, memory_bound);
    model.compute_bound = flop_bound < memory_bound;

    return model;
}

PerformanceAnalyzer::PerformanceStats PerformanceAnalyzer::get_statistics() const {
    if (metrics_history_.empty()) {
        return PerformanceStats{0.0, 0.0, 0.0, 0.0, 0.0};
    }

    double mean_iterations = 0.0;
    double mean_time = 0.0;
    double mean_rate = 0.0;

    for (const auto& m : metrics_history_) {
        mean_iterations += m.num_iterations;
        mean_time += m.total_time.count();
        mean_rate += m.convergence_rate;
    }

    int32_t n = metrics_history_.size();
    mean_iterations /= n;
    mean_time /= n;
    mean_rate /= n;

    double var_iterations = 0.0;
    double var_time = 0.0;

    for (const auto& m : metrics_history_) {
        var_iterations += (m.num_iterations - mean_iterations) *
                         (m.num_iterations - mean_iterations);
        var_time += (m.total_time.count() - mean_time) *
                   (m.total_time.count() - mean_time);
    }

    var_iterations /= n;
    var_time /= n;

    return PerformanceStats{
        mean_iterations,
        std::sqrt(var_iterations),
        mean_time,
        std::sqrt(var_time),
        mean_rate
    };
}

void PerformanceAnalyzer::print_detailed_report() const {
    std::cout << "\n=== Detailed Performance Report ===\n";

    auto stats = get_statistics();
    std::cout << "Iterations: " << std::fixed << std::setprecision(1)
              << stats.mean_iterations << " ± " << stats.std_iterations << "\n";
    std::cout << "Time: " << stats.mean_time_ms << " ± " << stats.std_time_ms << " ms\n";
    std::cout << "Convergence Rate: " << stats.mean_convergence_rate << "\n";
}

void PerformanceAnalyzer::reset() {
    metrics_history_.clear();
}

// ============================================================================
// Global Metrics
// ============================================================================

GlobalMetrics& GlobalMetrics::instance() {
    static GlobalMetrics instance;
    return instance;
}

void GlobalMetrics::record_metrics(const std::string& solver_name,
                                   const SolverMetrics& metrics) {
    analyzers_[solver_name].record_solve_metrics(metrics);
}

const SolverMetrics& GlobalMetrics::get_last_metrics(
    const std::string& solver_name) const {

    auto it = analyzers_.find(solver_name);
    if (it != analyzers_.end()) {
        return SolverMetrics();  // Simplified
    }
    return SolverMetrics();
}

PerformanceAnalyzer& GlobalMetrics::get_analyzer(const std::string& solver_name) {
    return analyzers_[solver_name];
}

void GlobalMetrics::print_all_metrics() const {
    for (const auto& [name, analyzer] : analyzers_) {
        std::cout << "\n=== " << name << " ===\n";
        analyzer.print_detailed_report();
    }
}

}  // namespace bendcore::solver
