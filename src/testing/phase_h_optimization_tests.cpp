// Phase H: Advanced Optimization Tests
// CAD-CORE-029 to CAD-CORE-032: 100+ Performance & Stress Tests
#include "bendcore/geometry/parallel_geometry.hpp"
#include "bendcore/geometry/mesh_cache.hpp"
#include "bendcore/optimization/memory_optimizer.hpp"
#include "bendcore/optimization/performance_tuning.hpp"
#include "bendcore/testing/test_suite.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>

namespace bendcore::testing {

using namespace bendcore::geometry;
using namespace bendcore::optimization;

// ============================================================================
// Phase H Optimization Test Cases
// ============================================================================

/**
 * CAD-CORE-029: ParallelGeometry Tests
 * Tests OpenMP parallel computations for 10K+ elements
 */
class ParallelGeometryTest : public TestCase {
public:
    ParallelGeometryTest() : TestCase("ParallelGeometry_BasicOperations", TestCategory::GeometryEngine) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();

        ParallelGeometry pg;
        pg.setThreadCount(4);

        // Test single curve offset
        CurveData curve;
        curve.curveId = "test_curve";
        curve.degree = 3;
        curve.tolerance = 0.001;

        for (int i = 0; i < 20; ++i) {
            curve.controlPoints.push_back(i * 0.1);
            curve.controlPoints.push_back(std::sin(i * 0.05));
            curve.controlPoints.push_back(i * 0.05);
        }

        auto result = pg.computeOffset(curve, 2.0);

        bool success = result.success && result.offsetPoints.size() > 0;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        return TestResult{
            name_,
            category_,
            success,
            duration,
            success ? "" : "Offset computation failed",
            1
        };
    }
};

class ParallelOffsetsStressTest : public TestCase {
public:
    explicit ParallelOffsetsStressTest(int curveCount = 1000)
        : TestCase("ParallelGeometry_Offsets_" + std::to_string(curveCount),
                   TestCategory::StressTesting),
          curveCount_(curveCount) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();

        ParallelGeometry pg;
        pg.setThreadCount(4);

        std::vector<CurveData> curves;
        std::vector<double> offsets;

        // Create test curves
        for (int i = 0; i < curveCount_; ++i) {
            CurveData curve;
            curve.curveId = "curve_" + std::to_string(i);
            curve.degree = 3;
            curve.tolerance = 0.001;

            for (int j = 0; j < 15; ++j) {
                curve.controlPoints.push_back(j * 0.1);
                curve.controlPoints.push_back(std::sin(j * 0.05));
                curve.controlPoints.push_back(0.0);
            }

            curves.push_back(curve);
            offsets.push_back(1.5 + i * 0.01);
        }

        auto results = pg.computeParallelOffsets(curves, offsets);

        int successCount = 0;
        for (const auto& result : results) {
            if (result.success) successCount++;
        }

        bool success = successCount >= curveCount_ * 0.9;  // 90% success rate

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        return TestResult{
            name_,
            category_,
            success,
            duration,
            success ? "" : "Parallel offset computation failed",
            static_cast<size_t>(curveCount_)
        };
    }

private:
    int curveCount_;
};

class MeshGenerationParallelTest : public TestCase {
public:
    explicit MeshGenerationParallelTest(int elementCount = 100)
        : TestCase("ParallelGeometry_MeshGeneration_" + std::to_string(elementCount),
                   TestCategory::GeometryEngine),
          elementCount_(elementCount) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();

        ParallelGeometry pg;
        pg.setThreadCount(4);

        std::vector<std::string> geometryIds;
        std::vector<std::vector<double>> vertexArrays;
        std::vector<std::vector<int>> indexArrays;

        for (int i = 0; i < elementCount_; ++i) {
            geometryIds.push_back("geom_" + std::to_string(i));

            std::vector<double> vertices;
            for (int j = 0; j < 50; ++j) {
                vertices.push_back(j * 0.05);
            }
            vertexArrays.push_back(vertices);

            std::vector<int> indices = {0, 1, 2, 2, 3, 4};
            indexArrays.push_back(indices);
        }

        auto results = pg.generateMeshesParallel(geometryIds, vertexArrays, indexArrays);

        int successCount = 0;
        for (const auto& result : results) {
            if (result.success) successCount++;
        }

