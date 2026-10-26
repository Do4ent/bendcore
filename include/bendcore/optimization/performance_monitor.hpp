// CAD-CORE-054: PerformanceMonitor - Advanced Performance Monitoring
// M5 Phase: Performance Optimization and Caching
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <chrono>
#include <functional>

namespace bendcore::optimization {

/**
 * PerformanceMonitor
 *
 * Comprehensive performance monitoring including:
 * - Benchmark framework
 * - Feature computation timing
 * - Cache hit/miss metrics
 * - Memory usage statistics
 * - Bottleneck identification
 * - Performance reporting
 */

struct PerformanceMetrics {
    std::string metricName;
    int64_t value;
    std::string unit;
    int64_t timestamp;
    int32_t sampleCount;
};

struct BenchmarkResult {
    std::string benchmarkName;
    int64_t executionTimeMs;
    int64_t memoryPeakBytes;
    double cacheHitRate;
    int64_t operationsCount;
    int32_t elementCount;
    bool passedThresholds;
    std::vector<PerformanceMetrics> metrics;
};

class PerformanceMonitor {
public:
    PerformanceMonitor();
    ~PerformanceMonitor();

    // Monitoring control
    void startMonitoring();
    void stopMonitoring();
    void resetMetrics();
    bool isMonitoring() const;

    // Metrics recording
    void recordMetric(const std::string& metricName, int64_t value, const std::string& unit);
    void recordComputationTime(const std::string& operationName, int64_t timeMs);
    void recordCacheHit(const std::string& cacheType);
    void recordCacheMiss(const std::string& cacheType);
    void recordMemoryUsage(size_t usageBytes);

    // Metrics retrieval
    struct MetricsSnapshot {
        int64_t totalComputationTimeMs;
        int64_t totalCacheHits;
        int64_t totalCacheMisses;
        double cacheHitRate;
        size_t peakMemoryUsageBytes;
        double averageComputationTimeMs;
        std::map<std::string, int64_t> computationTimeByOperation;
        std::map<std::string, std::pair<int64_t, int64_t>> cacheStatsByType;
    };

    MetricsSnapshot getSnapshot() const;

    // Benchmarking - moved to BenchmarkFramework

    // Bottleneck detection
    struct Bottleneck {
        std::string name;
        double timePercentage;
        std::string severity;  // "Critical", "High", "Medium", "Low"
        std::string recommendation;
    };

    std::vector<Bottleneck> detectBottlenecks(int topCount = 10) const;

    // Reporting
    std::string generatePerformanceReport() const;
    std::string generateJsonReport() const;
    std::string generateHtmlReport() const;

    // Thresholds
    void setPerformanceThresholds(
        int64_t computationTimeMs,
        size_t memoryBytes,
        double minCacheHitRate
    );

    bool meetsPerformanceTargets() const;

private:
    bool monitoring_ = false;

    std::map<std::string, std::vector<int64_t>> computationTimes_;
    std::map<std::string, std::pair<int64_t, int64_t>> cacheStats_;

    std::vector<size_t> memoryUsageSamples_;
    size_t peakMemoryUsage_ = 0;

    int64_t computationTimeThresholdMs_ = 1000;
    size_t memoryThresholdBytes_ = 536870912;  // 512 MB
    double minCacheHitRate_ = 0.7;

    int64_t monitoringStartTime_ = 0;
};

class BenchmarkFramework {
public:
    BenchmarkFramework();
    ~BenchmarkFramework();

    // Predefined benchmarks
    struct BenchmarkResult {
        std::string benchmarkName;
        int64_t executionTimeMs;
        int64_t memoryPeakBytes;
        double cacheHitRate;
        int64_t operationsCount;
        int32_t elementCount;
        bool passedThresholds;
        std::vector<PerformanceMetrics> metrics;
    };

    struct BenchmarkConfig {
        std::string name;
        int32_t elementCount;
        int32_t iterationCount;
        int64_t timeThresholdMs;
        size_t memoryThresholdBytes;
        double minCacheHitRate;
    };

    BenchmarkResult benchmark1KElements();
    BenchmarkResult benchmark10KElements();
    BenchmarkResult benchmark100KElements();
    BenchmarkResult benchmarkCustomElements(int32_t elementCount);

    // Stress testing
    struct StressTestConfig {
        int32_t startElements;
        int32_t maxElements;
        int32_t stepSize;
        int64_t timeoutMs;
        bool testMemoryPressure;
        bool testConcurrency;
    };

    std::vector<BenchmarkResult> runStressTest(const StressTestConfig& config);

    // Comparative benchmarking
    struct ComparisonResult {
        std::string implementation1;
        std::string implementation2;
        int64_t time1Ms;
        int64_t time2Ms;
        double improvementPercent;
    };

    ComparisonResult compareImplementations(
        const std::string& impl1Name,
        std::function<void()> impl1Code,
        const std::string& impl2Name,
        std::function<void()> impl2Code
    );

    // Result aggregation
    struct AggregateResults {
        size_t totalBenchmarksRun;
        size_t successfulBenchmarks;
        double averageExecutionTimeMs;
        double minExecutionTimeMs;
        double maxExecutionTimeMs;
        double standardDeviation;
        std::vector<BenchmarkResult> results;
    };

    AggregateResults getLastResults() const;
    std::string generateComprehensiveReport() const;

private:
    std::vector<BenchmarkResult> lastResults_;
    PerformanceMonitor monitor_;
};

}  // namespace bendcore::optimization
