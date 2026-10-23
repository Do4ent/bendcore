// Geometry Caching System (Q3 2027, Task 1, Subtask 5)
// LRU cache for imported geometry with SHA256-based key hashing
#pragma once

#include <bendcore/geometry_analyzer.hpp>

#include <string>
#include <map>
#include <optional>
#include <memory>
#include <chrono>
#include <mutex>

namespace bendcore::cache {

using ExtractedParams = bendcore::geometry::ExtractedParams;

// Cache entry structure
struct CacheEntry {
    std::string shape_data;              // Serialized shape data
    ExtractedParams params;              // Extracted parameters
    std::chrono::system_clock::time_point timestamp;  // Cache entry time
    size_t memory_bytes = 0;             // Memory used by this entry
};

// Geometry cache with LRU eviction
class GeometryCache {
public:
    using CacheKey = std::string;  // SHA256 hash

    GeometryCache();
    ~GeometryCache();

    // Add entry to cache
    // Returns cache key (SHA256 hash)
    CacheKey add(
        const std::string& source_file,
        const std::string& shape_data,
        const ExtractedParams& params
    );

    // Get entry from cache
    // Returns cached entry or empty optional if not found
    std::optional<CacheEntry> get(const CacheKey& key);

    // Remove specific entry from cache
    void invalidate(const CacheKey& key);

    // Clear all cache entries
    void clear();

    // Get cache statistics
    size_t size() const;
    size_t memory_used() const;
    size_t max_memory() const;

    // Compute hash for cache key
    static CacheKey compute_hash(const std::string& source_file);

private:
    std::map<CacheKey, CacheEntry> cache_;
    std::map<CacheKey, std::chrono::system_clock::time_point> access_times_;  // LRU tracking
    mutable std::mutex cache_mutex_;

    // 100MB limit
    static constexpr size_t MAX_MEMORY_BYTES = 100 * 1024 * 1024;
    size_t current_memory_bytes_ = 0;

    // Evict oldest entry if cache exceeds limit
    void evict_lru_if_needed();

    // Estimate size of cached data
    static size_t estimate_entry_size(const CacheEntry& entry);

    // SHA256 hash function
    static std::string sha256(const std::string& input);
};

}  // namespace bendcore::cache
