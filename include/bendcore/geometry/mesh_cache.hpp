// CAD-CORE-030: MeshCache - LRU Cache for Mesh Geometry
// Phase H: Advanced Optimization for 10K+ Elements
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include <list>
#include <mutex>

namespace bendcore::geometry {

/**
 * MeshCache
 *
 * LRU (Least Recently Used) cache for mesh geometry data.
 * Optimizes performance for 10K+ element assemblies by caching:
 * - Vertex arrays (with duplicates removal)
 * - Index arrays (triangulation)
 * - Normal vectors
 * - Bounding boxes
 * - Pre-computed mesh statistics
 *
 * Features:
 * - Automatic eviction of least recently used items
 * - Configurable size limits (memory-based)
 * - Cache statistics tracking
 * - Thread-safe operations
 * - Validation checksums
 */

struct MeshData {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    std::vector<float> normals;
    uint32_t checksumVertices = 0;
    uint32_t checksumIndices = 0;

    size_t getMemorySize() const {
        return vertices.size() * sizeof(float) +
               indices.size() * sizeof(unsigned int) +
               normals.size() * sizeof(float) +
               sizeof(MeshData);
    }
};

struct MeshStatistics {
    std::string meshId;
    int32_t vertexCount;
    int32_t triangleCount;
    int32_t normalCount;
    double memoryUsageMb;
    int64_t creationTime;
    int64_t lastAccessTime;
    int32_t accessCount;
};

class MeshCache {
public:
    explicit MeshCache(size_t maxMemorySizeMb = 512);
    ~MeshCache();

    // Cache operations
    bool getMesh(const std::string& meshId, MeshData& outMesh);
    void putMesh(const std::string& meshId, const MeshData& mesh);
    bool removeMesh(const std::string& meshId);
    void clear();

    // Batch operations (for multiple meshes)
    std::vector<bool> getMeshBatch(
        const std::vector<std::string>& meshIds,
        std::vector<MeshData>& outMeshes
    );

    void putMeshBatch(
        const std::vector<std::string>& meshIds,
        const std::vector<MeshData>& meshes
    );

    // Cache metrics
    struct CacheStats {
        int64_t hitCount;
        int64_t missCount;
        double hitRate;
        size_t cachedMeshCount;
        size_t currentMemoryUsageMb;
        size_t maxMemoryUsageMb;
        std::vector<MeshStatistics> meshStats;
    };

    CacheStats getStatistics() const;
    void printStatistics() const;

    // Configuration
    void setMaxMemorySize(size_t maxMemorySizeMb);
    size_t getMaxMemorySize() const;

    size_t getCurrentMemoryUsage() const;
    int32_t getCachedMeshCount() const;

    // LRU eviction
    int32_t evictLeastRecentlyUsed();
    int32_t evictByMemoryPressure();

    // Validation
    bool validateMesh(const std::string& meshId) const;
    bool validateChecksum(
        const std::string& meshId,
        const MeshData& mesh
    ) const;

    // Statistics for specific mesh
    bool getMeshStatistics(
        const std::string& meshId,
        MeshStatistics& outStats
    ) const;

    // Streaming support for large assemblies
    struct StreamingConfig {
        size_t chunkSize;  // Vertices per chunk
        double priorityThreshold;  // Access frequency threshold
        bool enableCompression;
    };

    void configureStreaming(const StreamingConfig& config);

    // Stream mesh data (for progressive loading)
    struct MeshChunk {
        std::string meshId;
        int32_t chunkIndex;
        std::vector<float> vertexData;
        std::vector<unsigned int> indexData;
        bool isLastChunk;
    };

    std::vector<MeshChunk> streamMesh(
        const std::string& meshId,
        size_t chunkSize
    );

    // Consolidation for memory efficiency
    void consolidateMeshes();

    // Dump cache state for debugging
    std::string dumpCacheState() const;

private:
    struct CacheEntry {
        MeshData data;
        MeshStatistics stats;
        std::list<std::string>::iterator listIterator;
    };

    size_t maxMemorySizeMb_;
    size_t currentMemoryUsageMb_;

    std::unordered_map<std::string, CacheEntry> cache_;
    std::list<std::string> lruList_;  // Most recent first

    int64_t hitCount_ = 0;
    int64_t missCount_ = 0;

    mutable std::mutex cacheMutex_;

    // Helper methods
    void updateLRU(const std::string& meshId);
    void evictIfNeeded(size_t neededMemory);
    uint32_t computeChecksum(const std::vector<float>& data) const;
    uint32_t computeChecksum(const std::vector<unsigned int>& data) const;
};

}  // namespace bendcore::geometry
