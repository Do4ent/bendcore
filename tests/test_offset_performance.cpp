#include <gtest/gtest.h>
#include "bendcore/parallel_offset.hpp"
#include "bendcore/parallel_offset_optimized.hpp"
#include "bendcore/combined_3d_offset.hpp"
#include "bendcore/combined_3d_offset_optimized.hpp"
#include "bendcore/offset_profiler.hpp"
#include <chrono>
#include <iostream>

using namespace bendcore;

// ============================================================================
// Тесты Parallel Offset Performance
// ============================================================================

class ParallelOffsetPerformanceTest : public ::testing::Test {
protected:
  ParallelOffsetCalculator::Parameters GetStandardParams() {
    return {
        .outer_diameter = 50.0,      // 50mm
        .wall_thickness = 2.0,       // 2mm стенка
        .bend_angle = 90.0,          // 90 градусов
        .offset_distance = 25.0,     // 25mm смещение
        .bend_radius = 100.0,        // 100mm радиус
        .material = "Steel"
    };
  }

  ParallelOffsetCalculator::Parameters GetHardParams() {
    return {
        .outer_diameter = 100.0,
        .wall_thickness = 3.0,
        .bend_angle = 45.0,
        .offset_distance = 50.0,
        .bend_radius = 150.0,
        .material = "Stainless Steel"
    };
  }
};

// Бенчмарк базовой версии
TEST_F(ParallelOffsetPerformanceTest, BaselinePerformance) {
  ParallelOffsetCalculator calc;
  OffsetProfiler profiler;

  auto params = GetStandardParams();

  profiler.start("baseline_calculate");
  for (int i = 0; i < 1000; ++i) {
    auto result = calc.calculate(params);
    EXPECT_TRUE(result.is_valid);
  }
  profiler.stop("baseline_calculate");

  auto stats = profiler.get_stats("baseline_calculate");
  std::cout << "\n═════ BASELINE PERFORMANCE ═════\n";
  std::cout << "1000 iterations of ParallelOffset.calculate()\n";
  std::cout << "Total: " << stats.total_time_ms << "ms\n";
  std::cout << "Average: " << stats.avg_time_ms << "ms per call\n";
  std::cout << "Min: " << stats.min_time_ms << "ms\n";
  std::cout << "Max: " << stats.max_time_ms << "ms\n";

  // Проверка что не слишком медленно
  EXPECT_LT(stats.avg_time_ms, 1.0);  // Менее 1ms на одну операцию
}

// Бенчмарк оптимизированной версии
TEST_F(ParallelOffsetPerformanceTest, OptimizedPerformance) {
  ParallelOffsetCalculatorOptimized calc;
  OffsetProfiler profiler;

  auto params = GetStandardParams();

  profiler.start("optimized_calculate");
  for (int i = 0; i < 1000; ++i) {
    auto result = calc.calculate(params);
    EXPECT_TRUE(result.is_valid);
  }
  profiler.stop("optimized_calculate");

  auto stats = profiler.get_stats("optimized_calculate");
  std::cout << "\n═════ OPTIMIZED PERFORMANCE ═════\n";
  std::cout << "1000 iterations of ParallelOffsetOptimized.calculate()\n";
  std::cout << "Total: " << stats.total_time_ms << "ms\n";
  std::cout << "Average: " << stats.avg_time_ms << "ms per call\n";
  std::cout << "Min: " << stats.min_time_ms << "ms\n";
  std::cout << "Max: " << stats.max_time_ms << "ms\n";

  EXPECT_LT(stats.avg_time_ms, 1.0);
}

// Сравнение производительности
TEST_F(ParallelOffsetPerformanceTest, ComparisonBaselineVsOptimized) {
  ParallelOffsetCalculator baseline;
  ParallelOffsetCalculatorOptimized optimized;
  OffsetProfiler profiler;

  auto params = GetStandardParams();
  int iterations = 10000;

  // Baseline
  profiler.start("baseline");
  for (int i = 0; i < iterations; ++i) {
    baseline.calculate(params);
  }
  profiler.stop("baseline");

  // Optimized
  profiler.start("optimized");
  for (int i = 0; i < iterations; ++i) {
    optimized.calculate(params);
  }
  profiler.stop("optimized");

  auto baseline_stats = profiler.get_stats("baseline");
  auto optimized_stats = profiler.get_stats("optimized");

  double improvement = ((baseline_stats.avg_time_ms - optimized_stats.avg_time_ms) /
                        baseline_stats.avg_time_ms) * 100.0;

  std::cout << "\n═════ COMPARISON ═════\n";
  std::cout << "Baseline avg:  " << baseline_stats.avg_time_ms << " ms\n";
  std::cout << "Optimized avg: " << optimized_stats.avg_time_ms << " ms\n";
  std::cout << "Improvement:   " << improvement << "%\n";

  // Ожидаем улучшение хотя бы 10%
  EXPECT_GT(improvement, 10.0) << "Optimization should provide at least 10% improvement";
}

// ============================================================================
// Тесты Combined 3D Offset Performance
// ============================================================================

