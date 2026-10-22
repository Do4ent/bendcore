// Advanced Performance Features: Distributed Caching & Persistence
// Phase 26 Stage D: Performance Optimization & Enhancement
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace bendcore::performance {

// ========== Persistent Cache ==========

class PersistentCache {
public:
    explicit PersistentCache(const std::string& dbPath = "");
    ~PersistentCache();

    // Store value in persistent cache
    bool store(const std::string& key, const std::string& value);

    // Retrieve from persistent cache
    bool retrieve(const std::string& key, std::string& outValue);

    // Check if key exists
    bool exists(const std::string& key) const;

    // Delete entry
    bool deleteEntry(const std::string& key);

    // Clear all entries
    void clear();

    // Get cache size in bytes
    size_t getSize() const;

    // Get number of entries
    size_t getEntryCount() const;

    // Vacuum database (optimize storage)
    bool vacuum();

    // Set cache retention policy (days)
    void setRetentionPolicy(int daysToKeep);

    // Get cache statistics
    struct CacheStats {
        size_t totalSize;      // bytes
        size_t entryCount;
        double hitRate;
        int64_t createdAt;
    };
    CacheStats getStats() const;

private:
    std::string dbPath;
    mutable std::map<std::string, std::string> inMemoryCache;
    int retentionDays;

    bool initializeDatabase();
};

// ========== Distributed Cache Client ==========

class DistributedCacheClient {
public:
    explicit DistributedCacheClient();
    ~DistributedCacheClient();

    // Connect to distributed cache server (Redis-style)
    bool connect(const std::string& host, int port, const std::string& password = "");

    // Disconnect
    void disconnect();

    // Check connection
    bool isConnected() const;

    // Set key-value with TTL (time-to-live in seconds)
    bool set(const std::string& key, const std::string& value, int ttlSeconds = 3600);

    // Get value
    bool get(const std::string& key, std::string& outValue);

    // Delete key
    bool del(const std::string& key);

    // Check key exists
    bool exists(const std::string& key) const;

    // Set TTL for key
    bool setTTL(const std::string& key, int ttlSeconds);

    // Get remaining TTL (returns -1 if no TTL, -2 if not exists)
    int getTTL(const std::string& key) const;

    // Increment integer value
    bool increment(const std::string& key, int delta = 1);

    // Append to string value
    bool append(const std::string& key, const std::string& value);

    // Get all keys matching pattern
    std::vector<std::string> getKeys(const std::string& pattern = "*") const;

    // Clear all keys
    void flushAll();

    // Get cache statistics
    struct DistributedStats {
        bool connected;
        std::string serverInfo;
        size_t usedMemory;
        int connectedClients;
        int64_t uptime;
    };
    DistributedStats getStats() const;

private:
    std::string host;
    int port;
    bool connected;
    // Would be actual Redis client library in real implementation
};

// ========== Tiered Cache Manager ==========

class TieredCacheManager {
public:
    enum CacheLevel {
        L1_Memory,      // In-memory LRU
        L2_Persistent,  // Persistent disk cache
        L3_Distributed  // Distributed cache (Redis)
    };

    explicit TieredCacheManager();
    ~TieredCacheManager();

    // Initialize cache levels
    bool initializeL1(size_t maxSize = 1000);
    bool initializeL2(const std::string& dbPath);
    bool initializeL3(const std::string& host, int port);

    // Get value from tiered cache (checks L1 → L2 → L3)
    bool get(const std::string& key, std::string& outValue, CacheLevel* hitLevel = nullptr);

    // Put value in tiered cache (writes to L1, optionally L2/L3)
    void put(
        const std::string& key,
        const std::string& value,
        bool persistToL2 = false,
        bool persistToL3 = false
    );

    // Promote value from lower tier to L1
    bool promote(const std::string& key);

    // Get hit rates per level
    struct TieredStats {
        double l1HitRate;
        double l2HitRate;
        double l3HitRate;
        double overallHitRate;
        size_t totalHits;
        size_t totalMisses;
    };
    TieredStats getStats() const;

    // Sync L1 to L2
    void syncL1ToL2();

    // Clear all levels
    void clearAll();

private:
    std::unique_ptr<LRUCache<std::string, std::string>> l1Cache;
    std::unique_ptr<PersistentCache> l2Cache;
    std::unique_ptr<DistributedCacheClient> l3Cache;

    int64_t l1Hits = 0, l1Misses = 0;
    int64_t l2Hits = 0, l2Misses = 0;
    int64_t l3Hits = 0, l3Misses = 0;
};

// ========== Performance Profiler ==========

class PerformanceProfiler {
public:
    explicit PerformanceProfiler();
    ~PerformanceProfiler();

    // Start profiling a named operation
    void startOperation(const std::string& name);

    // End profiling and record results
    double endOperation(const std::string& name);

    // Get operation statistics
    struct OperationStats {
        std::string name;
        int callCount;
        double totalTime;
        double averageTime;
        double minTime;
        double maxTime;
    };
    OperationStats getOperationStats(const std::string& name) const;

    // Get all operation statistics
    std::vector<OperationStats> getAllStats() const;

    // Reset profiling data
    void reset();

    // Get slowest operations (top N)
    std::vector<OperationStats> getSlowest(size_t count = 10) const;

    // Get most frequent operations
    std::vector<OperationStats> getMostFrequent(size_t count = 10) const;

    // Export profiling report
    std::string exportReport() const;

private:
    struct OperationData {
        int callCount = 0;
        double totalTime = 0.0;
        double minTime = std::numeric_limits<double>::max();
        double maxTime = 0.0;
    };

    std::map<std::string, OperationData> operations;
    std::map<std::string, int64_t> startTimes;
};

// ========== Auto-Tuning System ==========

class AutoTuner {
public:
    explicit AutoTuner();
    ~AutoTuner();

    // Analyze performance and suggest optimizations
    struct TuningRecommendation {
        std::string category;  // "cache", "threading", "memory", etc.
        std::string issue;
        std::string recommendation;
        double expectedImprovement;  // 0.0-1.0
    };

    std::vector<TuningRecommendation> analyzePerformance(
        const PerformanceMetrics& metrics,
        const std::vector<PerformanceProfiler::OperationStats>& stats
    ) const;

    // Apply recommended optimization
    bool applyTuning(const TuningRecommendation& recommendation);

    // Get current tuning configuration
    struct TuningConfig {
        size_t cacheSize;
        int threadCount;
        int lodThreshold;
        bool distributedCacheEnabled;
        bool persistenceEnabled;
    };
    TuningConfig getCurrentConfig() const;

private:
    TuningConfig currentConfig;

    // Analysis helpers
    bool analyzeCacheUtilization(
        const PerformanceMetrics& metrics,
        std::vector<TuningRecommendation>& recommendations
    ) const;

    bool analyzeThreading(
        const std::vector<PerformanceProfiler::OperationStats>& stats,
        std::vector<TuningRecommendation>& recommendations
    ) const;

    bool analyzeMemory(
        const PerformanceMetrics& metrics,
        std::vector<TuningRecommendation>& recommendations
    ) const;
};

}  // namespace bendcore::performance
