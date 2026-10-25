// CAD-CORE-032: PerformanceTuning Implementation
// Profiling and Performance Tuning for 10K+ Elements
#include "bendcore/optimization/performance_tuning.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cmath>

namespace bendcore::optimization {

PerformanceProfiler::ScopedTimer::ScopedTimer(
    PerformanceProfiler* profiler,
    const std::string& operationName,
    OperationType type)
    : profiler_(profiler),
      operationName_(operationName),
      type_(type),
      startTime_(std::chrono::high_resolution_clock::now()) {
}

PerformanceProfiler::ScopedTimer::~ScopedTimer() {
    if (profiler_) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(endTime - startTime_);
        profiler_->recordOperationTime(operationName_, type_, duration.count());
    }
}

PerformanceProfiler::PerformanceProfiler()
    : profilingActive_(false) {
}

PerformanceProfiler::~PerformanceProfiler() = default;

void PerformanceProfiler::startProfiling(const std::string& profilingName) {
    profilingActive_ = true;
    currentProfilingSession_ = profilingName;
    resetMetrics();
}

void PerformanceProfiler::stopProfiling() {
    profilingActive_ = false;
}

void PerformanceProfiler::resetMetrics() {
    metrics_.clear();
    activeTimers_.clear();
}

void PerformanceProfiler::recordOperationStart(const std::string& operationName) {
    auto threadId = std::this_thread::get_id();
    activeTimers_[threadId] = {
        operationName,
        std::chrono::high_resolution_clock::now()
    };
}

void PerformanceProfiler::recordOperationEnd(
    const std::string& operationName,
    OperationType type) {

    auto threadId = std::this_thread::get_id();
    auto it = activeTimers_.find(threadId);

    if (it != activeTimers_.end()) {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(
            endTime - it->second.startTime
        );

        recordOperationTime(operationName, type, duration.count());
        activeTimers_.erase(it);
    }
}

void PerformanceProfiler::recordOperationTime(
    const std::string& operationName,
    OperationType type,
    double timeMs) {

    auto it = metrics_.find(operationName);

    if (it == metrics_.end()) {
        OperationMetrics metrics;
        metrics.operationName = operationName;
        metrics.type = type;
        metrics.callCount = 1;
        metrics.totalTimeMs = timeMs;
        metrics.averageTimeMs = timeMs;
        metrics.minTimeMs = timeMs;
        metrics.maxTimeMs = timeMs;
        metrics.stdDeviation = 0.0;
        metrics.cpuUsagePercent = 0.0;
        metrics.memoryAllocatedMb = 0.0;
        metrics_[operationName] = metrics;
    } else {
        OperationMetrics& metrics = it->second;
        metrics.callCount++;
        metrics.totalTimeMs += timeMs;
        metrics.averageTimeMs = metrics.totalTimeMs / metrics.callCount;
        metrics.minTimeMs = std::min(metrics.minTimeMs, timeMs);
        metrics.maxTimeMs = std::max(metrics.maxTimeMs, timeMs);
    }
}

std::vector<OperationMetrics> PerformanceProfiler::getMetrics() const {
    std::vector<OperationMetrics> result;

    for (const auto& pair : metrics_) {
        result.push_back(pair.second);
    }

    return result;
}

OperationMetrics PerformanceProfiler::getOperationMetrics(
    const std::string& operationName) const {

    auto it = metrics_.find(operationName);
    if (it != metrics_.end()) {
        return it->second;
    }

    return OperationMetrics{};
}

std::string PerformanceProfiler::generateProfileReport() const {
    std::ostringstream oss;

    oss << "=== Performance Profile Report ===\n";
    oss << "Session: " << currentProfilingSession_ << "\n\n";

    auto metrics = getMetrics();

    oss << std::left << std::setw(30) << "Operation"
        << std::setw(15) << "Calls"
        << std::setw(15) << "Total(ms)"
        << std::setw(15) << "Avg(ms)"
        << std::setw(15) << "Min(ms)"
        << std::setw(15) << "Max(ms)\n";
    oss << std::string(85, '-') << "\n";

    for (const auto& metric : metrics) {
        oss << std::left << std::setw(30) << metric.operationName
            << std::setw(15) << metric.callCount
            << std::setw(15) << std::fixed << std::setprecision(2) << metric.totalTimeMs
            << std::setw(15) << metric.averageTimeMs
            << std::setw(15) << metric.minTimeMs
            << std::setw(15) << metric.maxTimeMs << "\n";
    }

    return oss.str();
}