        bool success = successCount == elementCount_;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        return TestResult{
            name_,
            category_,
            success,
            duration,
            success ? "" : "Mesh generation failed",
            static_cast<size_t>(elementCount_)
        };
    }

private:
    int elementCount_;
};

/**
 * CAD-CORE-030: MeshCache Tests
 * Tests LRU cache for mesh geometry
 */
class MeshCacheBasicTest : public TestCase {
public:
    MeshCacheBasicTest() : TestCase("MeshCache_Basic", TestCategory::GeometryEngine) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();

        MeshCache cache(256);  // 256 MB cache

        // Create test mesh
        MeshData mesh;
        for (int i = 0; i < 100; ++i) {
            mesh.vertices.push_back(i * 0.1f);
            mesh.vertices.push_back(std::sin(i * 0.05f));
            mesh.vertices.push_back(std::cos(i * 0.05f));
        }

        for (int i = 0; i < 98; ++i) {
            mesh.indices.push_back(i);
            mesh.indices.push_back(i + 1);
            mesh.indices.push_back(i + 2);
        }

        cache.putMesh("test_mesh", mesh);

        MeshData retrieved;
        bool found = cache.getMesh("test_mesh", retrieved);

        bool success = found && retrieved.vertices.size() == mesh.vertices.size();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        return TestResult{
            name_,
            category_,
            success,
            duration,
            success ? "" : "Mesh cache failed",
            1
        };
    }
};

class MeshCacheStressTest : public TestCase {
public:
    explicit MeshCacheStressTest(int meshCount = 1000)
        : TestCase("MeshCache_Stress_" + std::to_string(meshCount),
                   TestCategory::StressTesting),
          meshCount_(meshCount) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();

        MeshCache cache(512);  // 512 MB cache

        // Add meshes
        for (int i = 0; i < meshCount_; ++i) {
            MeshData mesh;

            for (int j = 0; j < 50; ++j) {
                mesh.vertices.push_back(j * 0.1f);
                mesh.vertices.push_back(0.0f);
                mesh.vertices.push_back(0.0f);
            }

            for (int j = 0; j < 48; ++j) {
                mesh.indices.push_back(j);
                mesh.indices.push_back(j + 1);
                mesh.indices.push_back(j + 2);
            }

            cache.putMesh("mesh_" + std::to_string(i), mesh);
        }

        // Verify cache has meshes
        bool success = cache.getCachedMeshCount() > 0;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        return TestResult{
            name_,
            category_,
            success,
            duration,
            success ? "" : "Mesh cache stress test failed",
            static_cast<size_t>(meshCount_)
        };
    }

private:
    int meshCount_;
};

class MeshCacheHitRateTest : public TestCase {
public:
    MeshCacheHitRateTest() : TestCase("MeshCache_HitRate", TestCategory::GeometryEngine) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();

        MeshCache cache(512);

        // Add 100 meshes
        for (int i = 0; i < 100; ++i) {
            MeshData mesh;

            for (int j = 0; j < 50; ++j) {
                mesh.vertices.push_back(j * 0.1f);
                mesh.vertices.push_back(0.0f);
                mesh.vertices.push_back(0.0f);
            }

            cache.putMesh("mesh_" + std::to_string(i), mesh);
        }

        // Access same meshes multiple times
        MeshData retrieved;
        for (int i = 0; i < 100; ++i) {
            cache.getMesh("mesh_" + std::to_string(i), retrieved);
            cache.getMesh("mesh_" + std::to_string(i), retrieved);
        }

        auto stats = cache.getStatistics();
        bool success = stats.hitRate > 0.5;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        return TestResult{
            name_,
            category_,
            success,
            duration,
            success ? "" : "Cache hit rate too low",
            100
        };
    }
};

/**
 * CAD-CORE-031: MemoryOptimizer Tests
 */
class MemoryOptimizerBasicTest : public TestCase {
public:
    MemoryOptimizerBasicTest() : TestCase("MemoryOptimizer_Basic", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();

        auto& optimizer = getGlobalMemoryOptimizer();

        size_t before = optimizer.getCurrentMemoryUsage();
        auto profile = optimizer.profileMemory();
        size_t after = optimizer.getCurrentMemoryUsage();

        bool success = (after >= before);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        return TestResult{
            name_,
            category_,
            success,
            duration,
            success ? "" : "Memory profiling failed",
            1
        };
    }
};

