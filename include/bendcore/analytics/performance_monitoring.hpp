#pragma once
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <mutex>

namespace bendcore::analytics {

struct CpuMetrics {
    double cpuUsagePercent;
    int32_t threadCount;
    int64_t contextSwitches;
};

struct MemoryMetrics {
    int64_t heapUsageBytes;
    int64_t peakHeapBytes;
    int64_t allocationsCount;
};

class PerformanceMonitoring {
public:
    PerformanceMonitoring();
    ~PerformanceMonitoring();
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const;
    void recordCpuMetrics(const CpuMetrics& m);
    CpuMetrics getCurrentCpuMetrics() const;
    void recordMemoryMetrics(const MemoryMetrics& m);
    MemoryMetrics getCurrentMemoryMetrics() const;
    int64_t startProfiling(const std::string& op);
    void stopProfiling(const std::string& op, int64_t start, bool success = true);
    std::vector<std::string> detectAnomalies() const;
    double getPerformanceScore() const;
    std::string generateReport() const;
    std::string generateJsonReport() const;
    void resetMetrics();

private:
    mutable std::mutex m_;
    bool monitoring_ = false;
    std::vector<CpuMetrics> cpuHist_;
    std::vector<MemoryMetrics> memHist_;
    std::map<std::string, std::vector<int64_t>> opMetrics_;
};

}
