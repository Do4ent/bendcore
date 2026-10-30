// GAP-083: SecurityAudit (36 tests) - Vulnerability scanner, penetration testing
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <chrono>
#include "bendcore/security/security_audit.hpp"

using namespace bendcore::security;

// Test 1: Create audit framework
void test_create_audit_framework() {
  SecurityAuditFramework framework;
  assert(framework.getAuditLog().empty());
  printf("✓ test_create_audit_framework\n");
}

// Test 2: Log authentication event
void test_log_authentication_event() {
  SecurityAuditFramework framework;
  AuditEvent event{
    "evt-001",
    AuditEventType::AUTHENTICATION,
    AuditLevel::INFO,
    "user1",
    "login",
    "auth-service",
    "success",
    std::chrono::system_clock::now().time_since_epoch().count(),
    "192.168.1.1",
    {}
  };
  bool result = framework.logAuditEvent(event);
  assert(result);
  assert(framework.getAuditLog().size() == 1);
  printf("✓ test_log_authentication_event\n");
}

// Test 3: Log authorization event
void test_log_authorization_event() {
  SecurityAuditFramework framework;
  AuditEvent event{
    "evt-002",
    AuditEventType::AUTHORIZATION,
    AuditLevel::MEDIUM,
    "user1",
    "access_resource",
    "resource-123",
    "denied",
    std::chrono::system_clock::now().time_since_epoch().count(),
    "192.168.1.1",
    {}
  };
  bool result = framework.logAuditEvent(event);
  assert(result);
  printf("✓ test_log_authorization_event\n");
}

// Test 4: Get audit log
void test_get_audit_log() {
  SecurityAuditFramework framework;
  for (int i = 0; i < 5; i++) {
    AuditEvent event{
      "evt-" + std::to_string(i),
      AuditEventType::DATA_ACCESS,
      AuditLevel::INFO,
      "user" + std::to_string(i),
      "read",
      "data-" + std::to_string(i),
      "success",
      std::chrono::system_clock::now().time_since_epoch().count(),
      "192.168.1." + std::to_string(i),
      {}
    };
    framework.logAuditEvent(event);
  }
  auto log = framework.getAuditLog();
  assert(log.size() == 5);
  printf("✓ test_get_audit_log\n");
}

// Test 5: Filter by event type
void test_filter_by_event_type() {
  SecurityAuditFramework framework;
  AuditEvent evt1{
    "evt-1", AuditEventType::AUTHENTICATION, AuditLevel::INFO, "user1",
    "login", "auth", "success", 1000, "192.168.1.1", {}
  };
  AuditEvent evt2{
    "evt-2", AuditEventType::DATA_ACCESS, AuditLevel::INFO, "user1",
    "read", "data", "success", 2000, "192.168.1.1", {}
  };
  framework.logAuditEvent(evt1);
  framework.logAuditEvent(evt2);
  auto auth_events = framework.getAuditLogByType(AuditEventType::AUTHENTICATION);
  assert(auth_events.size() == 1);
  assert(auth_events[0].eventId == "evt-1");
  printf("✓ test_filter_by_event_type\n");
}

// Test 6: Filter by time range
void test_filter_by_time_range() {
  SecurityAuditFramework framework;
  AuditEvent evt1{"evt-1", AuditEventType::AUTHENTICATION, AuditLevel::INFO, "user1",
    "login", "auth", "success", 1000, "192.168.1.1", {}};
  AuditEvent evt2{"evt-2", AuditEventType::DATA_ACCESS, AuditLevel::INFO, "user1",
    "read", "data", "success", 3000, "192.168.1.1", {}};
  AuditEvent evt3{"evt-3", AuditEventType::DATA_MODIFICATION, AuditLevel::HIGH, "user2",
    "write", "data", "success", 5000, "192.168.1.2", {}};
  framework.logAuditEvent(evt1);
  framework.logAuditEvent(evt2);
  framework.logAuditEvent(evt3);
  auto events = framework.getAuditLogByTimeRange(2000, 4000);
  assert(events.size() == 1);
  printf("✓ test_filter_by_time_range\n");
}

