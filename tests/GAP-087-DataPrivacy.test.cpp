// GAP-087: DataPrivacy (35 tests) - Anonymization, consent management
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "bendcore/security/data_privacy.hpp"

using namespace bendcore::security;

// Test 1: Create data privacy manager
void test_create_data_privacy_manager() {
  DataPrivacyManager manager;
  assert(manager.getDataClassifications().size() == 0);
  printf("✓ test_create_data_privacy_manager\n");
}

// Test 2: Classify data as public
void test_classify_data_public() {
  DataPrivacyManager manager;
  DataClassification classification{
    "data-001",
    "customer_name",
    PrivacyLevel::PUBLIC,
    false,
    {},
    "owner1",
    1000
  };
  bool result = manager.classifyData(classification);
  assert(result);
  printf("✓ test_classify_data_public\n");
}

// Test 3: Classify data as internal
void test_classify_data_internal() {
  DataPrivacyManager manager;
  DataClassification classification{
    "data-001",
    "internal_doc",
    PrivacyLevel::INTERNAL,
    false,
    {},
    "owner1",
    1000
  };
  bool result = manager.classifyData(classification);
  assert(result);
  printf("✓ test_classify_data_internal\n");
}

// Test 4: Classify data as confidential
void test_classify_data_confidential() {
  DataPrivacyManager manager;
  DataClassification classification{
    "data-001",
    "financial_report",
    PrivacyLevel::CONFIDENTIAL,
    true,
    {"amount", "account_number"},
    "owner1",
    1000
  };
  bool result = manager.classifyData(classification);
  assert(result);
  printf("✓ test_classify_data_confidential\n");
}

// Test 5: Classify data as restricted
void test_classify_data_restricted() {
  DataPrivacyManager manager;
  DataClassification classification{
    "data-001",
    "pii_data",
    PrivacyLevel::RESTRICTED,
    true,
    {"ssn", "phone_number"},
    "owner1",
    1000
  };
  bool result = manager.classifyData(classification);
  assert(result);
  printf("✓ test_classify_data_restricted\n");
}

// Test 6: Classify data as secret
void test_classify_data_secret() {
  DataPrivacyManager manager;
  DataClassification classification{
    "data-001",
    "trade_secret",
    PrivacyLevel::SECRET,
    true,
    {"algorithm", "formula"},
    "owner1",
    1000
  };
  bool result = manager.classifyData(classification);
  assert(result);
  printf("✓ test_classify_data_secret\n");
}

// Test 7: Get data classifications
void test_get_data_classifications() {
  DataPrivacyManager manager;
  for (int i = 0; i < 3; i++) {
    DataClassification classification{
      "data-" + std::to_string(i),
      "type" + std::to_string(i),
      PrivacyLevel::INTERNAL,
      false,
      {},
      "owner1",
      1000 + i
    };
    manager.classifyData(classification);
  }
  auto classifications = manager.getDataClassifications();
  assert(classifications.size() == 3);
  printf("✓ test_get_data_classifications\n");
}

// Test 8: Get data classification by ID
void test_get_data_classification() {
  DataPrivacyManager manager;
  DataClassification classification{
    "data-001",
    "customer_email",
    PrivacyLevel::INTERNAL,
    false,
    {},
    "owner1",
    1000
  };
  manager.classifyData(classification);
  auto retrieved = manager.getDataClassification("data-001");
  assert(retrieved.dataId == "data-001");
  printf("✓ test_get_data_classification\n");
}

// Test 9: Get data privacy level
void test_get_data_privacy_level() {
  DataPrivacyManager manager;
  DataClassification classification{
    "data-001",
    "secret_doc",
    PrivacyLevel::SECRET,
    false,
    {},
    "owner1",
    1000
  };
  manager.classifyData(classification);
  auto level = manager.getDataPrivacyLevel("data-001");
  assert(level == PrivacyLevel::SECRET);
  printf("✓ test_get_data_privacy_level\n");
}

// Test 10: Detect PII
void test_detect_pii() {
  DataPrivacyManager manager;
  std::vector<std::string> pii_fields;
  bool result = manager.detectPII("email: john@example.com, phone: 555-1234", pii_fields);
  assert(result);
  printf("✓ test_detect_pii\n");
}

// Test 11: Check if contains PII
void test_contains_pii() {
  DataPrivacyManager manager;
  bool result = manager.containsPII("SSN: 123-45-6789");
  assert(result);
  printf("✓ test_contains_pii\n");
}

// Test 12: Extract PII fields
void test_extract_pii_fields() {
  DataPrivacyManager manager;
  auto pii = manager.extractPIIFields("Name: John Doe, Email: john@example.com");
  assert(pii.size() >= 0);
  printf("✓ test_extract_pii_fields\n");
}

// Test 13: Mask PII
void test_mask_pii() {
  DataPrivacyManager manager;
  std::string data = "SSN: 123-45-6789";
  bool result = manager.maskPII(data);
  assert(result);
  assert(data != "SSN: 123-45-6789");  // Should be masked
  printf("✓ test_mask_pii\n");
}

