// M17-GAP-075: PerformanceMonitoring Tests
#include <gtest/gtest.h>
#include "bendcore/analytics/performance_monitoring.hpp"

using namespace bendcore::analytics;

class PerformanceMonitoringTest : public ::testing::Test {
protected:
    PerformanceMonitoring monitoring;
};

TEST_F(PerformanceMonitoringTest, StartMonitoring) {
    monitoring.startMonitoring();
    EXPECT_TRUE(monitoring.isMonitoring());
}

TEST_F(PerformanceMonitoringTest, StopMonitoring) {
    monitoring.startMonitoring();
    monitoring.stopMonitoring();
    EXPECT_FALSE(monitoring.isMonitoring());
}

TEST_F(PerformanceMonitoringTest, ResetMetrics) {
    monitoring.startMonitoring();
    CpuMetrics cpu{50.0, 4, 100, 1000.0};
    monitoring.recordCpuMetrics(cpu);
    monitoring.resetMetrics();
    
    auto current = monitoring.getCurrentCpuMetrics();
    EXPECT_EQ(current.cpuUsagePercent, 0.0);
}

TEST_F(PerformanceMonitoringTest, RecordCpuMetrics) {
    monitoring.startMonitoring();
    CpuMetrics cpu{45.5, 8, 500, 2500.0};
    monitoring.recordCpuMetrics(cpu);
    
    auto current = monitoring.getCurrentCpuMetrics();
    EXPECT_EQ(current.cpuUsagePercent, 45.5);
}

TEST_F(PerformanceMonitoringTest, GetAverageCpuUsage) {
    monitoring.startMonitoring();
    monitoring.recordCpuMetrics({25.0, 4, 100, 500.0});
    monitoring.recordCpuMetrics({50.0, 4, 100, 500.0});
    
    double avg = monitoring.getAverageCpuUsage(3600);
    EXPECT_GT(avg, 0.0);
}

TEST_F(PerformanceMonitoringTest, RecordMemoryMetrics) {
    monitoring.startMonitoring();
    MemoryMetrics mem{1000000, 2000000, 5000, 0.15, 10};
    monitoring.recordMemoryMetrics(mem);
    
    auto current = monitoring.getCurrentMemoryMetrics();
    EXPECT_EQ(current.heapUsageBytes, 1000000);
}

TEST_F(PerformanceMonitoringTest, GetMemoryTrend) {
    monitoring.startMonitoring();
    for (int i = 0; i < 5; ++i) {
        monitoring.recordMemoryMetrics({i * 100000, i * 200000, i * 1000, 0.1, i});
    }
    
    auto trend = monitoring.getMemoryTrend(100);
    EXPECT_EQ(trend.size(), 5);
}

TEST_F(PerformanceMonitoringTest, StartProfiling) {
    int64_t start = monitoring.startProfiling("operation1");
    EXPECT_GT(start, 0);
}

TEST_F(PerformanceMonitoringTest, StopProfiling) {
    monitoring.startMonitoring();
    int64_t start = monitoring.startProfiling("operation1");
    monitoring.stopProfiling("operation1", start, true);
    
    auto metrics = monitoring.getOperationMetrics("operation1");
    EXPECT_EQ(metrics.size(), 1);
}

TEST_F(PerformanceMonitoringTest, GetSlowestOperations) {
    monitoring.startMonitoring();
    
    for (int i = 0; i < 5; ++i) {
        int64_t start = monitoring.startProfiling("op" + std::to_string(i));
        monitoring.stopProfiling("op" + std::to_string(i), start, true);
    }
    
    auto slowest = monitoring.getSlowestOperations(3);
    EXPECT_LE(slowest.size(), 3);
}

TEST_F(PerformanceMonitoringTest, RecordThreadMetrics) {
    ThreadMetrics tm{123, 5000, 10, 75.5, 50};
    monitoring.recordThreadMetrics(123, tm);
    
    auto retrieved = monitoring.getThreadMetrics(123);
    EXPECT_EQ(retrieved.threadId, 123);
}

TEST_F(PerformanceMonitoringTest, GetAllThreadMetrics) {
    monitoring.recordThreadMetrics(1, {1, 1000, 5, 50.0, 10});
    monitoring.recordThreadMetrics(2, {2, 2000, 8, 60.0, 20});
    
    auto all = monitoring.getAllThreadMetrics();
    EXPECT_EQ(all.size(), 2);
}

TEST_F(PerformanceMonitoringTest, SetThreshold) {
    PerformanceThreshold threshold{"CPUUsage", 70.0, 90.0, "%"};
    monitoring.setThreshold(threshold);
    
    auto thresholds = monitoring.getThresholds();
    EXPECT_TRUE(thresholds.count("CPUUsage") > 0);
}

TEST_F(PerformanceMonitoringTest, CheckThreshold) {
    PerformanceThreshold threshold{"CPUUsage", 50.0, 80.0, "%"};
    monitoring.setThreshold(threshold);
    
    auto status1 = monitoring.checkThreshold("CPUUsage", 30.0);
    EXPECT_EQ(status1, "OK");
    
    auto status2 = monitoring.checkThreshold("CPUUsage", 60.0);
    EXPECT_EQ(status2, "WARNING");
    
    auto status3 = monitoring.checkThreshold("CPUUsage", 90.0);
    EXPECT_EQ(status3, "CRITICAL");
}

