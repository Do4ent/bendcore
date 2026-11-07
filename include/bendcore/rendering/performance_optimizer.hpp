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
    void addToCache(int mesh_id, int vertices, long size) {
        cache_[mesh_id] = {mesh_id, vertices, size, true};
        total_cache_size_ += size;
    }

    void removeFromCache(int mesh_id) {
        auto it = cache_.find(mesh_id);
        if (it != cache_.end()) {
            total_cache_size_ -= it->second.cache_size;
            cache_.erase(it);
        }
    }

    bool isInCache(int mesh_id) const {
        return cache_.find(mesh_id) != cache_.end();
    }

    size_t getCacheSize() const { return cache_.size(); }

    long getTotalCacheMemory() const { return total_cache_size_; }

    void clearCache() {
        cache_.clear();
        total_cache_size_ = 0;
    }

    double getCacheHitRate() const {
        if (cache_accesses_ == 0) return 0;
        return static_cast<double>(cache_hits_) / cache_accesses_;
    }

    void recordCacheAccess(bool hit) {
        cache_accesses_++;
        if (hit) cache_hits_++;
    }

    int getParallelThreadCount() const { return thread_count_; }
    void setParallelThreadCount(int count) { thread_count_ = count; }

    double estimateSpeedup() const {
        return std::min(static_cast<double>(thread_count_), 8.0) * 0.85;
    }

private:
    std::map<int, CacheEntry> cache_;
    long total_cache_size_ = 0;
    long cache_accesses_ = 0;
    long cache_hits_ = 0;
    int thread_count_ = 4;
};

} // namespace bendcore::rendering