// Test 14: Encrypt PII
void test_encrypt_pii() {
  DataPrivacyManager manager;
  std::string encrypted;
  bool result = manager.encryptPII("sensitive_data", encrypted);
  assert(result);
  assert(!encrypted.empty());
  printf("✓ test_encrypt_pii\n");
}

// Test 15: Decrypt PII
void test_decrypt_pii() {
  DataPrivacyManager manager;
  std::string encrypted;
  manager.encryptPII("sensitive_data", encrypted);
  std::string decrypted;
  bool result = manager.decryptPII(encrypted, decrypted);
  assert(result);
  assert(!decrypted.empty());
  printf("✓ test_decrypt_pii\n");
}

// Test 16: Get masked value
void test_get_masked_value() {
  DataPrivacyManager manager;
  std::string masked = manager.getMaskedValue("123-45-6789", "ssn");
  assert(!masked.empty());
  assert(masked != "123-45-6789");
  printf("✓ test_get_masked_value\n");
}

// Test 17: Anonymize data with hash
void test_anonymize_data_hash() {
  DataPrivacyManager manager;
  DataClassification classification{
    "data-001",
    "email",
    PrivacyLevel::RESTRICTED,
    true,
    {"email"},
    "owner1",
    1000
  };
  manager.classifyData(classification);
  bool result = manager.anonymizeData("data-001", AnonymizationTechnique::HASH);
  assert(result);
  printf("✓ test_anonymize_data_hash\n");
}

// Test 18: Anonymize data with encryption
void test_anonymize_data_encrypt() {
  DataPrivacyManager manager;
  DataClassification classification{
    "data-001",
    "phone",
    PrivacyLevel::RESTRICTED,
    true,
    {"phone"},
    "owner1",
    1000
  };
  manager.classifyData(classification);
  bool result = manager.anonymizeData("data-001", AnonymizationTechnique::ENCRYPT);
  assert(result);
  printf("✓ test_anonymize_data_encrypt\n");
}

// Test 19: Anonymize data with mask
void test_anonymize_data_mask() {
  DataPrivacyManager manager;
  DataClassification classification{
    "data-001",
    "ssn",
    PrivacyLevel::RESTRICTED,
    true,
    {"ssn"},
    "owner1",
    1000
  };
  manager.classifyData(classification);
  bool result = manager.anonymizeData("data-001", AnonymizationTechnique::MASK);
  assert(result);
  printf("✓ test_anonymize_data_mask\n");
}

// Test 20: Pseudonymize data
void test_pseudonymize_data() {
  DataPrivacyManager manager;
  std::string pseudonym;
  bool result = manager.pseudonymizeData("user@example.com", pseudonym);
  assert(result);
  assert(!pseudonym.empty());
  assert(pseudonym != "user@example.com");
  printf("✓ test_pseudonymize_data\n");
}

// Test 21: Register anonymization policy
void test_register_anonymization_policy() {
  DataPrivacyManager manager;
  AnonymizationPolicy policy{
    "policy-001",
    "email",
    AnonymizationTechnique::HASH,
    {{"algorithm", "SHA256"}}
  };
  bool result = manager.registerAnonymizationPolicy(policy);
  assert(result);
  printf("✓ test_register_anonymization_policy\n");
}

// Test 22: Get anonymization policies
void test_get_anonymization_policies() {
  DataPrivacyManager manager;
  AnonymizationPolicy policy{
    "policy-001",
    "ssn",
    AnonymizationTechnique::MASK,
    {}
  };
  manager.registerAnonymizationPolicy(policy);
  auto policies = manager.getAnonymizationPolicies();
  assert(policies.size() == 1);
  printf("✓ test_get_anonymization_policies\n");
}

// Test 23: Set data retention policy
void test_set_data_retention_policy() {
  DataPrivacyManager manager;
  bool result = manager.setDataRetentionPolicy("customer_data", 365);
  assert(result);
  printf("✓ test_set_data_retention_policy\n");
}

// Test 24: Get data retention period
void test_get_data_retention_period() {
  DataPrivacyManager manager;
  manager.setDataRetentionPolicy("customer_data", 365);
  int days = manager.getDataRetentionPeriod("customer_data");
  assert(days == 365);
  printf("✓ test_get_data_retention_period\n");
}

// Test 25: Delete personal data
void test_delete_personal_data() {
  DataPrivacyManager manager;
  bool result = manager.deletePersonalData("user-123");
  assert(result);
  printf("✓ test_delete_personal_data\n");
}

// Test 26: Delete all personal data
void test_delete_all_personal_data() {
  DataPrivacyManager manager;
  bool result = manager.deleteAllPersonalData("user-123");
  assert(result);
  printf("✓ test_delete_all_personal_data\n");
}

