#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace bendcore::security {

// ==============================================================================
// SECURITY AUDIT - GAP-083
// ==============================================================================
// Audit framework, event logging, security assessments, vulnerability scanning

enum class AuditLevel {
    CRITICAL,
    HIGH,
    MEDIUM,
    LOW,
    INFO
};

enum class AuditEventType {
    AUTHENTICATION,
    AUTHORIZATION,
    DATA_ACCESS,
    DATA_MODIFICATION,
    CONFIG_CHANGE,
    SECURITY_EVENT,
    VULNERABILITY_SCAN,
    COMPLIANCE_CHECK
};

struct AuditEvent {
    std::string eventId;
    AuditEventType type;
    AuditLevel level;
    std::string userId;
    std::string action;
    std::string resource;
    std::string result;  // "success", "failure", "denied"
    int64_t timestamp;
    std::string ipAddress;
    std::map<std::string, std::string> metadata;
};

struct VulnerabilityReport {
    std::string vulnerabilityId;
    std::string title;
    std::string description;
    AuditLevel severity;
    std::string affectedComponent;
    std::string cveId;
    bool isPatched;
    std::string remediationAdvice;
};

struct SecurityAssessment {
    std::string assessmentId;
    int64_t timestamp;
    float securityScore;
    std::vector<VulnerabilityReport> vulnerabilities;
    std::vector<std::string> recommendations;
};

class SecurityAuditFramework {
public:
    SecurityAuditFramework();
    ~SecurityAuditFramework();

    // Audit logging
    bool logAuditEvent(const AuditEvent& event);
    std::vector<AuditEvent> getAuditLog();
    std::vector<AuditEvent> getAuditLogByType(AuditEventType type);
    std::vector<AuditEvent> getAuditLogByTimeRange(int64_t startTime, int64_t endTime);
    std::vector<AuditEvent> getAuditLogByUser(const std::string& userId);

    // Audit filtering and searching
    std::vector<AuditEvent> searchAuditLog(const std::string& query);
    std::vector<AuditEvent> filterByLevel(AuditLevel level);
    std::vector<AuditEvent> filterByResult(const std::string& result);

    // Audit report generation
    std::string generateAuditReport();
    std::string generateAuditReportByTimeRange(int64_t startTime, int64_t endTime);
    std::string generateComplianceReport();

    // Vulnerability scanning
    bool performVulnerabilityScan();
    std::vector<VulnerabilityReport> getVulnerabilities();
    bool isVulnerable(const std::string& componentName);
    VulnerabilityReport getVulnerabilityDetails(const std::string& vulnerabilityId);

    // Security assessment
    SecurityAssessment performSecurityAssessment();
    float calculateSecurityScore();
    std::vector<std::string> getSecurityRecommendations();

    // Access monitoring
    bool monitorUserAccess(const std::string& userId);
    std::vector<AuditEvent> getUserAccessHistory(const std::string& userId);
    int32_t getFailedLoginCount(const std::string& userId);

    // Suspicious activity detection
    bool detectSuspiciousActivity();
    std::vector<AuditEvent> getSuspiciousActivities();
    bool flagAnomalousActivity(const AuditEvent& event);

    // Audit log management
    bool archiveAuditLog(int64_t beforeTimestamp);
    bool deleteAuditLog(int64_t beforeTimestamp);
    size_t getAuditLogSize() const;

    // Integrity verification
    std::string calculateAuditLogHash();
    bool verifyAuditLogIntegrity();

    // Real-time monitoring
    void enableRealTimeMonitoring(bool enable);
    bool isRealTimeMonitoringEnabled() const;

private:
    std::vector<AuditEvent> auditLog;
    std::vector<VulnerabilityReport> vulnerabilities;
    std::vector<SecurityAssessment> assessments;
    bool realTimeMonitoringEnabled;
};

}  // namespace bendcore::security
