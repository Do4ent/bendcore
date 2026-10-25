// Geometry Caching System Implementation (Q3 2027, Task 1, Subtask 5)
#include <bendcore/geometry_cache.hpp>

#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <fstream>
#include <sys/stat.h>

namespace bendcore::cache {

GeometryCache::GeometryCache() : current_memory_bytes_(0) {
}

GeometryCache::~GeometryCache() {
    clear();
}

std::string GeometryCache::sha256(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()),
           input.length(),
           hash);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return oss.str();
}

GeometryCache::CacheKey GeometryCache::compute_hash(const std::string& source_file) {
    // PERF-001: FIXED — Include file content hash, not just path
    // Previous approach (sha256 of path only) leads to cache reuse with stale data

    std::string key_input = source_file;

    try {
        // Add file modification time to key
        struct stat file_stat;
        if (stat(source_file.c_str(), &file_stat) == 0) {
            key_input += "|";
            key_input += std::to_string(file_stat.st_mtime);
            key_input += "|";
            key_input += std::to_string(file_stat.st_size);
        }

        // Read file content and include its hash
        std::ifstream file(source_file, std::ios::binary);
        if (file) {
            std::string file_contents((std::istreambuf_iterator<char>(file)),
                                     std::istreambuf_iterator<char>());
            key_input += "|content_hash:";
            key_input += sha256(file_contents);
        }
    } catch (...) {
        // If we can't read file metadata/content, at least use the path
    }

    return sha256(key_input);
}

size_t GeometryCache::estimate_entry_size(const CacheEntry& entry) {
    return entry.shape_data.capacity() + sizeof(ExtractedParams) + 100;  // Rough estimate
}

GeometryCache::CacheKey GeometryCache::add(
    const std::string& source_file,
    const std::string& shape_data,
    const ExtractedParams& params) {

    std::lock_guard<std::mutex> lock(cache_mutex_);

    CacheKey key = compute_hash(source_file);

    // Create cache entry
    CacheEntry entry;
    entry.shape_data = shape_data;
    entry.params = params;
    entry.timestamp = std::chrono::system_clock::now();
    entry.memory_bytes = estimate_entry_size(entry);

    // Add to cache
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        // Remove old entry from memory count
        current_memory_bytes_ -= it->second.memory_bytes;
    }

    cache_[key] = entry;
    access_times_[key] = entry.timestamp;
    current_memory_bytes_ += entry.memory_bytes;

    // Evict old entries if needed
    evict_lru_if_needed();

    return key;
}

std::optional<CacheEntry> GeometryCache::get(const CacheKey& key) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        // Update access time for LRU
        access_times_[key] = std::chrono::system_clock::now();
        return it->second;
    }

    return std::nullopt;
}

void GeometryCache::invalidate(const CacheKey& key) {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        current_memory_bytes_ -= it->second.memory_bytes;
        cache_.erase(it);
        access_times_.erase(key);
    }
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

void GeometryCache::evict_lru_if_needed() {
    // Not thread-safe - must be called within lock

    while (current_memory_bytes_ > MAX_MEMORY_BYTES && !cache_.empty()) {
        // Find oldest entry
        auto oldest_it = access_times_.begin();
        CacheKey oldest_key = oldest_it->first;

        for (auto it = access_times_.begin(); it != access_times_.end(); ++it) {
            if (it->second < oldest_it->second) {
                oldest_it = it;
                oldest_key = it->first;
            }
        }

        // Evict oldest entry
        auto cache_it = cache_.find(oldest_key);
        if (cache_it != cache_.end()) {
            current_memory_bytes_ -= cache_it->second.memory_bytes;
            cache_.erase(cache_it);
        }

        access_times_.erase(oldest_key);
    }
}

}  // namespace bendcore::cache
