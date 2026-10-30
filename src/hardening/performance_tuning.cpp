#include "bendcore/hardening/performance_tuning.hpp"
#include <sstream>
#include <algorithm>
#include <numeric>

namespace bendcore::hardening {

// ProfileScope
ProfileScope::ProfileScope(const std::string& name) : name_(name) {}
ProfileScope::~ProfileScope() {}

// PerformanceProfiler
PerformanceProfiler::PerformanceProfiler() : running_(false) {}

void PerformanceProfiler::startProfiler() { running_ = true; }
void PerformanceProfiler::stopProfiler() { running_ = false; }
bool PerformanceProfiler::isRunning() const { return running_; }

void PerformanceProfiler::recordMetric(const std::string& name, double duration_ms) {
    auto& m = metrics_[name];
    m.call_count++;
    m.total_duration += duration_ms;
    if (duration_ms > m.peak_duration) m.peak_duration = duration_ms;
}

int PerformanceProfiler::getCallCount(const std::string& name) const {
    auto it = metrics_.find(name);
    if (it == metrics_.end()) return 0;
    return it->second.call_count;
}

double PerformanceProfiler::getAverageDuration(const std::string& name) const {
    auto it = metrics_.find(name);
    if (it == metrics_.end() || it->second.call_count == 0) return 0.0;
    return it->second.total_duration / it->second.call_count;
}

double PerformanceProfiler::getPeakDuration(const std::string& name) const {
    auto it = metrics_.find(name);
    if (it == metrics_.end()) return 0.0;
    return it->second.peak_duration;
}

std::map<std::string, ProfileMetric> PerformanceProfiler::getAllMetrics() const {
    return metrics_;
}

void PerformanceProfiler::clearMetrics() { metrics_.clear(); }

std::string PerformanceProfiler::generateReport() const {
    std::ostringstream oss;
    oss << "Performance Report:\n";
    for (const auto& [name, m] : metrics_) {
        oss << name << ": calls=" << m.call_count << " avg=" << (m.call_count ? m.total_duration / m.call_count : 0) << "ms\n";
    }
    return oss.str();
}

// MemoryTracker
MemoryTracker::MemoryTracker() : heap_used_(0), peak_usage_(0), active_allocations_(0) {}

void MemoryTracker::recordAllocation(int64_t size) {
    heap_used_ += size;
    active_allocations_++;
    if (heap_used_ > peak_usage_) peak_usage_ = heap_used_;
}

void MemoryTracker::recordDeallocation(int64_t size) {
    heap_used_ -= size;
    if (active_allocations_ > 0) active_allocations_--;
}

MemoryUsage MemoryTracker::getCurrentUsage() const {
    return {heap_used_, heap_used_};
}

int MemoryTracker::getActiveAllocationCount() const { return active_allocations_; }

PeakUsage MemoryTracker::getPeakUsage() const { return {peak_usage_}; }

void MemoryTracker::resetStatistics() {
    heap_used_ = 0;
    peak_usage_ = 0;
    active_allocations_ = 0;
}

// CacheOptimizer
CacheOptimizer::CacheOptimizer(int capacity)
    : capacity_(capacity), hits_(0), misses_(0) {}

void CacheOptimizer::put(const std::string& key, const std::string& value) {
    data_[key] = value;
}

std::string CacheOptimizer::get(const std::string& key) {
    auto it = data_.find(key);
    if (it != data_.end()) {
        hits_++;
        return it->second;
    }
    misses_++;
    return "";
}

bool CacheOptimizer::contains(const std::string& key) const {
    return data_.find(key) != data_.end();
}

int CacheOptimizer::getHitCount() const { return hits_; }
int CacheOptimizer::getMissCount() const { return misses_; }

double CacheOptimizer::getHitRatio() const {
    int total = hits_ + misses_;
    if (total == 0) return 0.0;
    return static_cast<double>(hits_) / total;
}

void CacheOptimizer::clear() { data_.clear(); }
int CacheOptimizer::getSize() const { return static_cast<int>(data_.size()); }

// ThreadPoolOptimizer
ThreadPoolOptimizer::ThreadPoolOptimizer(int num_threads)
    : num_threads_(num_threads), pending_tasks_(0) {}

ThreadPoolOptimizer::~ThreadPoolOptimizer() {}

int ThreadPoolOptimizer::getNumThreads() const { return num_threads_; }

void ThreadPoolOptimizer::submitTask(std::function<void()> task) {
    // Execute synchronously for simplicity
    task();
}

int ThreadPoolOptimizer::getPendingTaskCount() const { return pending_tasks_; }

void ThreadPoolOptimizer::setNumThreads(int n) { num_threads_ = n; }

double ThreadPoolOptimizer::getThreadUtilization() const { return 0.0; }

void ThreadPoolOptimizer::shutdown() {}

// CPUProfiler
CPUProfiler::CPUProfiler() : profiling_(false) {}

void CPUProfiler::startProfiling() { profiling_ = true; }
void CPUProfiler::stopProfiling() { profiling_ = false; }

std::vector<ProfileDataEntry> CPUProfiler::getHotspots(int n) const {
    (void)n;
    return {};
}

std::vector<ProfileDataEntry> CPUProfiler::getProfileData() const {
    return {};
}

}  // namespace bendcore::hardening
