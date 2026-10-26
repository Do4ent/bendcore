// CAD-CORE-050: GeometryCache Implementation
#include "bendcore/optimization/geometry_cache.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>

namespace bendcore::optimization {

GeometryCache::GeometryCache(size_t maxCacheSizeBytes)
    : maxCacheSize_(maxCacheSizeBytes), currentCacheSize_(0) {}

GeometryCache::~GeometryCache() {
    clearCache();
}

bool GeometryCache::cacheGeometry(
    const std::string& entityId,
    const std::vector<uint8_t>& topologyData,
    const std::vector<uint8_t>& geometryData) {

    size_t totalSize = topologyData.size() + geometryData.size();

    if (totalSize > maxCacheSize_) {
        return false;  // Too large for cache
    }

    // Check if we need to make space
    if (currentCacheSize_ + totalSize > maxCacheSize_) {
        evictUntilBelow(maxCacheSize_ - totalSize);
    }

    auto geometryData_ptr = std::make_unique<GeometryData>();
    geometryData_ptr->id = entityId;
    geometryData_ptr->topologyData = topologyData;
    geometryData_ptr->geometryData = geometryData;
    geometryData_ptr->memorySizeBytes = totalSize;
    geometryData_ptr->creationTime = std::chrono::system_clock::now().time_since_epoch().count();
    geometryData_ptr->lastAccessTime = geometryData_ptr->creationTime;
    geometryData_ptr->accessCount = 0;

    geometryCache_[entityId] = std::move(geometryData_ptr);
    currentCacheSize_ += totalSize;

    return true;
}

std::unique_ptr<GeometryData> GeometryCache::getGeometry(const std::string& entityId) {
    auto it = geometryCache_.find(entityId);
    if (it != geometryCache_.end()) {
        totalHits_++;
        it->second->lastAccessTime = std::chrono::system_clock::now().time_since_epoch().count();
        it->second->accessCount++;

        auto result = std::make_unique<GeometryData>();
        *result = *it->second;
        return result;
    }

    totalMisses_++;
    return nullptr;
}

void GeometryCache::invalidateGeometry(
    const std::string& entityId,
    const std::string& reason) {

    auto it = geometryCache_.find(entityId);
    if (it != geometryCache_.end()) {
        currentCacheSize_ -= it->second->memorySizeBytes;
        geometryCache_.erase(it);

        totalInvalidations_++;

        if (monitoringEnabled_) {
            CacheInvalidationTracker tracker;
            tracker.entityId = entityId;
            tracker.invalidationReason = reason;
            tracker.invalidationTime = std::chrono::system_clock::now().time_since_epoch().count();
            tracker.wasPropagated = false;
            invalidationHistory_.push_back(tracker);
        }
    }
}

void GeometryCache::invalidateCascade(
    const std::string& entityId,
    const std::vector<std::string>& dependentEntities) {

    // Invalidate the primary entity
    invalidateGeometry(entityId, "Cascade invalidation");

    // Invalidate all dependents
    for (const auto& dependent : dependentEntities) {
        invalidateGeometry(dependent, "Cascade dependent");

        if (monitoringEnabled_ && !invalidationHistory_.empty()) {
            invalidationHistory_.back().cascadedEntities.push_back(dependent);
            invalidationHistory_.back().wasPropagated = true;
        }
    }
}

GeometryCache::CacheStats GeometryCache::getStatistics() const {
    CacheStats stats;
    stats.totalEntriesCount = geometryCache_.size() + topologyCache_.size();
    stats.totalMemoryBytes = currentCacheSize_;
    stats.maxMemoryBytes = maxCacheSize_;
    stats.hitRate = (totalHits_ + totalMisses_) > 0
        ? static_cast<double>(totalHits_) / (totalHits_ + totalMisses_)
        : 0.0;
    stats.totalHits = totalHits_;
    stats.totalMisses = totalMisses_;
    stats.totalInvalidations = totalInvalidations_;
    stats.utilizationPercent = maxCacheSize_ > 0
        ? (100.0 * currentCacheSize_) / maxCacheSize_
        : 0.0;

    // Find largest entries
    std::vector<std::pair<std::string, size_t>> entries;
    for (const auto& entry : geometryCache_) {
        entries.push_back({entry.first, entry.second->memorySizeBytes});
    }
    std::sort(entries.rbegin(), entries.rend());

    for (size_t i = 0; i < std::min(size_t(5), entries.size()); ++i) {
        stats.largestEntries.push_back(entries[i].first);
    }

    return stats;
}

size_t GeometryCache::getCurrentMemoryUsage() const {
    return currentCacheSize_;
}

size_t GeometryCache::getMaxMemoryUsage() const {
    return maxCacheSize_;
}

void GeometryCache::setMaxMemoryUsage(size_t maxBytes) {
    maxCacheSize_ = maxBytes;
    if (currentCacheSize_ > maxCacheSize_) {
        evictUntilBelow(maxCacheSize_);
    }
}

bool GeometryCache::isMemoryLimited() const {
    return currentCacheSize_ > (maxCacheSize_ * 0.9);
}

double GeometryCache::getMemoryUtilizationPercent() const {
    return maxCacheSize_ > 0
        ? (100.0 * currentCacheSize_) / maxCacheSize_
        : 0.0;
}

void GeometryCache::evictLRU(size_t countToEvict) {
    std::vector<std::pair<int64_t, std::string>> lruList;

    for (const auto& entry : geometryCache_) {
        lruList.push_back({entry.second->lastAccessTime, entry.first});
    }

    std::sort(lruList.begin(), lruList.end());

    for (size_t i = 0; i < std::min(countToEvict, lruList.size()); ++i) {
        invalidateGeometry(lruList[i].second, "LRU eviction");
    }
}

void GeometryCache::evictOldest(size_t countToEvict) {
    std::vector<std::pair<int64_t, std::string>> creationList;

    for (const auto& entry : geometryCache_) {
        creationList.push_back({entry.second->creationTime, entry.first});
    }

    std::sort(creationList.begin(), creationList.end());

    for (size_t i = 0; i < std::min(countToEvict, creationList.size()); ++i) {
        invalidateGeometry(creationList[i].second, "Age eviction");
    }
}

void GeometryCache::clearCache() {
    geometryCache_.clear();
    topologyCache_.clear();
    currentCacheSize_ = 0;
}

bool GeometryCache::cacheTopologyElement(const TopologyElement& element) {
    auto elementPtr = std::make_unique<TopologyElement>(element);
    currentCacheSize_ += element.data.size();
    topologyCache_[element.id] = std::move(elementPtr);
    return true;
}

std::unique_ptr<GeometryCache::TopologyElement> GeometryCache::getTopologyElement(
    const std::string& elementId) {

    auto it = topologyCache_.find(elementId);
    if (it != topologyCache_.end()) {
        totalHits_++;
        auto result = std::make_unique<TopologyElement>();
        *result = *it->second;
        return result;
    }

    totalMisses_++;
    return nullptr;
}

void GeometryCache::invalidateTopologyElement(const std::string& elementId) {
    auto it = topologyCache_.find(elementId);
    if (it != topologyCache_.end()) {
        currentCacheSize_ -= it->second->data.size();
        topologyCache_.erase(it);
        totalInvalidations_++;
    }
}

std::vector<CacheInvalidationTracker> GeometryCache::getInvalidationHistory() const {
    return invalidationHistory_;
}

void GeometryCache::clearInvalidationHistory() {
    invalidationHistory_.clear();
}

void GeometryCache::enableMonitoring() {
    monitoringEnabled_ = true;
}

void GeometryCache::disableMonitoring() {
    monitoringEnabled_ = false;
}

bool GeometryCache::isMonitoringEnabled() const {
    return monitoringEnabled_;
}

std::string GeometryCache::generateCacheReport() const {
    std::ostringstream oss;
    auto stats = getStatistics();

    oss << "=== Geometry Cache Report ===\n";
    oss << "Total Entries: " << stats.totalEntriesCount << "\n";
    oss << "Memory Usage: " << stats.totalMemoryBytes / (1024 * 1024) << " MB / "
        << stats.maxMemoryBytes / (1024 * 1024) << " MB\n";
    oss << "Utilization: " << stats.utilizationPercent << "%\n";
    oss << "Hit Rate: " << stats.hitRate * 100 << "%\n";
    oss << "Total Hits: " << stats.totalHits << "\n";
    oss << "Total Misses: " << stats.totalMisses << "\n";
    oss << "Total Invalidations: " << stats.totalInvalidations << "\n";

    return oss.str();
}

void GeometryCache::updateCacheSize() {
    currentCacheSize_ = 0;
    for (const auto& entry : geometryCache_) {
        currentCacheSize_ += entry.second->memorySizeBytes;
    }
    for (const auto& entry : topologyCache_) {
        currentCacheSize_ += entry.second->data.size();
    }
}

void GeometryCache::evictUntilBelow(size_t targetSize) {
    while (currentCacheSize_ > targetSize && !geometryCache_.empty()) {
        evictLRU(1);
    }
}

std::string GeometryCache::getLargestEntry() const {
    std::string largest;
    size_t maxSize = 0;

    for (const auto& entry : geometryCache_) {
        if (entry.second->memorySizeBytes > maxSize) {
            maxSize = entry.second->memorySizeBytes;
            largest = entry.first;
        }
    }

    return largest;
}

}  // namespace bendcore::optimization
