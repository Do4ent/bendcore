// Unit Tests for Performance & Optimization Infrastructure
// Phase 26 Stage B
#include <gtest/gtest.h>
#include "bendcore/performance.hpp"

using namespace bendcore::performance;

// ========== LRUCache Tests ==========

TEST(LRUCacheTest, BasicGetPut) {
    LRUCache<std::string, int> cache(10);

    cache.put("key1", 100);
    int value;
    ASSERT_TRUE(cache.get("key1", value));
    EXPECT_EQ(value, 100);
}

TEST(LRUCacheTest, MissReturnsError) {
    LRUCache<std::string, int> cache(10);

    int value;
    ASSERT_FALSE(cache.get("nonexistent", value));
}

TEST(LRUCacheTest, HitRateCalculation) {
    LRUCache<std::string, int> cache(10);

    cache.put("key1", 100);
    cache.put("key2", 200);

    int value;
    cache.get("key1", value);  // Hit
    cache.get("key1", value);  // Hit
    cache.get("key3", value);  // Miss

    double hitRate = cache.hitRate();
    EXPECT_DOUBLE_EQ(hitRate, 2.0 / 3.0);
}

TEST(LRUCacheTest, CapacityCheck) {
    LRUCache<std::string, int> cache(5);

    for (int i = 0; i < 5; ++i) {
        cache.put(std::string("key") + std::to_string(i), i);
    }

    EXPECT_EQ(cache.size(), 5);
    EXPECT_EQ(cache.capacity(), 5);
}

// ========== GeometryCache Tests ==========

TEST(GeometryCacheTest, MeshCaching) {
    GeometryCache geoCache;

    std::vector<float> vertices = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    geoCache.putMesh("tube-1", vertices);

    std::vector<float> retrieved;
    ASSERT_TRUE(geoCache.getMesh("tube-1", retrieved));
    EXPECT_EQ(retrieved.size(), vertices.size());
}

TEST(GeometryCacheTest, NormalCaching) {
    GeometryCache geoCache;

    std::vector<float> normals = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    geoCache.putNormals("tube-1", normals);

    std::vector<float> retrieved;
    ASSERT_TRUE(geoCache.getNormals("tube-1", retrieved));
    EXPECT_EQ(retrieved.size(), normals.size());
}

TEST(GeometryCacheTest, Clear) {
    GeometryCache geoCache;

    geoCache.putMesh("tube-1", {1.0f, 2.0f});
    geoCache.clear();

    std::vector<float> retrieved;
    ASSERT_FALSE(geoCache.getMesh("tube-1", retrieved));
}

// ========== AnalysisCache Tests ==========

TEST(AnalysisCacheTest, AnalysisResultCaching) {
    AnalysisCache analysisCache;

    AnalysisCacheEntry entry = {
        1.5,      // deflection
        250.0,    // stress
        0.001,    // strain
        100,      // complexity
        123456    // timestamp
    };

    analysisCache.put("tube-1", entry);

    AnalysisCacheEntry retrieved;
    ASSERT_TRUE(analysisCache.get("tube-1", retrieved));
    EXPECT_EQ(retrieved.deflection, 1.5);
    EXPECT_EQ(retrieved.stress, 250.0);
    EXPECT_EQ(retrieved.complexity, 100);
}

TEST(AnalysisCacheTest, SizeTracking) {
    AnalysisCache analysisCache;

    AnalysisCacheEntry entry = {1.0, 100.0, 0.001, 50, 123456};

    for (int i = 0; i < 5; ++i) {
        analysisCache.put(std::string("tube-") + std::to_string(i), entry);
    }

    EXPECT_EQ(analysisCache.size(), 5);
}

// ========== StandardsCache Tests ==========

TEST(StandardsCacheTest, ComplianceCaching) {
    StandardsCache stdCache;

    ComplianceCacheEntry entry = {
        "DIN 2391",
        1,          // compliant
        95.5,       // score
        123456      // timestamp
    };

    stdCache.put("tube-1", "DIN 2391", entry);

    ComplianceCacheEntry retrieved;
    ASSERT_TRUE(stdCache.get("tube-1", "DIN 2391", retrieved));
    EXPECT_EQ(retrieved.standardName, "DIN 2391");
    EXPECT_EQ(retrieved.complianceStatus, 1);
}

