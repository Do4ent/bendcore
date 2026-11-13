#include <gtest/gtest.h>
#include <memory>
#include "../include/bendcore/mobile/mobile_performance.hpp"

class MobilePerformanceTest : public ::testing::Test {
protected:
    std::unique_ptr<bendcore::mobile::MobilePerformance> perf;

    void SetUp() override {
        perf = std::make_unique<bendcore::mobile::MobilePerformance>();
    }
};

TEST_F(MobilePerformanceTest, InitializePerformance) {
    EXPECT_NE(perf.get(), nullptr);
}

TEST_F(MobilePerformanceTest, OptimizeMemory) {
    auto optimized = perf->optimizeMemoryUsage();
    EXPECT_TRUE(optimized);
}

TEST_F(MobilePerformanceTest, OptimizeBattery) {
    auto optimized = perf->optimizeBatteryUsage();
    EXPECT_TRUE(optimized);
}

TEST_F(MobilePerformanceTest, OptimizeCPU) {
    auto optimized = perf->optimizeCPUUsage();
    EXPECT_TRUE(optimized);
}

TEST_F(MobilePerformanceTest, EnableLowPowerMode) {
    auto enabled = perf->enableLowPowerMode(true);
    EXPECT_TRUE(enabled);
}

TEST_F(MobilePerformanceTest, ReduceAnimations) {
    auto reduced = perf->reduceAnimationFrameRate(30);
    EXPECT_TRUE(reduced);
}

TEST_F(MobilePerformanceTest, CompressImages) {
    auto compressed = perf->compressImages(0.8);
    EXPECT_TRUE(compressed);
}

TEST_F(MobilePerformanceTest, LimitBackgroundProcesses) {
    auto limited = perf->limitBackgroundProcesses(2);
    EXPECT_TRUE(limited);
}

TEST_F(MobilePerformanceTest, GetMemoryUsage) {
    auto memory = perf->getMemoryUsage();
    EXPECT_GT(memory, 0);
}

TEST_F(MobilePerformanceTest, GetBatteryLevel) {
    auto battery = perf->getBatteryLevel();
    EXPECT_GE(battery, 0.0f);
    EXPECT_LE(battery, 100.0f);
}

TEST_F(MobilePerformanceTest, GetCPUUsage) {
    auto cpu = perf->getCPUUsage();
    EXPECT_GE(cpu, 0.0f);
    EXPECT_LE(cpu, 100.0f);
}

TEST_F(MobilePerformanceTest, MonitorThermalState) {
    auto thermal = perf->getThermalbatteryState();
    EXPECT_FALSE(thermal.empty());
}

TEST_F(MobilePerformanceTest, DeferNonCriticalOperations) {
    auto deferred = perf->deferNonCriticalOperations(true);
    EXPECT_TRUE(deferred);
}

TEST_F(MobilePerformanceTest, LazyLoadAssets) {
    auto lazy = perf->enableLazyLoading(true);
    EXPECT_TRUE(lazy);
}

TEST_F(MobilePerformanceTest, CompileShaders) {
    auto compiled = perf->precompileShaders();
    EXPECT_TRUE(compiled);
}

TEST_F(MobilePerformanceTest, CacheAssets) {
    auto cached = perf->cacheAssets();
    EXPECT_TRUE(cached);
}

TEST_F(MobilePerformanceTest, MonitorFrameRate) {
    auto frames = perf->getFrameRate();
    EXPECT_GT(frames, 0.0f);
}

TEST_F(MobilePerformanceTest, ReduceNetworkUsage) {
    auto reduced = perf->reduceNetworkUsage(true);
    EXPECT_TRUE(reduced);
}

TEST_F(MobilePerformanceTest, CompressNetwork) {
    auto compressed = perf->enableNetworkCompression(true);
    EXPECT_TRUE(compressed);
}

TEST_F(MobilePerformanceTest, OfflineRendering) {
    auto offline = perf->enableOfflineRendering(true);
    EXPECT_TRUE(offline);
}

TEST_F(MobilePerformanceTest, MemoryProfiling) {
    auto profile = perf->profileMemoryUsage();
    EXPECT_GT(profile.peak_memory, 0);
}

TEST_F(MobilePerformanceTest, ClearCaches) {
    auto cleared = perf->clearCaches();
    EXPECT_TRUE(cleared);
}

TEST_F(MobilePerformanceTest, UnloadUnusedAssets) {
    auto unloaded = perf->unloadUnusedAssets();
    EXPECT_TRUE(unloaded);
}

TEST_F(MobilePerformanceTest, GetPerformanceMetrics) {
    auto metrics = perf->getPerformanceMetrics();
    EXPECT_FALSE(metrics.empty());
}

TEST_F(MobilePerformanceTest, GetPerformanceScore) {
    auto score = perf->getPerformanceScore();
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

TEST_F(MobilePerformanceTest, PerformanceProfiling) {
    auto profile_token = perf->startProfilingPerformance();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto profile = perf->stopProfilingPerformance(profile_token);
    EXPECT_GT(profile.execution_time_ms, 0);
}

TEST_F(MobilePerformanceTest, AdaptivePerformance) {
    auto adapted = perf->adaptPerformanceToDevice();
    EXPECT_TRUE(adapted);
}

TEST_F(MobilePerformanceTest, GetPerformanceVersion) {
    auto version = perf->getPerformanceVersion();
    EXPECT_FALSE(version.empty());
}

TEST_F(MobilePerformanceTest, MonitorNetworkQuality) {
    auto quality = perf->monitorNetworkQuality();
    EXPECT_FALSE(quality.empty());
}

TEST_F(MobilePerformanceTest, AutomaticOptimization) {
    auto auto_opt = perf->enableAutomaticOptimization(true);
    EXPECT_TRUE(auto_opt);
}

TEST_F(MobilePerformanceTest, PerformanceNotifications) {
    bool notified = false;
    perf->onPerformanceWarning([&notified]() { notified = true; });
    perf->triggerPerformanceWarning();
    EXPECT_TRUE(notified);
}

TEST_F(MobilePerformanceTest, AsyncOptimization) {
    auto future = perf->optimizeAsync();
    EXPECT_TRUE(future.valid());
}

TEST_F(MobilePerformanceTest, BatteryEstimate) {
    auto estimate = perf->estimateBatteryConsumption();
    EXPECT_GT(estimate, 0.0f);
}