class Combined3DOffsetPerformanceTest : public ::testing::Test {
protected:
  Combined3DOffsetCalculator::Parameters GetStandardParams() {
    return {
        .start_point = Point3D(0.0, 0.0, 0.0),
        .end_point = Point3D(1000.0, 500.0, 200.0),
        .outer_diameter = 50.0,
        .wall_thickness = 2.5,
        .bend_radius = 100.0,
        .num_bends = 3,
        .distribution = "equal",
        .inclination_angle = 15.0,
        .springback_correction = true,
        .material = "steel"
    };
  }

  Combined3DOffsetCalculator::Parameters GetComplexParams() {
    return {
        .start_point = Point3D(0.0, 0.0, 0.0),
        .end_point = Point3D(2000.0, 1000.0, 500.0),
        .outer_diameter = 75.0,
        .wall_thickness = 3.0,
        .bend_radius = 150.0,
        .num_bends = 5,
        .distribution = "custom",
        .custom_bend_positions = {0.2, 0.4, 0.5, 0.7, 0.85},
        .inclination_angle = 30.0,
        .springback_correction = true,
        .material = "aluminum"
    };
  }
};

TEST_F(Combined3DOffsetPerformanceTest, BaselinePerformance) {
  Combined3DOffsetCalculator calc;
  OffsetProfiler profiler;

  auto params = GetStandardParams();

  profiler.start("baseline_3d_calculate");
  for (int i = 0; i < 500; ++i) {
    auto result = calc.calculate(params);
    EXPECT_TRUE(result.success);
  }
  profiler.stop("baseline_3d_calculate");

  auto stats = profiler.get_stats("baseline_3d_calculate");
  std::cout << "\n═════ BASELINE 3D PERFORMANCE ═════\n";
  std::cout << "500 iterations of Combined3DOffset.calculate()\n";
  std::cout << "Total: " << stats.total_time_ms << "ms\n";
  std::cout << "Average: " << stats.avg_time_ms << "ms per call\n";
  std::cout << "Min: " << stats.min_time_ms << "ms\n";
  std::cout << "Max: " << stats.max_time_ms << "ms\n";

  EXPECT_LT(stats.avg_time_ms, 2.0);  // Менее 2ms на одну операцию
}

TEST_F(Combined3DOffsetPerformanceTest, OptimizedPerformance) {
  Combined3DOffsetCalculatorOptimized calc;
  OffsetProfiler profiler;

  auto params = GetStandardParams();

  profiler.start("optimized_3d_calculate");
  for (int i = 0; i < 500; ++i) {
    auto result = calc.calculate(params);
    EXPECT_TRUE(result.success);
  }
  profiler.stop("optimized_3d_calculate");

  auto stats = profiler.get_stats("optimized_3d_calculate");
  std::cout << "\n═════ OPTIMIZED 3D PERFORMANCE ═════\n";
  std::cout << "500 iterations of Combined3DOffsetOptimized.calculate()\n";
  std::cout << "Total: " << stats.total_time_ms << "ms\n";
  std::cout << "Average: " << stats.avg_time_ms << "ms per call\n";
  std::cout << "Min: " << stats.min_time_ms << "ms\n";
  std::cout << "Max: " << stats.max_time_ms << "ms\n";

  EXPECT_LT(stats.avg_time_ms, 2.0);
}

TEST_F(Combined3DOffsetPerformanceTest, ComparisonBaselineVsOptimized) {
  Combined3DOffsetCalculator baseline;
  Combined3DOffsetCalculatorOptimized optimized;
  OffsetProfiler profiler;

  auto params = GetStandardParams();
  int iterations = 5000;

  profiler.start("baseline_3d");
  for (int i = 0; i < iterations; ++i) {
    baseline.calculate(params);
  }
  profiler.stop("baseline_3d");

  profiler.start("optimized_3d");
  for (int i = 0; i < iterations; ++i) {
    optimized.calculate(params);
  }
  profiler.stop("optimized_3d");

  auto baseline_stats = profiler.get_stats("baseline_3d");
  auto optimized_stats = profiler.get_stats("optimized_3d");

  double improvement = ((baseline_stats.avg_time_ms - optimized_stats.avg_time_ms) /
                        baseline_stats.avg_time_ms) * 100.0;

  std::cout << "\n═════ 3D COMPARISON ═════\n";
  std::cout << "Baseline avg:  " << baseline_stats.avg_time_ms << " ms\n";
  std::cout << "Optimized avg: " << optimized_stats.avg_time_ms << " ms\n";
  std::cout << "Improvement:   " << improvement << "%\n";

  EXPECT_GT(improvement, 15.0) << "3D Optimization should provide at least 15% improvement";
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(Combined3DOffsetPerformanceTest, StressTestComplexCalculations) {
  Combined3DOffsetCalculatorOptimized calc;
  OffsetProfiler profiler;

  auto params = GetComplexParams();

  profiler.start("stress_test");
  for (int i = 0; i < 1000; ++i) {
    auto result = calc.calculate(params);
    EXPECT_TRUE(result.success) << "Calculation failed in stress test";
  }
  profiler.stop("stress_test");

  auto stats = profiler.get_stats("stress_test");
  std::cout << "\n═════ STRESS TEST (complex params) ═════\n";
  std::cout << "1000 iterations with complex parameters\n";
  std::cout << "Average: " << stats.avg_time_ms << "ms per call\n";
  std::cout << "Max: " << stats.max_time_ms << "ms\n";

  EXPECT_LT(stats.avg_time_ms, 3.0);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