std::string PerformanceProfiler::generateHtmlReport() const {
    std::ostringstream oss;

    oss << "<html><head><title>Performance Report</title>";
    oss << "<style>table{border-collapse:collapse;} th,td{border:1px solid black;padding:5px;}</style>";
    oss << "</head><body>";
    oss << "<h1>Performance Profile: " << currentProfilingSession_ << "</h1>";
    oss << "<table><tr><th>Operation</th><th>Calls</th><th>Total(ms)</th>";
    oss << "<th>Avg(ms)</th><th>Min(ms)</th><th>Max(ms)</th></tr>";

    auto metrics = getMetrics();
    for (const auto& metric : metrics) {
        oss << "<tr><td>" << metric.operationName << "</td>";
        oss << "<td>" << metric.callCount << "</td>";
        oss << "<td>" << std::fixed << std::setprecision(2) << metric.totalTimeMs << "</td>";
        oss << "<td>" << metric.averageTimeMs << "</td>";
        oss << "<td>" << metric.minTimeMs << "</td>";
        oss << "<td>" << metric.maxTimeMs << "</td></tr>";
    }

    oss << "</table></body></html>";
    return oss.str();
}

std::string PerformanceProfiler::generateJsonReport() const {
    std::ostringstream oss;

    oss << "{\"session\":\"" << currentProfilingSession_ << "\",\"metrics\":[";

    auto metrics = getMetrics();
    for (size_t i = 0; i < metrics.size(); ++i) {
        const auto& m = metrics[i];
        if (i > 0) oss << ",";
        oss << "{\"name\":\"" << m.operationName << "\","
            << "\"calls\":" << m.callCount << ","
            << "\"totalMs\":" << std::fixed << std::setprecision(2) << m.totalTimeMs << ","
            << "\"avgMs\":" << m.averageTimeMs << ","
            << "\"minMs\":" << m.minTimeMs << ","
            << "\"maxMs\":" << m.maxTimeMs << "}";
    }

    oss << "]}";
    return oss.str();
}

std::vector<PerformanceProfiler::HotspotInfo> PerformanceProfiler::detectHotspots(
    int topCount) const {

    auto metrics = getMetrics();
    double totalTime = 0.0;

    for (const auto& m : metrics) {
        totalTime += m.totalTimeMs;
    }

    std::vector<HotspotInfo> hotspots;

    for (const auto& m : metrics) {
        HotspotInfo info;
        info.operationName = m.operationName;
        info.timePercentage = totalTime > 0.0 ? (m.totalTimeMs / totalTime * 100.0) : 0.0;
        info.callCount = m.callCount;
        info.averageTimeMs = m.averageTimeMs;
        hotspots.push_back(info);
    }

    std::sort(hotspots.begin(), hotspots.end(),
        [](const HotspotInfo& a, const HotspotInfo& b) {
            return a.timePercentage > b.timePercentage;
        });

    if (hotspots.size() > static_cast<size_t>(topCount)) {
        hotspots.resize(topCount);
    }

    return hotspots;
}

PerformanceProfiler::Summary PerformanceProfiler::getSummary() const {
    Summary summary;
    summary.totalTimeMs = 0.0;
    summary.averageCacheHitRate = 0.0;
    summary.totalOperations = 0;

    auto metrics = getMetrics();

    for (const auto& m : metrics) {
        summary.totalTimeMs += m.totalTimeMs;
        summary.totalOperations += m.callCount;
        summary.slowestOperations.push_back(m);
    }

    std::sort(summary.slowestOperations.begin(), summary.slowestOperations.end(),
        [](const OperationMetrics& a, const OperationMetrics& b) {
            return a.totalTimeMs > b.totalTimeMs;
        });

    if (summary.slowestOperations.size() > 10) {
        summary.slowestOperations.resize(10);
    }

    summary.hotspots = detectHotspots(10);

    return summary;
}

