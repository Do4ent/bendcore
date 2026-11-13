#pragma once

#include <map>
#include <vector>
#include <cmath>
#include <algorithm>

namespace bendcore::rendering {

struct CacheEntry {
    int mesh_id;
    int vertex_count;
    long cache_size;
    bool is_gpu_resident;
};

class PerformanceOptimization {
public:
    void addToCache(int mesh_id, int vertices, long size);
    void removeFromCache(int mesh_id);
    bool isInCache(int mesh_id) const;
    size_t getCacheSize() const;
    long getTotalCacheMemory() const;
    void clearCache();
    double getCacheHitRate() const;
    void recordCacheAccess(bool hit);
    int getParallelThreadCount() const;
    void setParallelThreadCount(int count);
    double estimateSpeedup() const;

private:
    std::map<int, CacheEntry> cache_;
    long total_cache_size_ = 0;
    long cache_accesses_ = 0;
    long cache_hits_ = 0;
    int thread_count_ = 4;
};

} // namespace bendcore::rendering
