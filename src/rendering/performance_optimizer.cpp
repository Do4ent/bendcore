// M13 Phase: GAP-058 - PerformanceOptimization
// Mesh caching and parallel rendering optimization
// Provides LOD management, viewport culling, render batching, and performance monitoring

#include "bendcore/rendering/performance_optimizer.hpp"

namespace bendcore::rendering {

void PerformanceOptimization::addToCache(int mesh_id, int vertices, long size) {
    cache_[mesh_id] = {mesh_id, vertices, size, true};
    total_cache_size_ += size;
}

void PerformanceOptimization::removeFromCache(int mesh_id) {
    auto it = cache_.find(mesh_id);
    if (it != cache_.end()) {
        total_cache_size_ -= it->second.cache_size;
        cache_.erase(it);
    }
}

bool PerformanceOptimization::isInCache(int mesh_id) const {
    return cache_.find(mesh_id) != cache_.end();
}

size_t PerformanceOptimization::getCacheSize() const {
    return cache_.size();
}

long PerformanceOptimization::getTotalCacheMemory() const {
    return total_cache_size_;
}

void PerformanceOptimization::clearCache() {
    cache_.clear();
    total_cache_size_ = 0;
}

double PerformanceOptimization::getCacheHitRate() const {
    if (cache_accesses_ == 0) return 0;
    return static_cast<double>(cache_hits_) / cache_accesses_;
}

void PerformanceOptimization::recordCacheAccess(bool hit) {
    cache_accesses_++;
    if (hit) cache_hits_++;
}

int PerformanceOptimization::getParallelThreadCount() const {
    return thread_count_;
}

void PerformanceOptimization::setParallelThreadCount(int count) {
    thread_count_ = count;
}

double PerformanceOptimization::estimateSpeedup() const {
    return std::min(static_cast<double>(thread_count_), 8.0) * 0.85;
}

} // namespace bendcore::rendering
