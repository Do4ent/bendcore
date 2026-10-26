#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace bendcore::security {

// ==============================================================================
// ACCESS LOGGING - GAP-086
// ==============================================================================
// Detailed audit trails, access logs, activity tracking, forensics

enum class AccessType {
    READ,
    WRITE,
    DELETE,
    EXECUTE,
    ADMIN
};

enum class AccessResult {
    ALLOWED,
    DENIED,
    REVOKED,
    EXPIRED
};

struct AccessLog {
    std::string logId;
    std::string userId;
    std::string resourceId;
    AccessType accessType;
    AccessResult result;
    std::string reason;
    int64_t timestamp;
    std::string ipAddress;
    std::string userAgent;
    std::map<std::string, std::string> contextData;
};

struct AccessPattern {
    std::string userId;
    AccessType accessType;
    std::string resourceType;
    int32_t frequency;
    int64_t firstSeen;
    int64_t lastSeen;
    bool isAnomaly;
};

class AccessLoggingSystem {
public:
    AccessLoggingSystem();
    ~AccessLoggingSystem();

    // Access logging
    bool logAccess(const AccessLog& log);
    std::vector<AccessLog> getAccessLog();
    std::vector<AccessLog> getAccessLogByUser(const std::string& userId);
    std::vector<AccessLog> getAccessLogByResource(const std::string& resourceId);
    std::vector<AccessLog> getAccessLogByTimeRange(int64_t startTime, int64_t endTime);

    // Access filtering
    std::vector<AccessLog> filterByAccessType(AccessType type);
    std::vector<AccessLog> filterByResult(AccessResult result);
    std::vector<AccessLog> filterByIPAddress(const std::string& ipAddress);

    // Access pattern analysis
    std::vector<AccessPattern> getAccessPatterns();
    AccessPattern getAccessPattern(const std::string& userId);
    bool detectAnomalousAccess(const AccessLog& log);
    std::vector<AccessLog> getAnomalousAccesses();

    // User activity tracking
    std::vector<AccessLog> getUserActivity(const std::string& userId);
    int32_t getAccessCountByUser(const std::string& userId);
    int32_t getDeniedAccessCount(const std::string& userId);
    std::map<std::string, int32_t> getResourceAccessCount();

    // Resource access tracking
    std::vector<AccessLog> getResourceAccesses(const std::string& resourceId);
    std::vector<std::string> getAccessorsForResource(const std::string& resourceId);
    int32_t getTotalAccessCount(const std::string& resourceId);

    // Privilege escalation detection
    bool detectPrivilegeEscalation();
    std::vector<AccessLog> getPrivilegeEscalationAttempts();
    bool flagSuspiciousEscalation(const AccessLog& log);

    // Forensic analysis
    std::string generateForensicReport();
    std::string generateForensicReportByUser(const std::string& userId);
    std::string generateForensicReportByResource(const std::string& resourceId);
    std::vector<AccessLog> reconstructAccessSequence(const std::string& userId);

    // Log export and archiving
    bool exportAccessLog(const std::string& outputPath);
    bool archiveAccessLog(int64_t beforeTimestamp);
    size_t getAccessLogSize() const;
    bool deleteOldLogs(int64_t beforeTimestamp);

    // Integrity and tamper protection
    std::string calculateLogHash();
    bool verifyLogIntegrity();
    bool enableLogTamperDetection(bool enable);
    bool isLogTamperDetectionEnabled() const;

    // Real-time monitoring
    bool enableRealTimeMonitoring(bool enable);
    bool isRealTimeMonitoringEnabled() const;
    std::vector<AccessLog> getRecentAccesses(int32_t count);

    // Statistics
    int32_t getTotalAccessCount() const;
    int32_t getAllowedAccessCount() const;
    int32_t getDeniedAccessCount() const;
    float getDenialRate() const;

private:
    std::vector<AccessLog> accessLog;
    std::vector<AccessPattern> patterns;
    bool realTimeMonitoringEnabled;
    bool tamperDetectionEnabled;
};

}  // namespace bendcore::security
