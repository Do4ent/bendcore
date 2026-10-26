// CAD-CORE-055: OptimizationProfiles - Performance Optimization Profiles
// M5 Phase: Performance Optimization and Caching
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace bendcore::optimization {

/**
 * OptimizationProfiles
 *
 * Adaptive optimization profiles for different use cases:
 * - Draft mode (quick feedback)
 * - Standard mode (balanced performance)
 * - High mode (maximum quality)
 *
 * Features:
 * - Adaptive caching based on profile
 * - Feature simplification options
 * - Viewport LOD (Level of Detail)
 * - Background computation scheduling
 */

enum class OptimizationProfile {
    Draft,      // Fast feedback, minimal memory usage
    Standard,   // Balanced performance and quality
    High        // Maximum quality and accuracy
};

struct CacheConfiguration {
    bool enableGeometryCache;
    bool enableComputationCache;
    size_t maxGeometryCacheSizeBytes;
    size_t maxComputationCacheSizeBytes;
    int64_t defaultTTLMilliseconds;
    bool enableLRUEviction;
    double cacheHitTargetRate;
};

struct ParallelConfiguration {
    bool enableParallelComputation;
    uint32_t threadCount;
    bool enableTaskGraph;
    bool enableDependencyResolution;
    int32_t maxQueuedTasks;
    bool enableWorkStealing;
};

struct MemoryConfiguration {
    bool enableLazyLoading;
    bool enableStreaming;
    size_t maxInMemoryElements;
    size_t streamingChunkSizeBytes;
    bool enableCompression;
    size_t memoryLimitBytes;
    bool enableGarbageCollection;
};

struct FeatureSimplification {
    bool enableSimplification;
    double detailLevel;  // 0.0 (least detail) to 1.0 (full detail)
    bool simplifyComplexCurves;
    bool mergeSmallFaces;
    int32_t maxTriangles;
};

struct ViewportLOD {
    bool enableLOD;
    double screenSpaceThreshold;
    int32_t maxDetailLevel;
    bool enableCulling;
    bool enableProgressiveLoading;
};

struct BackgroundComputation {
    bool enableBackgroundTasks;
    int32_t maxBackgroundThreads;
    int64_t backgroundTaskPriorityLevel;
    bool prioritizeVisibleGeometry;
    int32_t maxBackgroundTasksPerFrame;
};

class ProfileConfiguration {
public:
    explicit ProfileConfiguration(OptimizationProfile profile = OptimizationProfile::Standard);

    // Profile management
    void setProfile(OptimizationProfile profile);
    OptimizationProfile getProfile() const;

    // Cache configuration
    const CacheConfiguration& getCacheConfig() const;
    void setCacheConfig(const CacheConfiguration& config);

    // Parallel configuration
    const ParallelConfiguration& getParallelConfig() const;
    void setParallelConfig(const ParallelConfiguration& config);

    // Memory configuration
    const MemoryConfiguration& getMemoryConfig() const;
    void setMemoryConfig(const MemoryConfiguration& config);

    // Feature simplification
    const FeatureSimplification& getFeatureSimplification() const;
    void setFeatureSimplification(const FeatureSimplification& config);

    // Viewport LOD
    const ViewportLOD& getViewportLOD() const;
    void setViewportLOD(const ViewportLOD& config);

    // Background computation
    const BackgroundComputation& getBackgroundComputation() const;
    void setBackgroundComputation(const BackgroundComputation& config);

    // Custom configuration
    void loadFromJson(const std::string& jsonConfig);
    std::string exportToJson() const;

    // Validation
    bool validate();
    std::vector<std::string> getValidationErrors() const;

    // Summary
    std::string generateSummary() const;

private:
    OptimizationProfile profile_;

    CacheConfiguration cacheConfig_;
    ParallelConfiguration parallelConfig_;
    MemoryConfiguration memoryConfig_;
    FeatureSimplification featureSimplification_;
    ViewportLOD viewportLOD_;
    BackgroundComputation backgroundComputation_;

    void initializeForProfile(OptimizationProfile profile);
    std::vector<std::string> validationErrors_;
};

class AdaptiveOptimizer {
public:
    AdaptiveOptimizer();
    ~AdaptiveOptimizer();

    // Profile switching
    void switchProfile(OptimizationProfile newProfile);
    void switchProfileAdaptively(
        int32_t elementCount,
        size_t availableMemoryBytes,
        double targetFrameTime
    );

    // Monitoring and adaptation
    void recordFrameMetrics(
        int64_t frameTimeMs,
        size_t memoryUsedBytes,
        double cacheHitRate
    );

    void recordComputationMetrics(
        int32_t elementCount,
        int64_t computationTimeMs
    );

    // Configuration queries
    ProfileConfiguration& getCurrentConfiguration();
    OptimizationProfile getCurrentProfile() const;

    // Recommendations
    struct OptimizationRecommendation {
        std::string title;
        std::string description;
        double estimatedImprovementPercent;
        std::string priority;
        OptimizationProfile suggestedProfile;
    };

    std::vector<OptimizationRecommendation> analyzeAndRecommend();

    // Statistics
    struct AdaptationStats {
        OptimizationProfile currentProfile;
        int32_t profileSwitchCount;
        double averageFrameTimeMs;
        double averageCacheHitRate;
        int64_t totalElementsProcessed;
        size_t peakMemoryUsageBytes;
    };

    AdaptationStats getStatistics() const;
    std::string generateAdaptationReport() const;

private:
    ProfileConfiguration currentConfig_;
    OptimizationProfile currentProfile_;

    // Metrics tracking
    std::vector<int64_t> frameTimeSamples_;
    std::vector<size_t> memorySamples_;
    std::vector<double> cacheHitRateSamples_;
    std::vector<int32_t> elementCountSamples_;

    int32_t profileSwitchCount_ = 0;

    void analyzeMetrics();
    OptimizationProfile recommendProfile() const;
};

// Global optimization context
class GlobalOptimizationContext {
public:
    static GlobalOptimizationContext& getInstance();

    ProfileConfiguration& getConfiguration();
    AdaptiveOptimizer& getAdaptiveOptimizer();

    void setProfile(OptimizationProfile profile);
    OptimizationProfile getProfile() const;

private:
    GlobalOptimizationContext();

    std::unique_ptr<ProfileConfiguration> configuration_;
    std::unique_ptr<AdaptiveOptimizer> adaptiveOptimizer_;
};

}  // namespace bendcore::optimization