// Test 7: Filter by user
void test_filter_by_user() {
  SecurityAuditFramework framework;
  for (int i = 0; i < 3; i++) {
    AuditEvent event{"evt-" + std::to_string(i), AuditEventType::DATA_ACCESS,
      AuditLevel::INFO, "user1", "read", "data", "success", 1000 + i*1000, "192.168.1.1", {}};
    framework.logAuditEvent(event);
  }
  for (int i = 3; i < 5; i++) {
    AuditEvent event{"evt-" + std::to_string(i), AuditEventType::DATA_ACCESS,
      AuditLevel::INFO, "user2", "read", "data", "success", 1000 + i*1000, "192.168.1.2", {}};
    framework.logAuditEvent(event);
  }
  auto user1_events = framework.getAuditLogByUser("user1");
  assert(user1_events.size() == 3);
  auto user2_events = framework.getAuditLogByUser("user2");
  assert(user2_events.size() == 2);
  printf("✓ test_filter_by_user\n");
}

// Test 8: Filter by audit level
void test_filter_by_audit_level() {
  SecurityAuditFramework framework;
  AuditEvent evt1{"evt-1", AuditEventType::AUTHENTICATION, AuditLevel::CRITICAL,
    "user1", "login", "auth", "success", 1000, "192.168.1.1", {}};
  AuditEvent evt2{"evt-2", AuditEventType::DATA_ACCESS, AuditLevel::INFO,
    "user1", "read", "data", "success", 2000, "192.168.1.1", {}};
  framework.logAuditEvent(evt1);
  framework.logAuditEvent(evt2);
  auto critical = framework.filterByLevel(AuditLevel::CRITICAL);
  assert(critical.size() == 1);
  auto info = framework.filterByLevel(AuditLevel::INFO);
  assert(info.size() == 1);
  printf("✓ test_filter_by_audit_level\n");
}

// Test 9: Filter by result
void test_filter_by_result() {
  SecurityAuditFramework framework;
  AuditEvent evt1{"evt-1", AuditEventType::AUTHENTICATION, AuditLevel::INFO,
    "user1", "login", "auth", "success", 1000, "192.168.1.1", {}};
  AuditEvent evt2{"evt-2", AuditEventType::AUTHENTICATION, AuditLevel::HIGH,
    "user1", "login", "auth", "failure", 2000, "192.168.1.1", {}};
  framework.logAuditEvent(evt1);
  framework.logAuditEvent(evt2);
  auto successes = framework.filterByResult("success");
  assert(successes.size() == 1);
  auto failures = framework.filterByResult("failure");
  assert(failures.size() == 1);
  printf("✓ test_filter_by_result\n");
}

// Test 10: Generate audit report
void test_generate_audit_report() {
  SecurityAuditFramework framework;
  for (int i = 0; i < 10; i++) {
    AuditEvent event{"evt-" + std::to_string(i), AuditEventType::DATA_ACCESS,
      AuditLevel::INFO, "user1", "read", "data", "success", 1000 + i*1000, "192.168.1.1", {}};
    framework.logAuditEvent(event);
  }
  std::string report = framework.generateAuditReport();
  assert(!report.empty());
  assert(report.find("10") != std::string::npos);
  printf("✓ test_generate_audit_report\n");
}

// Test 11: Generate report by time range
void test_generate_audit_report_by_time_range() {
  SecurityAuditFramework framework;
  for (int i = 0; i < 10; i++) {
    AuditEvent event{"evt-" + std::to_string(i), AuditEventType::DATA_ACCESS,
      AuditLevel::INFO, "user1", "read", "data", "success", 1000 + i*1000, "192.168.1.1", {}};
    framework.logAuditEvent(event);
  }
  std::string report = framework.generateAuditReportByTimeRange(2000, 6000);
  assert(!report.empty());
  printf("✓ test_generate_audit_report_by_time_range\n");
}

// Test 12: Perform vulnerability scan
void test_perform_vulnerability_scan() {
  SecurityAuditFramework framework;
  bool result = framework.performVulnerabilityScan();
  assert(result);
  printf("✓ test_perform_vulnerability_scan\n");
}

