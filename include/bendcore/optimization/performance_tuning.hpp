// CAD-CORE-032: PerformanceTuning - Profiling and Performance Tuning
// Phase H: Advanced Optimization for 10K+ Elements
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <chrono>

namespace bendcore::optimization {

/**
 * PerformanceTuning
 *
 * Comprehensive profiling and performance tuning framework.
 *
 * Features:
 * - Function-level profiling
 * - CPU and memory hotspot detection
 * - Performance bottleneck identification
 * - Benchmark suite for 10K+ elements
 * - Automatic performance recommendations
 * - Historical performance tracking
 */

enum class OperationType {
    GeometryComputation,
    CacheOperation,
    IOOperation,
    MemoryAllocation,
    Rendering,
    Analysis,
    Other
};

struct OperationMetrics {
    OperationType type;
    std::string operationName;
    int64_t callCount;
    double totalTimeMs;
    double averageTimeMs;
    double minTimeMs;
    double maxTimeMs;
    double stdDeviation;
    double cpuUsagePercent;
    double memoryAllocatedMb;
};

struct PerformanceProfile {
    int64_t timestamp;
    int32_t elementCount;
    double totalComputeTimeMs;
    double geometryTimeMs;
    double cacheTimeMs;
    double renderingTimeMs;
    double memoryUsageMb;
    double cacheHitRate;
    std::vector<OperationMetrics> operations;
};

class PerformanceProfiler {
public:
    PerformanceProfiler();
    ~PerformanceProfiler();

    // Profiling control
    void startProfiling(const std::string& profilingName);
    void stopProfiling();
    void resetMetrics();

    // Operation timing
    class ScopedTimer {
    public:
        ScopedTimer(
            PerformanceProfiler* profiler,
            const std::string& operationName,
            OperationType type
        );
        ~ScopedTimer();

    private:
        PerformanceProfiler* profiler_;
        std::string operationName_;
        OperationType type_;
        std::chrono::high_resolution_clock::time_point startTime_;
    };

    void recordOperationStart(const std::string& operationName);
    void recordOperationEnd(
        const std::string& operationName,
        OperationType type
    );

    // Record time manually
    void recordOperationTime(
        const std::string& operationName,
        OperationType type,
        double timeMs
    );

    // Get metrics
    std::vector<OperationMetrics> getMetrics() const;
    OperationMetrics getOperationMetrics(const std::string& operationName) const;

    // Generate report
    std::string generateProfileReport() const;
    std::string generateHtmlReport() const;
    std::string generateJsonReport() const;

    // Hotspot detection
    struct HotspotInfo {
        std::string operationName;
        double timePercentage;
        double callCount;
        double averageTimeMs;
    };

    std::vector<HotspotInfo> detectHotspots(int topCount = 10) const;

    // Performance summary
    struct Summary {
        double totalTimeMs;
        double averageCacheHitRate;
        int64_t totalOperations;
        std::vector<OperationMetrics> slowestOperations;
        std::vector<HotspotInfo> hotspots;
    };

    Summary getSummary() const;

private:
    struct OperationTimer {
        std::string name;
        std::chrono::high_resolution_clock::time_point startTime;
    };

    std::map<std::string, OperationMetrics> metrics_;
    std::map<std::thread::id, OperationTimer> activeTimers_;
    bool profilingActive_ = false;
    std::string currentProfilingSession_;
};

class BenchmarkSuite {
public:
    BenchmarkSuite();
    ~BenchmarkSuite();

    // Predefined benchmarks
    struct BenchmarkResult {
        std::string benchmarkName;
        int32_t elementCount;
        double totalTimeMs;
        double memoryPeakMb;
        double cacheHitRate;
        int64_t operationCount;
        bool passedTargets;
        std::vector<OperationMetrics> metrics;
    };

    // Run benchmark for specific element count
    BenchmarkResult benchmark1000Elements();
    BenchmarkResult benchmark5000Elements();
    BenchmarkResult benchmark10000Elements();
    BenchmarkResult benchmark25000Elements();
    BenchmarkResult benchmarkCustom(int32_t elementCount);

    // Performance targets
    struct PerformanceTargets {
        double maxTotalTimeMs;
        double maxMemoryMb;
        double minCacheHitRate;
        int64_t maxOperationTimeMs;
    };

    static PerformanceTargets getTargetsFor(int32_t elementCount);

    // Stress test
    struct StressTestConfig {
        int32_t maxElementCount;
        int32_t stepSize;
        bool testConcurrency;
        int32_t threadCount;
    };

    std::vector<BenchmarkResult> runStressTest(const StressTestConfig& config);

    // Comparative benchmarking
    struct ComparativeResult {
        std::string variant1Name;
        std::string variant2Name;
        double variant1TimeMs;
        double variant2TimeMs;
        double improvementPercent;
    };

    ComparativeResult compareImplementations(
        const std::string& variant1Name,
        std::function<void()> variant1Code,
        const std::string& variant2Name,
        std::function<void()> variant2Code
    );

    // Generate benchmark report
    std::string generateBenchmarkReport(
        const std::vector<BenchmarkResult>& results
    ) const;

private:
    PerformanceProfiler profiler_;
    std::vector<BenchmarkResult> lastResults_;
};

// Performance recommendation engine
class PerformanceRecommender {
public:
    struct Recommendation {
        std::string title;
        std::string description;
        double potentialSpeedupPercent;
        std::string priority;  // "High", "Medium", "Low"
    };

    static std::vector<Recommendation> analyzePerformance(
        const PerformanceProfile& profile
    );

    static std::string generateOptimizationPlan(
        const std::vector<Recommendation>& recommendations
    );
};

}  // namespace bendcore::optimization
