// Performance & Optimization Demo
// Phase 26 Stage B: Demonstrates caching, parallelization, and benchmarking

#include "bendcore/performance.hpp"
#include <iostream>
#include <iomanip>

using namespace bendcore::performance;

int main() {
    std::cout << "\n=== bendcore Performance & Optimization Demo ===\n\n";

    // ========== 1. Cache System Demo ==========
    {
        std::cout << "1. LRU Cache System:\n";
        LRUCache<std::string, int> cache(3);

        cache.put("tube-1", 100);
        cache.put("tube-2", 200);
        cache.put("tube-3", 300);

        int value;
        cache.get("tube-1", value);  // Hit
        cache.get("tube-2", value);  // Hit
        cache.get("tube-4", value);  // Miss
        cache.get("tube-1", value);  // Hit

        std::cout << "   Cache size: " << cache.size() << " / " << cache.capacity() << "\n";
        std::cout << "   Hit rate: " << std::fixed << std::setprecision(1)
                  << (cache.hitRate() * 100) << "%\n\n";
    }

    // ========== 2. Geometry Cache Demo ==========
    {
        std::cout << "2. Geometry Cache:\n";
        GeometryCache geoCache;

        std::vector<float> vertices = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 2.0f, 0.0f, 0.0f};
        geoCache.putMesh("tube-1", vertices);

        std::vector<float> normals = {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
        geoCache.putNormals("tube-1", normals);

        std::vector<float> retrievedVertices, retrievedNormals;
        geoCache.getMesh("tube-1", retrievedVertices);
        geoCache.getNormals("tube-1", retrievedNormals);

        std::cout << "   Cached mesh vertices: " << retrievedVertices.size() << "\n";
        std::cout << "   Cached normals: " << retrievedNormals.size() << "\n";
        std::cout << "   Geometry cache hit rate: " << std::fixed << std::setprecision(1)
                  << (geoCache.hitRate() * 100) << "%\n\n";
    }

    // ========== 3. Analysis Cache Demo ==========
    {
        std::cout << "3. Analysis Result Cache:\n";
        AnalysisCache analysisCache;

        for (int i = 0; i < 5; ++i) {
            AnalysisCacheEntry entry = {
                1.5 + i * 0.1,          // deflection
                250.0 + i * 10,         // stress
                0.001 + i * 0.0001,     // strain
                100 * (i + 1),          // complexity
                123456 + i              // timestamp
            };

            std::string tubeId = "tube-" + std::to_string(i);
            analysisCache.put(tubeId, entry);
        }

        std::cout << "   Cached analysis entries: " << analysisCache.size() << "\n";

        AnalysisCacheEntry entry;
        if (analysisCache.get("tube-2", entry)) {
            std::cout << "   Retrieved tube-2: deflection=" << std::fixed << std::setprecision(2)
                      << entry.deflection << " stress=" << entry.stress << "\n";
        }
        std::cout << "   Cache hit rate: " << (analysisCache.hitRate() * 100) << "%\n\n";
    }

    // ========== 4. Standards Compliance Cache Demo ==========
    {
        std::cout << "4. Standards Compliance Cache:\n";
        StandardsCache stdCache;

        ComplianceCacheEntry din = {
            "DIN 2391",
            1,          // compliant
            95.5,       // score
            123456      // timestamp
        };

        ComplianceCacheEntry iso = {
            "ISO 286",
            1,          // compliant
            92.0,       // score
            123456      // timestamp
        };

        stdCache.put("tube-1", "DIN 2391", din);
        stdCache.put("tube-1", "ISO 286", iso);

        ComplianceCacheEntry retrievedDin, retrievedIso;
        stdCache.get("tube-1", "DIN 2391", retrievedDin);
        stdCache.get("tube-1", "ISO 286", retrievedIso);

        std::cout << "   Standards cached for tube-1:\n";
        std::cout << "     - " << retrievedDin.standardName << ": " << retrievedDin.complianceScore << "% compliant\n";
        std::cout << "     - " << retrievedIso.standardName << ": " << retrievedIso.complianceScore << "% compliant\n";
        std::cout << "   Cache hit rate: " << (stdCache.hitRate() * 100) << "%\n\n";
    }

    // ========== 5. Parallel Analysis Demo ==========
    {
        std::cout << "5. Parallel Analyzer:\n";
        ParallelAnalyzer analyzer(4);

        std::vector<std::string> tubeIds = {"tube-1", "tube-2", "tube-3", "tube-4"};
        std::vector<double> ods = {20.0, 25.0, 30.0, 35.0};
        std::vector<double> walls = {1.5, 2.0, 2.5, 3.0};

        auto analysisResults = analyzer.analyzeTubesParallel(tubeIds, ods, walls);
        auto stdResults = analyzer.checkStandardsParallel(tubeIds, ods, walls);

        std::cout << "   Threads: " << analyzer.getThreadCount() << "\n";
        std::cout << "   Analysis results:\n";
        for (size_t i = 0; i < analysisResults.size(); ++i) {
            std::cout << "     tube-" << (i+1) << ": deflection=" << std::fixed << std::setprecision(3)
                      << analysisResults[i].deflection << ", stress=" << analysisResults[i].stress << "\n";
        }
        std::cout << "   Standards check:\n";
        for (size_t i = 0; i < stdResults.size(); ++i) {
            std::cout << "     tube-" << (i+1) << ": " << stdResults[i].standardName
                      << " (" << stdResults[i].complianceScore << "% compliant)\n";
        }
        std::cout << "\n";
    }

    // ========== 6. Performance Monitor Demo ==========
    {
        std::cout << "6. Performance Monitor:\n";
        PerformanceMonitor monitor;

        for (int i = 0; i < 10; ++i) {
            monitor.recordAnalysisTime(50.0 + i * 2);
            monitor.recordTubeProcessed();
            if (i % 2 == 0) {
                monitor.recordCacheHit();
            } else {
                monitor.recordCacheMiss();
            }
            monitor.recordMemoryUsage(10.0 + i * 0.5);
        }

        auto metrics = monitor.getMetrics();
        std::cout << "   Total tubes processed: " << metrics.totalTubesProcessed << "\n";
        std::cout << "   Cache hits: " << metrics.cacheHits << "\n";
        std::cout << "   Cache misses: " << metrics.cacheMisses << "\n";
        std::cout << "   Cache hit rate: " << (metrics.analysisCacheHitRate * 100) << "%\n";
        std::cout << "   Average analysis time: " << std::fixed << std::setprecision(2)
                  << metrics.averageAnalysisTime << " ms\n";
        std::cout << "   Peak memory usage: " << metrics.peakMemoryUsage << " MB\n\n";
    }

    // ========== 7. LOD Manager Demo ==========
    {
        std::cout << "7. Level of Detail (LOD) Management:\n";
        LODManager lodManager;

        struct LODTestCase {
            int componentCount;
            const char* description;
        };

        LODTestCase testCases[] = {
            {500, "Small assembly"},
            {1500, "Medium assembly"},
            {2500, "Large assembly"},
            {5000, "Very large assembly"},
            {7500, "Extreme assembly"}
        };

        for (const auto& testCase : testCases) {
            LODLevel lod = lodManager.calculateLOD(testCase.componentCount);
            double reduction = lodManager.getLODReductionFactor(lod);
            size_t memory = lodManager.estimateMemoryUsage(testCase.componentCount, lod);

            const char* lodStr = "";
            switch (lod) {
                case LODLevel::VeryHigh: lodStr = "VeryHigh (1.0x)"; break;
                case LODLevel::High: lodStr = "High (0.8x)"; break;
                case LODLevel::Medium: lodStr = "Medium (0.6x)"; break;
                case LODLevel::Low: lodStr = "Low (0.4x)"; break;
                case LODLevel::VeryLow: lodStr = "VeryLow (0.2x)"; break;
            }

            std::cout << "   " << testCase.componentCount << " components (" << testCase.description
                      << "): " << lodStr << " ≈ " << (memory / 1024.0) << " MB\n";
        }
        std::cout << "\n";
    }

    // ========== 8. Benchmark Suite Demo ==========
    {
        std::cout << "8. Benchmark Suite:\n";
        BenchmarkSuite suite;

        std::cout << "   Running benchmark for 2500 components...\n";
        auto result2500 = suite.benchmark2500();
        suite.printBenchmarkReport(result2500);

        std::cout << "\n   Running benchmark for 5000 components...\n";
        auto result5000 = suite.benchmark5000();
        suite.printBenchmarkReport(result5000);
    }

    std::cout << "\n=== Demo Complete ===\n\n";
    return 0;
}
