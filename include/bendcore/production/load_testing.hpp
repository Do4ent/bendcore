#ifndef BENDCORE_PRODUCTION_LOAD_TESTING_HPP
#define BENDCORE_PRODUCTION_LOAD_TESTING_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>

namespace bendcore::production {

/**
 * Load test configuration
 */
struct LoadTestConfig {
  std::string name;
  int initial_threads = 1;
  int max_threads = 100;
  int ramp_up_threads_per_sec = 10;
  std::chrono::seconds ramp_up_duration{60};
  std::chrono::seconds sustained_duration{120};
  std::chrono::seconds ramp_down_duration{30};
  std::string output_file;
};

/**
 * Load test result metrics
 */
struct LoadTestResult {
  std::string test_name;
  std::chrono::system_clock::time_point start_time;
  std::chrono::system_clock::time_point end_time;
  int total_requests = 0;
  int successful_requests = 0;
  int failed_requests = 0;
  double mean_response_time_ms = 0.0;
  double min_response_time_ms = 0.0;
  double max_response_time_ms = 0.0;
  double p95_response_time_ms = 0.0;
  double p99_response_time_ms = 0.0;
  double throughput_requests_per_sec = 0.0;
  double error_rate = 0.0;
  std::map<int, int> response_code_distribution;
};

/**
 * Load testing framework
 */
class LoadTester {
 public:
  using LoadFunction = std::function<bool()>;

  LoadTester();
  ~LoadTester();

  // Run load test with given function
  LoadTestResult run_load_test(const LoadTestConfig& config,
                             LoadFunction load_function);

  // Run stress test (progressively increase load until failure)
  LoadTestResult run_stress_test(const std::string& test_name,
                                LoadFunction load_function,
                                int max_threads = 1000);

  // Run endurance test (sustained load over time)
  LoadTestResult run_endurance_test(const std::string& test_name,
                                   LoadFunction load_function,
                                   int num_threads,
                                   std::chrono::seconds duration);

  // Get test history
  std::vector<LoadTestResult> get_test_history() const;

  // Export results to file
  bool export_results(const LoadTestResult& result,
                     const std::string& filepath) const;

 private:
  std::vector<LoadTestResult> test_history_;
  mutable std::shared_mutex mutex_;

  LoadTestResult execute_test(const LoadTestConfig& config,
                             LoadFunction load_function);
};

/**
 * Benchmark utilities
 */
class BenchmarkSuite {
 public:
  BenchmarkSuite();
  ~BenchmarkSuite() = default;

  // Register benchmark
  void register_benchmark(const std::string& name,
                        std::function<void()> benchmark_func);

  // Run all benchmarks
  std::map<std::string, double> run_all_benchmarks(int iterations = 1000);

  // Run specific benchmark
  double run_benchmark(const std::string& name, int iterations = 1000);

  // Compare benchmark results
  std::map<std::string, double> compare_results(
    const std::vector<std::map<std::string, double>>& results) const;

 private:
  std::map<std::string, std::function<void()>> benchmarks_;
};

/**
 * Performance baseline manager
 */
class PerformanceBaseline {
 public:
  PerformanceBaseline();
  ~PerformanceBaseline() = default;

  // Set baseline performance
  void set_baseline(const std::string& metric_name, double baseline_value);

  // Get baseline
  std::optional<double> get_baseline(const std::string& metric_name) const;

  // Check if performance meets baseline
  bool meets_baseline(const std::string& metric_name, double actual_value) const;

  // Get all baselines
  std::map<std::string, double> get_all_baselines() const;

  // Export baselines
  bool export_baselines(const std::string& filepath) const;

 private:
  std::map<std::string, double> baselines_;
  mutable std::shared_mutex mutex_;
};

} // namespace bendcore::production

#endif // BENDCORE_PRODUCTION_LOAD_TESTING_HPP