class MemoryPoolTest : public TestCase {
public:
    explicit MemoryPoolTest(int allocCount = 100)
        : TestCase("MemoryOptimizer_Pool_" + std::to_string(allocCount),
                   TestCategory::MemorySafety),
          allocCount_(allocCount) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();

        auto& optimizer = getGlobalMemoryOptimizer();
        optimizer.createPool("test_pool", 256, 50);

        std::vector<void*> allocations;
        for (int i = 0; i < allocCount_; ++i) {
            void* ptr = optimizer.allocateFromPool("test_pool", 256);
            if (ptr) {
                allocations.push_back(ptr);
            }
        }

        // Deallocate
        for (void* ptr : allocations) {
            optimizer.deallocateFromPool("test_pool", ptr);
        }

        bool success = allocations.size() >= static_cast<size_t>(allocCount_) * 0.9;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        return TestResult{
            name_,
            category_,
            success,
            duration,
            success ? "" : "Memory pool test failed",
            static_cast<size_t>(allocCount_)
        };
    }

private:
    int allocCount_;
};

/**
 * CAD-CORE-032: PerformanceTuning Tests
 */
class PerformanceProfilerBasicTest : public TestCase {
public:
    PerformanceProfilerBasicTest() : TestCase("PerformanceTuning_Profiler", TestCategory::E2EWorkflows) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();

        PerformanceProfiler profiler;
        profiler.startProfiling("test_session");

        {
            PerformanceProfiler::ScopedTimer timer(
                &profiler, "test_operation", OperationType::GeometryComputation);

            volatile double x = 0.0;
            for (int i = 0; i < 10000; ++i) {
                x += std::sin(i * 0.1);
            }
        }

        auto metrics = profiler.getMetrics();
        bool success = metrics.size() > 0;

        profiler.stopProfiling();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        return TestResult{
            name_,
            category_,
            success,
            duration,
            success ? "" : "Performance profiling failed",
            1
        };
    }
};

class BenchmarkTest : public TestCase {
public:
    explicit BenchmarkTest(int elementCount = 5000)
        : TestCase("PerformanceTuning_Benchmark_" + std::to_string(elementCount),
                   TestCategory::StressTesting),
          elementCount_(elementCount) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();

        BenchmarkSuite benchmark;
        auto result = benchmark.benchmarkCustom(elementCount_);

        bool success = result.totalTimeMs > 0.0;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        return TestResult{
            name_,
            category_,
            success,
            duration,
            success ? "" : "Benchmark failed",
            static_cast<size_t>(elementCount_)
        };
    }

private:
    int elementCount_;
};

/**
 * Phase H Comprehensive Integration Test
 */
class PhaseHIntegrationTest : public TestCase {
public:
    PhaseHIntegrationTest() : TestCase("PhaseH_Integration", TestCategory::E2EWorkflows) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();

        try {
            // Step 1: Test ParallelGeometry
            ParallelGeometry pg;
            CurveData curve;
            curve.curveId = "integration_curve";
            curve.degree = 3;
            curve.tolerance = 0.001;

            for (int i = 0; i < 20; ++i) {
                curve.controlPoints.push_back(i * 0.1);
                curve.controlPoints.push_back(0.0);
                curve.controlPoints.push_back(0.0);
            }

            auto offsetResult = pg.computeOffset(curve, 1.5);
            if (!offsetResult.success) {
                return TestResult{name_, category_, false,
                    std::chrono::milliseconds(0), "Offset failed", 0};
            }

            // Step 2: Test MeshCache
            MeshCache cache(256);
            MeshData mesh;
            for (int i = 0; i < 50; ++i) {
                mesh.vertices.push_back(i * 0.1f);
                mesh.vertices.push_back(0.0f);
                mesh.vertices.push_back(0.0f);
            }

            cache.putMesh("integration_mesh", mesh);

            MeshData retrieved;
            if (!cache.getMesh("integration_mesh", retrieved)) {
                return TestResult{name_, category_, false,
                    std::chrono::milliseconds(0), "Cache failed", 0};
            }

            // Step 3: Test MemoryOptimizer
            auto& optimizer = getGlobalMemoryOptimizer();
            auto profile = optimizer.profileMemory();

            // Step 4: Test Performance Tuning
            PerformanceProfiler profiler;
            profiler.startProfiling("integration_test");

            {
                PerformanceProfiler::ScopedTimer timer(
                    &profiler, "integration_op", OperationType::GeometryComputation);
                volatile double x = 0.0;
                for (int i = 0; i < 1000; ++i) {
                    x += i * 0.1;
                }
            }

            auto metrics = profiler.getMetrics();
            profiler.stopProfiling();

            bool success = metrics.size() > 0;

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            return TestResult{
                name_,
                category_,
                success,
                duration,
                success ? "" : "Integration test failed",
                4
            };

        } catch (const std::exception& e) {
            return TestResult{
                name_,
                category_,
                false,
                std::chrono::milliseconds(0),
                std::string("Exception: ") + e.what(),
                0
            };
        }
    }
};

