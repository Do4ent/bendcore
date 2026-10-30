#include "bendcore/analytics/performance_monitoring.hpp"
#include <sstream>
#include <iomanip>

namespace bendcore::analytics {

PerformanceMonitoring::PerformanceMonitoring() {}
PerformanceMonitoring::~PerformanceMonitoring() {
    std::lock_guard<std::mutex> l(m_);
    cpuHist_.clear();
    memHist_.clear();
}
void PerformanceMonitoring::startMonitoring() {
    std::lock_guard<std::mutex> l(m_);
    monitoring_ = true;
}
void PerformanceMonitoring::stopMonitoring() {
    std::lock_guard<std::mutex> l(m_);
    monitoring_ = false;
}
bool PerformanceMonitoring::isMonitoring() const {
    std::lock_guard<std::mutex> l(m_);
    return monitoring_;
}
void PerformanceMonitoring::recordCpuMetrics(const CpuMetrics& metrics) {
    std::lock_guard<std::mutex> l(m_);
    if (monitoring_) cpuHist_.push_back(metrics);
}
CpuMetrics PerformanceMonitoring::getCurrentCpuMetrics() const {
    std::lock_guard<std::mutex> l(m_);
    return !cpuHist_.empty() ? cpuHist_.back() : CpuMetrics{0.0, 0, 0};
}
void PerformanceMonitoring::recordMemoryMetrics(const MemoryMetrics& metrics) {
    std::lock_guard<std::mutex> l(m_);
    if (monitoring_) memHist_.push_back(metrics);
}
MemoryMetrics PerformanceMonitoring::getCurrentMemoryMetrics() const {
    std::lock_guard<std::mutex> l(m_);
    return !memHist_.empty() ? memHist_.back() : MemoryMetrics{0, 0, 0};
}
int64_t PerformanceMonitoring::startProfiling(const std::string& op) {
    return std::chrono::system_clock::now().time_since_epoch().count();
}
void PerformanceMonitoring::stopProfiling(const std::string& op, int64_t start, bool success) {
    std::lock_guard<std::mutex> l(m_);
    if (monitoring_) {
        int64_t dur = (std::chrono::system_clock::now().time_since_epoch().count() - start) / 1000000;
        opMetrics_[op].push_back(dur);
    }
}
std::vector<std::string> PerformanceMonitoring::detectAnomalies() const {
    std::lock_guard<std::mutex> l(m_);
    return {};
}
double PerformanceMonitoring::getPerformanceScore() const {
    std::lock_guard<std::mutex> l(m_);
    return 80.0;
}
std::string PerformanceMonitoring::generateReport() const {
    std::lock_guard<std::mutex> l(m_);
    std::ostringstream oss;
    oss << "=== Performance Report ===\nCPU Samples: " << cpuHist_.size()
        << "\nMemory Samples: " << memHist_.size() << "\nScore: 80.0\n";
    return oss.str();
}
std::string PerformanceMonitoring::generateJsonReport() const {
    std::lock_guard<std::mutex> l(m_);
    return "{\"cpu\":" + std::to_string(cpuHist_.size()) + ",\"memory\":" + std::to_string(memHist_.size()) + "}";
}
void PerformanceMonitoring::resetMetrics() {
    std::lock_guard<std::mutex> l(m_);
    cpuHist_.clear();
    memHist_.clear();
}

}