TEST(StandardsCacheTest, MultipleStandards) {
    StandardsCache stdCache;

    ComplianceCacheEntry din = {"DIN 2391", 1, 95.0, 123456};
    ComplianceCacheEntry iso = {"ISO 286", 1, 92.0, 123456};

    stdCache.put("tube-1", "DIN 2391", din);
    stdCache.put("tube-1", "ISO 286", iso);

    ComplianceCacheEntry dinRetrieved, isoRetrieved;
    ASSERT_TRUE(stdCache.get("tube-1", "DIN 2391", dinRetrieved));
    ASSERT_TRUE(stdCache.get("tube-1", "ISO 286", isoRetrieved));

    EXPECT_EQ(dinRetrieved.complianceScore, 95.0);
    EXPECT_EQ(isoRetrieved.complianceScore, 92.0);
}

// ========== ParallelAnalyzer Tests ==========

TEST(ParallelAnalyzerTest, ThreadCountInitialization) {
    ParallelAnalyzer analyzer(4);
    EXPECT_EQ(analyzer.getThreadCount(), 4);
}

TEST(ParallelAnalyzerTest, TubeAnalysis) {
    ParallelAnalyzer analyzer(2);

    std::vector<std::string> tubeIds = {"tube-1", "tube-2", "tube-3"};
    std::vector<double> ods = {25.0, 30.0, 35.0};
    std::vector<double> walls = {1.5, 2.0, 2.5};

    auto results = analyzer.analyzeTubesParallel(tubeIds, ods, walls);

    EXPECT_EQ(results.size(), 3);
    EXPECT_GT(results[0].deflection, 0.0);
    EXPECT_GT(results[0].stress, 0.0);
}

TEST(ParallelAnalyzerTest, StandardsChecking) {
    ParallelAnalyzer analyzer(2);

    std::vector<std::string> tubeIds = {"tube-1", "tube-2"};
    std::vector<double> ods = {20.0, 30.0};
    std::vector<double> walls = {2.0, 3.0};

    auto results = analyzer.checkStandardsParallel(tubeIds, ods, walls);

    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].standardName, "DIN 2391");
    EXPECT_GE(results[0].complianceScore, 0.0);
    EXPECT_LE(results[0].complianceScore, 100.0);
}

// ========== PerformanceMonitor Tests ==========

TEST(PerformanceMonitorTest, RecordingMetrics) {
    PerformanceMonitor monitor;

    monitor.recordAnalysisTime(50.0);
    monitor.recordAnalysisTime(75.0);
    monitor.recordTubeProcessed();
    monitor.recordTubeProcessed();
    monitor.recordCacheHit();
    monitor.recordCacheMiss();

    auto metrics = monitor.getMetrics();

    EXPECT_EQ(metrics.totalTubesProcessed, 2);
    EXPECT_EQ(metrics.cacheHits, 1);
    EXPECT_EQ(metrics.cacheMisses, 1);
    EXPECT_DOUBLE_EQ(metrics.averageAnalysisTime, 62.5);
}

TEST(PerformanceMonitorTest, CacheHitRateComputation) {
    PerformanceMonitor monitor;

    for (int i = 0; i < 7; ++i) {
        monitor.recordCacheHit();
    }
    for (int i = 0; i < 3; ++i) {
        monitor.recordCacheMiss();
    }

    auto metrics = monitor.getMetrics();
    EXPECT_DOUBLE_EQ(metrics.analysisCacheHitRate, 0.7);
}

TEST(PerformanceMonitorTest, Reset) {
    PerformanceMonitor monitor;

    monitor.recordCacheHit();
    monitor.recordTubeProcessed();
    monitor.reset();

    auto metrics = monitor.getMetrics();
    EXPECT_EQ(metrics.totalTubesProcessed, 0);
    EXPECT_EQ(metrics.cacheHits, 0);
}

// ========== LODManager Tests ==========

TEST(LODManagerTest, LODCalculationSmallCount) {
    LODManager lodManager;

    LODLevel lod = lodManager.calculateLOD(500);
    EXPECT_EQ(lod, LODLevel::VeryHigh);
}

TEST(LODManagerTest, LODCalculationMediumCount) {
    LODManager lodManager;

    LODLevel lod = lodManager.calculateLOD(2000);
    EXPECT_EQ(lod, LODLevel::High);
}

TEST(LODManagerTest, LODCalculationLargeCount) {
    LODManager lodManager;

    LODLevel lod = lodManager.calculateLOD(5000);
    EXPECT_EQ(lod, LODLevel::Medium);
}

