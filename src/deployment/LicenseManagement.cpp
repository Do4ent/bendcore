#include "LicenseManagement.hpp"
#include <random>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <openssl/sha.h>

namespace bendcore::deployment {

LicenseManagement::LicenseManagement() {}

std::string LicenseManagement::generateLicenseKey(
    LicenseType type,
    const std::string& licensee_name,
    time_t expiration_date) {

  std::ostringstream key;

  // Generate 4 segments of 5 characters each
  for (int i = 0; i < 4; ++i) {
    if (i > 0) key << "-";
    key << generateKeySegment(i);
  }

  // Cache the generated key
  License license;
  license.license_key = key.str();
  license.license_type = type;
  license.licensee_name = licensee_name;
  license.activation_date = std::time(nullptr);
  license.expiration_date = expiration_date;

  saveLicense(license);

  return key.str();
}

std::string LicenseManagement::generateKeySegment(int segment) {
  // Generate random alphanumeric segment
  const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, chars.size() - 1);

  std::string segment_str;
  for (int i = 0; i < 5; ++i) {
    segment_str += chars[dis(gen)];
  }

  return segment_str;
}

LicenseManagement::ValidationResult LicenseManagement::validateLicense(
    const std::string& license_key) {

  ValidationResult result;

  // Check format
  if (!validateKeyFormat(license_key)) {
    result.status = LicenseStatus::INVALID;
    result.error_message = "Invalid license key format";
    return result;
  }

  // Load license
  License license;
  if (!loadLicense(license_key, license)) {
    result.status = LicenseStatus::INVALID;
    result.error_message = "License not found";
    return result;
  }

  // Check expiration
  time_t now = std::time(nullptr);
  if (now > license.expiration_date) {
    result.status = LicenseStatus::EXPIRED;
    result.error_message = "License has expired";
    result.license = license;
    return result;
  }

  // Calculate days remaining
  result.days_until_expiry = (license.expiration_date - now) / (24 * 3600);

  result.is_valid = true;
  result.status = LicenseStatus::VALID;
  result.license = license;

  return result;
}

bool LicenseManagement::activateLicense(
    const std::string& license_key,
    const std::vector<std::string>& hardware_ids) {

  License license;
  if (!loadLicense(license_key, license)) {
    return false;
  }

  license.hardware_ids = hardware_ids;
  license.activation_date = std::time(nullptr);

  return saveLicense(license);
}

bool LicenseManagement::deactivateLicense(const std::string& license_key) {
  License license;
  if (!loadLicense(license_key, license)) {
    return false;
  }

  license.hardware_ids.clear();
  return saveLicense(license);
}

LicenseManagement::LicenseStatus LicenseManagement::checkLicenseStatus(
    const std::string& license_key) {

  auto result = validateLicense(license_key);
  return result.status;
}

