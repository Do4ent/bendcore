// CAD-CORE-050: GeometryCache - Geometry Caching System
// M5 Phase: Performance Optimization and Caching
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <chrono>

namespace bendcore::optimization {

/**
 * GeometryCache
 *
 * Advanced caching system for B-Rep geometry and topology data.
 * Provides intelligent invalidation, cascade detection, and memory monitoring.
 */

struct GeometryData {
    std::string id;
    std::vector<uint8_t> topologyData;
    std::vector<uint8_t> geometryData;
    size_t memorySizeBytes;
    int64_t creationTime;
    int64_t lastAccessTime;
    int32_t accessCount;
};

struct CacheInvalidationTracker {
    std::string entityId;
    std::string invalidationReason;
    int64_t invalidationTime;
    std::vector<std::string> cascadedEntities;
    bool wasPropagated;
};

class GeometryCache {
public:
    GeometryCache(size_t maxCacheSizeBytes = 536870912);  // 512 MB default
    ~GeometryCache();

    // Cache operations
    bool cacheGeometry(
        const std::string& entityId,
        const std::vector<uint8_t>& topologyData,
        const std::vector<uint8_t>& geometryData
    );

    std::unique_ptr<GeometryData> getGeometry(const std::string& entityId);

    void invalidateGeometry(
        const std::string& entityId,
        const std::string& reason = "Unknown"
    );

    void invalidateCascade(
        const std::string& entityId,
        const std::vector<std::string>& dependentEntities
    );

    // Cache statistics
    struct CacheStats {
        size_t totalEntriesCount;
        size_t totalMemoryBytes;
        size_t maxMemoryBytes;
        double hitRate;
        int64_t totalHits;
        int64_t totalMisses;
        int64_t totalInvalidations;
        double utilizationPercent;
        std::vector<std::string> largestEntries;
    };

    CacheStats getStatistics() const;

    // Memory management
    size_t getCurrentMemoryUsage() const;
    size_t getMaxMemoryUsage() const;
    void setMaxMemoryUsage(size_t maxBytes);

    bool isMemoryLimited() const;
    double getMemoryUtilizationPercent() const;

    void evictLRU(size_t countToEvict = 1);
    void evictOldest(size_t countToEvict = 1);
    void clearCache();

    // Face/Edge/Vertex caching
    struct TopologyElement {
        std::string id;
        std::string type;  // "Face", "Edge", "Vertex"
        std::vector<uint8_t> data;
        std::vector<std::string> adjacentElements;
    };

    bool cacheTopologyElement(const TopologyElement& element);
    std::unique_ptr<TopologyElement> getTopologyElement(const std::string& elementId);
    void invalidateTopologyElement(const std::string& elementId);

    // Invalidation tracking
    std::vector<CacheInvalidationTracker> getInvalidationHistory() const;
    void clearInvalidationHistory();

    // Monitoring
    void enableMonitoring();
    void disableMonitoring();
    bool isMonitoringEnabled() const;

    std::string generateCacheReport() const;

private:
    std::map<std::string, std::unique_ptr<GeometryData>> geometryCache_;
    std::map<std::string, std::unique_ptr<TopologyElement>> topologyCache_;

    size_t maxCacheSize_;
    size_t currentCacheSize_;

    int64_t totalHits_ = 0;
    int64_t totalMisses_ = 0;
    int64_t totalInvalidations_ = 0;

    std::vector<CacheInvalidationTracker> invalidationHistory_;
    bool monitoringEnabled_ = false;

    void updateCacheSize();
    void evictUntilBelow(size_t targetSize);
    std::string getLargestEntry() const;
};

}  // namespace bendcore::optimization