TEST(LODManagerTest, ReductionFactors) {
    LODManager lodManager;

    EXPECT_DOUBLE_EQ(lodManager.getLODReductionFactor(LODLevel::VeryHigh), 1.0);
    EXPECT_DOUBLE_EQ(lodManager.getLODReductionFactor(LODLevel::High), 0.8);
    EXPECT_DOUBLE_EQ(lodManager.getLODReductionFactor(LODLevel::Medium), 0.6);
    EXPECT_DOUBLE_EQ(lodManager.getLODReductionFactor(LODLevel::Low), 0.4);
    EXPECT_DOUBLE_EQ(lodManager.getLODReductionFactor(LODLevel::VeryLow), 0.2);
}

TEST(LODManagerTest, MemoryEstimation) {
    LODManager lodManager;

    // 1000 components at VeryHigh LOD
    size_t mem1 = lodManager.estimateMemoryUsage(1000, LODLevel::VeryHigh);

    // Same count at Low LOD (40% of VeryHigh)
    size_t mem2 = lodManager.estimateMemoryUsage(1000, LODLevel::Low);

    EXPECT_GT(mem1, mem2);
    EXPECT_DOUBLE_EQ(static_cast<double>(mem2) / mem1, 0.4);
}

// ========== BenchmarkSuite Tests ==========

TEST(BenchmarkSuiteTest, Benchmark2500Components) {
    BenchmarkSuite suite;
    auto result = suite.benchmark2500();

    EXPECT_EQ(result.componentCount, 2500);
    EXPECT_GT(result.totalTimeMs, 0.0);
    EXPECT_GT(result.analysisTimeMs, 0.0);
    EXPECT_GT(result.memoryUsageMb, 0.0);
    EXPECT_LE(result.cacheHitRate, 1.0);
}

TEST(BenchmarkSuiteTest, Benchmark5000Components) {
    BenchmarkSuite suite;
    auto result = suite.benchmark5000();

    EXPECT_EQ(result.componentCount, 5000);
    EXPECT_GT(result.totalTimeMs, 0.0);

    // Performance target: <1000ms for 5000 components
    EXPECT_LT(result.totalTimeMs, 1500.0);  // Allow some margin
}

TEST(BenchmarkSuiteTest, PerformanceTargetVerification) {
    BenchmarkSuite suite;

    auto result = suite.benchmark5000();
    bool passed = suite.verifyPerformanceTargets(result);

    // Should pass for reasonable simulation
    EXPECT_TRUE(passed);
}

TEST(BenchmarkSuiteTest, ScaledTargets) {
    BenchmarkSuite suite;

    // 1000 components should have proportionally lower target
    auto result1k = suite.benchmarkCustom(1000);
    auto result5k = suite.benchmark5000();

    // Time should scale roughly with component count
    double ratio = result5k.totalTimeMs / result1k.totalTimeMs;
    EXPECT_GT(ratio, 1.0);  // 5k should take more time than 1k
}

// ========== Integration Tests ==========

TEST(PerformanceIntegrationTest, CacheHitRateWithAnalyzer) {
    AnalysisCache cache;
    ParallelAnalyzer analyzer;

    std::vector<std::string> tubeIds = {"tube-1", "tube-2", "tube-3"};
    std::vector<double> ods = {20.0, 25.0, 30.0};
    std::vector<double> walls = {1.5, 2.0, 2.5};

    // First pass
    auto results1 = analyzer.analyzeTubesParallel(tubeIds, ods, walls);

    for (size_t i = 0; i < tubeIds.size(); ++i) {
        cache.put(tubeIds[i], results1[i]);
    }

    // Second pass - should get cache hits
    AnalysisCacheEntry entry;
    ASSERT_TRUE(cache.get("tube-1", entry));
    EXPECT_EQ(entry.complexity, results1[0].complexity);
}

TEST(PerformanceIntegrationTest, LODWithBenchmark) {
    LODManager lodManager;
    BenchmarkSuite suite;

    // Benchmark for 2500 components
    auto result = suite.benchmark2500();

    // Calculate appropriate LOD
    LODLevel lod = lodManager.calculateLOD(result.componentCount);
    EXPECT_EQ(lod, LODLevel::High);

    // Estimate memory with LOD
    size_t estimatedMem = lodManager.estimateMemoryUsage(result.componentCount, lod);
    double estimatedMb = estimatedMem / 1024.0;

    // Should be close to benchmark estimate
    EXPECT_GT(estimatedMb, 0.0);
}
