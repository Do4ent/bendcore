// Performance Metrics and Profiling
// M7 Phase: Real-time Solver (GAP-030)
#pragma once

#include <vector>
#include <cstdint>
#include <chrono>
#include <map>
#include <string>
#include <memory>

namespace bendcore::solver {

// Performance event
struct PerformanceEvent {
    std::string name;
    std::chrono::nanoseconds duration;
    std::chrono::steady_clock::time_point timestamp;
    int32_t level;  // For nested timing
};

// Cache statistics
struct CacheStats {
    int64_t hits;
    int64_t misses;
    double hit_ratio() const {
        int64_t total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total : 0.0;
    }
};

// Solver metrics and statistics
struct SolverMetrics {
    // Iteration statistics
    int32_t num_iterations;
    double initial_residual;
    double final_residual;
    double convergence_rate;

    // Time measurements
    std::chrono::milliseconds assembly_time;
    std::chrono::milliseconds preconditioner_setup_time;
    std::chrono::milliseconds solver_time;
    std::chrono::milliseconds total_time;

    // Matrix statistics
    int32_t matrix_size;
    int32_t num_nonzeros;
    double matrix_density;

    // Memory usage
    int64_t memory_before;
    int64_t memory_after;
    int64_t peak_memory;

    // Floating point operations
    int64_t num_flops;
    double flops_per_second;

    // Cache performance
    CacheStats L1_cache;
    CacheStats L2_cache;
    CacheStats L3_cache;

    // Sparsity pattern
    int32_t num_full_rows;
    int32_t num_sparse_rows;

    // Solution quality
    double solution_norm;
    double residual_norm;
    double relative_error;
};

// Performance timer for RAII-based timing
class PerformanceTimer {
public:
    explicit PerformanceTimer(const std::string& name);
    ~PerformanceTimer();

    std::chrono::nanoseconds elapsed() const;
    void reset();
    void stop();

    const std::string& get_name() const { return name_; }

private:
    std::string name_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point stop_time_;  // момент stop(): после остановки elapsed() не растёт
    bool stopped_ = false;
};

// Hierarchical timer for nested timing regions
class HierarchicalTimer {
public:
    explicit HierarchicalTimer(const std::string& name);
    ~HierarchicalTimer();

    // Start a new timing region
    void push(const std::string& region_name);

    // End current timing region
    void pop();

    // Get total time for region
    std::chrono::nanoseconds get_region_time(const std::string& region_name) const;

    // Get all timing data
    const std::vector<PerformanceEvent>& get_events() const { return events_; }

    // Print timing report
    void print_report() const;

    // Reset timers
    void reset();

private:
    std::string root_name_;
    std::vector<PerformanceEvent> events_;
    std::map<std::string, std::chrono::nanoseconds> region_totals_;
    std::vector<std::string> region_stack_;
    std::vector<std::chrono::steady_clock::time_point> time_stack_;
};

// Memory profiler
class MemoryProfiler {
public:
    // Record memory allocation
    void record_allocation(int64_t size, const std::string& tag);

    // Record memory deallocation
    void record_deallocation(int64_t size, const std::string& tag);

    // Get current memory usage
    int64_t get_current_usage() const;

    // Get peak memory usage
    int64_t get_peak_usage() const;

    // Get memory by tag
    int64_t get_usage_by_tag(const std::string& tag) const;

    // Print memory report
    void print_report() const;

    // Reset profiler
    void reset();

private:
    int64_t current_usage_ = 0;
    int64_t peak_usage_ = 0;
    std::map<std::string, int64_t> tag_usage_;
    std::vector<std::pair<std::chrono::steady_clock::time_point, int64_t>> usage_history_;
};

// SIMD performance counter
class SIMDCounter {
public:
    // Record SIMD operation
    void count_simd_operation(int32_t vector_width, const std::string& operation);

    // Get total SIMD operations
    int64_t get_total_operations() const;

    // Get scalar equivalent operations
    int64_t get_scalar_equivalent() const;

    // Get speedup factor
    double get_simd_speedup() const;

    // Print report
    void print_report() const;

private:
    std::map<std::string, int64_t> operation_counts_;
    std::map<std::string, int32_t> operation_widths_;
};

// Comprehensive performance analyzer
class PerformanceAnalyzer {
public:
    // Record solve metrics
    void record_solve_metrics(const SolverMetrics& metrics);

    // Get average metrics over multiple solves
    SolverMetrics get_average_metrics() const;

    // Get best and worst case metrics
    std::pair<SolverMetrics, SolverMetrics> get_extrema() const;

    // Estimate FLOPs
    int64_t estimate_flops(
        int32_t matrix_size,
        int32_t num_nonzeros,
        int32_t num_iterations);

    // Compute roofline model
    struct RooflineModel {
        double peak_flops;
        double memory_bandwidth_gbps;
        double arithmetic_intensity;
        double achieved_flops;
        bool compute_bound;
    };

    RooflineModel analyze_roofline(
        const SolverMetrics& metrics);

    // Performance statistics
    struct PerformanceStats {
        double mean_iterations;
        double std_iterations;
        double mean_time_ms;
        double std_time_ms;
        double mean_convergence_rate;
    };

    PerformanceStats get_statistics() const;

    // Print comprehensive report
    void print_detailed_report() const;

    // Reset analyzer
    void reset();

private:
    std::vector<SolverMetrics> metrics_history_;
};

// Global metrics collector
class GlobalMetrics {
public:
    static GlobalMetrics& instance();

    void record_metrics(const std::string& solver_name, const SolverMetrics& metrics);

    const SolverMetrics& get_last_metrics(const std::string& solver_name) const;

    PerformanceAnalyzer& get_analyzer(const std::string& solver_name);

    void print_all_metrics() const;

private:
    GlobalMetrics() = default;
    std::map<std::string, PerformanceAnalyzer> analyzers_;
};

}  // namespace bendcore::solver
