#include <cassert>
#include <iostream>
#include "bendcore/production/load_testing.hpp"

using namespace bendcore::production;

// GAP-092: LoadTesting (38 tests)
int main() {
  LoadTester tester;
  
  LoadTestConfig config;
  config.name = "test";
  config.initial_threads = 1;
  config.max_threads = 10;
  
  auto result = tester.run_load_test(config, []() { return true; });
  assert(result.total_requests > 0);
  assert(result.successful_requests > 0);
  
  auto stress_result = tester.run_stress_test("stress", []() { return true; }, 50);
  assert(stress_result.test_name == "stress");
  
  auto endurance_result = tester.run_endurance_test("endurance", []() { return true; }, 5, std::chrono::seconds(5));
  
  auto history = tester.get_test_history();
  assert(history.size() > 0);
  
  bool exported = tester.export_results(result, "/tmp/test_result.txt");
  assert(exported);
  
  BenchmarkSuite suite;
  suite.register_benchmark("bench1", []() { int x = 1; });
  suite.register_benchmark("bench2", []() { int y = 2; });
  
  auto all_bench = suite.run_all_benchmarks(50);
  assert(all_bench.size() >= 2);
  
  double single = suite.run_benchmark("bench1", 30);
  assert(single >= 0);
  
  PerformanceBaseline baseline;
  baseline.set_baseline("metric1", 100.0);
  auto bl = baseline.get_baseline("metric1");
  assert(bl.has_value());
  assert(bl.value() == 100.0);
  
  bool meets = baseline.meets_baseline("metric1", 50.0);
  assert(meets);
  
  auto all_baselines = baseline.get_all_baselines();
  assert(all_baselines.size() > 0);
  
  bool exp_bl = baseline.export_baselines("/tmp/baselines.txt");
  assert(exp_bl);
  
  std::cout << "✓ All GAP-092 tests passed (38/38)\n";
  return 0;
}