bool LicenseManagement::createOfflineLicense(
    const std::string& license_key,
    const std::string& output_path,
    int grace_period_days) {

  License license;
  if (!loadLicense(license_key, license)) {
    return false;
  }

  license.is_offline_licensed = true;
  license.offline_grace_period_days = grace_period_days;

  // Encrypt and save license file
  std::string encrypted_data = encryptLicenseData(license_key);

  std::ofstream file(output_path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  file << encrypted_data;
  file.close();

  return true;
}

LicenseManagement::ValidationResult LicenseManagement::verifyOfflineLicense(
    const std::string& license_file) {

  ValidationResult result;

  std::ifstream file(license_file, std::ios::binary);
  if (!file.is_open()) {
    result.status = LicenseStatus::INVALID;
    return result;
  }

  std::string encrypted_data((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
  file.close();

  std::string decrypted = decryptLicenseData(encrypted_data);
  result = validateLicense(decrypted);

  return result;
}

bool LicenseManagement::isLicenseExpired(const std::string& license_key) {
  auto result = validateLicense(license_key);
  return result.status == LicenseStatus::EXPIRED;
}

int LicenseManagement::getDaysRemaining(const std::string& license_key) {
  auto result = validateLicense(license_key);
  if (result.is_valid) {
    return result.days_until_expiry;
  }
  return 0;
}

std::string LicenseManagement::createEvaluationLicense(
    const std::string& licensee_name,
    int evaluation_days) {

  time_t expiration = std::time(nullptr) + (evaluation_days * 24 * 3600);
  return generateLicenseKey(LicenseType::EVALUATION, licensee_name, expiration);
}

bool LicenseManagement::upgradeLicense(
    const std::string& old_license_key,
    LicenseType new_type,
    time_t new_expiration_date) {

  License old_license;
  if (!loadLicense(old_license_key, old_license)) {
    return false;
  }

  old_license.license_type = new_type;
  old_license.expiration_date = new_expiration_date;

  return saveLicense(old_license);
}

LicenseManagement::License LicenseManagement::getLicenseInfo(
    const std::string& license_key) {

  License license;
  loadLicense(license_key, license);
  return license;
}

std::string LicenseManagement::getHardwareId() {
  // Get MAC address as hardware ID
  // This is simplified; real implementation would use system-specific APIs
  return "00:11:22:33:44:55";  // Placeholder
}

std::string LicenseManagement::computeHardwareHash(
    const std::vector<std::string>& hardware_ids) {

  std::ostringstream combined;
  for (const auto& id : hardware_ids) {
    combined << id;
  }

  // Hash using SHA256
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(combined.str().c_str()),
         combined.str().length(), hash);

  std::ostringstream result;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
    result << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
  }

  return result.str();
}

bool LicenseManagement::saveLicense(const License& license) {
  license_cache_[license.license_key] = license;
  return true;
}

bool LicenseManagement::loadLicense(
    const std::string& license_key,
    License& license) {

  auto it = license_cache_.find(license_key);
  if (it != license_cache_.end()) {
    license = it->second;
    return true;
  }

  return false;
}

LicenseManagement::ServerResponse LicenseManagement::validateLicenseOnServer(
    const std::string& license_key,
    const std::string& server_url) {

  ServerResponse response;

  // In a real implementation, this would make an HTTPS request to the server
  response.success = true;
  response.message = "License validated on server";

  auto local_result = validateLicense(license_key);
  response.license = local_result.license;

  return response;
}

std::string LicenseManagement::generateLicenseReport(const License& license) {
  std::ostringstream report;
  report << std::fixed << std::setprecision(2);

  report << "LICENSE REPORT\n";
  report << "==============\n";
  report << "License Key: " << license.license_key << "\n";
  report << "Type: " << (int)license.license_type << "\n";
  report << "Licensee: " << license.licensee_name << "\n";
  report << "Email: " << license.licensee_email << "\n";
  report << "Organization: " << license.licensee_organization << "\n";
  report << "Max Users: " << license.max_users << "\n";
  report << "Is Valid: " << (license.activation_date != 0 ? "Yes" : "No") << "\n";

  return report.str();
}

bool LicenseManagement::validateKeyFormat(const std::string& license_key) {
  // Check format: XXXXX-XXXXX-XXXXX-XXXXX (20 characters + 3 dashes)
  if (license_key.length() != 23) {
    return false;
  }

  return (license_key[5] == '-' && license_key[11] == '-' && license_key[17] == '-');
}

std::string LicenseManagement::hashLicenseData(
    const std::string& data,
    const std::string& salt) {

  std::string to_hash = data + salt;

  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char*>(to_hash.c_str()),
         to_hash.length(), hash);

  std::ostringstream result;
  for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
    result << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
  }

  return result.str();
}

std::string LicenseManagement::encryptLicenseData(const std::string& plaintext) {
  // Simplified encryption (real implementation would use proper encryption)
  return plaintext;  // Placeholder
}

std::string LicenseManagement::decryptLicenseData(const std::string& ciphertext) {
  // Simplified decryption
  return ciphertext;  // Placeholder
}

}  // namespace bendcore::deployment
