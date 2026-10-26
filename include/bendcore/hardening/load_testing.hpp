#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace bendcore::hardening {

// ==============================================================================
// LOAD TESTING - GAP-092
// ==============================================================================
// Stress testing, capacity planning, load simulation, bottleneck detection

enum class LoadTestType {
    STRESS,
    LOAD,
    SPIKE,
    ENDURANCE,
    VOLUME
};

struct LoadProfile {
    int32_t minConcurrentUsers;
    int32_t maxConcurrentUsers;
    int32_t rampUpTime;  // seconds
    int32_t sustainTime; // seconds
    int32_t rampDownTime; // seconds
};

struct LoadTestResult {
    std::string testId;
    LoadTestType type;
    int64_t startTime;
    int64_t endTime;
    int32_t totalRequests;
    int32_t successfulRequests;
    int32_t failedRequests;
    double averageResponseTime;
    double peakResponseTime;
    double throughput;
    int32_t maxConcurrentUsers;
};

struct BottleneckReport {
    std::string bottleneckId;
    std::string component;
    std::string description;
    double impactPercentage;
    std::vector<std::string> recommendations;
};

class LoadTestingEngine {
public:
    LoadTestingEngine();
    ~LoadTestingEngine();

    // Load test execution
    bool startLoadTest(LoadTestType type, const LoadProfile& profile);
    bool stopLoadTest();
    bool pauseLoadTest();
    bool resumeLoadTest();
    LoadTestResult getLastTestResult();

    // Load simulation
    bool simulateLoad(int32_t concurrentUsers);
    bool simulateGradualLoad(int32_t startUsers, int32_t endUsers, int32_t durationSeconds);
    bool simulateSpikeLoad(int32_t users, int32_t durationSeconds);
    bool simulateVariableLoad(const std::vector<int32_t>& userCounts);

    // Stress testing
    bool runStressTest();
    bool runBreakingPointTest();
    int32_t findMaxCapacity();

    // Capacity planning
    int32_t getMaxConcurrentUsers() const;
    double getMaxThroughput() const;
    std::map<std::string, double> getResourceLimits();
    bool canHandle(int32_t concurrentUsers) const;

    // Bottleneck detection
    std::vector<BottleneckReport> detectBottlenecks();
    BottleneckReport getBottleneck(const std::string& component);
    bool analyzeBottleneck(const std::string& bottleneckId);

    // Performance metrics during test
    double getCurrentThroughput() const;
    double getCurrentResponseTime() const;
    int32_t getActiveConnections() const;
    float getErrorRate() const;

    // Resource monitoring
    float getCpuUsageDuringTest() const;
    float getMemoryUsageDuringTest() const;
    float getDiskIODuringTest() const;
    float getNetworkUtilization() const;

    // Test reporting
    LoadTestResult getTestResult(const std::string& testId);
    std::string generateLoadTestReport();
    std::string generateCapacityReport();
    std::string compareTestResults(const std::string& testId1, const std::string& testId2);

    // SLA compliance
    bool setSLAThreshold(const std::string& metric, double threshold);
    bool checkSLACompliance();
    std::vector<std::string> getViolatedSLAs();

    // Endurance testing
    bool runEnduranceTest(int32_t concurrentUsers, int64_t durationMinutes);
    bool checkMemoryLeaksUnderLoad();

    // Volume testing
    bool runVolumeTest(int64_t dataSize);
    bool testLargeDatasetHandling();

private:
    std::vector<LoadTestResult> testResults;
    std::vector<BottleneckReport> bottlenecks;
    LoadProfile currentLoadProfile;
    bool testRunning;
};

}  // namespace bendcore::hardening
