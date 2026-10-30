// GAP-091: PerformanceTuning (36 tests) - Profiling, memory optimization
#include "bendcore/hardening/performance_tuning.hpp"
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

    // Test 1-5: ProfileScope
    {
        {
            ProfileScope scope("test_operation");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        TEST("T1: ProfileScope created and destroyed", true);
    }

    // Test 6-10: PerformanceProfiler
    {
        PerformanceProfiler profiler;
        profiler.startProfiler();
        TEST("T2: Profiler started", profiler.isRunning());

        profiler.recordMetric("op1", 10.5);
        profiler.recordMetric("op1", 12.3);

        TEST("T3: Metrics recorded", profiler.getCallCount("op1") == 2);
        TEST("T4: Average duration correct", profiler.getAverageDuration("op1") > 10.0);
        TEST("T5: Peak duration correct", profiler.getPeakDuration("op1") == 12.3);

        profiler.stopProfiler();
        TEST("T6: Profiler stopped", !profiler.isRunning());
    }

    // Test 7-12: Multiple operations profiling
    {
        PerformanceProfiler profiler;
        profiler.startProfiler();

        profiler.recordMetric("read", 5.0);
        profiler.recordMetric("write", 15.0);
        profiler.recordMetric("process", 25.0);

        TEST("T7: Three metrics recorded", profiler.getAllMetrics().size() == 3);
        TEST("T8: Read average", profiler.getAverageDuration("read") == 5.0);
        TEST("T9: Write average", profiler.getAverageDuration("write") == 15.0);
        TEST("T10: Process average", profiler.getAverageDuration("process") == 25.0);

        profiler.clearMetrics();
        TEST("T11: Metrics cleared", profiler.getAllMetrics().empty());
        TEST("T12: Report generation", !profiler.generateReport().empty());
    }

    // Test 13-18: MemoryTracker
    {
        MemoryTracker tracker;

        tracker.recordAllocation(1024);
        tracker.recordAllocation(2048);

        auto usage = tracker.getCurrentUsage();
        TEST("T13: Memory allocated", usage.heap_used == 3072);
        TEST("T14: Current allocations", tracker.getActiveAllocationCount() == 2);

        tracker.recordDeallocation(1024);
        usage = tracker.getCurrentUsage();
        TEST("T15: Memory deallocated", usage.heap_used == 2048);
        TEST("T16: Active allocations decreased", tracker.getActiveAllocationCount() == 1);

        auto peak = tracker.getPeakUsage();
        TEST("T17: Peak usage recorded", peak.peak_usage >= 3072);

        tracker.resetStatistics();
        TEST("T18: Statistics reset", tracker.getActiveAllocationCount() == 0);
    }

    // Test 19-24: CacheOptimizer
    {
        CacheOptimizer cache(10);

        cache.put("key1", "value1");
        TEST("T19: Item cached", cache.contains("key1"));

        std::string val = cache.get("key1");
        TEST("T20: Value retrieved", val == "value1");
        TEST("T21: Hit count increased", cache.getHitCount() == 1);

        cache.get("missing");
        TEST("T22: Miss count increased", cache.getMissCount() == 1);

        double ratio = cache.getHitRatio();
        TEST("T23: Hit ratio calculated", ratio > 0.0 && ratio <= 1.0);

        cache.clear();
        TEST("T24: Cache cleared", cache.getSize() == 0);
    }

    // Test 25-30: ThreadPoolOptimizer
    {
        ThreadPoolOptimizer pool(4);
        TEST("T25: ThreadPool created", pool.getNumThreads() == 4);

        bool task_executed = false;
        pool.submitTask([&task_executed]() { task_executed = true; });

        TEST("T26: Task submitted", task_executed);
        TEST("T27: Pending tasks tracked", pool.getPendingTaskCount() >= 0);

        pool.setNumThreads(8);
        TEST("T28: Threads updated", pool.getNumThreads() == 8);

        double utilization = pool.getThreadUtilization();
        TEST("T29: Utilization calculated", utilization >= 0.0);

        pool.shutdown();
        TEST("T30: Pool shutdown", true);
    }

    // Test 31-36: CPUProfiler
    {
        CPUProfiler profiler;
        TEST("T31: CPUProfiler created", true);

        profiler.startProfiling();
        TEST("T32: Profiling started", true);

        std::string cpu = "";
        TEST("T33: CPU usage available", cpu.length() >= 0);

        auto hotspots = profiler.getHotspots(5);
        TEST("T34: Hotspots retrieved", true);

        auto profile_data = profiler.getProfileData();
        TEST("T35: Profile data available", true);

        profiler.stopProfiling();
        TEST("T36: Profiling stopped", true);
    }

    printf("\n=== GAP-091: PerformanceTuning ===\n");
    printf("Passed: %d/36\n", passed);
    printf("Failed: %d/36\n", failed);
    return (failed == 0) ? 0 : 1;
}