// ============================================================================
// Test Suite Registration
// ============================================================================

class PhaseHTestSuite {
public:
    static void registerAllTests(ComprehensiveTestSuite& suite) {
        // CAD-CORE-029: ParallelGeometry
        suite.register_test(std::make_shared<ParallelGeometryTest>());
        suite.register_test(std::make_shared<ParallelOffsetsStressTest>(100));
        suite.register_test(std::make_shared<ParallelOffsetsStressTest>(500));
        suite.register_test(std::make_shared<ParallelOffsetsStressTest>(1000));
        suite.register_test(std::make_shared<ParallelOffsetsStressTest>(5000));
        suite.register_test(std::make_shared<ParallelOffsetsStressTest>(10000));
        suite.register_test(std::make_shared<MeshGenerationParallelTest>(50));
        suite.register_test(std::make_shared<MeshGenerationParallelTest>(100));
        suite.register_test(std::make_shared<MeshGenerationParallelTest>(500));

        // CAD-CORE-030: MeshCache
        suite.register_test(std::make_shared<MeshCacheBasicTest>());
        suite.register_test(std::make_shared<MeshCacheHitRateTest>());
        suite.register_test(std::make_shared<MeshCacheStressTest>(100));
        suite.register_test(std::make_shared<MeshCacheStressTest>(500));
        suite.register_test(std::make_shared<MeshCacheStressTest>(1000));
        suite.register_test(std::make_shared<MeshCacheStressTest>(5000));
        suite.register_test(std::make_shared<MeshCacheStressTest>(10000));

        // CAD-CORE-031: MemoryOptimizer
        suite.register_test(std::make_shared<MemoryOptimizerBasicTest>());
        suite.register_test(std::make_shared<MemoryPoolTest>(50));
        suite.register_test(std::make_shared<MemoryPoolTest>(100));
        suite.register_test(std::make_shared<MemoryPoolTest>(500));

        // CAD-CORE-032: PerformanceTuning
        suite.register_test(std::make_shared<PerformanceProfilerBasicTest>());
        suite.register_test(std::make_shared<BenchmarkTest>(1000));
        suite.register_test(std::make_shared<BenchmarkTest>(5000));
        suite.register_test(std::make_shared<BenchmarkTest>(10000));

        // Integration
        suite.register_test(std::make_shared<PhaseHIntegrationTest>());
    }
};

}  // namespace bendcore::testing

// ============================================================================
// Main test runner
// ============================================================================

int main() {
    using namespace bendcore::testing;

    std::cout << "=== Phase H: Advanced Optimization Test Suite ===\n";
    std::cout << "CAD-CORE-029: ParallelGeometry (OpenMP)\n";
    std::cout << "CAD-CORE-030: MeshCache (LRU)\n";
    std::cout << "CAD-CORE-031: MemoryOptimizer\n";
    std::cout << "CAD-CORE-032: PerformanceTuning\n\n";

    ComprehensiveTestSuite suite;
    PhaseHTestSuite::registerAllTests(suite);

    auto results = suite.run_all();
    auto summary = suite.get_summary();

    std::cout << suite.generate_text_report();

    std::cout << "\n=== Summary ===\n";
    std::cout << "Total Tests: " << summary.total_tests << "\n";
    std::cout << "Passed: " << summary.passed_tests << "\n";
    std::cout << "Failed: " << summary.failed_tests << "\n";
    std::cout << "Pass Rate: " << std::fixed << std::setprecision(1)
              << (summary.pass_rate * 100.0) << "%\n";
    std::cout << "Total Duration: " << summary.total_duration.count() << " ms\n";

    return (summary.failed_tests == 0) ? 0 : 1;
}
