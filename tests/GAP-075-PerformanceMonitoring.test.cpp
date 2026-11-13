#include <gtest/gtest.h>
#include <memory>
#include "../include/bendcore/analytics/performance_monitoring.hpp"

class PerformanceMonitoringTest : public ::testing::Test {
protected:
    std::unique_ptr<bendcore::analytics::PerformanceMonitoring> monitor;

    void SetUp() override {
        monitor = std::make_unique<bendcore::analytics::PerformanceMonitoring>();
    }
};

TEST_F(PerformanceMonitoringTest, InitializeMonitoring) {
    EXPECT_NE(monitor.get(), nullptr);
}

TEST_F(PerformanceMonitoringTest, TrackCPUUsage) {
    monitor->trackCPUUsage(45.5);
    auto cpu = monitor->getCPUUsage();
    EXPECT_EQ(cpu, 45.5);
}

TEST_F(PerformanceMonitoringTest, TrackMemoryUsage) {
    monitor->trackMemoryUsage(2048);
    auto memory = monitor->getMemoryUsage();
    EXPECT_EQ(memory, 2048);
}

TEST_F(PerformanceMonitoringTest, TrackDiskIO) {
    monitor->trackDiskIOOperation(512, "read");
    auto io = monitor->getDiskIOStatistics();
    EXPECT_GT(io.total_operations, 0);
}

TEST_F(PerformanceMonitoringTest, TrackNetworkUsage) {
    monitor->trackNetworkUsage(1024, "upload");
    auto net = monitor->getNetworkStatistics();
    EXPECT_GT(net.bytes_transferred, 0);
}

TEST_F(PerformanceMonitoringTest, MeasureLatency) {
    auto start = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GE(duration.count(), 10);
}

TEST_F(PerformanceMonitoringTest, TrackThroughput) {
    monitor->trackThroughput(1000, std::chrono::seconds(1));
    auto throughput = monitor->getCurrentThroughput();
    EXPECT_GT(throughput, 0.0f);
}

TEST_F(PerformanceMonitoringTest, ProfileFunction) {
    auto profile_token = monitor->startProfiling("test_function");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    auto result = monitor->stopProfiling(profile_token);
    EXPECT_GT(result.execution_time_ms, 0);
}

TEST_F(PerformanceMonitoringTest, TrackGarbageCollection) {
    monitor->trackGarbageCollection(100, 50);
    auto gc_stats = monitor->getGarbageCollectionStatistics();
    EXPECT_GT(gc_stats.total_collections, 0);
}

TEST_F(PerformanceMonitoringTest, IdentifyBottlenecks) {
    monitor->trackCPUUsage(95.0);
    monitor->trackMemoryUsage(7000);
    auto bottlenecks = monitor->identifyBottlenecks();
    EXPECT_GT(bottlenecks.size(), 0);
}

