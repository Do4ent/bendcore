// Geometry cache for the import pipeline (Q3 2027, Task 1, Subtask 5).
// Header: include/bendcore/geometry_cache.hpp (bendcore::cache).
// LRU by last access, bounded by MAX_MEMORY_BYTES, thread-safe; the key is
// SHA-256 of the source file path.
#include <bendcore/geometry_cache.hpp>
#include <bendcore/drawing.hpp>

namespace bendcore::cache {

GeometryCache::GeometryCache() = default;
GeometryCache::~GeometryCache() = default;

std::string GeometryCache::sha256(const std::string& input) {
    return bendcore::drawing::sha256_hex(input);
}

GeometryCache::CacheKey GeometryCache::compute_hash(const std::string& source_file) {
    return sha256(source_file);
}

size_t GeometryCache::estimate_entry_size(const CacheEntry& entry) {
    const ExtractedParams& p = entry.params;
    return sizeof(CacheEntry) + entry.shape_data.capacity() +
           (p.bend_angles.capacity() + p.bend_radii.capacity()) * sizeof(double) +
           p.shape_type.capacity() + p.analysis_notes.capacity();
}

GeometryCache::CacheKey GeometryCache::add(const std::string& source_file,
                                           const std::string& shape_data,
                                           const ExtractedParams& params) {
    const CacheKey key = compute_hash(source_file);
    const auto now = std::chrono::system_clock::now();

    CacheEntry entry;
    entry.shape_data = shape_data;
    entry.params = params;
    entry.timestamp = now;
    entry.memory_bytes = estimate_entry_size(entry);

    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        current_memory_bytes_ -= it->second.memory_bytes;
        it->second = std::move(entry);
        current_memory_bytes_ += it->second.memory_bytes;
    } else {
        current_memory_bytes_ += entry.memory_bytes;
        cache_.emplace(key, std::move(entry));
    }
    access_times_[key] = now;
    evict_lru_if_needed();
    return key;
}

std::optional<CacheEntry> GeometryCache::get(const CacheKey& key) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return std::nullopt;
    }
    access_times_[key] = std::chrono::system_clock::now();
    return it->second;
}

void GeometryCache::invalidate(const CacheKey& key) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_.find(key);
    if (it == cache_.end()) return;
    current_memory_bytes_ -= it->second.memory_bytes;
    cache_.erase(it);
    access_times_.erase(key);
}

void GeometryCache::clear() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.clear();
    access_times_.clear();
    current_memory_bytes_ = 0;
}

size_t GeometryCache::size() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return cache_.size();
}

size_t GeometryCache::memory_used() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    return current_memory_bytes_;
}

size_t GeometryCache::max_memory() const {
    return MAX_MEMORY_BYTES;
}

// Caller holds cache_mutex_. Evicts least recently used entries until the
// cache fits; an entry larger than the whole budget is not kept.
void GeometryCache::evict_lru_if_needed() {
    while (current_memory_bytes_ > MAX_MEMORY_BYTES && !cache_.empty()) {
        auto oldest = access_times_.begin();
        for (auto it = access_times_.begin(); it != access_times_.end(); ++it) {
            if (it->second < oldest->second) oldest = it;
        }
        auto entry = cache_.find(oldest->first);
        if (entry != cache_.end()) {
            current_memory_bytes_ -= entry->second.memory_bytes;
            cache_.erase(entry);
        }
        access_times_.erase(oldest);
    }
}

}  // namespace bendcore::cache
