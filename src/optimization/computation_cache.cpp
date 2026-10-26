// CAD-CORE-051: ComputationCache Implementation
#include "bendcore/optimization/computation_cache.hpp"
#include <algorithm>
#include <sstream>
#include <iostream>

namespace bendcore::optimization {

ComputationCache::ComputationCache(size_t maxCacheSize, int64_t defaultTtlMs)
    : maxCacheSize_(maxCacheSize), currentSize_(0), defaultTtl_(defaultTtlMs) {}

ComputationCache::~ComputationCache() {
    clearCache();
}

bool ComputationCache::cacheComputation(
    const std::string& key,
    const std::vector<uint8_t>& value,
    ComputationType type,
    const std::vector<std::string>& dependencies,
    int64_t ttlMs) {

    removeExpiredEntries();

    if (value.size() > maxCacheSize_) {
        return false;
    }

    if (currentSize_ + value.size() > maxCacheSize_) {
        evictUntilBelow(maxCacheSize_ - value.size());
    }

    auto entry = std::make_unique<CacheEntry>();
    entry->key = key;
    entry->value = value;
    entry->computationType = (type == ComputationType::FeatureComputation ? "Feature" :
                              type == ComputationType::BooleanOperation ? "Boolean" :
                              type == ComputationType::FilletOperation ? "Fillet" :
                              type == ComputationType::OffsetOperation ? "Offset" :
                              type == ComputationType::SerializationData ? "Serialization" :
                              type == ComputationType::DrawingCalculation ? "Drawing" :
                              type == ComputationType::TopologyAnalysis ? "Topology" : "Other");
    entry->creationTime = std::chrono::system_clock::now().time_since_epoch().count();
    entry->lastAccessTime = entry->creationTime;
    entry->ttlMilliseconds = (ttlMs < 0) ? defaultTtl_ : ttlMs;
    entry->accessCount = 0;
    entry->dependencies = dependencies;
    entry->isValid = true;

    // Track dependency relationships
    for (const auto& dep : dependencies) {
        dependencyGraph_[key].push_back(dep);
    }

    currentSize_ += value.size();
    cache_[key] = std::move(entry);

    return true;
}

std::unique_ptr<std::vector<uint8_t>> ComputationCache::getComputation(const std::string& key) {
    auto it = cache_.find(key);
    if (it != cache_.end() && it->second->isValid) {
        if (!isExpired(it->second.get())) {
            totalHits_++;
            it->second->lastAccessTime = std::chrono::system_clock::now().time_since_epoch().count();
            it->second->accessCount++;

            auto result = std::make_unique<std::vector<uint8_t>>(it->second->value);
            return result;
        } else {
            // Expired
            currentSize_ -= it->second->value.size();
            cache_.erase(it);
            totalExpired_++;
        }
    }

    totalMisses_++;
    return nullptr;
}

bool ComputationCache::contains(const std::string& key) const {
    auto it = cache_.find(key);
    return it != cache_.end() && it->second->isValid;
}

void ComputationCache::invalidate(const std::string& key) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        it->second->isValid = false;
        currentSize_ -= it->second->value.size();
        cache_.erase(it);
    }
}

void ComputationCache::invalidateDependents(const std::string& key) {
    // Find all entries that depend on this key
    std::vector<std::string> toInvalidate;

    for (const auto& entry : cache_) {
        for (const auto& dep : entry.second->dependencies) {
            if (dep == key) {
                toInvalidate.push_back(entry.first);
                break;
            }
        }
    }

    for (const auto& id : toInvalidate) {
        invalidate(id);
    }
}

ComputationCache::CacheStats ComputationCache::getStatistics() const {
    CacheStats stats;
    stats.totalEntries = cache_.size();
    stats.totalMemoryBytes = currentSize_;
    stats.maxMemoryBytes = maxCacheSize_;
    stats.hitRate = (totalHits_ + totalMisses_) > 0
        ? static_cast<double>(totalHits_) / (totalHits_ + totalMisses_)
        : 0.0;
    stats.totalHits = totalHits_;
    stats.totalMisses = totalMisses_;
    stats.totalEvictions = totalEvictions_;
    stats.expiredEntries = totalExpired_;

    for (const auto& entry : cache_) {
        stats.entriesByType[entry.second->computationType]++;
    }

    return stats;
}

size_t ComputationCache::getCurrentMemoryUsage() const {
    return currentSize_;
}

void ComputationCache::setMaxMemorySize(size_t maxBytes) {
    maxCacheSize_ = maxBytes;
    if (currentSize_ > maxCacheSize_) {
        evictUntilBelow(maxCacheSize_);
    }
}

void ComputationCache::compactCache() {
    removeExpiredEntries();
}

void ComputationCache::clearCache() {
    cache_.clear();
    dependencyGraph_.clear();
    currentSize_ = 0;
}

void ComputationCache::setDefaultTTL(int64_t ttlMs) {
    defaultTtl_ = ttlMs;
}

