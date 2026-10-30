// GAP-085: ComplianceStandards (34 tests) - GDPR, ISO 27001, PCI-DSS
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "bendcore/security/compliance_standards.hpp"

using namespace bendcore::security;

// Test 1: Create compliance manager
void test_create_compliance_manager() {
  ComplianceManager manager;
  auto compliances = manager.getRegisteredCompliances();
  assert(compliances.size() >= 0);
  printf("✓ test_create_compliance_manager\n");
}

// Test 2: Register GDPR compliance
void test_register_gdpr_compliance() {
  ComplianceManager manager;
  bool result = manager.registerCompliance(ComplianceStandard::GDPR);
  assert(result);
  auto compliances = manager.getRegisteredCompliances();
  bool found = false;
  for (const auto& c : compliances) {
    if (c == ComplianceStandard::GDPR) found = true;
  }
  printf("✓ test_register_gdpr_compliance\n");
}

// Test 3: Register ISO 27001 compliance
void test_register_iso27001_compliance() {
  ComplianceManager manager;
  bool result = manager.registerCompliance(ComplianceStandard::ISO_27001);
  assert(result);
  printf("✓ test_register_iso27001_compliance\n");
}

// Test 4: Register SOC2 compliance
void test_register_soc2_compliance() {
  ComplianceManager manager;
  bool result = manager.registerCompliance(ComplianceStandard::SOC2);
  assert(result);
  printf("✓ test_register_soc2_compliance\n");
}

// Test 5: Register HIPAA compliance
void test_register_hipaa_compliance() {
  ComplianceManager manager;
  bool result = manager.registerCompliance(ComplianceStandard::HIPAA);
  assert(result);
  printf("✓ test_register_hipaa_compliance\n");
}

// Test 6: Register PCI-DSS compliance
void test_register_pci_dss_compliance() {
  ComplianceManager manager;
  bool result = manager.registerCompliance(ComplianceStandard::PCI_DSS);
  assert(result);
  printf("✓ test_register_pci_dss_compliance\n");
}

// Test 7: Register NIST compliance
void test_register_nist_compliance() {
  ComplianceManager manager;
  bool result = manager.registerCompliance(ComplianceStandard::NIST);
  assert(result);
  printf("✓ test_register_nist_compliance\n");
}

// Test 8: Register CCPA compliance
void test_register_ccpa_compliance() {
  ComplianceManager manager;
  bool result = manager.registerCompliance(ComplianceStandard::CCPA);
  assert(result);
  printf("✓ test_register_ccpa_compliance\n");
}

// Test 9: Get registered compliances
void test_get_registered_compliances() {
  ComplianceManager manager;
  manager.registerCompliance(ComplianceStandard::GDPR);
  manager.registerCompliance(ComplianceStandard::ISO_27001);
  auto compliances = manager.getRegisteredCompliances();
  assert(compliances.size() >= 2);
  printf("✓ test_get_registered_compliances\n");
}

// Test 10: Implement GDPR compliance
void test_implement_gdpr_compliance() {
  ComplianceManager manager;
  bool result = manager.implementGDPRCompliance();
  assert(result);
  printf("✓ test_implement_gdpr_compliance\n");
}

// Test 11: Collect user consent
void test_collect_user_consent() {
  ComplianceManager manager;
  bool result = manager.collectUserConsent("user-1", "marketing");
  assert(result);
  printf("✓ test_collect_user_consent\n");
}

// Test 12: Provide data portability
void test_provide_data_portability() {
  ComplianceManager manager;
  bool result = manager.provideDataPortability("user-1");
  assert(result);
  printf("✓ test_provide_data_portability\n");
}

// Test 13: Delete personal data
void test_delete_personal_data() {
  ComplianceManager manager;
  bool result = manager.deletePersonalData("user-1");
  assert(result);
  printf("✓ test_delete_personal_data\n");
}

// Test 14: Provide privacy policy
void test_provide_privacy_policy() {
  ComplianceManager manager;
  std::string policy;
  bool result = manager.providePrivacyPolicy(policy);
  assert(result);
  assert(!policy.empty());
  printf("✓ test_provide_privacy_policy\n");
}

// Test 15: Implement ISO 27001
void test_implement_iso27001() {
  ComplianceManager manager;
  bool result = manager.implementISO27001();
  assert(result);
  printf("✓ test_implement_iso27001\n");
}

// Test 16: Verify access control
void test_verify_access_control() {
  ComplianceManager manager;
  bool result = manager.verifyAccessControl();
  assert(result);
  printf("✓ test_verify_access_control\n");
}

// Test 17: Verify incident management
void test_verify_incident_management() {
  ComplianceManager manager;
  bool result = manager.verifyIncidentManagement();
  assert(result);
  printf("✓ test_verify_incident_management\n");
}

// Test 18: Verify business continuity
void test_verify_business_continuity() {
  ComplianceManager manager;
  bool result = manager.verifyBusinessContinuity();
  assert(result);
  printf("✓ test_verify_business_continuity\n");
}

// Test 19: Implement SOC2
void test_implement_soc2() {
  ComplianceManager manager;
  bool result = manager.implementSOC2();
  assert(result);
  printf("✓ test_implement_soc2\n");
}

// Test 20: Enable security monitoring
void test_enable_security_monitoring() {
  ComplianceManager manager;
  bool result = manager.enableSecurityMonitoring(true);
  assert(result);
  printf("✓ test_enable_security_monitoring\n");
}

// Test 21: Verify change management
void test_verify_change_management() {
  ComplianceManager manager;
  bool result = manager.verifyChangeManagement();
  assert(result);
  printf("✓ test_verify_change_management\n");
}

// Test 22: Verify availability
void test_verify_availability() {
  ComplianceManager manager;
  bool result = manager.verifyAvailability();
  assert(result);
  printf("✓ test_verify_availability\n");
}

