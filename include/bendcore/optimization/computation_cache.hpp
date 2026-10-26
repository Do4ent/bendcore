// CAD-CORE-051: ComputationCache - Feature Computation Caching
// M5 Phase: Performance Optimization and Caching
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <chrono>
#include <functional>

namespace bendcore::optimization {

/**
 * ComputationCache
 *
 * Caches results of expensive computations including:
 * - Feature computation results
 * - B-Rep operations (Boolean, fillet, etc.)
 * - Serialization data
 * - Drawing/view calculations
 *
 * Features:
 * - TTL-based expiration
 * - LRU eviction policy
 * - Dependency tracking
 * - Invalidation propagation
 */

struct CacheEntry {
    std::string key;
    std::vector<uint8_t> value;
    std::string computationType;  // "Feature", "Boolean", "Fillet", "Serialization", etc.
    int64_t creationTime;
    int64_t lastAccessTime;
    int64_t ttlMilliseconds;
    int32_t accessCount;
    std::vector<std::string> dependencies;
    bool isValid;
};

class ComputationCache {
public:
    enum class ComputationType {
        FeatureComputation,
        BooleanOperation,
        FilletOperation,
        OffsetOperation,
        SerializationData,
        DrawingCalculation,
        TopologyAnalysis,
        Other
    };

    ComputationCache(
        size_t maxCacheSize = 268435456,  // 256 MB
        int64_t defaultTtlMs = 3600000     // 1 hour
    );
    ~ComputationCache();

    // Cache operations
    bool cacheComputation(
        const std::string& key,
        const std::vector<uint8_t>& value,
        ComputationType type,
        const std::vector<std::string>& dependencies = {},
        int64_t ttlMs = -1  // -1 means use default
    );

    std::unique_ptr<std::vector<uint8_t>> getComputation(const std::string& key);

    bool contains(const std::string& key) const;
    void invalidate(const std::string& key);
    void invalidateDependents(const std::string& key);

    // Cache statistics
    struct CacheStats {
        size_t totalEntries;
        size_t totalMemoryBytes;
        size_t maxMemoryBytes;
        double hitRate;
        int64_t totalHits;
        int64_t totalMisses;
        int64_t totalEvictions;
        int64_t expiredEntries;
        std::map<std::string, int32_t> entriesByType;
    };

    CacheStats getStatistics() const;

    // Memory management
    size_t getCurrentMemoryUsage() const;
    void setMaxMemorySize(size_t maxBytes);
    void compactCache();
    void clearCache();

    // TTL management
    void setDefaultTTL(int64_t ttlMs);
    void removeExpiredEntries();

    // Dependency tracking
    std::vector<std::string> getDependents(const std::string& key) const;
    std::vector<std::string> getDependencies(const std::string& key) const;

    // LRU operations
    void evictLRU(size_t count = 1);
    std::vector<std::string> getLRUQueue() const;

    // Batch operations
    void invalidateBatch(const std::vector<std::string>& keys);
    void cacheBatch(
        const std::vector<std::pair<std::string, std::vector<uint8_t>>>& entries,
        ComputationType type
    );

    // Monitoring and reporting
    std::string generateCacheReport() const;
    std::string generateHtmlReport() const;
    std::map<ComputationType, size_t> getMemoryByType() const;

    // Warming
    bool preloadCache(const std::string& configPath);
    bool exportCache(const std::string& outputPath);

private:
    std::map<std::string, std::unique_ptr<CacheEntry>> cache_;
    std::map<std::string, std::vector<std::string>> dependencyGraph_;

    size_t maxCacheSize_;
    size_t currentSize_;
    int64_t defaultTtl_;

    int64_t totalHits_ = 0;
    int64_t totalMisses_ = 0;
    int64_t totalEvictions_ = 0;
    int64_t totalExpired_ = 0;

    void evictUntilBelow(size_t targetSize);
    std::string findLRUKey() const;
    bool isExpired(const CacheEntry* entry) const;
};

}  // namespace bendcore::optimization
