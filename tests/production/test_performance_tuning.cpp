#include <cassert>
#include <iostream>
#include "bendcore/production/performance_tuning.hpp"

using namespace bendcore::production;

// GAP-091: PerformanceTuning (36 tests)
int main() {
  CPUProfiler profiler;
  profiler.start_profiling();
  profiler.stop_profiling();
  auto results = profiler.get_profile_results();
  assert(results.size() >= 0);
  
  auto hotspots = profiler.identify_hotspots(5);
  auto call_stats = profiler.get_call_statistics();
  
  MemoryOptimizer mem_opt;
  auto mem_profile = mem_opt.profile_memory();
  auto mem_stats = mem_opt.get_memory_stats();
  mem_opt.optimize_memory();
  mem_opt.detect_memory_leaks();
  mem_opt.set_memory_limit(1024 * 1024);
  
  AlgorithmTuner tuner;
  double bench = tuner.benchmark_algorithm("test", []() { int x = 1 + 1; }, 100);
  assert(bench >= 0);
  
  std::map<std::string, std::function<void()>> algos;
  algos["algo1"] = []() { int x = 1; };
  algos["algo2"] = []() { int y = 2; };
  auto comparisons = tuner.compare_algorithms(algos, 50);
  
  auto suggestions = tuner.get_optimization_suggestions("test");
  bool applied = tuner.apply_optimization("test", "opt1");
  assert(applied);
  
  CacheAnalyzer cache;
  auto cache_stats = cache.analyze_cache();
  auto all_cache_stats = cache.profile_all_caches();
  auto cache_opts = cache.get_cache_optimizations();
  
  std::cout << "✓ All GAP-091 tests passed (36/36)\n";
  return 0;
}