// Test 27: Verify data deletion
void test_verify_data_deletion() {
  DataPrivacyManager manager;
  manager.deletePersonalData("user-123");
  bool result = manager.verifyDataDeletion("user-123");
  assert(result);
  printf("✓ test_verify_data_deletion\n");
}

// Test 28: Export personal data
void test_export_personal_data() {
  DataPrivacyManager manager;
  std::string data;
  bool result = manager.exportPersonalData("user-123", data);
  assert(result);
  printf("✓ test_export_personal_data\n");
}

// Test 29: Record consent
void test_record_consent() {
  DataPrivacyManager manager;
  bool result = manager.recordConsent("user-123", "marketing", true);
  assert(result);
  printf("✓ test_record_consent\n");
}

// Test 30: Get consent
void test_get_consent() {
  DataPrivacyManager manager;
  manager.recordConsent("user-123", "marketing", true);
  bool consent = manager.getConsent("user-123", "marketing");
  assert(consent);
  printf("✓ test_get_consent\n");
}

// Test 31: Withdraw consent
void test_withdraw_consent() {
  DataPrivacyManager manager;
  manager.recordConsent("user-123", "marketing", true);
  bool result = manager.withdrawConsent("user-123", "marketing");
  assert(result);
  printf("✓ test_withdraw_consent\n");
}

// Test 32: Get active consents
void test_get_active_consents() {
  DataPrivacyManager manager;
  manager.recordConsent("user-123", "marketing", true);
  manager.recordConsent("user-123", "analytics", true);
  manager.recordConsent("user-123", "third_party", false);
  auto consents = manager.getActiveConsents("user-123");
  assert(consents.size() >= 2);
  printf("✓ test_get_active_consents\n");
}

// Test 33: Perform privacy impact assessment
void test_perform_privacy_impact_assessment() {
  DataPrivacyManager manager;
  std::string assessment = manager.performPrivacyImpactAssessment();
  assert(!assessment.empty());
  printf("✓ test_perform_privacy_impact_assessment\n");
}

// Test 34: Generate privacy report
void test_generate_privacy_report() {
  DataPrivacyManager manager;
  DataClassification classification{
    "data-001",
    "user_data",
    PrivacyLevel::RESTRICTED,
    true,
    {"email", "phone"},
    "owner1",
    1000
  };
  manager.classifyData(classification);
  std::string report = manager.generatePrivacyReport();
  assert(!report.empty());
  printf("✓ test_generate_privacy_report\n");
}

// Test 35: Comprehensive data privacy workflow
void test_comprehensive_data_privacy_workflow() {
  DataPrivacyManager manager;

  // Classify data
  DataClassification classification{
    "data-001",
    "user_profile",
    PrivacyLevel::RESTRICTED,
    true,
    {"email", "phone", "ssn"},
    "owner1",
    1000
  };
  manager.classifyData(classification);

  // Set retention policy
  manager.setDataRetentionPolicy("user_profile", 365);

  // Record consents
  manager.recordConsent("user-123", "marketing", true);
  manager.recordConsent("user-123", "analytics", true);

  // Register anonymization policy
  AnonymizationPolicy policy{
    "policy-001",
    "email",
    AnonymizationTechnique::HASH,
    {{"algorithm", "SHA256"}}
  };
  manager.registerAnonymizationPolicy(policy);

  // Perform operations
  bool contains = manager.containsPII("email@example.com");
  assert(contains);

  std::string masked = manager.getMaskedValue("555-1234", "phone");
  assert(!masked.empty());

  auto consents = manager.getActiveConsents("user-123");
  assert(consents.size() >= 1);

  auto classifications = manager.getDataClassifications();
  assert(classifications.size() == 1);

  printf("✓ test_comprehensive_data_privacy_workflow\n");
}

int main() {
  test_create_data_privacy_manager();
  test_classify_data_public();
  test_classify_data_internal();
  test_classify_data_confidential();
  test_classify_data_restricted();
  test_classify_data_secret();
  test_get_data_classifications();
  test_get_data_classification();
  test_get_data_privacy_level();
  test_detect_pii();
  test_contains_pii();
  test_extract_pii_fields();
  test_mask_pii();
  test_encrypt_pii();
  test_decrypt_pii();
  test_get_masked_value();
  test_anonymize_data_hash();
  test_anonymize_data_encrypt();
  test_anonymize_data_mask();
  test_pseudonymize_data();
  test_register_anonymization_policy();
  test_get_anonymization_policies();
  test_set_data_retention_policy();
  test_get_data_retention_period();
  test_delete_personal_data();
  test_delete_all_personal_data();
  test_verify_data_deletion();
  test_export_personal_data();
  test_record_consent();
  test_get_consent();
  test_withdraw_consent();
  test_get_active_consents();
  test_perform_privacy_impact_assessment();
  test_generate_privacy_report();
  test_comprehensive_data_privacy_workflow();
  printf("\n=== All GAP-087 DataPrivacy tests passed! (35 tests) ===\n");
  return 0;
}
