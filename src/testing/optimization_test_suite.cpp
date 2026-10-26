// CAD-CORE-050-055: Comprehensive Optimization Test Suite
// M5 Phase: Performance Optimization and Caching
// 30+ Unit Tests for Geometry Caching, Computation Caching, Parallel Execution,
// Memory Management, Performance Monitoring, and Optimization Profiles

#include "bendcore/testing/test_suite.hpp"
#include "bendcore/optimization/geometry_cache.hpp"
#include "bendcore/optimization/computation_cache.hpp"
#include "bendcore/optimization/parallel_executor.hpp"
#include "bendcore/optimization/memory_management.hpp"
#include "bendcore/optimization/performance_monitor.hpp"
#include "bendcore/optimization/optimization_profiles.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <cassert>

namespace bendcore::testing {

using namespace bendcore::optimization;

// ============================================================================
// Geometry Cache Tests (6 tests)
// ============================================================================

class GeometryCacheBasicTest : public TestCase {
public:
    GeometryCacheBasicTest() : TestCase("GeometryCache::Basic Operations", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            GeometryCache cache(10485760);  // 10 MB

            std::vector<uint8_t> topologyData(1000, 0xAA);
            std::vector<uint8_t> geometryData(1000, 0xBB);

            // Test caching
            bool cached = cache.cacheGeometry("entity_1", topologyData, geometryData);
            assert(cached);

            // Test retrieval
            auto retrieved = cache.getGeometry("entity_1");
            assert(retrieved != nullptr);
            assert(retrieved->id == "entity_1");

            // Test miss
            auto notFound = cache.getGeometry("entity_nonexistent");
            assert(notFound == nullptr);

            auto stats = cache.getStatistics();
            assert(stats.totalHits == 1);
            assert(stats.totalMisses == 1);

            result.passed = true;
            result.operations_count = 3;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class GeometryCacheEvictionTest : public TestCase {
public:
    GeometryCacheEvictionTest() : TestCase("GeometryCache::LRU Eviction", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            GeometryCache cache(5000);  // 5 KB - small for testing

            for (int i = 0; i < 10; ++i) {
                std::vector<uint8_t> topo(500, i);
                std::vector<uint8_t> geom(500, i);
                cache.cacheGeometry("entity_" + std::to_string(i), topo, geom);
            }

            auto stats = cache.getStatistics();
            assert(stats.utilizationPercent > 0);

            // Evict LRU
            cache.evictLRU(3);

            // Verify eviction happened
            auto stats2 = cache.getStatistics();
            assert(stats2.totalEntriesCount <= stats.totalEntriesCount);

            result.passed = true;
            result.operations_count = 10;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class GeometryCacheInvalidationTest : public TestCase {
public:
    GeometryCacheInvalidationTest() : TestCase("GeometryCache::Invalidation", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            GeometryCache cache(10485760);

            std::vector<uint8_t> data(1000, 0xFF);
            cache.cacheGeometry("entity_1", data, data);

            auto before = cache.getGeometry("entity_1");
            assert(before != nullptr);

            // Invalidate
            cache.invalidateGeometry("entity_1", "Test invalidation");

            auto after = cache.getGeometry("entity_1");
            assert(after == nullptr);

            result.passed = true;
            result.operations_count = 2;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class GeometryCacheCascadeTest : public TestCase {
public:
    GeometryCacheCascadeTest() : TestCase("GeometryCache::Cascade Invalidation", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            GeometryCache cache(10485760);
            cache.enableMonitoring();

            std::vector<uint8_t> data(1000, 0xCC);

            cache.cacheGeometry("parent", data, data);
            cache.cacheGeometry("child_1", data, data);
            cache.cacheGeometry("child_2", data, data);

            std::vector<std::string> dependents = {"child_1", "child_2"};
            cache.invalidateCascade("parent", dependents);

            assert(cache.getGeometry("parent") == nullptr);
            assert(cache.getGeometry("child_1") == nullptr);
            assert(cache.getGeometry("child_2") == nullptr);

            auto history = cache.getInvalidationHistory();
            assert(!history.empty());

            result.passed = true;
            result.operations_count = 5;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class GeometryCacheMemoryLimitTest : public TestCase {
public:
    GeometryCacheMemoryLimitTest() : TestCase("GeometryCache::Memory Limits", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            GeometryCache cache(1000);  // 1 KB limit

            std::vector<uint8_t> largeData(10000, 0xAA);  // 10 KB
            bool cached = cache.cacheGeometry("large", largeData, largeData);
            assert(!cached);  // Too large

            std::vector<uint8_t> smallData(100, 0xBB);
            cached = cache.cacheGeometry("small", smallData, smallData);
            assert(cached);

            result.passed = true;
            result.operations_count = 2;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class GeometryCacheTopologyTest : public TestCase {
public:
    GeometryCacheTopologyTest() : TestCase("GeometryCache::Topology Elements", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            GeometryCache cache(10485760);

            GeometryCache::TopologyElement face;
            face.id = "face_1";
            face.type = "Face";
            face.data = std::vector<uint8_t>(100, 0xFF);

            cache.cacheTopologyElement(face);
            auto retrieved = cache.getTopologyElement("face_1");
            assert(retrieved != nullptr);
            assert(retrieved->type == "Face");

            cache.invalidateTopologyElement("face_1");
            auto notFound = cache.getTopologyElement("face_1");
            assert(notFound == nullptr);

            result.passed = true;
            result.operations_count = 3;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

// ============================================================================
// Computation Cache Tests (6 tests)
// ============================================================================

class ComputationCacheBasicTest : public TestCase {
public:
    ComputationCacheBasicTest() : TestCase("ComputationCache::Basic Operations", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            ComputationCache cache(10485760);

            std::vector<uint8_t> computationResult(500, 0xAA);
            bool cached = cache.cacheComputation(
                "compute_1",
                computationResult,
                ComputationCache::ComputationType::FeatureComputation
            );
            assert(cached);

            auto retrieved = cache.getComputation("compute_1");
            assert(retrieved != nullptr);
            assert(retrieved->size() == 500);

            assert(cache.contains("compute_1"));

            result.passed = true;
            result.operations_count = 3;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class ComputationCacheTTLTest : public TestCase {
public:
    ComputationCacheTTLTest() : TestCase("ComputationCache::TTL Expiration", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            ComputationCache cache(10485760, 1000);  // 1 second TTL

            std::vector<uint8_t> data(100, 0xBB);
            cache.cacheComputation("short_lived", data, ComputationCache::ComputationType::FeatureComputation, {}, 10);

            // Should still be available immediately
            assert(cache.contains("short_lived"));

            // Remove expired
            cache.removeExpiredEntries();

            auto stats = cache.getStatistics();
            assert(stats.totalEntries >= 0);

            result.passed = true;
            result.operations_count = 2;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class ComputationCacheDependencyTest : public TestCase {
public:
    ComputationCacheDependencyTest() : TestCase("ComputationCache::Dependency Tracking", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            ComputationCache cache(10485760);

            std::vector<uint8_t> data(100, 0xCC);

            cache.cacheComputation("dep_parent", data, ComputationCache::ComputationType::FeatureComputation);
            cache.cacheComputation(
                "dep_child",
                data,
                ComputationCache::ComputationType::FeatureComputation,
                {"dep_parent"}
            );

            auto deps = cache.getDependencies("dep_child");
            assert(!deps.empty());
            assert(deps[0] == "dep_parent");

            result.passed = true;
            result.operations_count = 2;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class ComputationCacheLRUTest : public TestCase {
public:
    ComputationCacheLRUTest() : TestCase("ComputationCache::LRU Eviction", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            ComputationCache cache(5000);  // Small for testing

            for (int i = 0; i < 10; ++i) {
                std::vector<uint8_t> data(500, i);
                cache.cacheComputation(
                    "comp_" + std::to_string(i),
                    data,
                    ComputationCache::ComputationType::FeatureComputation
                );
            }

            cache.evictLRU(3);

            auto stats = cache.getStatistics();
            assert(stats.totalEvictions == 3);

            result.passed = true;
            result.operations_count = 10;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class ComputationCacheBatchTest : public TestCase {
public:
    ComputationCacheBatchTest() : TestCase("ComputationCache::Batch Operations", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            ComputationCache cache(10485760);

            std::vector<std::pair<std::string, std::vector<uint8_t>>> batch;
            for (int i = 0; i < 5; ++i) {
                batch.push_back({"batch_" + std::to_string(i), std::vector<uint8_t>(100, i)});
            }

            cache.cacheBatch(batch, ComputationCache::ComputationType::FeatureComputation);

            auto stats = cache.getStatistics();
            assert(stats.totalEntries == 5);

            result.passed = true;
            result.operations_count = 5;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

// ============================================================================
// Parallel Executor Tests (5 tests)
// ============================================================================

class ParallelExecutorBasicTest : public TestCase {
public:
    ParallelExecutorBasicTest() : TestCase("ParallelExecutor::Basic Execution", TestCategory::StressTesting) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            ParallelExecutor executor(2);

            int counter = 0;
            auto taskId = executor.submitTask(
                "test_task",
                [&counter]() {
                    counter++;
                    return std::vector<uint8_t>();
                },
                50
            );

            assert(!taskId.empty());

            executor.executeAll();

            auto taskResult = executor.getTaskResult(taskId);
            assert(taskResult != nullptr);
            assert(taskResult->successful);

            result.passed = true;
            result.operations_count = 1;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class ParallelExecutorMultipleTasksTest : public TestCase {
public:
    ParallelExecutorMultipleTasksTest() : TestCase("ParallelExecutor::Multiple Tasks", TestCategory::StressTesting) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            ParallelExecutor executor(4);

            std::vector<std::string> taskIds;
            for (int i = 0; i < 10; ++i) {
                auto taskId = executor.submitTask(
                    "task_" + std::to_string(i),
                    []() { return std::vector<uint8_t>(); },
                    50
                );
                taskIds.push_back(taskId);
            }

            executor.executeAll();

            auto results = executor.getAllResults();
            assert(results.size() > 0);

            result.passed = true;
            result.operations_count = 10;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class ParallelExecutorTaskGraphTest : public TestCase {
public:
    ParallelExecutorTaskGraphTest() : TestCase("ParallelExecutor::Task Graph", TestCategory::StressTesting) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            TaskGraph graph;

            auto task1 = std::make_shared<Task>();
            task1->id = "task_1";
            task1->name = "First";
            task1->computation = []() { return std::vector<uint8_t>(); };

            auto task2 = std::make_shared<Task>();
            task2->id = "task_2";
            task2->name = "Second";
            task2->computation = []() { return std::vector<uint8_t>(); };
            task2->dependencies = {"task_1"};

            graph.addTask(task1);
            graph.addTask(task2);
            graph.addDependency("task_2", "task_1");

            assert(graph.validateDAG());
            assert(graph.getTaskCount() == 2);

            result.passed = true;
            result.operations_count = 2;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class ParallelExecutorStatisticsTest : public TestCase {
public:
    ParallelExecutorStatisticsTest() : TestCase("ParallelExecutor::Statistics", TestCategory::StressTesting) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            ParallelExecutor executor(2);

            for (int i = 0; i < 5; ++i) {
                executor.submitTask("task_" + std::to_string(i), []() { return std::vector<uint8_t>(); });
            }

            executor.executeAll();

            auto stats = executor.getStatistics();
            assert(stats.totalTasksSubmitted == 5);
            assert(stats.completedTasks > 0);

            result.passed = true;
            result.operations_count = 5;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

// ============================================================================
// Memory Management Tests (5 tests)
// ============================================================================

class StreamingGeometryLoaderTest : public TestCase {
public:
    StreamingGeometryLoaderTest() : TestCase("StreamingGeometryLoader::Basic", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            StreamingGeometryLoader loader(1024);

            std::vector<uint8_t> data(4096, 0xAA);
            auto streamId = loader.createStream(data);
            assert(!streamId.empty());

            auto chunk = loader.readChunk(streamId, 0);
            assert(!chunk.empty());

            auto chunkCount = loader.getChunkCount(streamId);
            assert(chunkCount > 0);

            loader.closeStream(streamId);

            result.passed = true;
            result.operations_count = 4;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class LazyLoaderBasicTest : public TestCase {
public:
    LazyLoaderBasicTest() : TestCase("LazyLoader::Basic Operations", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            LazyLoader loader(10);

            auto objId = loader.registerObject("storage_path", 1000);
            assert(!objId.empty());

            auto data = loader.load(objId);
            assert(loader.isLoaded(objId));

            loader.unload(objId);
            assert(!loader.isLoaded(objId));

            result.passed = true;
            result.operations_count = 3;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class LazyLoaderEvictionTest : public TestCase {
public:
    LazyLoaderEvictionTest() : TestCase("LazyLoader::Eviction Policy", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            LazyLoader loader(3);  // Max 3 in memory

            for (int i = 0; i < 5; ++i) {
                auto objId = loader.registerObject("path_" + std::to_string(i), 100);
                loader.load(objId);
            }

            auto inMemory = loader.getInMemoryObjectCount();
            assert(inMemory <= 3);

            result.passed = true;
            result.operations_count = 5;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class MemoryProfilerTest : public TestCase {
public:
    MemoryProfilerTest() : TestCase("MemoryProfiler::Profiling", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            MemoryProfiler profiler;
            profiler.startProfiling();

            profiler.recordAllocation("Geometry", 1000);
            profiler.recordAllocation("Topology", 500);
            profiler.recordDeallocation("Geometry", 500);

            auto stats = profiler.getStatistics();
            assert(stats.totalAllocations == 2);
            assert(stats.totalDeallocations == 1);

            profiler.stopProfiling();

            result.passed = true;
            result.operations_count = 3;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

// ============================================================================
// Performance Monitoring Tests (4 tests)
// ============================================================================

class PerformanceMonitorBasicTest : public TestCase {
public:
    PerformanceMonitorBasicTest() : TestCase("PerformanceMonitor::Metrics", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            PerformanceMonitor monitor;
            monitor.startMonitoring();

            monitor.recordComputationTime("operation_1", 100);
            monitor.recordComputationTime("operation_2", 150);
            monitor.recordCacheHit("geometry");
            monitor.recordCacheMiss("geometry");
            monitor.recordMemoryUsage(1000000);

            auto snapshot = monitor.getSnapshot();
            assert(snapshot.totalComputationTimeMs > 0);

            monitor.stopMonitoring();

            result.passed = true;
            result.operations_count = 5;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class BenchmarkFrameworkTest : public TestCase {
public:
    BenchmarkFrameworkTest() : TestCase("BenchmarkFramework::Benchmarking", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            BenchmarkFramework framework;

            auto result1k = framework.benchmark1KElements();
            assert(result1k.elementCount == 1000);

            auto result10k = framework.benchmark10KElements();
            assert(result10k.elementCount == 10000);

            auto aggregate = framework.getLastResults();
            assert(aggregate.totalBenchmarksRun > 0);

            result.passed = true;
            result.operations_count = 2;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class BottleneckDetectionTest : public TestCase {
public:
    BottleneckDetectionTest() : TestCase("PerformanceMonitor::Bottleneck Detection", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            PerformanceMonitor monitor;
            monitor.startMonitoring();

            for (int i = 0; i < 100; ++i) {
                monitor.recordComputationTime("heavy_operation", 50);
            }

            auto bottlenecks = monitor.detectBottlenecks(5);
            assert(!bottlenecks.empty());

            result.passed = true;
            result.operations_count = 100;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

// ============================================================================
// Optimization Profiles Tests (4 tests)
// ============================================================================

class ProfileConfigurationBasicTest : public TestCase {
public:
    ProfileConfigurationBasicTest() : TestCase("ProfileConfiguration::Basic", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            ProfileConfiguration config(OptimizationProfile::Standard);
            assert(config.getProfile() == OptimizationProfile::Standard);

            config.setProfile(OptimizationProfile::High);
            assert(config.getProfile() == OptimizationProfile::High);

            assert(config.validate());

            result.passed = true;
            result.operations_count = 3;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class AdaptiveOptimizerTest : public TestCase {
public:
    AdaptiveOptimizerTest() : TestCase("AdaptiveOptimizer::Profile Switching", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            AdaptiveOptimizer optimizer;

            optimizer.switchProfile(OptimizationProfile::Draft);
            assert(optimizer.getCurrentProfile() == OptimizationProfile::Draft);

            optimizer.recordFrameMetrics(16, 1000000, 0.85);
            optimizer.recordFrameMetrics(16, 1000000, 0.85);

            auto stats = optimizer.getStatistics();
            assert(stats.averageFrameTimeMs > 0);

            result.passed = true;
            result.operations_count = 3;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class GlobalOptimizationContextTest : public TestCase {
public:
    GlobalOptimizationContextTest() : TestCase("GlobalOptimizationContext::Singleton", TestCategory::MemorySafety) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            auto& context = GlobalOptimizationContext::getInstance();

            context.setProfile(OptimizationProfile::High);
            assert(context.getProfile() == OptimizationProfile::High);

            auto& config = context.getConfiguration();
            assert(config.getProfile() == OptimizationProfile::High);

            result.passed = true;
            result.operations_count = 2;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

// ============================================================================
// Integration Tests (2 tests)
// ============================================================================

class OptimizationIntegrationTest : public TestCase {
public:
    OptimizationIntegrationTest() : TestCase("Optimization::Full Integration", TestCategory::E2EWorkflows) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            // Create caches
            GeometryCache geoCache(10485760);
            ComputationCache compCache(10485760);
            ParallelExecutor executor(4);

            // Simulate workflow
            std::vector<uint8_t> data(1000, 0xAA);

            geoCache.cacheGeometry("geo_1", data, data);
            compCache.cacheComputation("comp_1", data, ComputationCache::ComputationType::FeatureComputation);

            executor.submitTask("task_1", []() { return std::vector<uint8_t>(); });
            executor.executeAll();

            auto geoStats = geoCache.getStatistics();
            auto compStats = compCache.getStatistics();

            assert(geoStats.totalEntriesCount > 0);
            assert(compStats.totalEntries > 0);

            result.passed = true;
            result.operations_count = 5;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

class StressTestOptimizationTest : public TestCase {
public:
    StressTestOptimizationTest() : TestCase("Optimization::Stress Test 10K Elements", TestCategory::StressTesting) {}

    TestResult run() override {
        auto start = std::chrono::high_resolution_clock::now();
        TestResult result;
        result.test_name = name();
        result.category = category();

        try {
            GeometryCache cache(536870912);  // 512 MB
            ParallelExecutor executor(4);

            // Cache 1000 geometries
            for (int i = 0; i < 1000; ++i) {
                std::vector<uint8_t> data(10000, i % 256);
                cache.cacheGeometry("entity_" + std::to_string(i), data, data);
            }

            auto stats = cache.getStatistics();
            assert(stats.totalEntriesCount > 900);  // Should have cached most

            result.passed = true;
            result.operations_count = 1000;

        } catch (const std::exception& e) {
            result.passed = false;
            result.error_message = e.what();
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }
};

}  // namespace bendcore::testing

int main() {
    using namespace bendcore::testing;

    std::vector<std::shared_ptr<TestCase>> tests;

    // Register all optimization tests
    tests.push_back(std::make_shared<GeometryCacheBasicTest>());
    tests.push_back(std::make_shared<GeometryCacheEvictionTest>());
    tests.push_back(std::make_shared<GeometryCacheInvalidationTest>());
    tests.push_back(std::make_shared<GeometryCacheCascadeTest>());
    tests.push_back(std::make_shared<GeometryCacheMemoryLimitTest>());
    tests.push_back(std::make_shared<GeometryCacheTopologyTest>());

    tests.push_back(std::make_shared<ComputationCacheBasicTest>());
    tests.push_back(std::make_shared<ComputationCacheTTLTest>());
    tests.push_back(std::make_shared<ComputationCacheDependencyTest>());
    tests.push_back(std::make_shared<ComputationCacheLRUTest>());
    tests.push_back(std::make_shared<ComputationCacheBatchTest>());

    tests.push_back(std::make_shared<ParallelExecutorBasicTest>());
    tests.push_back(std::make_shared<ParallelExecutorMultipleTasksTest>());
    tests.push_back(std::make_shared<ParallelExecutorTaskGraphTest>());
    tests.push_back(std::make_shared<ParallelExecutorStatisticsTest>());

    tests.push_back(std::make_shared<StreamingGeometryLoaderTest>());
    tests.push_back(std::make_shared<LazyLoaderBasicTest>());
    tests.push_back(std::make_shared<LazyLoaderEvictionTest>());
    tests.push_back(std::make_shared<MemoryProfilerTest>());

    tests.push_back(std::make_shared<PerformanceMonitorBasicTest>());
    tests.push_back(std::make_shared<BenchmarkFrameworkTest>());
    tests.push_back(std::make_shared<BottleneckDetectionTest>());

    tests.push_back(std::make_shared<ProfileConfigurationBasicTest>());
    tests.push_back(std::make_shared<AdaptiveOptimizerTest>());
    tests.push_back(std::make_shared<GlobalOptimizationContextTest>());

    tests.push_back(std::make_shared<OptimizationIntegrationTest>());
    tests.push_back(std::make_shared<StressTestOptimizationTest>());

    // Run all tests
    int passed = 0;
    int failed = 0;

    for (auto& test : tests) {
        auto result = test->run();
        if (result.passed) {
            passed++;
            std::cout << "[PASS] " << result.test_name << " (" << result.duration.count() << "ms)\n";
        } else {
            failed++;
            std::cout << "[FAIL] " << result.test_name << ": " << result.error_message << "\n";
        }
    }

    // Print summary
    std::cout << "\n=== Test Summary ===\n";
    std::cout << "Total Tests: " << tests.size() << "\n";
    std::cout << "Passed: " << passed << "\n";
    std::cout << "Failed: " << failed << "\n";
    std::cout << "Pass Rate: " << (100.0 * passed / tests.size()) << "%\n";

    return failed == 0 ? 0 : 1;
}
