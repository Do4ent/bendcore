#include "bendcore/cloud/analytics_monitoring.hpp"

#include <algorithm>
#include <ctime>
#include <sstream>
#include <set>

namespace bendcore::cloud {

AnalyticsMonitoringSystem::AnalyticsMonitoringSystem() = default;
AnalyticsMonitoringSystem::~AnalyticsMonitoringSystem() = default;

void AnalyticsMonitoringSystem::recordMetric(
    const std::string& metricName,
    MetricType type,
    double value,
    const std::map<std::string, std::string>& tags
) {
    Metric metric;
    metric.metricId = "metric_" + std::to_string(metrics.size() + 1);
    metric.metricName = metricName;
    metric.type = type;
    metric.value = value;
    metric.timestamp = std::time(nullptr);
    metric.tags = tags;

    metrics.push_back(metric);
}

std::vector<Metric> AnalyticsMonitoringSystem::getMetrics(
    const std::string& metricName,
    int64_t fromTimestamp,
    int64_t toTimestamp
) const {
    std::vector<Metric> result;

    for (const auto& metric : metrics) {
        if (metric.metricName != metricName) {
            continue;
        }

        if (fromTimestamp > 0 && metric.timestamp < fromTimestamp) {
            continue;
        }

        if (toTimestamp > 0 && metric.timestamp > toTimestamp) {
            continue;
        }

        result.push_back(metric);
    }

    return result;
}

uint64_t AnalyticsMonitoringSystem::recordEvent(
    EventType eventType,
    const std::string& userId,
    const std::string& projectId,
    const std::string& description,
    const std::map<std::string, std::string>& metadata
) {
    AnalyticsEvent event;
    event.eventId = nextEventId++;
    event.eventType = eventType;
    event.userId = userId;
    event.projectId = projectId;
    event.timestamp = std::time(nullptr);
    event.description = description;
    event.metadata = metadata;

    events.push_back(event);
    return event.eventId;
}

uint64_t AnalyticsMonitoringSystem::recordEventWithDuration(
    EventType eventType,
    const std::string& userId,
    int durationMs,
    const std::string& projectId,
    const std::string& description
) {
    AnalyticsEvent event;
    event.eventId = nextEventId++;
    event.eventType = eventType;
    event.userId = userId;
    event.projectId = projectId;
    event.timestamp = std::time(nullptr);
    event.description = description;
    event.durationMs = durationMs;

    events.push_back(event);
    return event.eventId;
}

std::vector<AnalyticsEvent> AnalyticsMonitoringSystem::getEvents(
    std::optional<EventType> eventType,
    std::optional<std::string> userId,
    int limit
) const {
    std::vector<AnalyticsEvent> result;

    for (const auto& event : events) {
        if (eventType.has_value() && event.eventType != eventType.value()) {
            continue;
        }

        if (userId.has_value() && event.userId != userId.value()) {
            continue;
        }

        result.push_back(event);
    }

    std::sort(result.begin(), result.end(),
        [](const AnalyticsEvent& a, const AnalyticsEvent& b) {
            return a.timestamp > b.timestamp;
        });

    if (limit > 0 && result.size() > static_cast<size_t>(limit)) {
        result.resize(limit);
    }

    return result;
}

void AnalyticsMonitoringSystem::recordPerformanceMetrics(
    double cpuUsage,
    double memoryUsage,
    double diskUsage,
    int activeConnections,
    double avgResponseTime,
    int requestsPerSecond,
    int errorCount,
    int warningCount
) {
    PerformanceMetrics perf;
    perf.cpuUsagePercent = cpuUsage;
    perf.memoryUsagePercent = memoryUsage;
    perf.diskUsagePercent = diskUsage;
    perf.activeConnections = activeConnections;
    perf.avgResponseTimeMs = avgResponseTime;
    perf.requestsPerSecond = requestsPerSecond;
    perf.errorCount = errorCount;
    perf.warningCount = warningCount;

    performanceHistory.push_back(perf);
}

std::optional<PerformanceMetrics> AnalyticsMonitoringSystem::getLatestPerformanceMetrics() const {
    if (performanceHistory.empty()) {
        return std::nullopt;
    }

    return performanceHistory.back();
}

std::vector<PerformanceMetrics> AnalyticsMonitoringSystem::getPerformanceHistory(
    int64_t fromTimestamp,
    int64_t toTimestamp
) const {
    // Simplified: return all performance records
    return performanceHistory;
}

UsageStatistics AnalyticsMonitoringSystem::getUserStatistics() const {
    UsageStatistics stats;
    stats.totalRequests = events.size();

    std::set<std::string> uniqueUsers;

    for (const auto& event : events) {
        uniqueUsers.insert(event.userId);
    }

    stats.totalUsers = uniqueUsers.size();
    stats.activeUsers = uniqueUsers.size() / 2;  // Simplified

    return stats;
}

UsageStatistics AnalyticsMonitoringSystem::getProjectStatistics() const {
    UsageStatistics stats;

    std::set<std::string> uniqueProjects;

    for (const auto& event : events) {
        if (!event.projectId.empty()) {
            uniqueProjects.insert(event.projectId);
        }
    }

    stats.totalProjects = uniqueProjects.size();

    return stats;
}

UsageStatistics AnalyticsMonitoringSystem::getSystemStatistics() const {
    UsageStatistics stats;
    stats.totalRequests = events.size();

    std::set<std::string> users;

    for (const auto& event : events) {
        users.insert(event.userId);
    }

    stats.totalUsers = users.size();
    stats.activeUsers = users.size();

    return stats;
}

void AnalyticsMonitoringSystem::recordError(
    const std::string& errorType,
    const std::string& errorMessage,
    const std::string& stackTrace,
    const std::string& userId,
    const std::string& projectId
) {
    ErrorRecord error;
    error.errorId = nextErrorId++;
    error.errorType = errorType;
    error.errorMessage = errorMessage;
    error.stackTrace = stackTrace;
    error.userId = userId;
    error.projectId = projectId;
    error.timestamp = std::time(nullptr);

    // Check if similar error exists
    for (auto& existing : errorRecords) {
        if (existing.errorType == errorType && existing.errorMessage == errorMessage) {
            existing.occurrenceCount++;
            return;
        }
    }

    errorRecords.push_back(error);
}

std::vector<ErrorRecord> AnalyticsMonitoringSystem::getErrors(
    std::optional<std::string> errorType,
    bool unresolvedOnly,
    int limit
) const {
    std::vector<ErrorRecord> result;

    for (const auto& error : errorRecords) {
        if (errorType.has_value() && error.errorType != errorType.value()) {
            continue;
        }

        if (unresolvedOnly && error.isResolved) {
            continue;
        }

        result.push_back(error);
    }

    std::sort(result.begin(), result.end(),
        [](const ErrorRecord& a, const ErrorRecord& b) {
            return a.timestamp > b.timestamp;
        });

    if (limit > 0 && result.size() > static_cast<size_t>(limit)) {
        result.resize(limit);
    }

    return result;
}

bool AnalyticsMonitoringSystem::resolveError(uint64_t errorId) {
    for (auto& error : errorRecords) {
        if (error.errorId == errorId) {
            error.isResolved = true;
            return true;
        }
    }

    return false;
}

int AnalyticsMonitoringSystem::getErrorRate() const {
    if (events.empty()) {
        return 0;
    }

    int totalErrors = 0;

    for (const auto& error : errorRecords) {
        totalErrors += error.occurrenceCount;
    }

    return static_cast<int>((totalErrors * 100) / events.size());
}

DashboardData AnalyticsMonitoringSystem::generateDashboard() const {
    DashboardData dashboard;

    std::set<std::string> uniqueUsers;

    for (const auto& event : events) {
        uniqueUsers.insert(event.userId);
    }

    dashboard.totalUsers = uniqueUsers.size();
    dashboard.activeUsers = uniqueUsers.size() / 2;

    std::set<std::string> uniqueProjects;

    for (const auto& event : events) {
        if (!event.projectId.empty()) {
            uniqueProjects.insert(event.projectId);
        }
    }

    dashboard.totalProjects = uniqueProjects.size();

    if (!performanceHistory.empty()) {
        dashboard.currentPerformance = performanceHistory.back();
        dashboard.avgResponseTimeMs = dashboard.currentPerformance.avgResponseTimeMs;
    }

    dashboard.errorRate = getErrorRate();

    auto errors = getErrors(std::nullopt, true, 5);
    dashboard.recentErrors = errors;

    for (size_t i = 0; i < std::min(metrics.size(), size_t(5)); ++i) {
        dashboard.topMetrics.push_back(metrics[i]);
    }

    return dashboard;
}

std::vector<Metric> AnalyticsMonitoringSystem::executeQuery(
    const AnalyticsQuery& query
) const {
    std::vector<Metric> result = getMetrics(
        query.metric,
        query.fromTimestamp,
        query.toTimestamp
    );

    return result;
}

void AnalyticsMonitoringSystem::setAlertThreshold(const AlertThreshold& threshold) {
    auto it = std::find_if(thresholds.begin(), thresholds.end(),
        [&threshold](const AlertThreshold& t) {
            return t.metricName == threshold.metricName;
        });

    if (it != thresholds.end()) {
        *it = threshold;
    } else {
        thresholds.push_back(threshold);
    }
}

std::vector<AnalyticsMonitoringSystem::AlertThreshold> AnalyticsMonitoringSystem::getAlertThresholds() const {
    return thresholds;
}

std::vector<AnalyticsMonitoringSystem::Alert> AnalyticsMonitoringSystem::getActiveAlerts() const {
    std::vector<Alert> activeAlerts;

    for (const auto& alert : alerts) {
        if (!alert.isAcknowledged) {
            activeAlerts.push_back(alert);
        }
    }

    return activeAlerts;
}

bool AnalyticsMonitoringSystem::acknowledgeAlert(uint64_t alertId) {
    for (auto& alert : alerts) {
        if (alert.alertId == alertId) {
            alert.isAcknowledged = true;
            return true;
        }
    }

    return false;
}

std::string AnalyticsMonitoringSystem::exportAnalyticsAsJson(
    int64_t fromTimestamp,
    int64_t toTimestamp
) const {
    std::ostringstream oss;
    oss << "{\"events\": [";

    for (size_t i = 0; i < events.size(); ++i) {
        const auto& event = events[i];

        if (fromTimestamp > 0 && event.timestamp < fromTimestamp) {
            continue;
        }

        if (toTimestamp > 0 && event.timestamp > toTimestamp) {
            continue;
        }

        if (i > 0) oss << ", ";

        oss << "{\"eventId\": " << event.eventId
            << ", \"userId\": \"" << event.userId
            << "\", \"timestamp\": " << event.timestamp << "}";
    }

    oss << "]}";
    return oss.str();
}

std::string AnalyticsMonitoringSystem::exportAnalyticsAsCsv(
    int64_t fromTimestamp,
    int64_t toTimestamp
) const {
    std::ostringstream oss;
    oss << "eventId,userId,projectId,timestamp,eventType\n";

    for (const auto& event : events) {
        if (fromTimestamp > 0 && event.timestamp < fromTimestamp) {
            continue;
        }

        if (toTimestamp > 0 && event.timestamp > toTimestamp) {
            continue;
        }

        oss << event.eventId << ","
            << event.userId << ","
            << event.projectId << ","
            << event.timestamp << ","
            << static_cast<int>(event.eventType) << "\n";
    }

    return oss.str();
}

AnalyticsMonitoringSystem::SystemHealth AnalyticsMonitoringSystem::checkSystemHealth() const {
    SystemHealth health;
    health.isHealthy = true;
    health.status = "healthy";
    health.lastCheckTimestamp = std::time(nullptr);

    if (!performanceHistory.empty()) {
        health.metrics = performanceHistory.back();

        if (health.metrics.cpuUsagePercent > 80) {
            health.isHealthy = false;
            health.status = "degraded";
            health.issues.push_back("High CPU usage");
        }

        if (health.metrics.memoryUsagePercent > 80) {
            health.isHealthy = false;
            health.status = "degraded";
            health.issues.push_back("High memory usage");
        }

        if (health.metrics.errorCount > 10) {
            health.isHealthy = false;
            health.status = "critical";
            health.issues.push_back("High error rate");
        }
    }

    return health;
}

int AnalyticsMonitoringSystem::pruneOldData(int64_t keepAfterTimestamp) {
    int removed = 0;

    auto it = std::remove_if(events.begin(), events.end(),
        [keepAfterTimestamp, &removed](const AnalyticsEvent& e) {
            if (e.timestamp < keepAfterTimestamp) {
                removed++;
                return true;
            }

            return false;
        });

    events.erase(it, events.end());

    return removed;
}

}  // namespace bendcore::cloud
