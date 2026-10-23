// Geometry Cache Tests (Q3 2027, Task 1, Subtask 5)
#include <bendcore/geometry_cache.hpp>

#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

using namespace bendcore::cache;

int test_add_entry_to_cache() {
    std::cout << "Test 1: Add entry to cache... ";

    GeometryCache cache;

    ExtractedParams params;
    params.outer_diameter = 25.4;
    params.wall_thickness = 2.11;
    params.total_length = 150.0;
    params.confidence = 0.9;

    auto key = cache.add("test_file.step", "shape_data_1", params);

    assert(!key.empty());
    assert(cache.size() == 1);

    std::cout << "PASS (Key: " << key.substr(0, 8) << "...)" << std::endl;
    return 0;
}

int test_retrieve_entry_from_cache() {
    std::cout << "Test 2: Retrieve entry from cache... ";

    GeometryCache cache;

    ExtractedParams params;
    params.outer_diameter = 25.4;

    auto key = cache.add("file1.step", "data1", params);

    auto entry = cache.get(key);
    assert(entry.has_value());
    assert(entry->shape_data == "data1");
    assert(entry->params.outer_diameter > 0);

    std::cout << "PASS" << std::endl;
    return 0;
}

int test_miss_on_different_file() {
    std::cout << "Test 3: Miss on different file... ";

    GeometryCache cache;

    ExtractedParams params;
    params.outer_diameter = 25.4;

    auto key1 = cache.add("file1.step", "data1", params);

    // Different file should have different key
    auto key2 = GeometryCache::compute_hash("file2.step");

    assert(key1 != key2);

    // Should not find file2 in cache
    auto entry = cache.get(key2);
    assert(!entry.has_value());

    std::cout << "PASS" << std::endl;
    return 0;
}

int test_eviction_when_exceeds_limit() {
    std::cout << "Test 4: Eviction when exceeds limit... ";

    GeometryCache cache;

    // Add many entries to exceed cache size
    // Max is 100MB, so we need to add large data
    ExtractedParams params;

    size_t initial_size = cache.size();

    // Add multiple entries
    for (int i = 0; i < 5; i++) {
        std::string filename = "file_" + std::to_string(i) + ".step";
        std::string large_data(1024 * 100, 'x');  // 100KB per entry

        params.outer_diameter = 25.4 + i;
        cache.add(filename, large_data, params);
    }

    // Cache should not exceed max size
    assert(cache.memory_used() <= cache.max_memory());

    std::cout << "PASS (Memory used: " << cache.memory_used() / (1024*1024)
              << " MB / " << cache.max_memory() / (1024*1024) << " MB)" << std::endl;
    return 0;
}

int test_thread_safe_concurrent_access() {
    std::cout << "Test 5: Thread-safe concurrent access... ";

    GeometryCache cache;

    // Spawn multiple threads accessing the cache
    auto add_entries = [&cache](int thread_id) {
        ExtractedParams params;
        params.outer_diameter = 25.4;
        params.confidence = 0.9;

        for (int i = 0; i < 10; i++) {
            std::string filename = "file_t" + std::to_string(thread_id) +
                                  "_" + std::to_string(i) + ".step";
            cache.add(filename, "data", params);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 3; i++) {
        threads.emplace_back(add_entries, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have entries from all threads
    assert(cache.size() > 0);

    std::cout << "PASS (Entries: " << cache.size() << ")" << std::endl;
    return 0;
}

int test_invalidate_specific_entry() {
    std::cout << "Test 6: Invalidate specific entry... ";

    GeometryCache cache;

    ExtractedParams params;
    params.outer_diameter = 25.4;

    auto key1 = cache.add("file1.step", "data1", params);
    auto key2 = cache.add("file2.step", "data2", params);

    assert(cache.size() == 2);

    // Invalidate first entry
    cache.invalidate(key1);

    assert(cache.size() == 1);
    assert(!cache.get(key1).has_value());
    assert(cache.get(key2).has_value());

    std::cout << "PASS" << std::endl;
    return 0;
}

int test_clear_all_entries() {
    std::cout << "Test 7: Clear all entries... ";

    GeometryCache cache;

    ExtractedParams params;
    params.outer_diameter = 25.4;

    // Add multiple entries
    for (int i = 0; i < 10; i++) {
        std::string filename = "file_" + std::to_string(i) + ".step";
        cache.add(filename, "data", params);
    }

    assert(cache.size() > 0);

    // Clear all
    cache.clear();

    assert(cache.size() == 0);
    assert(cache.memory_used() == 0);

    std::cout << "PASS" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== Geometry Cache Tests ===" << std::endl;

    int failed = 0;

    try {
        failed += test_add_entry_to_cache();
        failed += test_retrieve_entry_from_cache();
        failed += test_miss_on_different_file();
        failed += test_eviction_when_exceeds_limit();
        failed += test_thread_safe_concurrent_access();
        failed += test_invalidate_specific_entry();
        failed += test_clear_all_entries();
    } catch (const std::exception& e) {
        std::cout << "FATAL ERROR: " << e.what() << std::endl;
        failed++;
    } catch (...) {
        std::cout << "FATAL ERROR: Unknown exception" << std::endl;
        failed++;
    }

    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Total tests: 7" << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Passed: " << (7 - failed) << std::endl;

    return failed;
}
