// CAD-CORE-031: MemoryOptimizer - Memory Optimization
// Phase H: Advanced Optimization for 10K+ Elements
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <map>

namespace bendcore::optimization {

/**
 * MemoryOptimizer
 *
 * Provides comprehensive memory optimization for large CAD assemblies.
 *
 * Features:
 * - Memory profiling and analysis
 * - Fragmentation reduction
 * - Pool-based allocation for geometry
 * - Object pooling for frequently created types
 * - Memory statistics and reporting
 * - Leak detection
 * - Streaming and paging for large datasets
 */

struct MemoryProfile {
    size_t totalAllocated;
    size_t totalUsed;
    size_t peakUsage;
    size_t fragmentationRatio;  // percentage 0-100
    int32_t allocationCount;
    int32_t freeCount;
    std::vector<std::pair<std::string, size_t>> allocationsByType;
};

struct MemoryAllocation {
    void* pointer;
    size_t size;
    std::string type;
    int64_t allocationTime;
    bool isActive;
};

class MemoryPool {
public:
    explicit MemoryPool(
        size_t blockSize,
        size_t initialBlockCount,
        const std::string& poolName
    );
    ~MemoryPool();

    void* allocate();
    void deallocate(void* ptr);

    struct PoolStats {
        size_t blockSize;
        size_t totalBlocks;
        size_t allocatedBlocks;
        size_t freeBlocks;
        size_t peakUsage;
        double utilizationPercentage;
    };

    PoolStats getStats() const;
    void grow(size_t additionalBlocks);
    void compact();

private:
    size_t blockSize_;
    std::vector<void*> blocks_;
    std::vector<bool> blockInUse_;
    size_t peakUsage_ = 0;
    std::string poolName_;
};

class ObjectPool {
public:
    template<typename T>
    class TypedPool {
    public:
        explicit TypedPool(size_t initialSize = 100)
            : initialSize_(initialSize) {
            for (size_t i = 0; i < initialSize_; ++i) {
                available_.push_back(std::make_unique<T>());
            }
        }

        std::unique_ptr<T> acquire() {
            if (available_.empty()) {
                return std::make_unique<T>();
            }
            auto obj = std::move(available_.back());
            available_.pop_back();
            return obj;
        }

        void release(std::unique_ptr<T> obj) {
            if (available_.size() < initialSize_ * 2) {
                available_.push_back(std::move(obj));
            }
        }

        size_t getAvailableCount() const {
            return available_.size();
        }

    private:
        size_t initialSize_;
        std::vector<std::unique_ptr<T>> available_;
    };
};

class MemoryOptimizer {
public:
    MemoryOptimizer();
    ~MemoryOptimizer();

    // Memory profiling
    MemoryProfile profileMemory() const;
    void startProfiling();
    void stopProfiling();

    // Get current usage
    size_t getCurrentMemoryUsage() const;
    size_t getPeakMemoryUsage() const;

    // Memory pools
    void createPool(
        const std::string& poolName,
        size_t blockSize,
        size_t initialBlockCount
    );

    void* allocateFromPool(
        const std::string& poolName,
        size_t size
    );

    void deallocateFromPool(
        const std::string& poolName,
        void* ptr
    );

    // Geometry-specific optimization
    void optimizeGeometryMemory();
    void compactGeometryStorage();

    // Cache optimization
    void optimizeCaches();
    void clearUnusedCaches();

    // Memory cleanup
    void compactMemory();
    void defragment();
    void fullGarbageCollection();

    // Memory limits
    void setMemoryLimit(size_t limitMb);
    size_t getMemoryLimit() const;
    bool isNearMemoryLimit() const;

    // Streaming configuration
    struct StreamingPolicy {
        size_t maxInMemoryElements;
        size_t chunkSize;
        bool enableCompression;
        std::string cachePath;
    };

    void configureStreaming(const StreamingPolicy& policy);

    // Analysis and reporting
    std::string generateMemoryReport() const;
    std::vector<std::string> detectLeaks() const;
    std::vector<MemoryAllocation> getTopMemoryConsumers(int topCount = 10) const;

    // Real-time monitoring
    struct MemoryMetrics {
        size_t currentUsageMb;
        size_t peakUsageMb;
        double fragmentationPercent;
        int64_t allocationsSinceStart;
        int64_t deallocssinceStart;
        std::vector<std::pair<std::string, double>> usageByCategory;
    };

    MemoryMetrics getMetrics() const;
    void printMetrics() const;

private:
    std::map<std::string, std::unique_ptr<MemoryPool>> pools_;
    size_t currentUsage_ = 0;
    size_t peakUsage_ = 0;
    size_t memoryLimit_ = 4096;  // 4GB default

    int64_t totalAllocations_ = 0;
    int64_t totalDeallocations_ = 0;

    std::vector<MemoryAllocation> allocationHistory_;
    bool profilingEnabled_ = false;

    StreamingPolicy streamingPolicy_;

    // Helper methods
    void updateMemoryStats();
};

// Global memory optimizer instance
MemoryOptimizer& getGlobalMemoryOptimizer();

}  // namespace bendcore::optimization
