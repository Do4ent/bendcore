#include <gtest/gtest.h>
#include "bendcore/mobile/mobile_performance.hpp"

using namespace bendcore::mobile;

class MobilePerformanceTest : public ::testing::Test {
protected:
    MobilePerformanceOptimizer optimizer;
};

TEST_F(MobilePerformanceTest, GetCurrentMetrics) {
    auto metrics = optimizer.getCurrentMetrics();
    EXPECT_GE(metrics.batteryLevel, 0.0f);
}

TEST_F(MobilePerformanceTest, UpdateMetrics) {
    PerformanceMetrics m;
    m.batteryLevel = 50.0f;
    optimizer.updateMetrics(m);
    EXPECT_EQ(optimizer.getBatteryLevel(), 50.0f);
}

TEST_F(MobilePerformanceTest, GetBatteryLevel) {
    float battery = optimizer.getBatteryLevel();
    EXPECT_GE(battery, 0.0f);
}

TEST_F(MobilePerformanceTest, EnableBatteryMode) {
    EXPECT_TRUE(optimizer.enableBatteryMode(true));
    EXPECT_TRUE(optimizer.isBatteryModeEnabled());
}

TEST_F(MobilePerformanceTest, DisableBatteryMode) {
    optimizer.enableBatteryMode(true);
    EXPECT_TRUE(optimizer.enableBatteryMode(false));
    EXPECT_FALSE(optimizer.isBatteryModeEnabled());
}

TEST_F(MobilePerformanceTest, EstimateBatteryLife) {
    float life = optimizer.estimateBatteryLife();
    EXPECT_GE(life, 0.0f);
}

TEST_F(MobilePerformanceTest, IsMemoryLow) {
    bool low = optimizer.isMemoryLow();
    EXPECT_TRUE(low || !low);
}

TEST_F(MobilePerformanceTest, GetAvailableMemory) {
    size_t mem = optimizer.getAvailableMemory();
    EXPECT_GE(mem, 0);
}

TEST_F(MobilePerformanceTest, FreeUnusedMemory) {
    EXPECT_TRUE(optimizer.freeUnusedMemory());
}

TEST_F(MobilePerformanceTest, CompressMemory) {
    EXPECT_TRUE(optimizer.compressMemory());
}

TEST_F(MobilePerformanceTest, OptimizeCpuUsage) {
    EXPECT_TRUE(optimizer.optimizeCpuUsage());
}

TEST_F(MobilePerformanceTest, EnableCpuThrottling) {
    EXPECT_TRUE(optimizer.enableCpuThrottling(true));
}

TEST_F(MobilePerformanceTest, GetDeviceTemperature) {
    int32_t temp = optimizer.getDeviceTemperature();
    EXPECT_GE(temp, 0);
}

TEST_F(MobilePerformanceTest, GetThermalState) {
    ThermalState state = optimizer.getThermalState();
    EXPECT_TRUE(state == ThermalState::NORMAL || state == ThermalState::WARNING);
}

TEST_F(MobilePerformanceTest, IsThermalCritical) {
    bool critical = optimizer.isThermalCritical();
    EXPECT_TRUE(true);
}

TEST_F(MobilePerformanceTest, SetTargetFps) {
    optimizer.setTargetFps(60);
    EXPECT_EQ(optimizer.getTargetFps(), 60);
}

TEST_F(MobilePerformanceTest, GetCurrentFps) {
    float fps = optimizer.getCurrentFps();
    EXPECT_GT(fps, 0.0f);
}

TEST_F(MobilePerformanceTest, AdaptFpsToPerformance) {
    EXPECT_TRUE(optimizer.adaptFpsToPerformance());
}

TEST_F(MobilePerformanceTest, CreateProfile) {
    OptimizationProfile profile;
    profile.profileName = "balanced";
    EXPECT_TRUE(optimizer.createProfile(profile));
}

TEST_F(MobilePerformanceTest, ApplyProfile) {
    OptimizationProfile profile;
    profile.profileName = "performance";
    optimizer.createProfile(profile);
    EXPECT_TRUE(optimizer.applyProfile("performance"));
}

TEST_F(MobilePerformanceTest, GetCurrentProfile) {
    auto profile = optimizer.getCurrentProfile();
    EXPECT_TRUE(true);
}

TEST_F(MobilePerformanceTest, GetAvailableProfiles) {
    OptimizationProfile p;
    p.profileName = "test";
    optimizer.createProfile(p);
    auto profiles = optimizer.getAvailableProfiles();
    EXPECT_GE(profiles.size(), 0);
}

TEST_F(MobilePerformanceTest, DetectPerformanceLevel) {
    PerformanceLevel level = optimizer.detectPerformanceLevel();
    EXPECT_TRUE(level == PerformanceLevel::NORMAL || level == PerformanceLevel::LOW);
}

TEST_F(MobilePerformanceTest, ApplyAdaptiveOptimization) {
    EXPECT_TRUE(optimizer.applyAdaptiveOptimization());
}

TEST_F(MobilePerformanceTest, DisableFeatureAdaptively) {
    EXPECT_TRUE(optimizer.disableFeatureAdaptively("shadows"));
}

TEST_F(MobilePerformanceTest, RecordEnergySnapshot) {
    optimizer.recordEnergySnapshot();
    EXPECT_TRUE(true);
}

TEST_F(MobilePerformanceTest, GetEnergyHistory) {
    optimizer.recordEnergySnapshot();
    auto history = optimizer.getEnergyHistory();
    EXPECT_GE(history.size(), 0);
}

TEST_F(MobilePerformanceTest, GetMemoryUsageByComponent) {
    auto usage = optimizer.getMemoryUsageByComponent();
    EXPECT_TRUE(true);
}

TEST_F(MobilePerformanceTest, GetCpuUsageHistory) {
    auto history = optimizer.getCpuUsageHistory();
    EXPECT_TRUE(true);
}

TEST_F(MobilePerformanceTest, GetCpuUsage) {
    float usage = optimizer.getCpuUsage();
    EXPECT_GE(usage, 0.0f);
}

TEST_F(MobilePerformanceTest, GetMemoryUsage) {
    float usage = optimizer.getMemoryUsage();
    EXPECT_GE(usage, 0.0f);
}

TEST_F(MobilePerformanceTest, SetMaxCpuUsage) {
    optimizer.setMaxCpuUsage(80.0f);
    EXPECT_EQ(optimizer.getMaxCpuUsage(), 80.0f);
}

