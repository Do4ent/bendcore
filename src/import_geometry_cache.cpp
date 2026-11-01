// Geometry Caching System (Q3 2027, Task 1, Subtask 5)
// LRU-кэш результатов импорта: ключ = SHA-256 от пути (+ размер и время изменения файла,
// если он существует), лимит памяти MAX_MEMORY_BYTES, потокобезопасен.
#include <bendcore/geometry_cache.hpp>
#include <bendcore/sha256.hpp>

#include <filesystem>
#include <system_error>

namespace bendcore::cache {

GeometryCache::GeometryCache() = default;
GeometryCache::~GeometryCache() = default;

std::string GeometryCache::sha256(const std::string& input) {
    return bendcore::sha256_hex(input);
}

GeometryCache::CacheKey GeometryCache::compute_hash(const std::string& source_file) {
    // Изменённый на диске файл получает новый ключ, поэтому устаревшая запись не вернётся.
    std::string material = "path:" + source_file;
    std::error_code ec;
    const auto size = std::filesystem::file_size(source_file, ec);
    if (!ec) {
        material += "|size:" + std::to_string(size);
        const auto mtime = std::filesystem::last_write_time(source_file, ec);
        if (!ec) {
            material += "|mtime:" + std::to_string(mtime.time_since_epoch().count());
        }
    }
    return sha256(material);
}

size_t GeometryCache::estimate_entry_size(const CacheEntry& entry) {
    const auto& p = entry.params;
    return sizeof(CacheEntry) + entry.shape_data.capacity() + p.shape_type.capacity() +
           p.analysis_notes.capacity() +
           (p.bend_angles.capacity() + p.bend_radii.capacity()) * sizeof(double);
}

GeometryCache::CacheKey GeometryCache::add(const std::string& source_file,
                                           const std::string& shape_data,
                                           const ExtractedParams& params) {
    CacheEntry entry;
    entry.shape_data = shape_data;
    entry.params = params;
    entry.timestamp = std::chrono::system_clock::now();
    entry.memory_bytes = estimate_entry_size(entry);

    const CacheKey key = compute_hash(source_file);

    std::lock_guard<std::mutex> lock(cache_mutex_);
    if (entry.memory_bytes > MAX_MEMORY_BYTES) {
        return key;  // запись больше всего кэша не храним
    }
    if (auto it = cache_.find(key); it != cache_.end()) {
        current_memory_bytes_ -= it->second.memory_bytes;
        cache_.erase(it);
    }
    current_memory_bytes_ += entry.memory_bytes;
    access_times_[key] = entry.timestamp;
    cache_.emplace(key, std::move(entry));
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
    if (it == cache_.end()) {
        return;
    }
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

// Вызывается под cache_mutex_.
void GeometryCache::evict_lru_if_needed() {
    while (current_memory_bytes_ > MAX_MEMORY_BYTES && !access_times_.empty()) {
        auto oldest = access_times_.begin();
        for (auto it = access_times_.begin(); it != access_times_.end(); ++it) {
            if (it->second < oldest->second) {
                oldest = it;
            }
        }
        if (auto entry = cache_.find(oldest->first); entry != cache_.end()) {
            current_memory_bytes_ -= entry->second.memory_bytes;
            cache_.erase(entry);
        }
        access_times_.erase(oldest);
    }
}

}  // namespace bendcore::cache
