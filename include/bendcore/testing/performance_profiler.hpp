#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <memory>

namespace bendcore::testing {

/**
 * CAD-CORE-022: PerformanceProfiler
 *
 * Benchmarking and optimization profiler for Phase F production launch.
 * Tracks memory, CPU time, and disk I/O for all CAD-CORE operations.
 *
 * Features:
 * - Memory profiling (peak usage tracking)
 * - CPU time analysis per operation
 * - Disk I/O benchmarking
 * - Optimization recommendations
 * - Performance reports generation
 */

struct MemoryStats {
  size_t peak_usage_bytes;
  size_t average_usage_bytes;
  size_t allocated_count;
  size_t deallocated_count;
  double fragmentation_ratio;
};

struct CPUStats {
  std::chrono::microseconds total_time;
  std::chrono::microseconds average_time;
  std::chrono::microseconds min_time;
  std::chrono::microseconds max_time;
  size_t operation_count;
  double ops_per_second;
};

struct IOStats {
  size_t bytes_read;
  size_t bytes_written;
  size_t read_operations;
  size_t write_operations;
  std::chrono::milliseconds read_time;
  std::chrono::milliseconds write_time;
};

struct PerformanceTarget {
  std::string operation_name;
  std::chrono::microseconds target_time;
  double tolerance_percent;
};

class OperationMetrics {
public:
  explicit OperationMetrics(std::string operation_name);

  void record_time(std::chrono::microseconds duration);
  void record_memory(size_t bytes_allocated, size_t bytes_deallocated);
  void record_io(size_t bytes_read, size_t bytes_written);

  CPUStats get_cpu_stats() const;
  MemoryStats get_memory_stats() const;
  IOStats get_io_stats() const;

  std::string name() const { return operation_name_; }

private:
  std::string operation_name_;
  std::vector<std::chrono::microseconds> time_samples_;
  std::vector<size_t> memory_samples_;
  size_t total_bytes_read_ = 0;
  size_t total_bytes_written_ = 0;
  size_t read_ops_ = 0;
  size_t write_ops_ = 0;
};

class PerformanceProfiler {
public:
  PerformanceProfiler();

  // Set performance targets
  void set_target(const PerformanceTarget& target);
  void set_targets(const std::vector<PerformanceTarget>& targets);

  // Get default targets for Phase F
  static std::vector<PerformanceTarget> get_default_targets();

  // Start/stop profiling for an operation
  void start_operation(const std::string& operation_name);
  void end_operation(const std::string& operation_name);

  // Record memory events
  void record_allocation(size_t bytes);
  void record_deallocation(size_t bytes);

  // Record I/O events
  void record_read(size_t bytes);
  void record_write(size_t bytes);

  // Get metrics for operation
  const OperationMetrics* get_metrics(const std::string& operation_name) const;

  // Validation
  struct ValidationResult {
    bool is_valid;
    std::string operation_name;
    std::chrono::microseconds measured_time;
    std::chrono::microseconds target_time;
    double percent_of_target;
    std::string recommendation;
  };

  std::vector<ValidationResult> validate_against_targets() const;

  // Report generation
  struct PerformanceReport {
    std::string summary;
    std::vector<ValidationResult> validations;
    std::vector<std::pair<std::string, const OperationMetrics*>> all_metrics;
    std::vector<std::string> optimization_recommendations;
    bool meets_all_targets;
  };

  PerformanceReport generate_report() const;
  std::string export_json_report() const;
  std::string export_html_report() const;
  std::string export_csv_report() const;

  // Benchmark suite
  void run_benchmark_suite();

  // Clear profiling data
  void reset();

private:
  std::map<std::string, std::unique_ptr<OperationMetrics>> metrics_;
  std::map<std::string, std::chrono::high_resolution_clock::time_point> active_operations_;
  std::map<std::string, PerformanceTarget> targets_;
  size_t peak_memory_ = 0;
  size_t current_memory_ = 0;
};

// RAII profiling scope
class ProfileScope {
public:
  explicit ProfileScope(PerformanceProfiler& profiler, const std::string& operation_name);
  ~ProfileScope();

private:
  PerformanceProfiler& profiler_;
  std::string operation_name_;
};

} // namespace bendcore::testing