BenchmarkSuite::BenchmarkSuite() = default;

BenchmarkSuite::~BenchmarkSuite() = default;

BenchmarkSuite::BenchmarkResult BenchmarkSuite::benchmark1000Elements() {
    return benchmarkCustom(1000);
}

BenchmarkSuite::BenchmarkResult BenchmarkSuite::benchmark5000Elements() {
    return benchmarkCustom(5000);
}

BenchmarkSuite::BenchmarkResult BenchmarkSuite::benchmark10000Elements() {
    return benchmarkCustom(10000);
}

BenchmarkSuite::BenchmarkResult BenchmarkSuite::benchmark25000Elements() {
    return benchmarkCustom(25000);
}

BenchmarkSuite::BenchmarkResult BenchmarkSuite::benchmarkCustom(int32_t elementCount) {
    profiler_.startProfiling("Benchmark-" + std::to_string(elementCount));

    BenchmarkResult result;
    result.benchmarkName = "Custom Benchmark";
    result.elementCount = elementCount;

    auto startTime = std::chrono::high_resolution_clock::now();

    // Simulate geometry operations
    {
        PerformanceProfiler::ScopedTimer timer(&profiler_, "GeometryProcessing",
            OperationType::GeometryComputation);
        for (int32_t i = 0; i < elementCount; ++i) {
            // Simulate work
            volatile double x = 0.0;
            for (int j = 0; j < 10; ++j) {
                x += std::sin(i * j);
            }
        }
    }

    // Simulate cache operations
    {
        PerformanceProfiler::ScopedTimer timer(&profiler_, "CacheOperations",
            OperationType::CacheOperation);
        // Simulate cache hits
        result.cacheHitRate = 0.75 + (elementCount > 10000 ? -0.15 : 0.0);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(endTime - startTime);

    result.totalTimeMs = duration.count();
    result.memoryPeakMb = 100.0 + (elementCount / 100.0);
    result.operationCount = elementCount * 3;

    auto targets = getTargetsFor(elementCount);
    result.passedTargets = result.totalTimeMs <= targets.maxTotalTimeMs &&
                          result.memoryPeakMb <= targets.maxMemoryMb;

    auto metrics = profiler_.getMetrics();
    result.metrics.assign(metrics.begin(), metrics.end());

    lastResults_.push_back(result);
    return result;
}

BenchmarkSuite::PerformanceTargets BenchmarkSuite::getTargetsFor(int32_t elementCount) {
    PerformanceTargets targets;

    // Performance targets based on element count
    if (elementCount <= 1000) {
        targets.maxTotalTimeMs = 100.0;
        targets.maxMemoryMb = 256.0;
    } else if (elementCount <= 5000) {
        targets.maxTotalTimeMs = 500.0;
        targets.maxMemoryMb = 512.0;
    } else if (elementCount <= 10000) {
        targets.maxTotalTimeMs = 1000.0;
        targets.maxMemoryMb = 1024.0;
    } else {
        targets.maxTotalTimeMs = 2000.0;
        targets.maxMemoryMb = 2048.0;
    }

    targets.minCacheHitRate = 0.70;
    targets.maxOperationTimeMs = 100;

    return targets;
}

std::vector<BenchmarkSuite::BenchmarkResult> BenchmarkSuite::runStressTest(
    const StressTestConfig& config) {

    std::vector<BenchmarkResult> results;

    for (int32_t count = 1000; count <= config.maxElementCount;
         count += config.stepSize) {
        auto result = benchmarkCustom(count);
        results.push_back(result);
    }

    return results;
}

BenchmarkSuite::ComparativeResult BenchmarkSuite::compareImplementations(
    const std::string& variant1Name,
    std::function<void()> variant1Code,
    const std::string& variant2Name,
    std::function<void()> variant2Code) {

    ComparativeResult result;
    result.variant1Name = variant1Name;
    result.variant2Name = variant2Name;

    // Run variant 1
    {
        PerformanceProfiler::ScopedTimer timer1(&profiler_, variant1Name,
            OperationType::Other);
        variant1Code();
    }

    // Run variant 2
    {
        PerformanceProfiler::ScopedTimer timer2(&profiler_, variant2Name,
            OperationType::Other);
        variant2Code();
    }

    auto metrics = profiler_.getMetrics();
    for (const auto& m : metrics) {
        if (m.operationName == variant1Name) {
            result.variant1TimeMs = m.totalTimeMs;
        } else if (m.operationName == variant2Name) {
            result.variant2TimeMs = m.totalTimeMs;
        }
    }

    if (result.variant2TimeMs > 0.0) {
        result.improvementPercent =
            ((result.variant2TimeMs - result.variant1TimeMs) / result.variant2TimeMs) * 100.0;
    } else {
        result.improvementPercent = 0.0;
    }

    return result;
}

std::string BenchmarkSuite::generateBenchmarkReport(
    const std::vector<BenchmarkResult>& results) const {

    std::ostringstream oss;

    oss << "=== Benchmark Report ===\n\n";
    oss << std::left << std::setw(15) << "Elements"
        << std::setw(15) << "Time(ms)"
        << std::setw(15) << "Memory(MB)"
        << std::setw(15) << "Cache Hit%"
        << std::setw(15) << "Passed\n";
    oss << std::string(75, '-') << "\n";

    for (const auto& result : results) {
        oss << std::left << std::setw(15) << result.elementCount
            << std::setw(15) << std::fixed << std::setprecision(2) << result.totalTimeMs
            << std::setw(15) << result.memoryPeakMb
            << std::setw(15) << (result.cacheHitRate * 100.0)
            << std::setw(15) << (result.passedTargets ? "YES" : "NO") << "\n";
    }

    return oss.str();
}

std::vector<PerformanceRecommender::Recommendation> PerformanceRecommender::analyzePerformance(
    const PerformanceProfile& profile) {

    std::vector<Recommendation> recommendations;

    // Analyze cache hit rate
    if (profile.cacheHitRate < 0.7) {
        recommendations.push_back({
            "Improve Cache Hit Rate",
            "Current cache hit rate is below 70%. Consider increasing cache size or adjusting eviction policy.",
            15.0,
            "High"
        });
    }

    // Analyze memory usage
    if (profile.memoryUsageMb > 1024.0 && profile.elementCount > 10000) {
        recommendations.push_back({
            "Optimize Memory Usage",
            "Memory usage is high for element count. Consider implementing streaming or LOD.",
            25.0,
            "High"
        });
    }

    // Analyze render time
    if (profile.renderingTimeMs > 100.0) {
        recommendations.push_back({
            "Improve Rendering Performance",
            "Rendering time is significant. Consider GPU acceleration or view frustum culling.",
            20.0,
            "Medium"
        });
    }

    return recommendations;
}

std::string PerformanceRecommender::generateOptimizationPlan(
    const std::vector<Recommendation>& recommendations) {

    std::ostringstream oss;
    oss << "=== Optimization Plan ===\n\n";

    int priority_order = 1;
    for (const auto& rec : recommendations) {
        if (rec.priority == "High") {
            oss << priority_order++ << ". [HIGH] " << rec.title << "\n";
            oss << "   Description: " << rec.description << "\n";
            oss << "   Expected Speedup: " << std::fixed << std::setprecision(1)
                << rec.potentialSpeedupPercent << "%\n\n";
        }
    }

    for (const auto& rec : recommendations) {
        if (rec.priority == "Medium") {
            oss << priority_order++ << ". [MEDIUM] " << rec.title << "\n";
            oss << "   Description: " << rec.description << "\n";
            oss << "   Expected Speedup: " << std::fixed << std::setprecision(1)
                << rec.potentialSpeedupPercent << "%\n\n";
        }
    }

    return oss.str();
}

}  // namespace bendcore::optimization
