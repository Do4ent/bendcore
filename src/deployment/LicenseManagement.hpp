#pragma once

#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <memory>

namespace bendcore::deployment {

// License Management System (CAD-CORE-043)
class LicenseManagement {
 public:
  enum class LicenseType {
    COMMUNITY,      // Free version
    PROFESSIONAL,   // Single user, 1 year
    ENTERPRISE,     // Multi-user, unlimited
    FLOATING,       // Network license
    EVALUATION      // 30-day trial
  };

  enum class LicenseStatus {
    VALID,
    EXPIRED,
    INVALID,
    REVOKED,
    INACTIVE
  };

  struct License {
    std::string license_key;  // 20-character key
    LicenseType license_type = LicenseType::COMMUNITY;
    std::string licensee_name;
    std::string licensee_email;
    std::string licensee_organization;
    time_t activation_date = 0;
    time_t expiration_date = 0;
    int max_users = 1;
    std::vector<std::string> hardware_ids;  // MAC addresses for binding
    bool is_offline_licensed = false;
    int offline_grace_period_days = 30;
    std::string version_constraint;  // e.g., ">=9.0.0"
  };

  struct ValidationResult {
    bool is_valid = false;
    LicenseStatus status = LicenseStatus::INVALID;
    License license;
    std::string error_message;
    time_t days_until_expiry = 0;
  };

  LicenseManagement();

  // Generate license key
  std::string generateLicenseKey(
    LicenseType type,
    const std::string& licensee_name,
    time_t expiration_date
  );

  // Validate license key
  ValidationResult validateLicense(const std::string& license_key);

  // Activate license
  bool activateLicense(
    const std::string& license_key,
    const std::vector<std::string>& hardware_ids
  );

  // Deactivate license
  bool deactivateLicense(const std::string& license_key);

  // Get license status
  LicenseStatus checkLicenseStatus(const std::string& license_key);

  // Create license file for offline use
  bool createOfflineLicense(
    const std::string& license_key,
    const std::string& output_path,
    int grace_period_days = 30
  );

  // Verify offline license
  ValidationResult verifyOfflineLicense(const std::string& license_file);

  // Check license expiration
  bool isLicenseExpired(const std::string& license_key);

  // Get days remaining
  int getDaysRemaining(const std::string& license_key);

  // Create evaluation license
  std::string createEvaluationLicense(
    const std::string& licensee_name,
    int evaluation_days = 30
  );

  // Upgrade license
  bool upgradeLicense(
    const std::string& old_license_key,
    LicenseType new_type,
    time_t new_expiration_date
  );

  // Get license information
  License getLicenseInfo(const std::string& license_key);

  // Hardware ID functions
  std::string getHardwareId();  // Get MAC address
  std::string computeHardwareHash(
    const std::vector<std::string>& hardware_ids
  );

  // License storage and retrieval
  bool saveLicense(const License& license);
  bool loadLicense(const std::string& license_key, License& license);

  // Licensing server communication
  struct ServerResponse {
    bool success = false;
    std::string message;
    License license;
  };

  ServerResponse validateLicenseOnServer(
    const std::string& license_key,
    const std::string& server_url
  );

  // Generate license report
  std::string generateLicenseReport(const License& license);

 private:
  // Key generation and validation
  std::string generateKeySegment(int segment);
  bool validateKeyFormat(const std::string& license_key);
  std::string hashLicenseData(
    const std::string& data,
    const std::string& salt
  );

  // Cryptographic functions
  std::string encryptLicenseData(const std::string& plaintext);
  std::string decryptLicenseData(const std::string& ciphertext);

  // License storage
  std::map<std::string, License> license_cache_;
  std::string license_storage_path_;

  // Server configuration
  const std::string LICENSE_SERVER_URL = "https://license.bendcore.com";
  const int LICENSE_KEY_LENGTH = 20;
};

}  // namespace bendcore::deployment
