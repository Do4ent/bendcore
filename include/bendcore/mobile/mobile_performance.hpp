#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace bendcore::mobile {

// ==============================================================================
// MOBILE PERFORMANCE - GAP-081
// ==============================================================================
// Battery optimization, memory management, CPU throttling, thermal management

enum class PerformanceLevel {
    CRITICAL,   // Battery critical, minimize everything
    LOW,        // Battery low, reduce features
    NORMAL,     // Standard operation
    HIGH,       // Performance mode
    MAXIMUM     // Full performance
};

enum class ThermalState {
    NORMAL,
    WARNING,
    CRITICAL
};

struct PerformanceMetrics {
    float cpuUsage;
    float memoryUsage;
    float batteryLevel;
    float batteryDrainRate;
    int32_t thermalTemperature;
    ThermalState thermalState;
    int64_t timestamp;
};

struct OptimizationProfile {
    std::string profileName;
    PerformanceLevel targetLevel;
    float maxCpuUsage;
    float maxMemoryUsage;
    float targetFps;
    bool reduceTextureQuality;
    bool reduceShadows;
    bool reduceParticles;
};

class MobilePerformanceOptimizer {
public:
    MobilePerformanceOptimizer();
    ~MobilePerformanceOptimizer();

    // Performance monitoring
    PerformanceMetrics getCurrentMetrics();
    void updateMetrics(const PerformanceMetrics& metrics);
    float getBatteryLevel() const;
    float getCpuUsage() const;
    float getMemoryUsage() const;

    // Battery optimization
    bool enableBatteryMode(bool enable);
    bool isBatteryModeEnabled() const;
    float estimateBatteryLife();
    bool optimizeForBatteryLife();
    int32_t calculateEnergyConsumption();

    // Memory management
    bool isMemoryLow() const;
    size_t getAvailableMemory() const;
    bool freeUnusedMemory();
    bool compressMemory();
    std::map<std::string, size_t> getMemoryUsageByComponent();

    // CPU optimization
    bool optimizeCpuUsage();
    void setMaxCpuUsage(float percentage);
    float getMaxCpuUsage() const;
    bool enableCpuThrottling(bool enable);
    std::vector<float> getCpuUsageHistory();

    // Thermal management
    ThermalState getThermalState() const;
    int32_t getDeviceTemperature() const;
    bool enableThermalThrottling(bool enable);
    bool isThermalCritical() const;

    // FPS optimization
    void setTargetFps(int32_t fps);
    int32_t getTargetFps() const;
    float getCurrentFps() const;
    bool adaptFpsToPerformance();

    // Performance profiles
    bool createProfile(const OptimizationProfile& profile);
    bool applyProfile(const std::string& profileName);
    OptimizationProfile getCurrentProfile() const;
    std::vector<std::string> getAvailableProfiles() const;

    // Adaptive optimization
    PerformanceLevel detectPerformanceLevel();
    bool applyAdaptiveOptimization();
    bool disableFeatureAdaptively(const std::string& featureName);

    // Energy profiling
    struct EnergySnapshot {
        int64_t timestamp;
        float cpuEnergy;
        float gpuEnergy;
        float displayEnergy;
        float networkEnergy;
    };
    void recordEnergySnapshot();
    std::vector<EnergySnapshot> getEnergyHistory();

private:
    PerformanceMetrics metrics;
    std::map<std::string, OptimizationProfile> profiles;
    OptimizationProfile currentProfile;
    bool batteryModeEnabled;
    bool cpuThrottlingEnabled;
    bool thermalThrottlingEnabled;
    std::vector<EnergySnapshot> energyHistory;
};

}  // namespace bendcore::mobile