void ComputationCache::removeExpiredEntries() {
    std::vector<std::string> expired;

    for (const auto& entry : cache_) {
        if (isExpired(entry.second.get())) {
            expired.push_back(entry.first);
        }
    }

    for (const auto& key : expired) {
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            currentSize_ -= it->second->value.size();
            cache_.erase(it);
            totalExpired_++;
        }
    }
}

std::vector<std::string> ComputationCache::getDependents(const std::string& key) const {
    std::vector<std::string> dependents;

    for (const auto& entry : cache_) {
        for (const auto& dep : entry.second->dependencies) {
            if (dep == key) {
                dependents.push_back(entry.first);
                break;
            }
        }
    }

    return dependents;
}

std::vector<std::string> ComputationCache::getDependencies(const std::string& key) const {
    auto it = dependencyGraph_.find(key);
    if (it != dependencyGraph_.end()) {
        return it->second;
    }
    return {};
}

void ComputationCache::evictLRU(size_t count) {
    std::vector<std::pair<int64_t, std::string>> lruList;

    for (const auto& entry : cache_) {
        lruList.push_back({entry.second->lastAccessTime, entry.first});
    }

    std::sort(lruList.begin(), lruList.end());

    for (size_t i = 0; i < std::min(count, lruList.size()); ++i) {
        invalidate(lruList[i].second);
        totalEvictions_++;
    }
}

std::vector<std::string> ComputationCache::getLRUQueue() const {
    std::vector<std::pair<int64_t, std::string>> lruList;

    for (const auto& entry : cache_) {
        lruList.push_back({entry.second->lastAccessTime, entry.first});
    }

    std::sort(lruList.begin(), lruList.end());

    std::vector<std::string> result;
    for (const auto& item : lruList) {
        result.push_back(item.second);
    }

    return result;
}

void ComputationCache::invalidateBatch(const std::vector<std::string>& keys) {
    for (const auto& key : keys) {
        invalidate(key);
    }
}

void ComputationCache::cacheBatch(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& entries,
    ComputationType type) {

    for (const auto& entry : entries) {
        cacheComputation(entry.first, entry.second, type);
    }
}

std::string ComputationCache::generateCacheReport() const {
    std::ostringstream oss;
    auto stats = getStatistics();

    oss << "=== Computation Cache Report ===\n";
    oss << "Total Entries: " << stats.totalEntries << "\n";
    oss << "Memory Usage: " << stats.totalMemoryBytes / (1024 * 1024) << " MB / "
        << stats.maxMemoryBytes / (1024 * 1024) << " MB\n";
    oss << "Hit Rate: " << stats.hitRate * 100 << "%\n";
    oss << "Total Hits: " << stats.totalHits << "\n";
    oss << "Total Misses: " << stats.totalMisses << "\n";
    oss << "Total Evictions: " << stats.totalEvictions << "\n";
    oss << "Expired Entries: " << stats.expiredEntries << "\n";

    return oss.str();
}

std::string ComputationCache::generateHtmlReport() const {
    std::ostringstream oss;
    auto stats = getStatistics();

    oss << "<h2>Computation Cache Report</h2>\n";
    oss << "<p>Total Entries: " << stats.totalEntries << "</p>\n";
    oss << "<p>Hit Rate: " << (stats.hitRate * 100) << "%</p>\n";

    return oss.str();
}

std::map<ComputationCache::ComputationType, size_t> ComputationCache::getMemoryByType() const {
    std::map<ComputationType, size_t> result;

    for (const auto& entry : cache_) {
        // Map back from string to enum
        ComputationType type = ComputationType::Other;
        if (entry.second->computationType == "Feature") type = ComputationType::FeatureComputation;
        else if (entry.second->computationType == "Boolean") type = ComputationType::BooleanOperation;
        // ... etc

        result[type] += entry.second->value.size();
    }

    return result;
}

bool ComputationCache::preloadCache(const std::string& configPath) {
    // Placeholder for preloading from file
    return true;
}

bool ComputationCache::exportCache(const std::string& outputPath) {
    // Placeholder for exporting to file
    return true;
}

void ComputationCache::evictUntilBelow(size_t targetSize) {
    while (currentSize_ > targetSize && !cache_.empty()) {
        evictLRU(1);
    }
}

std::string ComputationCache::findLRUKey() const {
    if (cache_.empty()) return "";

    std::string lruKey = cache_.begin()->first;
    int64_t minTime = cache_.begin()->second->lastAccessTime;

    for (const auto& entry : cache_) {
        if (entry.second->lastAccessTime < minTime) {
            minTime = entry.second->lastAccessTime;
            lruKey = entry.first;
        }
    }

    return lruKey;
}

bool ComputationCache::isExpired(const CacheEntry* entry) const {
    if (entry->ttlMilliseconds <= 0) return false;

    int64_t currentTime = std::chrono::system_clock::now().time_since_epoch().count() / 1000000;
    int64_t entryTime = entry->lastAccessTime / 1000000;

    return (currentTime - entryTime) > entry->ttlMilliseconds;
}

}  // namespace bendcore::optimization
