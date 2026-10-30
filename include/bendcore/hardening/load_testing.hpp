#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace bendcore::hardening {

// ==============================================================================
// LOAD TESTING - GAP-092
// ==============================================================================

enum class LoadLevel {
    LIGHT,
    MODERATE,
    HEAVY,
    EXTREME
};

struct LoadTestResult {
    int total_operations;
    int successful_operations;
    int failed_operations;
    int error_count;
    double average_response_time_ms;
    double max_response_time_ms;
    double min_response_time_ms;
    double throughput_ops_per_sec;
};

struct StressMetrics {
    bool test_passed;
    int peak_concurrent_users;
    int timeout_count;
    double average_response_time_ms;
};

class LoadSimulator {
public:
    LoadSimulator();
    void setLoadLevel(LoadLevel level);
    LoadLevel getLoadLevel() const;
    bool isRunning() const;
    void runLoadTest(std::function<bool()> operation, int duration_seconds);
    LoadTestResult getLastResult() const;
private:
    LoadLevel level_;
    bool running_;
    LoadTestResult last_result_;
};

class ConcurrentLoadTest {
public:
    explicit ConcurrentLoadTest(int concurrent_users);
    int getConcurrentUsers() const;
    void runTest(std::function<bool()> operation, int duration_seconds);
    LoadTestResult getResult() const;
    std::vector<double> getResponseTimes() const;
private:
    int concurrent_users_;
    LoadTestResult result_;
    std::vector<double> response_times_;
};

class StressTestRunner {
public:
    StressTestRunner();
    void configureTest(int start_users, int step, int max_users);
    StressMetrics runStressTest(std::function<bool()> operation);
    StressMetrics getLastMetrics() const;
private:
    int start_users_;
    int step_;
    int max_users_;
    StressMetrics last_metrics_;
};

class LatencyAnalyzer {
public:
    LatencyAnalyzer();
    void recordLatency(double ms);
    double getAverageLatency() const;
    double getMedianLatency() const;
    double getMaxLatency() const;
    double getMinLatency() const;
    double getStandardDeviation() const;
    double getPercentile(double percentile) const;
    int getOutlierCount(double threshold) const;
    void clearData();
private:
    std::vector<double> latencies_;
};

class EnduranceTestRunner {
public:
    EnduranceTestRunner();
    bool isRunning() const;
    void start(std::function<bool()> op, int duration_minutes, int users);
    void stop();
private:
    bool running_;
};

class SpikeTestRunner {
public:
    SpikeTestRunner();
    void configureSpikeTest(int base_users, int spike_users, int spike_duration_ms);
    bool runSpikeTest(std::function<bool()> operation, int duration_seconds);
private:
    int base_users_;
    int spike_users_;
    int spike_duration_ms_;
};

}  // namespace bendcore::hardening

namespace bendcore {
    namespace production = hardening;
}
