#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>

namespace bendcore::cloud {

// Metric types
enum class MetricType {
    Counter,
    Gauge,
    Histogram,
    Timer
};

// Event types
enum class EventType {
    UserLogin,
    UserLogout,
    ProjectCreated,
    ProjectModified,
    ProjectDeleted,
    FileUploaded,
    FileDownloaded,
    CollaborationStarted,
    CollaborationEnded,
    ErrorOccurred,
    PerformanceAlert
};

// Metric event
struct Metric {
    std::string metricId;
    std::string metricName;
    MetricType type = MetricType::Counter;
    double value = 0.0;
    int64_t timestamp = 0;
    std::map<std::string, std::string> tags;
};

// Analytics event
struct AnalyticsEvent {
    uint64_t eventId = 0;
    EventType eventType;
    std::string userId;
    std::string projectId;
    int64_t timestamp = 0;
    std::string description;
    std::map<std::string, std::string> metadata;
    int durationMs = 0;
};

// Performance metrics
struct PerformanceMetrics {
    double cpuUsagePercent = 0.0;
    double memoryUsagePercent = 0.0;
    double diskUsagePercent = 0.0;
    int activeConnections = 0;
    double avgResponseTimeMs = 0.0;
    int requestsPerSecond = 0;
    int errorCount = 0;
    int warningCount = 0;
};

// Usage statistics
struct UsageStatistics {
    int totalRequests = 0;
    int totalUsers = 0;
    int activeUsers = 0;
    int64_t totalDataBytes = 0;
    int totalProjects = 0;
    int totalCollaborativeSessions = 0;
    std::map<std::string, int> actionCounts;  // "create", "read", "update", "delete", etc.
};

// Error tracking
struct ErrorRecord {
    uint64_t errorId = 0;
    std::string errorType;
    std::string errorMessage;
    std::string stackTrace;
    std::string userId;
    std::string projectId;
    int64_t timestamp = 0;
    int occurrenceCount = 1;
    bool isResolved = false;
};

// Dashboard data
struct DashboardData {
    int totalUsers = 0;
    int activeUsers = 0;
    int totalProjects = 0;
    double avgResponseTimeMs = 0.0;
    int errorRate = 0;  // percentage
    std::vector<Metric> topMetrics;
    std::vector<ErrorRecord> recentErrors;
    PerformanceMetrics currentPerformance;
};

// Analytics and Monitoring System
class AnalyticsMonitoringSystem {
public:
    explicit AnalyticsMonitoringSystem();
    ~AnalyticsMonitoringSystem();

    // Metrics recording
    void recordMetric(
        const std::string& metricName,
        MetricType type,
        double value,
        const std::map<std::string, std::string>& tags = {}
    );

    std::vector<Metric> getMetrics(
        const std::string& metricName,
        int64_t fromTimestamp = 0,
        int64_t toTimestamp = 0
    ) const;

    // Event tracking
    uint64_t recordEvent(
        EventType eventType,
        const std::string& userId,
        const std::string& projectId = "",
        const std::string& description = "",
        const std::map<std::string, std::string>& metadata = {}
    );

    uint64_t recordEventWithDuration(
        EventType eventType,
        const std::string& userId,
        int durationMs,
        const std::string& projectId = "",
        const std::string& description = ""
    );

    std::vector<AnalyticsEvent> getEvents(
        std::optional<EventType> eventType = {},
        std::optional<std::string> userId = {},
        int limit = 100
    ) const;

    // Performance monitoring
    void recordPerformanceMetrics(
        double cpuUsage,
        double memoryUsage,
        double diskUsage,
        int activeConnections,
        double avgResponseTime,
        int requestsPerSecond,
        int errorCount,
        int warningCount
    );

    std::optional<PerformanceMetrics> getLatestPerformanceMetrics() const;

    std::vector<PerformanceMetrics> getPerformanceHistory(
        int64_t fromTimestamp = 0,
        int64_t toTimestamp = 0
    ) const;

    // Usage statistics
    UsageStatistics getUserStatistics() const;

    UsageStatistics getProjectStatistics() const;

    UsageStatistics getSystemStatistics() const;

    // Error tracking
    void recordError(
        const std::string& errorType,
        const std::string& errorMessage,
        const std::string& stackTrace,
        const std::string& userId = "",
        const std::string& projectId = ""
    );

    std::vector<ErrorRecord> getErrors(
        std::optional<std::string> errorType = {},
        bool unresolvedOnly = false,
        int limit = 50
    ) const;

    bool resolveError(uint64_t errorId);

    int getErrorRate() const;

    // Dashboard generation
    DashboardData generateDashboard() const;

    // Analytics queries
    struct AnalyticsQuery {
        std::string metric;
        int64_t fromTimestamp = 0;
        int64_t toTimestamp = 0;
        std::string aggregation;  // "sum", "avg", "max", "min"
        std::string groupBy;      // "hour", "day", "week"
    };

    std::vector<Metric> executeQuery(const AnalyticsQuery& query) const;

    // Alerts and thresholds
    struct AlertThreshold {
        std::string metricName;
        double warningThreshold = 0.0;
        double criticalThreshold = 0.0;
        std::string comparisonType;  // "greater_than", "less_than"
    };

    void setAlertThreshold(const AlertThreshold& threshold);

    std::vector<AlertThreshold> getAlertThresholds() const;

    // Real-time alerts
    struct Alert {
        uint64_t alertId = 0;
        std::string metricName;
        std::string severity;  // "warning", "critical"
        std::string message;
        int64_t timestamp = 0;
        bool isAcknowledged = false;
    };

    std::vector<Alert> getActiveAlerts() const;

    bool acknowledgeAlert(uint64_t alertId);

    // Export functionality
    std::string exportAnalyticsAsJson(
        int64_t fromTimestamp = 0,
        int64_t toTimestamp = 0
    ) const;

    std::string exportAnalyticsAsCsv(
        int64_t fromTimestamp = 0,
        int64_t toTimestamp = 0
    ) const;

    // Health check
    struct SystemHealth {
        bool isHealthy = true;
        std::string status;  // "healthy", "degraded", "critical"
        std::vector<std::string> issues;
        PerformanceMetrics metrics;
        int64_t lastCheckTimestamp = 0;
    };

    SystemHealth checkSystemHealth() const;

    // Cleanup old data
    int pruneOldData(int64_t keepAfterTimestamp);

private:
    std::vector<Metric> metrics;
    std::vector<AnalyticsEvent> events;
    std::vector<PerformanceMetrics> performanceHistory;
    std::vector<ErrorRecord> errorRecords;
    std::vector<Alert> alerts;
    std::vector<AlertThreshold> thresholds;
    uint64_t nextEventId = 1;
    uint64_t nextErrorId = 1;
    uint64_t nextAlertId = 1;
};

}  // namespace bendcore::cloud
