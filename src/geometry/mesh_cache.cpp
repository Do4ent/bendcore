// CAD-CORE-030: MeshCache Implementation
// LRU Cache for Mesh Geometry
#include "bendcore/geometry/mesh_cache.hpp"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace bendcore::geometry {

MeshCache::MeshCache(size_t maxMemorySizeMb)
    : maxMemorySizeMb_(maxMemorySizeMb),
      currentMemoryUsageMb_(0),
      hitCount_(0),
      missCount_(0) {
}

MeshCache::~MeshCache() {
    clear();
}

bool MeshCache::getMesh(const std::string& meshId, MeshData& outMesh) {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto it = cache_.find(meshId);
    if (it != cache_.end()) {
        outMesh = it->second.data;
        hitCount_++;
        updateLRU(meshId);
        return true;
    }

    missCount_++;
    return false;
}

void MeshCache::putMesh(const std::string& meshId, const MeshData& mesh) {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    size_t meshMemSize = mesh.getMemorySize() / (1024 * 1024);  // Convert to MB
    evictIfNeeded(meshMemSize);

    // Create cache entry with statistics
    CacheEntry entry;
    entry.data = mesh;
    entry.stats.meshId = meshId;
    entry.stats.vertexCount = mesh.vertices.size() / 3;
    entry.stats.triangleCount = mesh.indices.size() / 3;
    entry.stats.normalCount = mesh.normals.size() / 3;
    entry.stats.memoryUsageMb = meshMemSize;
    entry.stats.creationTime = std::chrono::system_clock::now().time_since_epoch().count();
    entry.stats.lastAccessTime = entry.stats.creationTime;
    entry.stats.accessCount = 0;

    cache_[meshId] = entry;
    currentMemoryUsageMb_ += meshMemSize;

    updateLRU(meshId);
}

bool MeshCache::removeMesh(const std::string& meshId) {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto it = cache_.find(meshId);
    if (it != cache_.end()) {
        currentMemoryUsageMb_ -= it->second.stats.memoryUsageMb;
        lruList_.erase(it->second.listIterator);
        cache_.erase(it);
        return true;
    }

    return false;
}

void MeshCache::clear() {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    cache_.clear();
    lruList_.clear();
    currentMemoryUsageMb_ = 0;
    hitCount_ = 0;
    missCount_ = 0;
}

std::vector<bool> MeshCache::getMeshBatch(
    const std::vector<std::string>& meshIds,
    std::vector<MeshData>& outMeshes) {

    std::vector<bool> results(meshIds.size());
    outMeshes.resize(meshIds.size());

    for (size_t i = 0; i < meshIds.size(); ++i) {
        results[i] = getMesh(meshIds[i], outMeshes[i]);
    }

    return results;
}

void MeshCache::putMeshBatch(
    const std::vector<std::string>& meshIds,
    const std::vector<MeshData>& meshes) {

    for (size_t i = 0; i < meshIds.size() && i < meshes.size(); ++i) {
        putMesh(meshIds[i], meshes[i]);
    }
}

MeshCache::CacheStats MeshCache::getStatistics() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    CacheStats stats;
    stats.hitCount = hitCount_;
    stats.missCount = missCount_;
    stats.hitRate = (hitCount_ + missCount_ > 0)
        ? static_cast<double>(hitCount_) / (hitCount_ + missCount_)
        : 0.0;
    stats.cachedMeshCount = cache_.size();
    stats.currentMemoryUsageMb = currentMemoryUsageMb_;
    stats.maxMemoryUsageMb = maxMemorySizeMb_;

    for (const auto& pair : cache_) {
        stats.meshStats.push_back(pair.second.stats);
    }

    return stats;
}

void MeshCache::printStatistics() const {
    auto stats = getStatistics();

    std::cout << "=== Mesh Cache Statistics ===" << std::endl;
    std::cout << "Hit Rate: " << (stats.hitRate * 100.0) << "%" << std::endl;
    std::cout << "Cached Meshes: " << stats.cachedMeshCount << std::endl;
    std::cout << "Memory Usage: " << stats.currentMemoryUsageMb << "/"
              << stats.maxMemoryUsageMb << " MB" << std::endl;
    std::cout << "Total Hits: " << stats.hitCount << std::endl;
    std::cout << "Total Misses: " << stats.missCount << std::endl;
}

void MeshCache::setMaxMemorySize(size_t maxMemorySizeMb) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    maxMemorySizeMb_ = maxMemorySizeMb;
    evictByMemoryPressure();
}

size_t MeshCache::getMaxMemorySize() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return maxMemorySizeMb_;
}

size_t MeshCache::getCurrentMemoryUsage() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return currentMemoryUsageMb_;
}

int32_t MeshCache::getCachedMeshCount() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return cache_.size();
}

int32_t MeshCache::evictLeastRecentlyUsed() {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    if (lruList_.empty()) {
        return 0;
    }

    std::string lruId = lruList_.back();
    auto it = cache_.find(lruId);
    if (it != cache_.end()) {
        currentMemoryUsageMb_ -= it->second.stats.memoryUsageMb;
        lruList_.erase(it->second.listIterator);
        cache_.erase(it);
        return 1;
    }

    return 0;
}