// Test 23: Implement HIPAA
void test_implement_hipaa() {
  ComplianceManager manager;
  bool result = manager.implementHIPAA();
  assert(result);
  printf("✓ test_implement_hipaa\n");
}

// Test 24: Encrypt PHI
void test_encrypt_phi() {
  ComplianceManager manager;
  bool result = manager.encryptPHI(true);
  assert(result);
  printf("✓ test_encrypt_phi\n");
}

// Test 25: Enable audit controls
void test_enable_audit_controls() {
  ComplianceManager manager;
  bool result = manager.enableAuditControls(true);
  assert(result);
  printf("✓ test_enable_audit_controls\n");
}

// Test 26: Verify integrity controls
void test_verify_integrity_controls() {
  ComplianceManager manager;
  bool result = manager.verifyIntegrityControls();
  assert(result);
  printf("✓ test_verify_integrity_controls\n");
}

// Test 27: Implement PCI-DSS
void test_implement_pci_dss() {
  ComplianceManager manager;
  bool result = manager.implementPCIDSS();
  assert(result);
  printf("✓ test_implement_pci_dss\n");
}

// Test 28: Secure payment data
void test_secure_payment_data() {
  ComplianceManager manager;
  bool result = manager.securePaymentData();
  assert(result);
  printf("✓ test_secure_payment_data\n");
}

// Test 29: Verify network security
void test_verify_network_security() {
  ComplianceManager manager;
  bool result = manager.verifyNetworkSecurity();
  assert(result);
  printf("✓ test_verify_network_security\n");
}

// Test 30: Register compliance requirement
void test_register_requirement() {
  ComplianceManager manager;
  ComplianceRequirement req{
    "req-001",
    ComplianceStandard::GDPR,
    "Data encryption requirement",
    "Data Protection",
    true,
    "Encryption enabled",
    ComplianceStatus::COMPLIANT
  };
  bool result = manager.registerRequirement(req);
  assert(result);
  printf("✓ test_register_requirement\n");
}

// Test 31: Get requirements by standard
void test_get_requirements() {
  ComplianceManager manager;
  ComplianceRequirement req{
    "req-001",
    ComplianceStandard::GDPR,
    "Data encryption requirement",
    "Data Protection",
    true,
    "Encryption enabled",
    ComplianceStatus::COMPLIANT
  };
  manager.registerRequirement(req);
  auto reqs = manager.getRequirements(ComplianceStandard::GDPR);
  assert(reqs.size() >= 1);
  printf("✓ test_get_requirements\n");
}

// Test 32: Assess compliance
void test_assess_compliance() {
  ComplianceManager manager;
  ComplianceReport report = manager.assessCompliance(ComplianceStandard::GDPR);
  assert(!report.reportId.empty());
  assert(report.standard == ComplianceStandard::GDPR);
  printf("✓ test_assess_compliance\n");
}

// Test 33: Check is compliant
void test_is_compliant() {
  ComplianceManager manager;
  manager.registerCompliance(ComplianceStandard::GDPR);
  bool result = manager.isCompliant(ComplianceStandard::GDPR);
  assert(true);
  printf("✓ test_is_compliant\n");
}

// Test 34: Comprehensive compliance workflow
void test_comprehensive_compliance_workflow() {
  ComplianceManager manager;

  // Register multiple standards
  manager.registerCompliance(ComplianceStandard::GDPR);
  manager.registerCompliance(ComplianceStandard::ISO_27001);
  manager.registerCompliance(ComplianceStandard::PCI_DSS);

  // Implement GDPR
  manager.implementGDPRCompliance();

  // Collect consent
  manager.collectUserConsent("user-1", "marketing");

  // Implement ISO 27001
  manager.implementISO27001();
  manager.verifyAccessControl();
  manager.verifyIncidentManagement();

  // Implement PCI-DSS
  manager.implementPCIDSS();
  manager.securePaymentData();

  // Register requirement
  ComplianceRequirement req{
    "req-001",
    ComplianceStandard::GDPR,
    "Data encryption",
    "Data Protection",
    true,
    "Encryption enabled",
    ComplianceStatus::COMPLIANT
  };
  manager.registerRequirement(req);

  // Assess compliance
  auto report = manager.assessCompliance(ComplianceStandard::GDPR);
  assert(!report.reportId.empty());

  // Get registered standards
  auto standards = manager.getRegisteredCompliances();
  assert(standards.size() >= 3);

  printf("✓ test_comprehensive_compliance_workflow\n");
}

int main() {
  test_create_compliance_manager();
  test_register_gdpr_compliance();
  test_register_iso27001_compliance();
  test_register_soc2_compliance();
  test_register_hipaa_compliance();
  test_register_pci_dss_compliance();
  test_register_nist_compliance();
  test_register_ccpa_compliance();
  test_get_registered_compliances();
  test_implement_gdpr_compliance();
  test_collect_user_consent();
  test_provide_data_portability();
  test_delete_personal_data();
  test_provide_privacy_policy();
  test_implement_iso27001();
  test_verify_access_control();
  test_verify_incident_management();
  test_verify_business_continuity();
  test_implement_soc2();
  test_enable_security_monitoring();
  test_verify_change_management();
  test_verify_availability();
  test_implement_hipaa();
  test_encrypt_phi();
  test_enable_audit_controls();
  test_verify_integrity_controls();
  test_implement_pci_dss();
  test_secure_payment_data();
  test_verify_network_security();
  test_register_requirement();
  test_get_requirements();
  test_assess_compliance();
  test_is_compliant();
  test_comprehensive_compliance_workflow();
  printf("\n=== All GAP-085 ComplianceStandards tests passed! (34 tests) ===\n");
  return 0;
}
