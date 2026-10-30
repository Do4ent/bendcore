#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cstdint>

namespace bendcore::hardening {

// ==============================================================================
// PERFORMANCE TUNING - GAP-091
// ==============================================================================

struct ProfileMetric {
    int call_count;
    double total_duration;
    double peak_duration;
};

struct MemoryUsage {
    int64_t heap_used;
    int64_t heap_total;
};

struct PeakUsage {
    int64_t peak_usage;
};

struct ProfileDataEntry {
    std::string name;
    double duration_ms;
};

class ProfileScope {
public:
    explicit ProfileScope(const std::string& name);
    ~ProfileScope();
private:
    std::string name_;
};

class PerformanceProfiler {
public:
    PerformanceProfiler();
    void startProfiler();
    void stopProfiler();
    bool isRunning() const;
    void recordMetric(const std::string& name, double duration_ms);
    int getCallCount(const std::string& name) const;
    double getAverageDuration(const std::string& name) const;
    double getPeakDuration(const std::string& name) const;
    std::map<std::string, ProfileMetric> getAllMetrics() const;
    void clearMetrics();
    std::string generateReport() const;
private:
    bool running_;
    std::map<std::string, ProfileMetric> metrics_;
};

class MemoryTracker {
public:
    MemoryTracker();
    void recordAllocation(int64_t size);
    void recordDeallocation(int64_t size);
    MemoryUsage getCurrentUsage() const;
    int getActiveAllocationCount() const;
    PeakUsage getPeakUsage() const;
    void resetStatistics();
private:
    int64_t heap_used_;
    int64_t peak_usage_;
    int active_allocations_;
};

class CacheOptimizer {
public:
    explicit CacheOptimizer(int capacity);
    void put(const std::string& key, const std::string& value);
    std::string get(const std::string& key);
    bool contains(const std::string& key) const;
    int getHitCount() const;
    int getMissCount() const;
    double getHitRatio() const;
    void clear();
    int getSize() const;
private:
    int capacity_;
    std::map<std::string, std::string> data_;
    int hits_;
    int misses_;
};

class ThreadPoolOptimizer {
public:
    explicit ThreadPoolOptimizer(int num_threads);
    ~ThreadPoolOptimizer();
    int getNumThreads() const;
    void submitTask(std::function<void()> task);
    int getPendingTaskCount() const;
    void setNumThreads(int n);
    double getThreadUtilization() const;
    void shutdown();
private:
    int num_threads_;
    int pending_tasks_;
};

class CPUProfiler {
public:
    CPUProfiler();
    void startProfiling();
    void stopProfiling();
    std::vector<ProfileDataEntry> getHotspots(int n) const;
    std::vector<ProfileDataEntry> getProfileData() const;
private:
    bool profiling_;
};

}  // namespace bendcore::hardening

namespace bendcore {
    namespace production = hardening;
}
