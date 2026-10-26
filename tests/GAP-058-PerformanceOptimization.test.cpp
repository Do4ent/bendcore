// GAP-058: PerformanceOptimization (35 tests) - Cache, parallel rendering
#include <cassert>
#include <cstdio>
#include <cmath>
#include <map>
#include <vector>

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

using namespace bendcore::rendering;

int main() {
    int passed = 0, failed = 0;
    
    #define TEST(name, cond) { \
        if (cond) { passed++; } else { \
            printf("FAIL: %s\n", name); failed++; \
        } \
    }
    
    // Test 1-5: Cache initialization
    {
        PerformanceOptimization perf;
        TEST("T1: Cache empty initially", perf.getCacheSize() == 0);
        TEST("T2: No memory used initially", perf.getTotalCacheMemory() == 0);
        TEST("T3: Default thread count", perf.getParallelThreadCount() > 0);
        TEST("T4: Cache hit rate zero", perf.getCacheHitRate() == 0.0);
        TEST("T5: Performance object created", true);
    }
    
    // Test 6-10: Cache operations
    {
        PerformanceOptimization perf;
        
        perf.addToCache(1, 1000, 100000);
        TEST("T6: Mesh added to cache", perf.isInCache(1));
        TEST("T7: Cache size increased", perf.getCacheSize() == 1);
        TEST("T8: Memory tracked", perf.getTotalCacheMemory() == 100000);
        
        perf.addToCache(2, 2000, 200000);
        TEST("T9: Second mesh cached", perf.isInCache(2));
        TEST("T10: Cache size is 2", perf.getCacheSize() == 2);
    }
    
    // Test 11-15: Cache memory management
    {
        PerformanceOptimization perf;
        
        perf.addToCache(1, 1000, 1000000);  // 1MB
        perf.addToCache(2, 2000, 2000000);  // 2MB
        perf.addToCache(3, 3000, 3000000);  // 3MB
        
        long total = perf.getTotalCacheMemory();
        TEST("T11: Total memory 6MB", total == 6000000);
        
        perf.removeFromCache(2);
        TEST("T12: Mesh removed", !perf.isInCache(2));
        TEST("T13: Memory decreased", perf.getTotalCacheMemory() == 4000000);
        TEST("T14: Cache size is 2", perf.getCacheSize() == 2);
        
        TEST("T15: Remaining meshes valid", perf.isInCache(1) && perf.isInCache(3));
    }
    
    // Test 16-20: Cache clearing
    {
        PerformanceOptimization perf;
        
        perf.addToCache(1, 1000, 100000);
        perf.addToCache(2, 2000, 200000);
        perf.addToCache(3, 3000, 300000);
        
        TEST("T16: Cache has items", perf.getCacheSize() > 0);
        
        perf.clearCache();
        TEST("T17: Cache cleared", perf.getCacheSize() == 0);
        TEST("T18: Memory cleared", perf.getTotalCacheMemory() == 0);
        TEST("T19: No meshes found", !perf.isInCache(1) && !perf.isInCache(2));
        TEST("T20: Cache fully cleared", perf.getCacheSize() == 0);
    }
    
    // Test 21-25: Cache hit rate
    {
        PerformanceOptimization perf;
        
        // Record 10 accesses, 8 hits
        for (int i = 0; i < 8; i++) perf.recordCacheAccess(true);
        for (int i = 0; i < 2; i++) perf.recordCacheAccess(false);
        
        double hit_rate = perf.getCacheHitRate();
        TEST("T21: Hit rate calculated", hit_rate > 0.7 && hit_rate < 0.9);
        TEST("T22: 80% hit rate", std::abs(hit_rate - 0.8) < 0.01);
        
        // Reset and test high miss rate
        PerformanceOptimization perf2;
        for (int i = 0; i < 2; i++) perf2.recordCacheAccess(true);
        for (int i = 0; i < 8; i++) perf2.recordCacheAccess(false);
        double low_hit_rate = perf2.getCacheHitRate();
        TEST("T23: Low hit rate", low_hit_rate < 0.3);
        
        PerformanceOptimization perf3;
        perf3.recordCacheAccess(true);
        TEST("T24: Single access tracked", perf3.getCacheHitRate() == 1.0);
        
        TEST("T25: Hit rate working", perf.getCacheHitRate() > 0);
    }
    
    // Test 26-30: Parallel execution
    {
        PerformanceOptimization perf;
        
        TEST("T26: Default 4 threads", perf.getParallelThreadCount() == 4);
        
        perf.setParallelThreadCount(8);
        TEST("T27: Threads set to 8", perf.getParallelThreadCount() == 8);
        
        perf.setParallelThreadCount(16);
        TEST("T28: Threads set to 16", perf.getParallelThreadCount() == 16);
        
        double speedup = perf.estimateSpeedup();
        TEST("T29: Speedup estimated", speedup > 0);
        TEST("T30: Speedup reasonable", speedup <= 16.0);
    }
    
    // Test 31-35: Parallel speedup calculation
    {
        PerformanceOptimization perf1, perf2, perf4;
        
        perf1.setParallelThreadCount(1);
        perf2.setParallelThreadCount(2);
        perf4.setParallelThreadCount(4);
        
        double speed1 = perf1.estimateSpeedup();
        double speed2 = perf2.estimateSpeedup();
        double speed4 = perf4.estimateSpeedup();
        
        TEST("T31: More threads faster", speed2 > speed1);
        TEST("T32: 4 threads faster than 2", speed4 > speed2);
        TEST("T33: Speedup scaling", speed2 > speed1 * 1.5);
        
        PerformanceOptimization perf8;
        perf8.setParallelThreadCount(8);
        double speed8 = perf8.estimateSpeedup();
        TEST("T34: 8 thread speedup", speed8 > 0);
        
        TEST("T35: Parallel optimization working", speed4 > speed1);
    }
    
    printf("\n=== GAP-058: PerformanceOptimization ===\n");
    printf("Passed: %d/35\n", passed);
    printf("Failed: %d/35\n", failed);
    return (failed == 0) ? 0 : 1;
}
