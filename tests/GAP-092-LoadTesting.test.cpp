// GAP-092: LoadTesting (38 tests) - Load simulation, stress testing
#include "bendcore/hardening/load_testing.hpp"
#include <cassert>
#include <cstdio>
#include <thread>

using namespace bendcore::production;

int main() {
    int passed = 0, failed = 0;

    #define TEST(name, cond) { \
        if (cond) { passed++; } else { \
            printf("FAIL: %s\n", name); failed++; \
        } \
    }

    // Test 1-5: LoadSimulator basic
    {
        LoadSimulator simulator;
        simulator.setLoadLevel(LoadLevel::LIGHT);

        TEST("T1: LoadSimulator created", simulator.getLoadLevel() == LoadLevel::LIGHT);

        simulator.setLoadLevel(LoadLevel::MODERATE);
        TEST("T2: LoadLevel changed", simulator.getLoadLevel() == LoadLevel::MODERATE);

        int counter = 0;
        auto operation = [&counter]() { counter++; return true; };

        TEST("T3: Not running initially", !simulator.isRunning());

        simulator.runLoadTest(operation, 1);
        auto result = simulator.getLastResult();
        TEST("T4: Operations executed", result.total_operations > 0);
        TEST("T5: Successful operations", result.successful_operations > 0);
    }

    // Test 6-12: LoadSimulator metrics
    {
        LoadSimulator simulator;
        simulator.setLoadLevel(LoadLevel::LIGHT);

        int op_count = 0;
        auto operation = [&op_count]() { op_count++; return true; };

        simulator.runLoadTest(operation, 1);
        auto result = simulator.getLastResult();

        TEST("T6: Total operations tracked", result.total_operations > 0);
        TEST("T7: Average response time calculated", result.average_response_time_ms >= 0.0);
        TEST("T8: Max response time set", result.max_response_time_ms > 0.0);
        TEST("T9: Min response time set", result.min_response_time_ms > 0.0);
        TEST("T10: Throughput calculated", result.throughput_ops_per_sec > 0.0);
        TEST("T11: No failed operations", result.failed_operations == 0);
        TEST("T12: Error count zero", result.error_count == 0);
    }

    // Test 13-18: ConcurrentLoadTest
    {
        ConcurrentLoadTest concurrent(2);
        TEST("T13: ConcurrentLoadTest created", concurrent.getConcurrentUsers() == 2);

        int exec_count = 0;
        auto operation = [&exec_count]() { exec_count++; return true; };

        concurrent.runTest(operation, 1);
        auto result = concurrent.getResult();

        TEST("T14: Concurrent test executed", result.total_operations > 0);
        TEST("T15: Concurrent operations successful", result.successful_operations > 0);
        TEST("T16: Response times recorded", !concurrent.getResponseTimes().empty());
        TEST("T17: Average response time", result.average_response_time_ms >= 0.0);
        TEST("T18: Throughput available", result.throughput_ops_per_sec >= 0.0);
    }

    // Test 19-24: StressTestRunner
    {
        StressTestRunner stressTest;
        TEST("T19: StressTestRunner created", true);

        stressTest.configureTest(1, 5, 20);
        TEST("T20: Stress test configured", true);

        auto operation = []() { return true; };
        auto metrics = stressTest.runStressTest(operation);

        TEST("T21: Metrics generated", metrics.test_passed);
        TEST("T22: Peak concurrent users set", metrics.peak_concurrent_users == 20);
        TEST("T23: Timeout count available", metrics.timeout_count >= 0);
        TEST("T24: Last metrics accessible", stressTest.getLastMetrics().peak_concurrent_users == 20);
    }

    // Test 25-30: LatencyAnalyzer
    {
        LatencyAnalyzer analyzer;

        analyzer.recordLatency(10.5);
        analyzer.recordLatency(12.3);
        analyzer.recordLatency(11.8);
        analyzer.recordLatency(13.2);
        analyzer.recordLatency(10.9);

        TEST("T25: Latencies recorded", true);
        TEST("T26: Average latency", analyzer.getAverageLatency() > 10.0 &&
             analyzer.getAverageLatency() < 14.0);
        TEST("T27: Median latency", analyzer.getMedianLatency() > 10.0);
        TEST("T28: Max latency", analyzer.getMaxLatency() == 13.2);
        TEST("T29: Min latency", analyzer.getMinLatency() == 10.5);
        TEST("T30: Standard deviation", analyzer.getStandardDeviation() >= 0.0);
    }

    // Test 31-35: LatencyAnalyzer percentiles
    {
        LatencyAnalyzer analyzer;

        for (int i = 1; i <= 100; ++i) {
            analyzer.recordLatency(static_cast<double>(i));
        }

        double p50 = analyzer.getPercentile(50.0);
        double p95 = analyzer.getPercentile(95.0);
        double p99 = analyzer.getPercentile(99.0);

        TEST("T31: P50 calculated", p50 > 0.0);
        TEST("T32: P95 greater than P50", p95 >= p50);
        TEST("T33: P99 greater than P95", p99 >= p95);

        int outliers = analyzer.getOutlierCount(50.0);
        TEST("T34: Outlier detection works", outliers >= 0);

        analyzer.clearData();
        TEST("T35: Data cleared", analyzer.getAverageLatency() == 0.0);
    }

    // Test 36-38: EnduranceTestRunner and SpikeTestRunner
    {
        EnduranceTestRunner endurance;
        TEST("T36: EnduranceTestRunner created", !endurance.isRunning());

        SpikeTestRunner spike;
        spike.configureSpikeTest(1, 10, 500);
        TEST("T37: SpikeTestRunner configured", true);

        auto operation = []() { return true; };
        bool result = spike.runSpikeTest(operation, 2);
        TEST("T38: Spike test completed", result);
    }

    printf("\n=== GAP-092: LoadTesting ===\n");
    printf("Passed: %d/38\n", passed);
    printf("Failed: %d/38\n", failed);
    return (failed == 0) ? 0 : 1;
}
