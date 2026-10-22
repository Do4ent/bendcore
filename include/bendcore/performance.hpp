// Performance & Optimization Infrastructure
// Phase 26 Stage B: Performance & Optimization
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <chrono>
#include <thread>

namespace bendcore::performance {

// ========== Cache System ==========

template<typename KeyT, typename ValueT>
class LRUCache {
public:
    explicit LRUCache(size_t maxSize = 1000) : maxSize(maxSize) {}
    ~LRUCache() = default;

    bool get(const KeyT& key, ValueT& outValue) {
        auto it = cache.find(key);
        if (it != cache.end()) {
            outValue = it->second.value;
            it->second.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
            hits++;
            return true;
        }
        misses++;
        return false;
    }

    void put(const KeyT& key, const ValueT& value) {
        if (cache.size() >= maxSize && cache.find(key) == cache.end()) {
            // Remove oldest entry
            auto oldest = cache.begin();
            for (auto it = cache.begin(); it != cache.end(); ++it) {
                if (it->second.timestamp < oldest->second.timestamp) {
                    oldest = it;
                }
            }
            cache.erase(oldest);
        }

        cache[key] = {value, std::chrono::system_clock::now().time_since_epoch().count()};
    }

    void clear() {
        cache.clear();
        hits = 0;
        misses = 0;
    }

    size_t size() const {
        return cache.size();
    }

    size_t capacity() const {
        return maxSize;
    }

    double hitRate() const {
        if (hits + misses == 0) return 0.0;
        return static_cast<double>(hits) / (hits + misses);
    }

private:
    struct CacheEntry {
        ValueT value;
        int64_t timestamp;
    };

    size_t maxSize;
    std::map<KeyT, CacheEntry> cache;
    int64_t hits = 0;
    int64_t misses = 0;
};

// ========== Geometry Cache ==========

class GeometryCache {
public:
    explicit GeometryCache();
    ~GeometryCache();

    // Cache mesh data
    bool getMesh(const std::string& tubeId, std::vector<float>& vertices);
    void putMesh(const std::string& tubeId, const std::vector<float>& vertices);

    // Cache normals
    bool getNormals(const std::string& tubeId, std::vector<float>& normals);
    void putNormals(const std::string& tubeId, const std::vector<float>& normals);

    void clear();
    double hitRate() const;

private:
    LRUCache<std::string, std::vector<float>> meshCache;
    LRUCache<std::string, std::vector<float>> normalCache;
};

// ========== Analysis Result Cache ==========

struct AnalysisCacheEntry {
    double deflection;
    double stress;
    double strain;
    int complexity;
    int64_t timestamp;
};

class AnalysisCache {
public:
    explicit AnalysisCache();
    ~AnalysisCache();

    bool get(const std::string& tubeId, AnalysisCacheEntry& outEntry);
    void put(const std::string& tubeId, const AnalysisCacheEntry& entry);

    void clear();
    double hitRate() const;
    size_t size() const;

private:
    LRUCache<std::string, AnalysisCacheEntry> cache;
};

// ========== Standards Cache ==========

struct ComplianceCacheEntry {
    std::string standardName;
    int complianceStatus;
    double complianceScore;
    int64_t timestamp;
};

class StandardsCache {
public:
    explicit StandardsCache();
    ~StandardsCache();

    bool get(const std::string& tubeId, const std::string& standard, ComplianceCacheEntry& outEntry);
    void put(const std::string& tubeId, const std::string& standard, const ComplianceCacheEntry& entry);

    void clear();
    double hitRate() const;

private:
    LRUCache<std::string, ComplianceCacheEntry> cache;  // Key: "tubeId:standard"
};

// ========== Parallel Processing ==========

class ParallelAnalyzer {
public:
    explicit ParallelAnalyzer(int numThreads = 4);
    ~ParallelAnalyzer();

    // Analyze multiple tubes in parallel
    std::vector<AnalysisCacheEntry> analyzeTubesParallel(
        const std::vector<std::string>& tubeIds,
        const std::vector<double>& ods,
        const std::vector<double>& walls
    );

    // Check standards for multiple tubes in parallel
    std::vector<ComplianceCacheEntry> checkStandardsParallel(
        const std::vector<std::string>& tubeIds,
        const std::vector<double>& ods,
        const std::vector<double>& walls
    );

    int getThreadCount() const;

private:
    int numThreads;
};

// ========== Performance Metrics ==========

struct PerformanceMetrics {
    double analysisCacheHitRate;
    double geometryCacheHitRate;
    double standardsCacheHitRate;

    double averageAnalysisTime;  // milliseconds
    double peakMemoryUsage;      // MB

    int totalTubesProcessed;
    int cacheHits;
    int cacheMisses;
};

class PerformanceMonitor {
public:
    explicit PerformanceMonitor();
    ~PerformanceMonitor();

    void recordAnalysisTime(double milliseconds);
    void recordCacheHit();
    void recordCacheMiss();
    void recordMemoryUsage(double mb);
    void recordTubeProcessed();

    PerformanceMetrics getMetrics() const;
    void reset();

    void printReport() const;

private:
    std::vector<double> analysisTimes;
    double peakMemory = 0.0;
    int totalTubes = 0;
    int hits = 0;
    int misses = 0;
};

// ========== Level of Detail (LOD) System ==========

enum class LODLevel {
    VeryHigh,  // Full detail
    High,      // 80% detail
    Medium,    // 60% detail
    Low,       // 40% detail
    VeryLow    // 20% detail
};

class LODManager {
public:
    explicit LODManager();
    ~LODManager();

    // Calculate appropriate LOD based on number of components
    LODLevel calculateLOD(int componentCount) const;

    // Get vertex count reduction factor for LOD
    double getLODReductionFactor(LODLevel lod) const;

    // Estimate memory usage for component count
    size_t estimateMemoryUsage(int componentCount, LODLevel lod) const;

private:
    const int HIGH_COMPONENT_THRESHOLD = 1000;
    const int MEDIUM_COMPONENT_THRESHOLD = 2500;
    const int LOW_COMPONENT_THRESHOLD = 5000;
};

// ========== Benchmark Suite ==========

struct BenchmarkResult {
    int componentCount;
    double totalTimeMs;
    double analysisTimeMs;
    double standardsCheckTimeMs;
    double exportTimeMs;
    double memoryUsageMb;
    double cacheHitRate;
};

class BenchmarkSuite {
public:
    explicit BenchmarkSuite();
    ~BenchmarkSuite();

    // Run benchmark on 2500-component assembly
    BenchmarkResult benchmark2500() const;

    // Run benchmark on 5000-component assembly
    BenchmarkResult benchmark5000() const;

    // Run custom benchmark
    BenchmarkResult benchmarkCustom(int componentCount) const;

    // Verify target performance (<1s for 5000 components)
    bool verifyPerformanceTargets(const BenchmarkResult& result) const;

    void printBenchmarkReport(const BenchmarkResult& result) const;

private:
    const double TARGET_TIME_MS = 1000.0;  // <1 second for 5000 components
};

}  // namespace bendcore::performance
