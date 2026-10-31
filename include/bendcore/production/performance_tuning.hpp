#ifndef BENDCORE_PRODUCTION_PERFORMANCE_TUNING_HPP
#define BENDCORE_PRODUCTION_PERFORMANCE_TUNING_HPP

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>

namespace bendcore::production {

/**
 * Performance metrics
 */
struct PerformanceMetric {
  std::string name;
  double cpu_usage_percent;
  double memory_usage_bytes;
  double execution_time_ms;
  int64_t operation_count;
  double throughput_ops_per_sec;
  std::chrono::system_clock::time_point timestamp;
};

/**
 * CPU profiler for identifying bottlenecks
 */
class CPUProfiler {
 public:
  CPUProfiler();
  ~CPUProfiler();

  // Start profiling
  void start_profiling();

  // Stop profiling
  void stop_profiling();

  // Get profiling results
  std::map<std::string, PerformanceMetric> get_profile_results() const;

  // Identify hotspots
  std::vector<std::pair<std::string, double>> identify_hotspots(int top_n = 10) const;

  // Get function call statistics
  std::map<std::string, int> get_call_statistics() const;

 private:
  bool profiling_ = false;
  std::map<std::string, PerformanceMetric> profile_data_;
  mutable std::shared_mutex mutex_;
};

/**
 * Memory optimizer
 */
class MemoryOptimizer {
 public:
  MemoryOptimizer();
  ~MemoryOptimizer() = default;

  // Profile memory usage
  std::map<std::string, size_t> profile_memory() const;

  // Get memory statistics
  struct MemoryStats {
    size_t total_allocated;
    size_t total_freed;
    size_t current_usage;
    size_t peak_usage;
    double fragmentation_ratio;
  };

  MemoryStats get_memory_stats() const;

  // Optimize memory usage
  void optimize_memory();

  // Detect memory leaks
  std::vector<std::string> detect_memory_leaks() const;

  // Set memory limits
  void set_memory_limit(size_t limit_bytes);

 private:
  size_t memory_limit_ = 0;
  size_t peak_usage_ = 0;
};

/**
 * Algorithm tuner for performance optimization
 */
class AlgorithmTuner {
 public:
  AlgorithmTuner();
  ~AlgorithmTuner() = default;

  // Benchmark algorithm
  double benchmark_algorithm(const std::string& algorithm_name,
                           std::function<void()> algorithm_func,
                           int iterations = 1000);

  // Compare algorithms
  std::map<std::string, double> compare_algorithms(
    const std::map<std::string, std::function<void()>>& algorithms,
    int iterations = 1000);

  // Get optimization suggestions
  std::vector<std::string> get_optimization_suggestions(
    const std::string& algorithm_name) const;

  // Apply optimization
  bool apply_optimization(const std::string& algorithm_name,
                        const std::string& optimization);

 private:
  std::map<std::string, std::vector<double>> benchmark_results_;
  mutable std::shared_mutex mutex_;
};

/**
 * Cache efficiency analyzer
 */
class CacheAnalyzer {
 public:
  CacheAnalyzer();
  ~CacheAnalyzer() = default;

  // Get cache statistics
  struct CacheStats {
    size_t hits;
    size_t misses;
    double hit_ratio;
    size_t total_accesses;
  };

  CacheStats analyze_cache() const;

  // Profile cache usage
  std::map<std::string, CacheStats> profile_all_caches() const;

  // Optimize cache configuration
  std::map<std::string, std::string> get_cache_optimizations() const;

 private:
  std::map<std::string, size_t> cache_hits_;
  std::map<std::string, size_t> cache_misses_;
};

} // namespace bendcore::production

#endif // BENDCORE_PRODUCTION_PERFORMANCE_TUNING_HPP
