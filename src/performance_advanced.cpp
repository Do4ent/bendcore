// Advanced Performance Features Implementation
// Phase 26 Stage D: Performance Optimization & Enhancement
#include "bendcore/performance_advanced.hpp"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <limits>

namespace bendcore::performance {

// ========== PersistentCache Implementation ==========

PersistentCache::PersistentCache(const std::string& dbPath)
    : dbPath(dbPath.empty() ? "bendcore_cache.db" : dbPath),
      retentionDays(30) {
    initializeDatabase();
}

PersistentCache::~PersistentCache() = default;

bool PersistentCache::initializeDatabase() {
    // In a real implementation, this would initialize SQLite or similar
    // For now, we use in-memory map
    return true;
}

bool PersistentCache::store(const std::string& key, const std::string& value) {
    inMemoryCache[key] = value;
    return true;
}

bool PersistentCache::retrieve(const std::string& key, std::string& outValue) {
    auto it = inMemoryCache.find(key);
    if (it != inMemoryCache.end()) {
        outValue = it->second;
        return true;
    }
    return false;
}

bool PersistentCache::exists(const std::string& key) const {
    return inMemoryCache.find(key) != inMemoryCache.end();
}

bool PersistentCache::deleteEntry(const std::string& key) {
    auto it = inMemoryCache.find(key);
    if (it != inMemoryCache.end()) {
        inMemoryCache.erase(it);
        return true;
    }
    return false;
}

void PersistentCache::clear() {
    inMemoryCache.clear();
}

size_t PersistentCache::getSize() const {
    size_t size = 0;
    for (const auto& [key, value] : inMemoryCache) {
        size += key.size() + value.size();
    }
    return size;
}

size_t PersistentCache::getEntryCount() const {
    return inMemoryCache.size();
}

bool PersistentCache::vacuum() {
    // In a real implementation, optimize database
    return true;
}

void PersistentCache::setRetentionPolicy(int daysToKeep) {
    retentionDays = daysToKeep;
}

PersistentCache::CacheStats PersistentCache::getStats() const {
    return {
        getSize(),
        getEntryCount(),
        0.75,  // Mock hit rate
        std::chrono::system_clock::now().time_since_epoch().count()
    };
}

// ========== DistributedCacheClient Implementation ==========

DistributedCacheClient::DistributedCacheClient()
    : host(""), port(0), connected(false) {}

DistributedCacheClient::~DistributedCacheClient() {
    disconnect();
}

bool DistributedCacheClient::connect(const std::string& host, int port, const std::string& password) {
    this->host = host;
    this->port = port;

    // In a real implementation, connect to Redis or similar
    // For now, just set connected to true
    connected = true;
    return true;
}

void DistributedCacheClient::disconnect() {
    connected = false;
}

bool DistributedCacheClient::isConnected() const {
    return connected;
}

bool DistributedCacheClient::set(const std::string& key, const std::string& value, int ttlSeconds) {
    if (!connected) return false;
    // In real implementation, set in distributed cache
    return true;
}

bool DistributedCacheClient::get(const std::string& key, std::string& outValue) {
    if (!connected) return false;
    // In real implementation, get from distributed cache
    return false;
}

bool DistributedCacheClient::del(const std::string& key) {
    if (!connected) return false;
    return true;
}

bool DistributedCacheClient::exists(const std::string& key) const {
    if (!connected) return false;
    return false;
}

bool DistributedCacheClient::setTTL(const std::string& key, int ttlSeconds) {
    if (!connected) return false;
    return true;
}

int DistributedCacheClient::getTTL(const std::string& key) const {
    if (!connected) return -2;
    return -1;  // No expiry
}

bool DistributedCacheClient::increment(const std::string& key, int delta) {
    if (!connected) return false;
    return true;
}

bool DistributedCacheClient::append(const std::string& key, const std::string& value) {
    if (!connected) return false;
    return true;
}

std::vector<std::string> DistributedCacheClient::getKeys(const std::string& pattern) const {
    if (!connected) return {};
    return {};
}

void DistributedCacheClient::flushAll() {
    if (!connected) return;
}

DistributedCacheClient::DistributedStats DistributedCacheClient::getStats() const {
    return {
        connected,
        connected ? "Redis 6.0" : "Disconnected",
        0,
        0,
        0
    };
}

// ========== TieredCacheManager Implementation ==========

TieredCacheManager::TieredCacheManager() = default;

TieredCacheManager::~TieredCacheManager() = default;

bool TieredCacheManager::initializeL1(size_t maxSize) {
    l1Cache = std::make_unique<LRUCache<std::string, std::string>>(maxSize);
    return true;
}

bool TieredCacheManager::initializeL2(const std::string& dbPath) {
    l2Cache = std::make_unique<PersistentCache>(dbPath);
    return true;
}

bool TieredCacheManager::initializeL3(const std::string& host, int port) {
    l3Cache = std::make_unique<DistributedCacheClient>();
    return l3Cache->connect(host, port);
}

bool TieredCacheManager::get(const std::string& key, std::string& outValue, CacheLevel* hitLevel) {
    // Try L1 first
    if (l1Cache && l1Cache->get(key, outValue)) {
        l1Hits++;
        if (hitLevel) *hitLevel = L1_Memory;
        return true;
    }
    l1Misses++;

    // Try L2
    if (l2Cache && l2Cache->retrieve(key, outValue)) {
        l2Hits++;
        if (hitLevel) *hitLevel = L2_Persistent;
        // Promote to L1
        if (l1Cache) l1Cache->put(key, outValue);
        return true;
    }
    l2Misses++;

    // Try L3
    if (l3Cache && l3Cache->get(key, outValue)) {
        l3Hits++;
        if (hitLevel) *hitLevel = L3_Distributed;
        // Promote to L1 and L2
        if (l2Cache) l2Cache->store(key, outValue);
        if (l1Cache) l1Cache->put(key, outValue);
        return true;
    }
    l3Misses++;

    if (hitLevel) *hitLevel = L1_Memory;  // Default
    return false;
}

void TieredCacheManager::put(
    const std::string& key,
    const std::string& value,
    bool persistToL2,
    bool persistToL3
) {
    if (l1Cache) l1Cache->put(key, value);
    if (persistToL2 && l2Cache) l2Cache->store(key, value);
    if (persistToL3 && l3Cache) l3Cache->set(key, value);
}

bool TieredCacheManager::promote(const std::string& key) {
    std::string value;
    return get(key, value);
}

TieredCacheManager::TieredStats TieredCacheManager::getStats() const {
    double l1Rate = (l1Hits + l1Misses > 0) ? static_cast<double>(l1Hits) / (l1Hits + l1Misses) : 0;
    double l2Rate = (l2Hits + l2Misses > 0) ? static_cast<double>(l2Hits) / (l2Hits + l2Misses) : 0;
    double l3Rate = (l3Hits + l3Misses > 0) ? static_cast<double>(l3Hits) / (l3Hits + l3Misses) : 0;
    int64_t totalHits = l1Hits + l2Hits + l3Hits;
    int64_t totalMisses = l1Misses + l2Misses + l3Misses;
    double overallRate = (totalHits + totalMisses > 0) ? static_cast<double>(totalHits) / (totalHits + totalMisses) : 0;

    return {
        l1Rate,
        l2Rate,
        l3Rate,
        overallRate,
        static_cast<size_t>(totalHits),
        static_cast<size_t>(totalMisses)
    };
}

void TieredCacheManager::syncL1ToL2() {
    // In real implementation, flush L1 to L2
}

void TieredCacheManager::clearAll() {
    if (l1Cache) l1Cache->clear();
    if (l2Cache) l2Cache->clear();
    if (l3Cache) l3Cache->flushAll();
}

// ========== PerformanceProfiler Implementation ==========

PerformanceProfiler::PerformanceProfiler() = default;

PerformanceProfiler::~PerformanceProfiler() = default;

void PerformanceProfiler::startOperation(const std::string& name) {
    startTimes[name] = std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

double PerformanceProfiler::endOperation(const std::string& name) {
    auto endTime = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto it = startTimes.find(name);

    if (it != startTimes.end()) {
        double elapsed = (endTime - it->second) / 1e6;  // Convert to milliseconds
        startTimes.erase(it);

        auto& opData = operations[name];
        opData.callCount++;
        opData.totalTime += elapsed;
        opData.minTime = std::min(opData.minTime, elapsed);
        opData.maxTime = std::max(opData.maxTime, elapsed);

        return elapsed;
    }

    return 0.0;
}

PerformanceProfiler::OperationStats PerformanceProfiler::getOperationStats(const std::string& name) const {
    auto it = operations.find(name);
    if (it == operations.end()) {
        return {name, 0, 0.0, 0.0, 0.0, 0.0};
    }

    const auto& data = it->second;
    return {
        name,
        data.callCount,
        data.totalTime,
        data.callCount > 0 ? data.totalTime / data.callCount : 0.0,
        data.minTime == std::numeric_limits<double>::max() ? 0.0 : data.minTime,
        data.maxTime
    };
}

std::vector<PerformanceProfiler::OperationStats> PerformanceProfiler::getAllStats() const {
    std::vector<OperationStats> stats;
    for (const auto& [name, data] : operations) {
        stats.push_back(getOperationStats(name));
    }
    return stats;
}

void PerformanceProfiler::reset() {
    operations.clear();
    startTimes.clear();
}

std::vector<PerformanceProfiler::OperationStats> PerformanceProfiler::getSlowest(size_t count) const {
    auto stats = getAllStats();
    std::sort(stats.begin(), stats.end(),
        [](const OperationStats& a, const OperationStats& b) {
            return a.averageTime > b.averageTime;
        });

    if (stats.size() > count) {
        stats.resize(count);
    }
    return stats;
}

std::vector<PerformanceProfiler::OperationStats> PerformanceProfiler::getMostFrequent(size_t count) const {
    auto stats = getAllStats();
    std::sort(stats.begin(), stats.end(),
        [](const OperationStats& a, const OperationStats& b) {
            return a.callCount > b.callCount;
        });

    if (stats.size() > count) {
        stats.resize(count);
    }
    return stats;
}

std::string PerformanceProfiler::exportReport() const {
    std::ostringstream oss;
    oss << "=== Performance Profile Report ===\n\n";

    auto stats = getAllStats();
    if (stats.empty()) {
        oss << "No operations profiled.\n";
        return oss.str();
    }

    oss << "Operation Statistics:\n";
    oss << std::setw(20) << "Operation" << " | "
        << std::setw(8) << "Calls" << " | "
        << std::setw(12) << "Total (ms)" << " | "
        << std::setw(12) << "Avg (ms)" << " | "
        << std::setw(12) << "Min (ms)" << " | "
        << std::setw(12) << "Max (ms)" << "\n";
    oss << std::string(90, '-') << "\n";

    for (const auto& stat : stats) {
        oss << std::setw(20) << stat.name << " | "
            << std::setw(8) << stat.callCount << " | "
            << std::fixed << std::setprecision(2)
            << std::setw(12) << stat.totalTime << " | "
            << std::setw(12) << stat.averageTime << " | "
            << std::setw(12) << stat.minTime << " | "
            << std::setw(12) << stat.maxTime << "\n";
    }

    return oss.str();
}

// ========== AutoTuner Implementation ==========

AutoTuner::AutoTuner()
    : currentConfig{1000, 4, 2500, false, false} {}

AutoTuner::~AutoTuner() = default;

std::vector<AutoTuner::TuningRecommendation> AutoTuner::analyzePerformance(
    const PerformanceMetrics& metrics,
    const std::vector<PerformanceProfiler::OperationStats>& stats
) const {
    std::vector<TuningRecommendation> recommendations;

    analyzeCacheUtilization(metrics, recommendations);
    analyzeThreading(stats, recommendations);
    analyzeMemory(metrics, recommendations);

    return recommendations;
}

bool AutoTuner::applyTuning(const TuningRecommendation& recommendation) {
    // In real implementation, apply tuning changes
    return true;
}

AutoTuner::TuningConfig AutoTuner::getCurrentConfig() const {
    return currentConfig;
}

bool AutoTuner::analyzeCacheUtilization(
    const PerformanceMetrics& metrics,
    std::vector<TuningRecommendation>& recommendations
) const {
    if (metrics.analysisCacheHitRate < 0.6) {
        recommendations.push_back({
            "cache",
            "Low cache hit rate",
            "Increase cache size from " + std::to_string(currentConfig.cacheSize),
            0.15
        });
    }

    return !recommendations.empty();
}

bool AutoTuner::analyzeThreading(
    const std::vector<PerformanceProfiler::OperationStats>& stats,
    std::vector<TuningRecommendation>& recommendations
) const {
    // Analyze if threading can be improved
    return false;
}

bool AutoTuner::analyzeMemory(
    const PerformanceMetrics& metrics,
    std::vector<TuningRecommendation>& recommendations
) const {
    if (metrics.peakMemoryUsage > 500.0) {
        recommendations.push_back({
            "memory",
            "High memory usage",
            "Enable distributed caching to offload to remote cache",
            0.25
        });
    }

    return !recommendations.empty();
}

}  // namespace bendcore::performance