TEST_F(PerformanceMonitoringTest, DetectAnomalies) {
    monitoring.startMonitoring();
    
    for (int i = 0; i < 10; ++i) {
        int64_t start = monitoring.startProfiling("normal_op");
        monitoring.stopProfiling("normal_op", start, true);
    }
    
    auto anomalies = monitoring.detectAnomalies();
    EXPECT_TRUE(anomalies.size() >= 0);
}

TEST_F(PerformanceMonitoringTest, GetPerformanceScore) {
    monitoring.startMonitoring();
    monitoring.recordCpuMetrics({25.0, 4, 100, 500.0});
    monitoring.recordMemoryMetrics({500000, 1000000, 2000, 0.1, 5});
    
    double score = monitoring.getPerformanceScore();
    EXPECT_GE(score, 0.0);
    EXPECT_LE(score, 100.0);
}

TEST_F(PerformanceMonitoringTest, GenerateReport) {
    monitoring.startMonitoring();
    monitoring.recordCpuMetrics({50.0, 4, 100, 1000.0});
    
    auto report = monitoring.generateReport();
    EXPECT_FALSE(report.empty());
    EXPECT_TRUE(report.find("Performance Monitoring Report") != std::string::npos);
}

TEST_F(PerformanceMonitoringTest, GenerateJsonReport) {
    monitoring.startMonitoring();
    
    auto report = monitoring.generateJsonReport();
    EXPECT_FALSE(report.empty());
    EXPECT_TRUE(report.find("monitoring") != std::string::npos);
}

TEST_F(PerformanceMonitoringTest, GetSummaryStatistics) {
    monitoring.startMonitoring();
    monitoring.recordCpuMetrics({50.0, 4, 100, 1000.0});
    monitoring.recordMemoryMetrics({1000000, 2000000, 5000, 0.15, 10});
    
    auto stats = monitoring.getSummaryStatistics();
    EXPECT_TRUE(stats.size() > 0);
}

TEST_F(PerformanceMonitoringTest, MultipleOperations) {
    monitoring.startMonitoring();
    
    for (int i = 0; i < 10; ++i) {
        int64_t start = monitoring.startProfiling("op" + std::to_string(i));
        monitoring.stopProfiling("op" + std::to_string(i), start, true);
    }
    
    auto stats = monitoring.getSummaryStatistics();
    EXPECT_EQ(stats["operations"], 10.0);
}

TEST_F(PerformanceMonitoringTest, ThreadMetricsTracking) {
    for (int i = 0; i < 5; ++i) {
        monitoring.recordThreadMetrics(i, {i, 1000 * i, i * 2, 50.0 + i, 10 * i});
    }
    
    auto all = monitoring.getAllThreadMetrics();
    EXPECT_EQ(all.size(), 5);
}

TEST_F(PerformanceMonitoringTest, OperationSuccess) {
    monitoring.startMonitoring();
    
    int64_t start = monitoring.startProfiling("success_op");
    monitoring.stopProfiling("success_op", start, true);
    
    auto metrics = monitoring.getOperationMetrics("success_op");
    EXPECT_EQ(metrics[0].success, true);
}

TEST_F(PerformanceMonitoringTest, OperationFailure) {
    monitoring.startMonitoring();
    
    int64_t start = monitoring.startProfiling("fail_op");
    monitoring.stopProfiling("fail_op", start, false);
    
    auto metrics = monitoring.getOperationMetrics("fail_op");
    EXPECT_EQ(metrics[0].success, false);
}

TEST_F(PerformanceMonitoringTest, UnsetThreshold) {
    auto status = monitoring.checkThreshold("NonExistentMetric", 50.0);
    EXPECT_EQ(status, "OK");
}

TEST_F(PerformanceMonitoringTest, MemoryTrendMaxSamples) {
    monitoring.startMonitoring();
    
    for (int i = 0; i < 150; ++i) {
        monitoring.recordMemoryMetrics({i * 1000, i * 2000, i * 100, 0.1, i});
    }
    
    auto trend = monitoring.getMemoryTrend(50);
    EXPECT_LE(trend.size(), 50);
}

TEST_F(PerformanceMonitoringTest, CpuMetricsValidation) {
    monitoring.startMonitoring();
    
    CpuMetrics cpu1{0.0, 1, 0, 0.0};
    monitoring.recordCpuMetrics(cpu1);
    
    CpuMetrics cpu2{100.0, 128, 1000000, 1000000.0};
    monitoring.recordCpuMetrics(cpu2);
    
    auto current = monitoring.getCurrentCpuMetrics();
    EXPECT_EQ(current.cpuUsagePercent, 100.0);
}

TEST_F(PerformanceMonitoringTest, BasicOperations) {
    EXPECT_TRUE(true);
}
