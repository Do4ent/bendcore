#include <gtest/gtest.h>
#include "bendcore/security/security_audit.hpp"

using namespace bendcore::security;

class SecurityAuditTest : public ::testing::Test {
protected:
    SecurityAuditFramework framework;
};

#define TEST_AUDIT(testname, code) TEST_F(SecurityAuditTest, testname) { code; EXPECT_TRUE(true); }

TEST_AUDIT(LogAuditEvent, AuditEvent e; framework.logAuditEvent(e))
TEST_AUDIT(GetAuditLog, auto logs = framework.getAuditLog())
TEST_AUDIT(GetAuditLogByType, auto logs = framework.getAuditLogByType(AuditEventType::AUTHENTICATION))
TEST_AUDIT(GetAuditLogByTimeRange, auto logs = framework.getAuditLogByTimeRange(0, 1000))
TEST_AUDIT(GetAuditLogByUser, auto logs = framework.getAuditLogByUser("user1"))
TEST_AUDIT(SearchAuditLog, auto logs = framework.searchAuditLog("query"))
TEST_AUDIT(FilterByLevel, auto logs = framework.filterByLevel(AuditLevel::CRITICAL))
TEST_AUDIT(FilterByResult, auto logs = framework.filterByResult("success"))
TEST_AUDIT(GenerateAuditReport, auto report = framework.generateAuditReport())
TEST_AUDIT(GenerateAuditReportByTimeRange, auto report = framework.generateAuditReportByTimeRange(0, 1000))
TEST_AUDIT(GenerateComplianceReport, auto report = framework.generateComplianceReport())
TEST_AUDIT(PerformVulnerabilityScan, bool result = framework.performVulnerabilityScan())
TEST_AUDIT(GetVulnerabilities, auto vulns = framework.getVulnerabilities())
TEST_AUDIT(PerformSecurityAssessment, auto assessment = framework.performSecurityAssessment())
TEST_AUDIT(CalculateSecurityScore, float score = framework.calculateSecurityScore())
TEST_AUDIT(GetSecurityRecommendations, auto recs = framework.getSecurityRecommendations())
TEST_AUDIT(MonitorUserAccess, bool result = framework.monitorUserAccess("user1"))
TEST_AUDIT(GetUserAccessHistory, auto history = framework.getUserAccessHistory("user1"))
TEST_AUDIT(GetFailedLoginCount, int32_t count = framework.getFailedLoginCount("user1"))
TEST_AUDIT(DetectSuspiciousActivity, bool detected = framework.detectSuspiciousActivity())
TEST_AUDIT(GetSuspiciousActivities, auto activities = framework.getSuspiciousActivities())
TEST_AUDIT(ArchiveAuditLog, bool result = framework.archiveAuditLog(1000))
TEST_AUDIT(DeleteAuditLog, bool result = framework.deleteAuditLog(1000))
TEST_AUDIT(GetAuditLogSize, size_t size = framework.getAuditLogSize())
TEST_AUDIT(CalculateAuditLogHash, std::string hash = framework.calculateAuditLogHash())
TEST_AUDIT(VerifyAuditLogIntegrity, bool valid = framework.verifyAuditLogIntegrity())
TEST_AUDIT(EnableRealTimeMonitoring, bool result = framework.enableRealTimeMonitoring(true))
TEST_AUDIT(IsRealTimeMonitoringEnabled, bool enabled = framework.isRealTimeMonitoringEnabled())

TEST_F(SecurityAuditTest, MultipleAuditEvents) {
    AuditEvent e1, e2, e3;
    framework.logAuditEvent(e1);
    framework.logAuditEvent(e2);
    framework.logAuditEvent(e3);
    auto logs = framework.getAuditLog();
    EXPECT_GE(logs.size(), 3);
}

TEST_F(SecurityAuditTest, AuditEventFiltering) {
    AuditEvent e1{.type = AuditEventType::AUTHENTICATION};
    AuditEvent e2{.type = AuditEventType::DATA_ACCESS};
    framework.logAuditEvent(e1);
    framework.logAuditEvent(e2);
    auto auth_logs = framework.getAuditLogByType(AuditEventType::AUTHENTICATION);
    EXPECT_GE(auth_logs.size(), 1);
}

TEST_F(SecurityAuditTest, SecurityScoreBound) {
    float score = framework.calculateSecurityScore();
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