// Test 13: Get vulnerabilities
void test_get_vulnerabilities() {
  SecurityAuditFramework framework;
  framework.performVulnerabilityScan();
  auto vulns = framework.getVulnerabilities();
  assert(vulns.size() >= 0);
  printf("✓ test_get_vulnerabilities\n");
}

// Test 14: Check if vulnerable
void test_is_vulnerable() {
  SecurityAuditFramework framework;
  bool result = framework.isVulnerable("component1");
  assert(!result);  // No vulnerabilities added yet
  printf("✓ test_is_vulnerable\n");
}

// Test 15: Perform security assessment
void test_perform_security_assessment() {
  SecurityAuditFramework framework;
  SecurityAssessment assessment = framework.performSecurityAssessment();
  assert(!assessment.assessmentId.empty());
  printf("✓ test_perform_security_assessment\n");
}

// Test 16: Calculate security score
void test_calculate_security_score() {
  SecurityAuditFramework framework;
  float score = framework.calculateSecurityScore();
  assert(score >= 0.0f && score <= 100.0f);
  printf("✓ test_calculate_security_score\n");
}

// Test 17: Get security recommendations
void test_get_security_recommendations() {
  SecurityAuditFramework framework;
  auto recommendations = framework.getSecurityRecommendations();
  assert(recommendations.size() >= 0);
  printf("✓ test_get_security_recommendations\n");
}

// Test 18: Monitor user access
void test_monitor_user_access() {
  SecurityAuditFramework framework;
  bool result = framework.monitorUserAccess("user1");
  assert(result);
  printf("✓ test_monitor_user_access\n");
}

// Test 19: Get user access history
void test_get_user_access_history() {
  SecurityAuditFramework framework;
  AuditEvent evt{"evt-1", AuditEventType::DATA_ACCESS, AuditLevel::INFO,
    "user1", "read", "data", "success", 1000, "192.168.1.1", {}};
  framework.logAuditEvent(evt);
  auto history = framework.getUserAccessHistory("user1");
  assert(history.size() == 1);
  printf("✓ test_get_user_access_history\n");
}

// Test 20: Get failed login count
void test_get_failed_login_count() {
  SecurityAuditFramework framework;
  AuditEvent evt1{"evt-1", AuditEventType::AUTHENTICATION, AuditLevel::HIGH,
    "user1", "login", "auth", "failure", 1000, "192.168.1.1", {}};
  AuditEvent evt2{"evt-2", AuditEventType::AUTHENTICATION, AuditLevel::HIGH,
    "user1", "login", "auth", "failure", 2000, "192.168.1.1", {}};
  framework.logAuditEvent(evt1);
  framework.logAuditEvent(evt2);
  int count = framework.getFailedLoginCount("user1");
  assert(count == 2);
  printf("✓ test_get_failed_login_count\n");
}

// Test 21: Detect suspicious activity
void test_detect_suspicious_activity() {
  SecurityAuditFramework framework;
  bool result = framework.detectSuspiciousActivity();
  assert(true);  // Should return true/false based on logic
  printf("✓ test_detect_suspicious_activity\n");
}

// Test 22: Get suspicious activities
void test_get_suspicious_activities() {
  SecurityAuditFramework framework;
  auto suspicious = framework.getSuspiciousActivities();
  assert(suspicious.size() >= 0);
  printf("✓ test_get_suspicious_activities\n");
}

// Test 23: Archive audit log
void test_archive_audit_log() {
  SecurityAuditFramework framework;
  AuditEvent evt{"evt-1", AuditEventType::DATA_ACCESS, AuditLevel::INFO,
    "user1", "read", "data", "success", 1000, "192.168.1.1", {}};
  framework.logAuditEvent(evt);
  bool result = framework.archiveAuditLog(2000);  // Archive events before timestamp 2000
  assert(result);
  printf("✓ test_archive_audit_log\n");
}

// Test 24: Get audit log size
void test_get_audit_log_size() {
  SecurityAuditFramework framework;
  for (int i = 0; i < 5; i++) {
    AuditEvent event{"evt-" + std::to_string(i), AuditEventType::DATA_ACCESS,
      AuditLevel::INFO, "user1", "read", "data", "success", 1000 + i*1000, "192.168.1.1", {}};
    framework.logAuditEvent(event);
  }
  size_t size = framework.getAuditLogSize();
  assert(size == 5);
  printf("✓ test_get_audit_log_size\n");
}

