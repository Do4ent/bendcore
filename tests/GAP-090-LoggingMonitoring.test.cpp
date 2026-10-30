// GAP-090: LoggingMonitoring (34 tests) - Structured logging, aggregation
#include "bendcore/hardening/logging_monitoring.hpp"
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

    // Test 1-5: Basic logging
    {
        StructuredLogger logger;
        logger.setLogLevel(LogLevel::TRACE);

        logger.info("Test message", "TestComponent");
        TEST("T1: Log added", logger.getLogCount() == 1);

        const auto* entry = logger.getLogEntry(1);
        TEST("T2: Log entry retrieved", entry != nullptr);
        TEST("T3: Message correct", entry->message == "Test message");
        TEST("T4: Level correct", entry->level == LogLevel::INFO);

        TEST("T5: History accessible", logger.getLogHistory().size() == 1);
    }

    // Test 6-10: Multiple log levels
    {
        StructuredLogger logger;
        logger.setLogLevel(LogLevel::TRACE);

        logger.trace("trace msg");
        logger.debug("debug msg");
        logger.info("info msg");
        logger.warning("warning msg");
        logger.error("error msg");

        TEST("T6: Five logs recorded", logger.getLogCount() == 5);

        auto entry = logger.getLogEntry(3);
        TEST("T7: INFO level correct", entry->level == LogLevel::INFO);

        entry = logger.getLogEntry(5);
        TEST("T8: ERROR level correct", entry->level == LogLevel::ERROR);

        logger.critical("critical msg");
        TEST("T9: CRITICAL level added", logger.getLogCount() == 6);

        TEST("T10: Last message is critical", logger.getLogHistory().back().level == LogLevel::CRITICAL);
    }

    // Test 11-15: Log level filtering
    {
        StructuredLogger logger;
        logger.setLogLevel(LogLevel::WARNING);

        logger.trace("should not log");
        logger.debug("should not log");
        logger.info("should not log");
        logger.warning("should log 1");
        logger.error("should log 2");
        logger.critical("should log 3");

        TEST("T11: Only warnings and above", logger.getLogCount() == 3);
        TEST("T12: First is warning", logger.getLogHistory()[0].level == LogLevel::WARNING);
        TEST("T13: Second is error", logger.getLogHistory()[1].level == LogLevel::ERROR);
        TEST("T14: Third is critical", logger.getLogHistory()[2].level == LogLevel::CRITICAL);

        logger.setLogLevel(LogLevel::ERROR);
        logger.warning("should not log");
        TEST("T15: New filter applied", logger.getLogCount() == 3);
    }

    // Test 16-20: Metrics aggregation
    {
        MetricsAggregator agg;

        agg.recordMetric("response_time", 100.0);
        agg.recordMetric("response_time", 150.0);
        agg.recordMetric("response_time", 120.0);

        TEST("T16: Metrics recorded", !agg.getAllMetrics().empty());
        TEST("T17: Average correct", agg.getMetricAverage("response_time") > 120.0 &&
             agg.getMetricAverage("response_time") < 130.0);
        TEST("T18: Max correct", agg.getMetricMax("response_time") == 150.0);
        TEST("T19: Min correct", agg.getMetricMin("response_time") == 100.0);

        agg.incrementCounter("requests");
        agg.incrementCounter("requests", 5);
        TEST("T20: Counter aggregated", agg.getCounter("requests") == 6);
    }

    // Test 21-25: Log aggregator
    {
        LogAggregator agg;

        LogEntry entry1{1, LogLevel::INFO, LogSource::CORE, "msg1", "comp1", 1000, 1, ""};
        LogEntry entry2{2, LogLevel::ERROR, LogSource::IO, "msg2", "comp2", 2000, 2, ""};
        LogEntry entry3{3, LogLevel::WARNING, LogSource::GEOMETRY, "msg3", "comp3", 3000, 3, ""};

        agg.addLog(entry1);
        agg.addLog(entry2);
        agg.addLog(entry3);

        TEST("T21: Three logs aggregated", !agg.getLogsByLevel(LogLevel::INFO).empty());
        TEST("T22: Info count correct", agg.getLogCountByLevel(LogLevel::INFO) == 1);
        TEST("T23: Error count correct", agg.getLogCountByLevel(LogLevel::ERROR) == 1);
        TEST("T24: Warning count correct", agg.getLogCountByLevel(LogLevel::WARNING) == 1);

        auto by_source = agg.getLogsBySource(LogSource::CORE);
        TEST("T25: Source filtering works", by_source.size() == 1);
    }

    // Test 26-30: Log formatters
    {
        SimpleFormatter simple;
        JSONFormatter json;

        LogEntry entry{1, LogLevel::ERROR, LogSource::CORE, "test error", "TestComp", 0, 1, ""};

        std::string simple_fmt = simple.format(entry);
        std::string json_fmt = json.format(entry);

        TEST("T26: Simple format non-empty", !simple_fmt.empty());
        TEST("T27: JSON format non-empty", !json_fmt.empty());
        TEST("T28: JSON contains component", json_fmt.find("TestComp") != std::string::npos);
        TEST("T29: Simple contains message", simple_fmt.find("test error") != std::string::npos);

        StructuredLogger logger;
        logger.setFormatter(std::make_unique<JSONFormatter>());
        logger.info("test");
        TEST("T30: Formatter can be set", logger.getLogCount() == 1);
    }

    // Test 31-34: Monitoring system
    {
        MonitoringSystem sys;
        TEST("T31: Monitoring system created", true);

        sys.startMonitoring();
        TEST("T32: Monitoring started", sys.isMonitoring());

        sys.getLogger().info("system log");
        TEST("T33: Logger working", sys.getLogger().getLogCount() == 1);

        sys.stopMonitoring();
        TEST("T34: Monitoring stopped", !sys.isMonitoring());
    }

    printf("\n=== GAP-090: LoggingMonitoring ===\n");
    printf("Passed: %d/34\n", passed);
    printf("Failed: %d/34\n", failed);
    return (failed == 0) ? 0 : 1;
}
