#include "bendcore/production/load_testing.hpp"
#include <thread>
#include <algorithm>
#include <numeric>
#include <fstream>

namespace bendcore::production {

LoadTester::LoadTester() {}

LoadTester::~LoadTester() {}

LoadTestResult LoadTester::run_load_test(const LoadTestConfig& config,
                                       LoadFunction load_function) {
  LoadTestResult result{
    .test_name = config.name,
    .start_time = std::chrono::system_clock::now(),
    .total_requests = 0,
    .successful_requests = 0,
    .failed_requests = 0
  };

  // Simulate load test execution
  std::vector<double> response_times;
  int thread_count = config.initial_threads;

  // Ramp up phase
  auto ramp_up_start = std::chrono::steady_clock::now();
  while (thread_count < config.max_threads &&
         std::chrono::steady_clock::now() - ramp_up_start < config.ramp_up_duration) {
    for (int i = 0; i < 10; ++i) {
      auto req_start = std::chrono::high_resolution_clock::now();
      bool success = load_function();
      auto req_end = std::chrono::high_resolution_clock::now();

      result.total_requests++;
      if (success) {
        result.successful_requests++;
      } else {
        result.failed_requests++;
      }

      double response_time = std::chrono::duration<double, std::milli>(req_end - req_start).count();
      response_times.push_back(response_time);
    }
    thread_count += config.ramp_up_threads_per_sec;
  }

  // Sustained phase
  auto sustained_start = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - sustained_start < config.sustained_duration) {
    if (load_function()) {
      result.successful_requests++;
    } else {
      result.failed_requests++;
    }
    result.total_requests++;
  }

  result.end_time = std::chrono::system_clock::now();

  // Calculate statistics
  if (!response_times.empty()) {
    std::sort(response_times.begin(), response_times.end());
    result.mean_response_time_ms = std::accumulate(response_times.begin(),
                                                   response_times.end(), 0.0) / response_times.size();
    result.min_response_time_ms = response_times.front();
    result.max_response_time_ms = response_times.back();
    result.p95_response_time_ms = response_times[static_cast<size_t>(0.95 * response_times.size())];
    result.p99_response_time_ms = response_times[static_cast<size_t>(0.99 * response_times.size())];
  }

  auto duration_sec = std::chrono::duration<double>(
    std::chrono::system_clock::now() - result.start_time).count();
  result.throughput_requests_per_sec = result.total_requests / duration_sec;
  result.error_rate = (result.total_requests > 0) ?
    static_cast<double>(result.failed_requests) / result.total_requests : 0.0;

  {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    test_history_.push_back(result);
  }

  return result;
}

LoadTestResult LoadTester::run_stress_test(const std::string& test_name,
                                          LoadFunction load_function,
                                          int max_threads) {
  LoadTestConfig config{
    .name = test_name,
    .max_threads = max_threads,
    .sustained_duration = std::chrono::seconds(30)
  };
  return execute_test(config, load_function);
}

LoadTestResult LoadTester::run_endurance_test(const std::string& test_name,
                                            LoadFunction load_function,
                                            int num_threads,
                                            std::chrono::seconds duration) {
  LoadTestConfig config{
    .name = test_name,
    .initial_threads = num_threads,
    .max_threads = num_threads,
    .sustained_duration = duration
  };
  return execute_test(config, load_function);
}

LoadTestResult LoadTester::execute_test(const LoadTestConfig& config,
                                       LoadFunction load_function) {
  return run_load_test(config, load_function);
}

std::vector<LoadTestResult> LoadTester::get_test_history() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  return test_history_;
}

bool LoadTester::export_results(const LoadTestResult& result,
                               const std::string& filepath) const {
  std::ofstream file(filepath);
  if (!file.is_open()) return false;

  file << "Test: " << result.test_name << "\n";
  file << "Total Requests: " << result.total_requests << "\n";
  file << "Successful: " << result.successful_requests << "\n";
  file << "Failed: " << result.failed_requests << "\n";
  file << "Mean Response Time: " << result.mean_response_time_ms << " ms\n";
  file << "P95 Response Time: " << result.p95_response_time_ms << " ms\n";
  file << "Throughput: " << result.throughput_requests_per_sec << " req/sec\n";

  return true;
}

BenchmarkSuite::BenchmarkSuite() {}

void BenchmarkSuite::register_benchmark(const std::string& name,
                                       std::function<void()> benchmark_func) {
  benchmarks_[name] = benchmark_func;
}

std::map<std::string, double> BenchmarkSuite::run_all_benchmarks(int iterations) {
  std::map<std::string, double> results;
  for (const auto& pair : benchmarks_) {
    results[pair.first] = run_benchmark(pair.first, iterations);
  }
  return results;
}

double BenchmarkSuite::run_benchmark(const std::string& name, int iterations) {
  auto it = benchmarks_.find(name);
  if (it == benchmarks_.end()) return 0.0;

  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    it->second();
  }
  auto end = std::chrono::high_resolution_clock::now();

  return std::chrono::duration<double, std::milli>(end - start).count() / iterations;
}

std::map<std::string, double> BenchmarkSuite::compare_results(
    const std::vector<std::map<std::string, double>>&) const {
  return std::map<std::string, double>();
}

PerformanceBaseline::PerformanceBaseline() {}

void PerformanceBaseline::set_baseline(const std::string& metric_name, double baseline_value) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  baselines_[metric_name] = baseline_value;
}

std::optional<double> PerformanceBaseline::get_baseline(const std::string& metric_name) const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  auto it = baselines_.find(metric_name);
  if (it != baselines_.end()) {
    return it->second;
  }
  return std::nullopt;
}

bool PerformanceBaseline::meets_baseline(const std::string& metric_name, double actual_value) const {
  auto baseline = get_baseline(metric_name);
  if (!baseline) return true;
  return actual_value <= baseline.value();
}

std::map<std::string, double> PerformanceBaseline::get_all_baselines() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  return baselines_;
}

bool PerformanceBaseline::export_baselines(const std::string& filepath) const {
  std::ofstream file(filepath);
  if (!file.is_open()) return false;

  {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    for (const auto& pair : baselines_) {
      file << pair.first << ": " << pair.second << "\n";
    }
  }
  return true;
}

} // namespace bendcore::production
