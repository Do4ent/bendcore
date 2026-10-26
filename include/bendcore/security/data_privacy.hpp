#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace bendcore::security {

// ==============================================================================
// DATA PRIVACY - GAP-087
// ==============================================================================
// PII protection, data anonymization, data masking, privacy controls

enum class PrivacyLevel {
    PUBLIC,
    INTERNAL,
    CONFIDENTIAL,
    RESTRICTED,
    SECRET
};

enum class AnonymizationTechnique {
    HASH,
    ENCRYPT,
    MASK,
    PSEUDONYMIZE,
    GENERALIZE,
    SUPPRESS
};

struct PersonallyIdentifiableInfo {
    std::string fieldName;
    std::string fieldValue;
    PrivacyLevel privacyLevel;
    bool isEncrypted;
    bool isAnonymized;
};

struct DataClassification {
    std::string dataId;
    std::string dataType;
    PrivacyLevel privacyLevel;
    bool containsPII;
    std::vector<std::string> piiFields;
    std::string owner;
    int64_t classifiedAt;
};

struct AnonymizationPolicy {
    std::string policyId;
    std::string fieldName;
    AnonymizationTechnique technique;
    std::map<std::string, std::string> parameters;
};

class DataPrivacyManager {
public:
    DataPrivacyManager();
    ~DataPrivacyManager();

    // Data classification
    bool classifyData(const DataClassification& classification);
    std::vector<DataClassification> getDataClassifications();
    DataClassification getDataClassification(const std::string& dataId);
    PrivacyLevel getDataPrivacyLevel(const std::string& dataId);

    // PII detection
    bool detectPII(const std::string& data, std::vector<std::string>& outPIIFields);
    bool containsPII(const std::string& data);
    std::vector<std::string> extractPIIFields(const std::string& data);

    // PII protection
    bool maskPII(std::string& data);
    bool encryptPII(const std::string& plaintext, std::string& outEncrypted);
    bool decryptPII(const std::string& encrypted, std::string& outPlaintext);
    std::string getMaskedValue(const std::string& originalValue, const std::string& fieldType);

    // Data anonymization
    bool anonymizeData(const std::string& dataId, AnonymizationTechnique technique);
    bool deanonymizeData(const std::string& anonymizedDataId, std::string& outOriginal);
    bool pseudonymizeData(const std::string& dataId, std::string& outPseudonym);

    // Anonymization policies
    bool registerAnonymizationPolicy(const AnonymizationPolicy& policy);
    bool applyAnonymizationPolicy(const std::string& policyId, std::string& data);
    std::vector<AnonymizationPolicy> getAnonymizationPolicies();

    // Data retention
    bool setDataRetentionPolicy(const std::string& dataType, int32_t retentionDays);
    int32_t getDataRetentionPeriod(const std::string& dataType);
    bool enforceDataRetention();
    std::vector<std::string> getExpiredData();

    // Right to be forgotten
    bool deletePersonalData(const std::string& userId);
    bool deleteAllPersonalData(const std::string& userId);
    bool verifyDataDeletion(const std::string& userId);

    // Data portability
    bool exportPersonalData(const std::string& userId, std::string& outData);
    bool generateDataExportReport(const std::string& userId);

    // Consent management
    bool recordConsent(const std::string& userId, const std::string& consentType, bool granted);
    bool getConsent(const std::string& userId, const std::string& consentType);
    bool withdrawConsent(const std::string& userId, const std::string& consentType);
    std::vector<std::string> getActiveConsents(const std::string& userId);

    // Privacy impact assessment
    std::string performPrivacyImpactAssessment();
    std::string generatePrivacyReport();

    // Data transfer restrictions
    bool restrictDataTransfer(const std::string& dataId, bool restrict);
    bool canTransferData(const std::string& dataId);
    std::vector<std::string> getRestrictedDataIds();

    // Vendor/processor compliance
    bool registerDataProcessor(const std::string& processorName);
    bool verifyDataProcessorCompliance(const std::string& processorName);
    std::vector<std::string> getApprovedDataProcessors();

    // Privacy settings
    bool setUserPrivacyLevel(const std::string& userId, PrivacyLevel level);
    PrivacyLevel getUserPrivacyLevel(const std::string& userId);
    std::map<std::string, PrivacyLevel> getUserPrivacySettings(const std::string& userId);

    // Privacy metrics
    int32_t getTotalPIIRecords() const;
    int32_t getAnonymizedRecordsCount() const;
    int32_t getEncryptedRecordsCount() const;
    float getPIICoveragePercentage() const;

private:
    std::vector<DataClassification> dataClassifications;
    std::vector<AnonymizationPolicy> policies;
    std::map<std::string, std::map<std::string, bool>> userConsents;
    std::map<std::string, PrivacyLevel> userPrivacyLevels;
    std::map<std::string, int32_t> dataRetentionPolicies;
};

}  // namespace bendcore::security
