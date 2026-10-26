#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace bendcore::security {

// ==============================================================================
// COMPLIANCE STANDARDS - GAP-085
// ==============================================================================
// GDPR, ISO 27001, SOC2, HIPAA compliance framework

enum class ComplianceStandard {
    GDPR,
    ISO_27001,
    SOC2,
    HIPAA,
    PCI_DSS,
    NIST,
    CCPA
};

enum class ComplianceStatus {
    COMPLIANT,
    NON_COMPLIANT,
    PARTIAL,
    UNKNOWN
};

struct ComplianceRequirement {
    std::string requirementId;
    ComplianceStandard standard;
    std::string description;
    std::string controlArea;
    bool isMandatory;
    std::string evidence;
    ComplianceStatus status;
};

struct ComplianceReport {
    std::string reportId;
    ComplianceStandard standard;
    int64_t generatedAt;
    float complianceScore;
    int32_t totalRequirements;
    int32_t compliantRequirements;
    std::vector<std::string> nonCompliantItems;
    std::vector<std::string> recommendations;
};

class ComplianceManager {
public:
    ComplianceManager();
    ~ComplianceManager();

    // Compliance registration
    bool registerCompliance(ComplianceStandard standard);
    std::vector<ComplianceStandard> getRegisteredCompliances();

    // GDPR compliance
    bool implementGDPRCompliance();
    bool collectUserConsent(const std::string& userId, const std::string& consentType);
    bool provideDataPortability(const std::string& userId);
    bool deletePersonalData(const std::string& userId);
    bool providePrivacyPolicy(std::string& outPolicy);

    // ISO 27001 compliance
    bool implementISO27001();
    bool verifyAccessControl();
    bool verifyIncidentManagement();
    bool verifyBusinessContinuity();

    // SOC2 compliance
    bool implementSOC2();
    bool enableSecurityMonitoring(bool enable);
    bool verifyChangeManagement();
    bool verifyAvailability();

    // HIPAA compliance (if applicable)
    bool implementHIPAA();
    bool encryptPHI(bool enable);
    bool enableAuditControls(bool enable);
    bool verifyIntegrityControls();

    // PCI DSS compliance (if applicable)
    bool implementPCIDSS();
    bool securePaymentData();
    bool verifyNetworkSecurity();

    // Compliance requirement management
    bool registerRequirement(const ComplianceRequirement& requirement);
    std::vector<ComplianceRequirement> getRequirements(ComplianceStandard standard);
    bool updateRequirementStatus(const std::string& requirementId, ComplianceStatus status);
    std::vector<ComplianceRequirement> getNonCompliantRequirements();

    // Compliance assessment
    ComplianceReport assessCompliance(ComplianceStandard standard);
    float calculateComplianceScore(ComplianceStandard standard);
    bool isCompliant(ComplianceStandard standard);

    // Compliance monitoring
    bool enableComplianceMonitoring(bool enable);
    bool checkContinuousCompliance();
    std::vector<std::string> getComplianceViolations();

    // Evidence management
    bool submitComplianceEvidence(const std::string& requirementId, const std::string& evidencePath);
    std::string getComplianceEvidence(const std::string& requirementId);
    bool verifyEvidenceValidity(const std::string& requirementId);

    // Audit trail for compliance
    bool generateComplianceAuditTrail();
    std::string exportComplianceReport(ComplianceStandard standard);

    // Data processing agreements
    bool registerDataProcessor(const std::string& processorName);
    bool updateDataProcessingAgreement(const std::string& processorName, const std::string& agreementPath);
    std::vector<std::string> getRegisteredDataProcessors();

    // Rights management
    bool provideAccessToPersonalData(const std::string& userId);
    bool rectifyPersonalData(const std::string& userId, const std::map<std::string, std::string>& corrections);
    bool restrictProcessing(const std::string& userId);

private:
    std::vector<ComplianceStandard> registeredCompliances;
    std::map<std::string, ComplianceRequirement> requirements;
    std::vector<ComplianceReport> reports;
    bool complianceMonitoringEnabled;
};

}  // namespace bendcore::security
