#include "bendcore/mobile/mobile_performance.hpp"
#include <mutex>
#include <stdexcept>

namespace bendcore::mobile {

namespace {
    thread_local std::mutex g_perf_mutex;
}

MobilePerformanceOptimizer::MobilePerformanceOptimizer()
    : batteryModeEnabled(false),
      cpuThrottlingEnabled(false),
      thermalThrottlingEnabled(false) {
    metrics.cpuUsage = 0.0f;
    metrics.memoryUsage = 0.0f;
    metrics.batteryLevel = 100.0f;
    metrics.batteryDrainRate = 0.5f;
    metrics.thermalTemperature = 35;
    metrics.thermalState = ThermalState::NORMAL;
    metrics.timestamp = 0;
}

MobilePerformanceOptimizer::~MobilePerformanceOptimizer() = default;

PerformanceMetrics MobilePerformanceOptimizer::getCurrentMetrics() {
    return metrics;
}

void MobilePerformanceOptimizer::updateMetrics(const PerformanceMetrics& newMetrics) {
    metrics = newMetrics;
}

float MobilePerformanceOptimizer::getBatteryLevel() const {
    return metrics.batteryLevel;
}

float MobilePerformanceOptimizer::getCpuUsage() const {
    return metrics.cpuUsage;
}

float MobilePerformanceOptimizer::getMemoryUsage() const {
    return metrics.memoryUsage;
}

bool MobilePerformanceOptimizer::enableBatteryMode(bool enable) {
    batteryModeEnabled = enable;
    return true;
}

bool MobilePerformanceOptimizer::isBatteryModeEnabled() const {
    return batteryModeEnabled;
}

float MobilePerformanceOptimizer::estimateBatteryLife() {
    if (metrics.batteryDrainRate <= 0) return -1.0f;
    return metrics.batteryLevel / metrics.batteryDrainRate;
}

bool MobilePerformanceOptimizer::optimizeForBatteryLife() {
    batteryModeEnabled = true;
    return true;
}

int32_t MobilePerformanceOptimizer::calculateEnergyConsumption() {
    return static_cast<int32_t>(metrics.cpuUsage * 100);
}

bool MobilePerformanceOptimizer::isMemoryLow() const {
    return metrics.memoryUsage > 80.0f;
}

size_t MobilePerformanceOptimizer::getAvailableMemory() const {
    return (100.0f - metrics.memoryUsage) * 10485760;  // Approximate in bytes
}

bool MobilePerformanceOptimizer::freeUnusedMemory() {
    metrics.memoryUsage *= 0.9f;
    return true;
}

bool MobilePerformanceOptimizer::compressMemory() {
    return true;
}

std::map<std::string, size_t> MobilePerformanceOptimizer::getMemoryUsageByComponent() {
    return std::map<std::string, size_t>();
}

bool MobilePerformanceOptimizer::optimizeCpuUsage() {
    cpuThrottlingEnabled = true;
    return true;
}

void MobilePerformanceOptimizer::setMaxCpuUsage(float percentage) {
    currentProfile.maxCpuUsage = percentage;
}

float MobilePerformanceOptimizer::getMaxCpuUsage() const {
    return currentProfile.maxCpuUsage;
}

bool MobilePerformanceOptimizer::enableCpuThrottling(bool enable) {
    cpuThrottlingEnabled = enable;
    return true;
}

std::vector<float> MobilePerformanceOptimizer::getCpuUsageHistory() {
    return std::vector<float>();
}

ThermalState MobilePerformanceOptimizer::getThermalState() const {
    return metrics.thermalState;
}

int32_t MobilePerformanceOptimizer::getDeviceTemperature() const {
    return metrics.thermalTemperature;
}

bool MobilePerformanceOptimizer::enableThermalThrottling(bool enable) {
    thermalThrottlingEnabled = enable;
    return true;
}

bool MobilePerformanceOptimizer::isThermalCritical() const {
    return metrics.thermalState == ThermalState::CRITICAL;
}

void MobilePerformanceOptimizer::setTargetFps(int32_t fps) {
    currentProfile.targetFps = fps;
}

int32_t MobilePerformanceOptimizer::getTargetFps() const {
    return static_cast<int32_t>(currentProfile.targetFps);
}

float MobilePerformanceOptimizer::getCurrentFps() const {
    return 60.0f;
}

bool MobilePerformanceOptimizer::adaptFpsToPerformance() {
    return true;
}

bool MobilePerformanceOptimizer::createProfile(const OptimizationProfile& profile) {
    profiles[profile.profileName] = profile;
    return true;
}

bool MobilePerformanceOptimizer::applyProfile(const std::string& profileName) {
    auto it = profiles.find(profileName);
    if (it != profiles.end()) {
        currentProfile = it->second;
        return true;
    }
    return false;
}

OptimizationProfile MobilePerformanceOptimizer::getCurrentProfile() const {
    return currentProfile;
}

std::vector<std::string> MobilePerformanceOptimizer::getAvailableProfiles() const {
    std::vector<std::string> names;
    for (const auto& p : profiles) {
        names.push_back(p.first);
    }
    return names;
}

PerformanceLevel MobilePerformanceOptimizer::detectPerformanceLevel() {
    if (metrics.batteryLevel < 20.0f) return PerformanceLevel::CRITICAL;
    if (metrics.batteryLevel < 50.0f) return PerformanceLevel::LOW;
    return PerformanceLevel::NORMAL;
}

bool MobilePerformanceOptimizer::applyAdaptiveOptimization() {
    return true;
}

bool MobilePerformanceOptimizer::disableFeatureAdaptively(const std::string& featureName) {
    return true;
}

void MobilePerformanceOptimizer::recordEnergySnapshot() {
    EnergySnapshot snap;
    snap.timestamp = 0;
    energyHistory.push_back(snap);
}

std::vector<MobilePerformanceOptimizer::EnergySnapshot> MobilePerformanceOptimizer::getEnergyHistory() {
    return energyHistory;
}

}  // namespace bendcore::mobile
