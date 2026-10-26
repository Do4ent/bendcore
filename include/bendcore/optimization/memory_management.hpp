// CAD-CORE-053: MemoryManagement - Advanced Memory Management
// M5 Phase: Performance Optimization and Caching
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <deque>

namespace bendcore::optimization {

/**
 * MemoryManagement
 *
 * Advanced memory management with:
 * - Streaming for large geometries
 * - Memory-efficient storage
 * - Lazy loading of complex features
 * - Garbage collection
 * - Memory profiling
 */

struct LazyLoadedObject {
    std::string id;
    std::string storageLocation;
    size_t storedSizeBytes;
    bool isLoaded;
    std::vector<uint8_t> cachedData;
    int64_t lastAccessTime;
    int32_t loadCount;
};

struct MemoryStream {
    std::string id;
    std::vector<uint8_t> buffer;
    size_t chunkSize;
    size_t currentChunkIndex;
    size_t totalChunks;
    bool compressionEnabled;
};

class StreamingGeometryLoader {
public:
    explicit StreamingGeometryLoader(size_t chunkSize = 1048576);  // 1 MB chunks
    ~StreamingGeometryLoader();

    // Streaming operations
    std::string createStream(const std::vector<uint8_t>& data, bool compressed = false);
    std::vector<uint8_t> readChunk(const std::string& streamId, size_t chunkIndex);
    std::vector<uint8_t> readAllChunks(const std::string& streamId);

    // Stream management
    void closeStream(const std::string& streamId);
    void resetStream(const std::string& streamId);
    size_t getChunkCount(const std::string& streamId) const;

    // Statistics
    struct StreamStats {
        size_t totalStreamsActive;
        size_t totalDataBytes;
        size_t chunkSize;
        double compressionRatio;
        std::vector<std::string> activeStreamIds;
    };

    StreamStats getStatistics() const;

private:
    std::map<std::string, std::unique_ptr<MemoryStream>> streams_;
    size_t chunkSize_;

    static std::string generateStreamId();
};

class LazyLoader {
public:
    explicit LazyLoader(size_t maxInMemoryObjects = 100);
    ~LazyLoader();

    // Lazy loading
    std::string registerObject(
        const std::string& storageLocation,
        size_t expectedSizeBytes
    );

    std::vector<uint8_t> load(const std::string& objectId);
    void unload(const std::string& objectId);
    void preload(const std::vector<std::string>& objectIds);

    // Object management
    bool isLoaded(const std::string& objectId) const;
    size_t getInMemoryObjectCount() const;
    size_t getTotalStoredSizeBytes() const;

    // Eviction policy
    void evictLRU();
    void evictOldest();
    void evictAll();

    // Monitoring
    struct LazyLoaderStats {
        size_t totalRegisteredObjects;
        size_t loadedObjects;
        size_t unloadedObjects;
        size_t totalStoredSizeBytes;
        size_t maxInMemoryObjects;
        double hitRate;
        int64_t totalLoads;
        int64_t totalEvictions;
    };

    LazyLoaderStats getStatistics() const;
    std::string generateReport() const;

private:
    std::map<std::string, std::unique_ptr<LazyLoadedObject>> objects_;
    std::deque<std::string> accessOrder_;

    size_t maxInMemoryObjects_;
    int64_t totalLoads_ = 0;
    int64_t totalHits_ = 0;
    int64_t totalEvictions_ = 0;

    static std::string generateObjectId();
    void updateAccessOrder(const std::string& objectId);
};

class MemoryProfiler {
public:
    MemoryProfiler();
    ~MemoryProfiler();

    // Profiling control
    void startProfiling();
    void stopProfiling();
    void resetMetrics();

    // Memory tracking
    void recordAllocation(const std::string& type, size_t sizeBytes);
    void recordDeallocation(const std::string& type, size_t sizeBytes);

    // Statistics
    struct MemoryStats {
        size_t totalAllocations;
        size_t totalDeallocations;
        size_t currentMemoryUsageBytes;
        size_t peakMemoryUsageBytes;
        std::map<std::string, size_t> memoryByType;
        double fragmentationPercent;
    };

    MemoryStats getStatistics() const;

    // Garbage collection
    void triggerGarbageCollection();
    void optimizeMemoryLayout();

    // Reporting
    std::string generateMemoryReport() const;
    std::vector<std::pair<std::string, size_t>> getMemoryByType() const;

    bool isProfilingActive() const;

private:
    bool profilingActive_ = false;
    size_t currentUsageBytes_ = 0;
    size_t peakUsageBytes_ = 0;

    std::map<std::string, size_t> memoryByType_;
    int64_t totalAllocations_ = 0;
    int64_t totalDeallocations_ = 0;
};

}  // namespace bendcore::optimization