TEST_F(PerformanceMonitoringTest, GetPerformanceScore) {
    monitor->trackCPUUsage(30.0);
    monitor->trackMemoryUsage(2048);
    auto score = monitor->getPerformanceScore();
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

TEST_F(PerformanceMonitoringTest, CompareSnapshots) {
    auto snap1 = monitor->captureSnapshot("before");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto snap2 = monitor->captureSnapshot("after");
    auto diff = monitor->compareSnapshots(snap1, snap2);
    EXPECT_NE(diff.cpu_delta, 0.0f);
}

TEST_F(PerformanceMonitoringTest, AlertOnThreshold) {
    bool alerted = false;
    monitor->setThresholdAlert("cpu", 80.0, [&alerted]() { alerted = true; });
    monitor->trackCPUUsage(85.0);
    monitor->checkThresholds();
    EXPECT_TRUE(alerted);
}

TEST_F(PerformanceMonitoringTest, TrackThreadCount) {
    monitor->trackThreadCount(4);
    auto threads = monitor->getThreadCount();
    EXPECT_EQ(threads, 4);
}

TEST_F(PerformanceMonitoringTest, TrackContextSwitches) {
    monitor->trackContextSwitches(1000);
    auto switches = monitor->getContextSwitches();
    EXPECT_EQ(switches, 1000);
}

TEST_F(PerformanceMonitoringTest, GetSystemMetrics) {
    monitor->trackCPUUsage(50.0);
    monitor->trackMemoryUsage(2048);
    auto metrics = monitor->getSystemMetrics();
    EXPECT_GT(metrics.system_metrics.size(), 0);
}

TEST_F(PerformanceMonitoringTest, TrackResponseTime) {
    monitor->trackResponseTime("operation1", 125);
    auto avg_response = monitor->getAverageResponseTime();
    EXPECT_GT(avg_response, 0.0f);
}

TEST_F(PerformanceMonitoringTest, GeneratePerformanceReport) {
    monitor->trackCPUUsage(40.0);
    monitor->trackMemoryUsage(2048);
    auto report = monitor->generatePerformanceReport();
    EXPECT_FALSE(report.empty());
}

TEST_F(PerformanceMonitoringTest, ExportMetrics) {
    monitor->trackCPUUsage(50.0);
    auto exported = monitor->exportMetrics("json");
    EXPECT_FALSE(exported.empty());
}

TEST_F(PerformanceMonitoringTest, ResetMetrics) {
    monitor->trackCPUUsage(50.0);
    monitor->resetMetrics();
    auto cpu = monitor->getCPUUsage();
    EXPECT_EQ(cpu, 0.0f);
}

TEST_F(PerformanceMonitoringTest, GetHistoricalData) {
    monitor->trackCPUUsage(40.0);
    monitor->trackCPUUsage(50.0);
    monitor->trackCPUUsage(60.0);
    auto history = monitor->getHistoricalData(3);
    EXPECT_EQ(history.size(), 3);
}

TEST_F(PerformanceMonitoringTest, TrackCacheMissRate) {
    monitor->trackCacheMiss(50);
    monitor->trackCacheHit(450);
    auto miss_rate = monitor->getCacheMissRate();
    EXPECT_EQ(miss_rate, 10.0f);
}

TEST_F(PerformanceMonitoringTest, ProfileMemoryAllocation) {
    auto alloc_profile = monitor->profileMemoryAllocations();
    EXPECT_GT(alloc_profile.total_allocations, 0);
}

TEST_F(PerformanceMonitoringTest, TrackFrameRate) {
    for (int i = 0; i < 60; i++) {
        monitor->trackFrameTime(16);
    }
    auto fps = monitor->getFrameRate();
    EXPECT_NEAR(fps, 60.0f, 5.0f);
}

TEST_F(PerformanceMonitoringTest, DetectMemoryLeaks) {
    auto leaks = monitor->detectMemoryLeaks();
    EXPECT_GE(leaks, 0);
}

TEST_F(PerformanceMonitoringTest, CalculatePowerConsumption) {
    monitor->trackCPUUsage(80.0);
    auto power = monitor->estimatePowerConsumption();
    EXPECT_GT(power, 0.0f);
}

TEST_F(PerformanceMonitoringTest, MonitoringVersion) {
    auto version = monitor->getMonitoringVersion();
    EXPECT_FALSE(version.empty());
}

TEST_F(PerformanceMonitoringTest, AsynchronousMonitoring) {
    auto future = monitor->startAsynchronousMonitoring();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto metrics = monitor->getAsynchronousMetrics();
    EXPECT_GT(metrics.size(), 0);
}

TEST_F(PerformanceMonitoringTest, SetSamplingRate) {
    monitor->setSamplingRate(100);
    auto rate = monitor->getSamplingRate();
    EXPECT_EQ(rate, 100);
}

TEST_F(PerformanceMonitoringTest, OptimizationSuggestions) {
    monitor->trackCPUUsage(95.0);
    monitor->trackMemoryUsage(6000);
    auto suggestions = monitor->getOptimizationSuggestions();
    EXPECT_GT(suggestions.size(), 0);
}

TEST_F(PerformanceMonitoringTest, TrendAnalysis) {
    monitor->trackCPUUsage(20.0);
    monitor->trackCPUUsage(30.0);
    monitor->trackCPUUsage(40.0);
    auto trend = monitor->analyzeTrend("cpu");
    EXPECT_NE(trend.direction, "");
}

TEST_F(PerformanceMonitoringTest, AlertHistory) {
    monitor->trackCPUUsage(85.0);
    monitor->setThresholdAlert("cpu", 80.0, []() {});
    monitor->checkThresholds();
    auto alert_history = monitor->getAlertHistory();
    EXPECT_GT(alert_history.size(), 0);
}
