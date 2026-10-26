#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace bendcore::hardening {

// ==============================================================================
// LOGGING & MONITORING - GAP-090
// ==============================================================================
// Comprehensive logging system, real-time monitoring, alerting, metrics

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

enum class MetricType {
    COUNTER,
    GAUGE,
    HISTOGRAM,
    TIMER
};

struct LogEntry {
    std::string logId;
    LogLevel level;
    std::string component;
    std::string message;
    int64_t timestamp;
    std::string threadId;
    std::map<std::string, std::string> contextData;
};

struct Metric {
    std::string metricId;
    std::string metricName;
    MetricType type;
    double value;
    int64_t timestamp;
    std::map<std::string, std::string> tags;
};

struct Alert {
    std::string alertId;
    std::string condition;
    std::string message;
    LogLevel severity;
    int64_t triggeredAt;
    bool acknowledged;
};

class LoggingMonitoringSystem {
public:
    LoggingMonitoringSystem();
    ~LoggingMonitoringSystem();

    // Logging
    bool log(LogLevel level, const std::string& component, const std::string& message);
    bool logWithContext(const LogEntry& entry);
    std::vector<LogEntry> getLogEntries();
    std::vector<LogEntry> getLogsByLevel(LogLevel level);
    std::vector<LogEntry> getLogsByComponent(const std::string& component);
    std::vector<LogEntry> getLogsByTimeRange(int64_t startTime, int64_t endTime);

    // Log configuration
    void setMinimumLogLevel(LogLevel level);
    LogLevel getMinimumLogLevel() const;
    bool enableComponentLogging(const std::string& component, bool enable);
    bool isComponentLoggingEnabled(const std::string& component) const;

    // Metric recording
    bool recordMetric(const Metric& metric);
    bool incrementCounter(const std::string& metricName, double delta);
    bool setGaugeValue(const std::string& metricName, double value);
    bool recordHistogram(const std::string& metricName, double value);
    bool recordTimer(const std::string& metricName, int64_t durationMs);

    // Metric retrieval
    std::vector<Metric> getMetrics();
    Metric getMetric(const std::string& metricName);
    std::vector<Metric> getMetricsByType(MetricType type);
    std::vector<Metric> getMetricsByTimeRange(int64_t startTime, int64_t endTime);

    // Alert management
    bool registerAlert(const std::string& condition, const std::string& message, LogLevel severity);
    bool triggerAlert(const Alert& alert);
    std::vector<Alert> getActiveAlerts();
    std::vector<Alert> getAllAlerts();
    bool acknowledgeAlert(const std::string& alertId);

    // Real-time monitoring
    bool enableRealTimeMonitoring(bool enable);
    bool isRealTimeMonitoringEnabled() const;
    std::vector<LogEntry> getRecentLogs(int32_t count);
    std::vector<Metric> getRecentMetrics(int32_t count);

    // Performance monitoring
    float getCpuUsagePercentage();
    float getMemoryUsagePercentage();
    int32_t getThreadCount();
    float getAverageResponseTime();

    // Log export
    bool exportLogs(const std::string& outputPath);
    bool exportMetrics(const std::string& outputPath);
    std::string generatePerformanceReport();

    // Log analysis
    std::vector<std::string> getErrorTrends();
    std::map<std::string, int32_t> getErrorFrequency();
    float getErrorRate();

    // Retention policies
    bool setLogRetentionPolicy(int32_t maxAgeDays);
    bool enforceRetention();
    bool archiveOldLogs();

private:
    std::vector<LogEntry> logs;
    std::vector<Metric> metrics;
    std::vector<Alert> alerts;
    LogLevel minimumLogLevel;
    bool realTimeMonitoringEnabled;
    std::map<std::string, bool> componentLoggingStatus;
};

}  // namespace bendcore::hardening
