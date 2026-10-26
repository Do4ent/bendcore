// CAD-CORE-055: OptimizationProfiles Implementation
#include "bendcore/optimization/optimization_profiles.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <thread>

namespace bendcore::optimization {

// ProfileConfiguration implementation
ProfileConfiguration::ProfileConfiguration(OptimizationProfile profile)
    : profile_(profile) {

    initializeForProfile(profile);
}

void ProfileConfiguration::setProfile(OptimizationProfile profile) {
    profile_ = profile;
    initializeForProfile(profile);
}

OptimizationProfile ProfileConfiguration::getProfile() const {
    return profile_;
}

const CacheConfiguration& ProfileConfiguration::getCacheConfig() const {
    return cacheConfig_;
}

void ProfileConfiguration::setCacheConfig(const CacheConfiguration& config) {
    cacheConfig_ = config;
}

const ParallelConfiguration& ProfileConfiguration::getParallelConfig() const {
    return parallelConfig_;
}

void ProfileConfiguration::setParallelConfig(const ParallelConfiguration& config) {
    parallelConfig_ = config;
}

const MemoryConfiguration& ProfileConfiguration::getMemoryConfig() const {
    return memoryConfig_;
}

void ProfileConfiguration::setMemoryConfig(const MemoryConfiguration& config) {
    memoryConfig_ = config;
}

const FeatureSimplification& ProfileConfiguration::getFeatureSimplification() const {
    return featureSimplification_;
}

void ProfileConfiguration::setFeatureSimplification(const FeatureSimplification& config) {
    featureSimplification_ = config;
}

const ViewportLOD& ProfileConfiguration::getViewportLOD() const {
    return viewportLOD_;
}

void ProfileConfiguration::setViewportLOD(const ViewportLOD& config) {
    viewportLOD_ = config;
}

const BackgroundComputation& ProfileConfiguration::getBackgroundComputation() const {
    return backgroundComputation_;
}

void ProfileConfiguration::setBackgroundComputation(const BackgroundComputation& config) {
    backgroundComputation_ = config;
}

void ProfileConfiguration::loadFromJson(const std::string& jsonConfig) {
    // Placeholder for JSON loading
}

std::string ProfileConfiguration::exportToJson() const {
    // Placeholder for JSON export
    return "{}";
}

bool ProfileConfiguration::validate() {
    validationErrors_.clear();

    if (cacheConfig_.maxGeometryCacheSizeBytes < 1000000) {
        validationErrors_.push_back("Geometry cache size too small");
    }

    if (parallelConfig_.threadCount == 0) {
        validationErrors_.push_back("Thread count must be greater than 0");
    }

    if (memoryConfig_.maxInMemoryElements == 0) {
        validationErrors_.push_back("Max in-memory elements must be greater than 0");
    }

    return validationErrors_.empty();
}

std::vector<std::string> ProfileConfiguration::getValidationErrors() const {
    return validationErrors_;
}

std::string ProfileConfiguration::generateSummary() const {
    std::ostringstream oss;

    oss << "Profile: ";
    switch (profile_) {
        case OptimizationProfile::Draft:
            oss << "Draft\n";
            break;
        case OptimizationProfile::Standard:
            oss << "Standard\n";
            break;
        case OptimizationProfile::High:
            oss << "High\n";
            break;
    }

    oss << "Cache Configuration:\n";
    oss << "  - Geometry Cache Enabled: " << cacheConfig_.enableGeometryCache << "\n";
    oss << "  - Geometry Cache Size: " << (cacheConfig_.maxGeometryCacheSizeBytes / (1024 * 1024)) << " MB\n";
    oss << "  - Computation Cache Size: " << (cacheConfig_.maxComputationCacheSizeBytes / (1024 * 1024)) << " MB\n";

    oss << "Parallel Configuration:\n";
    oss << "  - Parallel Enabled: " << parallelConfig_.enableParallelComputation << "\n";
    oss << "  - Thread Count: " << parallelConfig_.threadCount << "\n";

    oss << "Memory Configuration:\n";
    oss << "  - Lazy Loading: " << memoryConfig_.enableLazyLoading << "\n";
    oss << "  - Streaming: " << memoryConfig_.enableStreaming << "\n";
    oss << "  - Max Memory: " << (memoryConfig_.memoryLimitBytes / (1024 * 1024)) << " MB\n";

    return oss.str();
}

void ProfileConfiguration::initializeForProfile(OptimizationProfile profile) {
    switch (profile) {
        case OptimizationProfile::Draft:
            // Draft: minimal caching, low memory usage
            cacheConfig_.enableGeometryCache = true;
            cacheConfig_.maxGeometryCacheSizeBytes = 67108864;  // 64 MB
            cacheConfig_.enableComputationCache = false;

            parallelConfig_.enableParallelComputation = false;
            parallelConfig_.threadCount = 1;

            memoryConfig_.enableLazyLoading = true;
            memoryConfig_.maxInMemoryElements = 100;
            memoryConfig_.memoryLimitBytes = 268435456;  // 256 MB

            featureSimplification_.enableSimplification = true;
            featureSimplification_.detailLevel = 0.3;

            viewportLOD_.enableLOD = true;
            viewportLOD_.maxDetailLevel = 2;

            backgroundComputation_.enableBackgroundTasks = false;
            break;

        case OptimizationProfile::Standard:
            // Standard: balanced caching and performance
            cacheConfig_.enableGeometryCache = true;
            cacheConfig_.maxGeometryCacheSizeBytes = 268435456;  // 256 MB
            cacheConfig_.enableComputationCache = true;
            cacheConfig_.maxComputationCacheSizeBytes = 134217728;  // 128 MB

            parallelConfig_.enableParallelComputation = true;
            parallelConfig_.threadCount = std::thread::hardware_concurrency();

            memoryConfig_.enableLazyLoading = true;
            memoryConfig_.maxInMemoryElements = 500;
            memoryConfig_.memoryLimitBytes = 1073741824;  // 1 GB

            featureSimplification_.enableSimplification = false;
            featureSimplification_.detailLevel = 1.0;

            viewportLOD_.enableLOD = true;
            viewportLOD_.maxDetailLevel = 4;

            backgroundComputation_.enableBackgroundTasks = true;
            backgroundComputation_.maxBackgroundThreads = 2;
            break;

        case OptimizationProfile::High:
            // High: aggressive caching, maximum quality
            cacheConfig_.enableGeometryCache = true;
            cacheConfig_.maxGeometryCacheSizeBytes = 536870912;  // 512 MB
            cacheConfig_.enableComputationCache = true;
            cacheConfig_.maxComputationCacheSizeBytes = 268435456;  // 256 MB

            parallelConfig_.enableParallelComputation = true;
            parallelConfig_.threadCount = std::thread::hardware_concurrency();

            memoryConfig_.enableLazyLoading = false;
            memoryConfig_.maxInMemoryElements = 10000;
            memoryConfig_.memoryLimitBytes = 4294967296;  // 4 GB

            featureSimplification_.enableSimplification = false;
            featureSimplification_.detailLevel = 1.0;

            viewportLOD_.enableLOD = true;
            viewportLOD_.maxDetailLevel = 8;

            backgroundComputation_.enableBackgroundTasks = true;
            backgroundComputation_.maxBackgroundThreads = 4;
            break;
    }
}

// AdaptiveOptimizer implementation
AdaptiveOptimizer::AdaptiveOptimizer()
    : currentProfile_(OptimizationProfile::Standard),
      currentConfig_(OptimizationProfile::Standard) {}

AdaptiveOptimizer::~AdaptiveOptimizer() {}

void AdaptiveOptimizer::switchProfile(OptimizationProfile newProfile) {
    currentProfile_ = newProfile;
    currentConfig_.setProfile(newProfile);
    profileSwitchCount_++;
}

void AdaptiveOptimizer::switchProfileAdaptively(
    int32_t elementCount,
    size_t availableMemoryBytes,
    double targetFrameTime) {

    OptimizationProfile recommended = currentProfile_;

    if (elementCount < 1000 && availableMemoryBytes > 536870912) {
        recommended = OptimizationProfile::Draft;
    } else if (elementCount < 10000 && availableMemoryBytes > 1073741824) {
        recommended = OptimizationProfile::Standard;
    } else {
        recommended = OptimizationProfile::High;
    }

    if (recommended != currentProfile_) {
        switchProfile(recommended);
    }
}

void AdaptiveOptimizer::recordFrameMetrics(
    int64_t frameTimeMs,
    size_t memoryUsedBytes,
    double cacheHitRate) {

    frameTimeSamples_.push_back(frameTimeMs);
    memorySamples_.push_back(memoryUsedBytes);
    cacheHitRateSamples_.push_back(cacheHitRate);

    // Keep only last 100 samples
    if (frameTimeSamples_.size() > 100) {
        frameTimeSamples_.erase(frameTimeSamples_.begin());
        memorySamples_.erase(memorySamples_.begin());
        cacheHitRateSamples_.erase(cacheHitRateSamples_.begin());
    }

    analyzeMetrics();
}

void AdaptiveOptimizer::recordComputationMetrics(
    int32_t elementCount,
    int64_t computationTimeMs) {

    elementCountSamples_.push_back(elementCount);

    if (elementCountSamples_.size() > 100) {
        elementCountSamples_.erase(elementCountSamples_.begin());
    }
}

ProfileConfiguration& AdaptiveOptimizer::getCurrentConfiguration() {
    return currentConfig_;
}

OptimizationProfile AdaptiveOptimizer::getCurrentProfile() const {
    return currentProfile_;
}

std::vector<AdaptiveOptimizer::OptimizationRecommendation> AdaptiveOptimizer::analyzeAndRecommend() {
    std::vector<OptimizationRecommendation> recommendations;

    if (!frameTimeSamples_.empty()) {
        double avgFrameTime = 0;
        for (int64_t time : frameTimeSamples_) {
            avgFrameTime += time;
        }
        avgFrameTime /= frameTimeSamples_.size();

        if (avgFrameTime > 16.67) {  // Below 60 FPS
            OptimizationRecommendation rec;
            rec.title = "Performance Issues Detected";
            rec.description = "Frame time exceeds 16.67ms, consider switching to Draft mode";
            rec.estimatedImprovementPercent = 30.0;
            rec.priority = "High";
            rec.suggestedProfile = OptimizationProfile::Draft;
            recommendations.push_back(rec);
        }
    }

    if (!cacheHitRateSamples_.empty()) {
        double avgHitRate = 0;
        for (double rate : cacheHitRateSamples_) {
            avgHitRate += rate;
        }
        avgHitRate /= cacheHitRateSamples_.size();

        if (avgHitRate < 0.6) {
            OptimizationRecommendation rec;
            rec.title = "Low Cache Hit Rate";
            rec.description = "Cache hit rate is below 60%, consider increasing cache size";
            rec.estimatedImprovementPercent = 20.0;
            rec.priority = "Medium";
            rec.suggestedProfile = OptimizationProfile::Standard;
            recommendations.push_back(rec);
        }
    }

    return recommendations;
}

AdaptiveOptimizer::AdaptationStats AdaptiveOptimizer::getStatistics() const {
    AdaptationStats stats;
    stats.currentProfile = currentProfile_;
    stats.profileSwitchCount = profileSwitchCount_;

    if (!frameTimeSamples_.empty()) {
        double sum = 0;
        for (int64_t time : frameTimeSamples_) {
            sum += time;
        }
        stats.averageFrameTimeMs = sum / frameTimeSamples_.size();
    }

    if (!cacheHitRateSamples_.empty()) {
        double sum = 0;
        for (double rate : cacheHitRateSamples_) {
            sum += rate;
        }
        stats.averageCacheHitRate = sum / cacheHitRateSamples_.size();
    }

    if (!memorySamples_.empty()) {
        for (size_t mem : memorySamples_) {
            if (mem > stats.peakMemoryUsageBytes) {
                stats.peakMemoryUsageBytes = mem;
            }
        }
    }

    if (!elementCountSamples_.empty()) {
        for (int32_t count : elementCountSamples_) {
            stats.totalElementsProcessed += count;
        }
    }

    return stats;
}

std::string AdaptiveOptimizer::generateAdaptationReport() const {
    std::ostringstream oss;
    auto stats = getStatistics();

    oss << "=== Adaptive Optimization Report ===\n";
    oss << "Current Profile: ";
    switch (stats.currentProfile) {
        case OptimizationProfile::Draft: oss << "Draft"; break;
        case OptimizationProfile::Standard: oss << "Standard"; break;
        case OptimizationProfile::High: oss << "High"; break;
    }
    oss << "\n";

    oss << "Profile Switches: " << stats.profileSwitchCount << "\n";
    oss << "Average Frame Time: " << stats.averageFrameTimeMs << " ms\n";
    oss << "Average Cache Hit Rate: " << (stats.averageCacheHitRate * 100) << "%\n";
    oss << "Peak Memory Usage: " << (stats.peakMemoryUsageBytes / (1024 * 1024)) << " MB\n";

    return oss.str();
}

void AdaptiveOptimizer::analyzeMetrics() {
    // Analyze and potentially recommend profile switches
}

OptimizationProfile AdaptiveOptimizer::recommendProfile() const {
    // Recommend based on current metrics
    return currentProfile_;
}

// GlobalOptimizationContext implementation
static GlobalOptimizationContext* g_globalContext = nullptr;

GlobalOptimizationContext& GlobalOptimizationContext::getInstance() {
    if (g_globalContext == nullptr) {
        g_globalContext = new GlobalOptimizationContext();
    }
    return *g_globalContext;
}

ProfileConfiguration& GlobalOptimizationContext::getConfiguration() {
    return *configuration_;
}

AdaptiveOptimizer& GlobalOptimizationContext::getAdaptiveOptimizer() {
    return *adaptiveOptimizer_;
}

void GlobalOptimizationContext::setProfile(OptimizationProfile profile) {
    configuration_->setProfile(profile);
    adaptiveOptimizer_->switchProfile(profile);
}

OptimizationProfile GlobalOptimizationContext::getProfile() const {
    return configuration_->getProfile();
}

GlobalOptimizationContext::GlobalOptimizationContext() {
    configuration_ = std::make_unique<ProfileConfiguration>(OptimizationProfile::Standard);
    adaptiveOptimizer_ = std::make_unique<AdaptiveOptimizer>();
}

}  // namespace bendcore::optimization