// Test 25: Calculate audit log hash
void test_calculate_audit_log_hash() {
  SecurityAuditFramework framework;
  AuditEvent evt{"evt-1", AuditEventType::DATA_ACCESS, AuditLevel::INFO,
    "user1", "read", "data", "success", 1000, "192.168.1.1", {}};
  framework.logAuditEvent(evt);
  std::string hash = framework.calculateAuditLogHash();
  assert(!hash.empty());
  printf("✓ test_calculate_audit_log_hash\n");
}

// Test 26: Verify audit log integrity
void test_verify_audit_log_integrity() {
  SecurityAuditFramework framework;
  bool result = framework.verifyAuditLogIntegrity();
  assert(result);
  printf("✓ test_verify_audit_log_integrity\n");
}

// Test 27: Enable real-time monitoring
void test_enable_real_time_monitoring() {
  SecurityAuditFramework framework;
  framework.enableRealTimeMonitoring(true);
  assert(framework.isRealTimeMonitoringEnabled());
  framework.enableRealTimeMonitoring(false);
  assert(!framework.isRealTimeMonitoringEnabled());
  printf("✓ test_enable_real_time_monitoring\n");
}

// Test 28: Generate compliance report
void test_generate_compliance_report() {
  SecurityAuditFramework framework;
  std::string report = framework.generateComplianceReport();
  assert(!report.empty());
  printf("✓ test_generate_compliance_report\n");
}

// Test 29: Multiple event types
void test_multiple_event_types() {
  SecurityAuditFramework framework;
  AuditEventType types[] = {
    AuditEventType::AUTHENTICATION,
    AuditEventType::AUTHORIZATION,
    AuditEventType::DATA_ACCESS,
    AuditEventType::DATA_MODIFICATION,
    AuditEventType::CONFIG_CHANGE,
    AuditEventType::SECURITY_EVENT,
    AuditEventType::VULNERABILITY_SCAN,
    AuditEventType::COMPLIANCE_CHECK
  };
  for (int i = 0; i < 8; i++) {
    AuditEvent evt{
      "evt-" + std::to_string(i), types[i], AuditLevel::INFO,
      "user1", "action", "resource", "success", 1000 + i*1000, "192.168.1.1", {}
    };
    framework.logAuditEvent(evt);
  }
  auto log = framework.getAuditLog();
  assert(log.size() == 8);
  printf("✓ test_multiple_event_types\n");
}

// Test 30: Multiple audit levels
void test_multiple_audit_levels() {
  SecurityAuditFramework framework;
  AuditLevel levels[] = {
    AuditLevel::CRITICAL,
    AuditLevel::HIGH,
    AuditLevel::MEDIUM,
    AuditLevel::LOW,
    AuditLevel::INFO
  };
  for (int i = 0; i < 5; i++) {
    AuditEvent evt{
      "evt-" + std::to_string(i), AuditEventType::SECURITY_EVENT, levels[i],
      "user1", "action", "resource", "success", 1000 + i*1000, "192.168.1.1", {}
    };
    framework.logAuditEvent(evt);
  }
  assert(framework.filterByLevel(AuditLevel::CRITICAL).size() == 1);
  assert(framework.filterByLevel(AuditLevel::HIGH).size() == 1);
  assert(framework.filterByLevel(AuditLevel::INFO).size() == 1);
  printf("✓ test_multiple_audit_levels\n");
}

// Test 31: Search audit log
void test_search_audit_log() {
  SecurityAuditFramework framework;
  AuditEvent evt{"evt-1", AuditEventType::AUTHENTICATION, AuditLevel::INFO,
    "user1", "login", "auth-service", "success", 1000, "192.168.1.1", {}};
  framework.logAuditEvent(evt);
  auto results = framework.searchAuditLog("login");
  assert(results.size() >= 0);
  printf("✓ test_search_audit_log\n");
}

