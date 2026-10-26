#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace bendcore::hardening {

// ==============================================================================
// PERFORMANCE TUNING - GAP-091
// ==============================================================================
// Benchmarking, profiling, optimization, performance optimization strategies

enum class ProfilerType {
    CPU,
    MEMORY,
    IO,
    NETWORK,
    GPU
};

struct BenchmarkResult {
    std::string benchmarkId;
    std::string testName;
    int64_t executionTimeMs;
    int32_t operationCount;
    double opsPerSecond;
    double memoryUsedMB;
    int64_t timestamp;
};

struct ProfileData {
    std::string functionName;
    int64_t totalTimeMs;
    int64_t callCount;
    double averageTimeMs;
    double percentageOfTotal;
};

struct OptimizationTip {
    std::string tipId;
    std::string description;
    double expectedImprovement;
    std::string affectedComponent;
    bool applied;
};

class PerformanceTuningEngine {
public:
    PerformanceTuningEngine();
    ~PerformanceTuningEngine();

    // Benchmarking
    bool runBenchmark(const std::string& testName, std::function<void()> testFunction);
    BenchmarkResult getBenchmarkResult(const std::string& benchmarkId);
    std::vector<BenchmarkResult> getAllBenchmarks();
    double compareBenchmarks(const std::string& benchmarkId1, const std::string& benchmarkId2);

    // Profiling
    bool startProfiling(ProfilerType type);
    bool stopProfiling();
    std::vector<ProfileData> getProfileData();
    std::vector<ProfileData> getHotFunctions(int32_t count);
    ProfileData getProfileData(const std::string& functionName);

    // CPU profiling
    std::vector<ProfileData> getCPUProfile();
    std::vector<std::string> getHotCPUFunctions();

    // Memory profiling
    std::vector<ProfileData> getMemoryProfile();
    std::vector<std::string> getHighMemoryAllocators();
    bool detectMemoryLeaks();

    // I/O profiling
    std::vector<ProfileData> getIOProfile();
    std::vector<std::string> getSlowIOOperations();

    // Optimization analysis
    std::vector<OptimizationTip> analyzePerformance();
    std::vector<OptimizationTip> getOptimizationTips();
    bool applyOptimization(const std::string& tipId);
    double getOptimizationImpact(const std::string& tipId);

    // Caching optimization
    bool enableCaching(bool enable);
    bool optimizeCacheSize();
    float getCacheHitRate();
    bool clearCache();

    // Algorithm optimization
    bool optimizeAlgorithm(const std::string& algorithmName);
    std::map<std::string, double> getAlgorithmPerformance();

    // Code optimization
    bool enableJIT(bool enable);
    bool enableVectorization(bool enable);
    bool enableParallelization(bool enable);

    // Performance reporting
    std::string generatePerformanceReport();
    std::string generateProfilingReport();
    std::string generateOptimizationReport();

    // Target setting
    bool setPerformanceTarget(const std::string& metricName, double targetValue);
    bool checkPerformanceTarget(const std::string& metricName);
    std::map<std::string, double> getPerformanceTargets();

    // Continuous optimization
    bool enableContinuousOptimization(bool enable);
    bool autoOptimize();
    std::vector<std::string> getOptimizationHistory();

private:
    std::vector<BenchmarkResult> benchmarks;
    std::vector<ProfileData> profileData;
    std::map<std::string, OptimizationTip> optimizationTips;
    std::map<std::string, double> performanceTargets;
    bool profilingActive;
};

}  // namespace bendcore::hardening