int32_t MeshCache::evictByMemoryPressure() {
    int32_t evicted = 0;

    while (currentMemoryUsageMb_ > maxMemorySizeMb_ && !lruList_.empty()) {
        evicted += evictLeastRecentlyUsed();
    }

    return evicted;
}

bool MeshCache::validateMesh(const std::string& meshId) const {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto it = cache_.find(meshId);
    if (it == cache_.end()) {
        return false;
    }

    const MeshData& mesh = it->second.data;
    return !mesh.vertices.empty() && !mesh.indices.empty();
}

bool MeshCache::validateChecksum(
    const std::string& meshId,
    const MeshData& mesh) const {

    uint32_t computedVertexChecksum = computeChecksum(mesh.vertices);
    uint32_t computedIndexChecksum = computeChecksum(mesh.indices);

    return (mesh.checksumVertices == computedVertexChecksum) &&
           (mesh.checksumIndices == computedIndexChecksum);
}

bool MeshCache::getMeshStatistics(
    const std::string& meshId,
    MeshStatistics& outStats) const {

    std::lock_guard<std::mutex> lock(cacheMutex_);

    auto it = cache_.find(meshId);
    if (it != cache_.end()) {
        outStats = it->second.stats;
        return true;
    }

    return false;
}

void MeshCache::configureStreaming(const StreamingConfig& config) {
    // Streaming configuration stored for later use
}

std::vector<MeshCache::MeshChunk> MeshCache::streamMesh(
    const std::string& meshId,
    size_t chunkSize) {

    std::vector<MeshChunk> chunks;

    MeshData mesh;
    if (!getMesh(meshId, mesh)) {
        return chunks;
    }

    // Split vertices into chunks
    size_t vertexCount = mesh.vertices.size() / 3;
    size_t numChunks = (vertexCount + chunkSize - 1) / chunkSize;

    for (size_t i = 0; i < numChunks; ++i) {
        MeshChunk chunk;
        chunk.meshId = meshId;
        chunk.chunkIndex = i;

        size_t startVertex = i * chunkSize;
        size_t endVertex = std::min(startVertex + chunkSize, vertexCount);

        size_t startIdx = startVertex * 3;
        size_t endIdx = endVertex * 3;

        chunk.vertexData.assign(
            mesh.vertices.begin() + startIdx,
            mesh.vertices.begin() + endIdx
        );

        // Include all indices that reference vertices in this chunk
        for (size_t j = 0; j < mesh.indices.size(); j += 3) {
            if (mesh.indices[j] >= startVertex && mesh.indices[j] < endVertex) {
                chunk.indexData.push_back(mesh.indices[j] - startVertex);
                if (j + 1 < mesh.indices.size()) chunk.indexData.push_back(mesh.indices[j + 1] - startVertex);
                if (j + 2 < mesh.indices.size()) chunk.indexData.push_back(mesh.indices[j + 2] - startVertex);
            }
        }

        chunk.isLastChunk = (i == numChunks - 1);
        chunks.push_back(chunk);
    }

    return chunks;
}

void MeshCache::consolidateMeshes() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    // Defragmentation could happen here
}

std::string MeshCache::dumpCacheState() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);

    std::ostringstream oss;
    oss << "Mesh Cache State:\n";
    oss << "  Total Meshes: " << cache_.size() << "\n";
    oss << "  Memory Usage: " << currentMemoryUsageMb_ << " MB\n";
    oss << "  Max Memory: " << maxMemorySizeMb_ << " MB\n";
    oss << "  Hit Rate: " << (hitCount_ + missCount_ > 0
        ? 100.0 * hitCount_ / (hitCount_ + missCount_) : 0.0) << "%\n";

    for (const auto& pair : cache_) {
        oss << "  - " << pair.first << ": "
            << pair.second.stats.memoryUsageMb << " MB, "
            << pair.second.stats.vertexCount << " vertices\n";
    }

    return oss.str();
}

void MeshCache::updateLRU(const std::string& meshId) {
    auto it = cache_.find(meshId);
    if (it != cache_.end()) {
        // Move to front of LRU list
        lruList_.erase(it->second.listIterator);
        lruList_.push_front(meshId);
        it->second.listIterator = lruList_.begin();

        // Update access statistics
        it->second.stats.lastAccessTime =
            std::chrono::system_clock::now().time_since_epoch().count();
        it->second.stats.accessCount++;
    }
}

void MeshCache::evictIfNeeded(size_t neededMemory) {
    while (currentMemoryUsageMb_ + neededMemory > maxMemorySizeMb_ && !lruList_.empty()) {
        evictLeastRecentlyUsed();
    }
}

uint32_t MeshCache::computeChecksum(const std::vector<float>& data) const {
    uint32_t checksum = 0;
    for (float val : data) {
        checksum = checksum * 31 + static_cast<uint32_t>(val);
    }
    return checksum;
}

uint32_t MeshCache::computeChecksum(const std::vector<unsigned int>& data) const {
    uint32_t checksum = 0;
    for (unsigned int val : data) {
        checksum = checksum * 31 + val;
    }
    return checksum;
}

}  // namespace bendcore::geometry