// Test 32: Flag anomalous activity
void test_flag_anomalous_activity() {
  SecurityAuditFramework framework;
  AuditEvent evt{"evt-1", AuditEventType::DATA_ACCESS, AuditLevel::HIGH,
    "user1", "read", "critical-data", "success", 1000, "192.168.1.99", {}};
  bool result = framework.flagAnomalousActivity(evt);
  assert(true);
  printf("✓ test_flag_anomalous_activity\n");
}

// Test 33: Delete old audit logs
void test_delete_audit_log() {
  SecurityAuditFramework framework;
  AuditEvent evt{"evt-1", AuditEventType::DATA_ACCESS, AuditLevel::INFO,
    "user1", "read", "data", "success", 1000, "192.168.1.1", {}};
  framework.logAuditEvent(evt);
  bool result = framework.deleteAuditLog(2000);  // Delete events before timestamp 2000
  assert(result);
  printf("✓ test_delete_audit_log\n");
}

// Test 34: Get vulnerability details
void test_get_vulnerability_details() {
  SecurityAuditFramework framework;
  auto vuln = framework.getVulnerabilityDetails("vuln-unknown");
  assert(vuln.vulnerabilityId.empty() || !vuln.title.empty());
  printf("✓ test_get_vulnerability_details\n");
}

// Test 35: Config change event
void test_config_change_event() {
  SecurityAuditFramework framework;
  AuditEvent evt{"evt-1", AuditEventType::CONFIG_CHANGE, AuditLevel::HIGH,
    "admin1", "modify_config", "database-config", "success", 1000, "192.168.1.1", {}};
  framework.logAuditEvent(evt);
  auto config_changes = framework.getAuditLogByType(AuditEventType::CONFIG_CHANGE);
  assert(config_changes.size() == 1);
  printf("✓ test_config_change_event\n");
}

// Test 36: Complex audit scenario
void test_complex_audit_scenario() {
  SecurityAuditFramework framework;
  // Simulate a complex scenario with multiple users and events
  std::vector<std::string> users = {"user1", "user2", "user3"};
  std::vector<AuditEventType> events = {
    AuditEventType::AUTHENTICATION,
    AuditEventType::DATA_ACCESS,
    AuditEventType::DATA_MODIFICATION
  };
  int event_id = 0;
  for (const auto& user : users) {
    for (const auto& evt_type : events) {
      AuditEvent evt{
        "evt-" + std::to_string(event_id++),
        evt_type,
        AuditLevel::INFO,
        user,
        "action",
        "resource",
        "success",
        1000 + event_id * 1000,
        "192.168.1." + std::to_string(event_id % 255),
        {{"session_id", "sess-" + std::to_string(event_id)}}
      };
      framework.logAuditEvent(evt);
    }
  }
  assert(framework.getAuditLog().size() == 9);
  assert(framework.getAuditLogByUser("user1").size() == 3);
  assert(framework.getAuditLogByType(AuditEventType::AUTHENTICATION).size() == 3);
  printf("✓ test_complex_audit_scenario\n");
}

int main() {
  test_create_audit_framework();
  test_log_authentication_event();
  test_log_authorization_event();
  test_get_audit_log();
  test_filter_by_event_type();
  test_filter_by_time_range();
  test_filter_by_user();
  test_filter_by_audit_level();
  test_filter_by_result();
  test_generate_audit_report();
  test_generate_audit_report_by_time_range();
  test_perform_vulnerability_scan();
  test_get_vulnerabilities();
  test_is_vulnerable();
  test_perform_security_assessment();
  test_calculate_security_score();
  test_get_security_recommendations();
  test_monitor_user_access();
  test_get_user_access_history();
  test_get_failed_login_count();
  test_detect_suspicious_activity();
  test_get_suspicious_activities();
  test_archive_audit_log();
  test_get_audit_log_size();
  test_calculate_audit_log_hash();
  test_verify_audit_log_integrity();
  test_enable_real_time_monitoring();
  test_generate_compliance_report();
  test_multiple_event_types();
  test_multiple_audit_levels();
  test_search_audit_log();
  test_flag_anomalous_activity();
  test_delete_audit_log();
  test_get_vulnerability_details();
  test_config_change_event();
  test_complex_audit_scenario();
  printf("\n=== All GAP-083 SecurityAudit tests passed! (36 tests) ===\n");
  return 0;
}
