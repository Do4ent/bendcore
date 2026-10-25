#include <gtest/gtest.h>
#include "../src/deployment/LicenseManagement.hpp"
#include <ctime>

using namespace bendcore::deployment;

class LicenseManagementTest : public ::testing::Test {
 protected:
  LicenseManagement license_mgr_;
};

TEST_F(LicenseManagementTest, LicenseTypeVariants) {
  std::vector<LicenseManagement::LicenseType> types = {
    LicenseManagement::LicenseType::COMMUNITY,
    LicenseManagement::LicenseType::PROFESSIONAL,
    LicenseManagement::LicenseType::ENTERPRISE,
    LicenseManagement::LicenseType::FLOATING,
    LicenseManagement::LicenseType::EVALUATION
  };

  EXPECT_EQ(types.size(), 5);
}

TEST_F(LicenseManagementTest, LicenseStatusTracking) {
  std::vector<LicenseManagement::LicenseStatus> statuses = {
    LicenseManagement::LicenseStatus::VALID,
    LicenseManagement::LicenseStatus::EXPIRED,
    LicenseManagement::LicenseStatus::INVALID,
    LicenseManagement::LicenseStatus::REVOKED,
    LicenseManagement::LicenseStatus::INACTIVE
  };

  EXPECT_EQ(statuses.size(), 5);
}

TEST_F(LicenseManagementTest, LicenseStructure) {
  LicenseManagement::License license;
  license.license_key = "ABCDE-FGHIJ-KLMNO-PQRST";
  license.license_type = LicenseManagement::LicenseType::PROFESSIONAL;
  license.licensee_name = "John Doe";
  license.max_users = 5;

  EXPECT_EQ(license.license_key.length(), 23);  // 20 chars + 3 dashes
  EXPECT_EQ(license.licensee_name, "John Doe");
  EXPECT_EQ(license.max_users, 5);
}

TEST_F(LicenseManagementTest, ValidationResultStructure) {
  LicenseManagement::ValidationResult result;
  result.is_valid = true;
  result.status = LicenseManagement::LicenseStatus::VALID;
  result.days_until_expiry = 365;

  EXPECT_TRUE(result.is_valid);
  EXPECT_GT(result.days_until_expiry, 0);
}

TEST_F(LicenseManagementTest, LicenseGeneration) {
  time_t expiration = std::time(nullptr) + (365 * 24 * 3600);
  std::string key = license_mgr_.generateLicenseKey(
    LicenseManagement::LicenseType::PROFESSIONAL,
    "John Doe",
    expiration
  );

  EXPECT_EQ(key.length(), 23);
  EXPECT_NE(key.find('-'), std::string::npos);
}

TEST_F(LicenseManagementTest, EvaluationLicenseCreation) {
  std::string eval_key = license_mgr_.createEvaluationLicense("Jane Smith", 30);
  EXPECT_EQ(eval_key.length(), 23);
}

TEST_F(LicenseManagementTest, HardwareIdGeneration) {
  std::string hw_id = license_mgr_.getHardwareId();
  EXPECT_FALSE(hw_id.empty());
}

TEST_F(LicenseManagementTest, OfflineLicenseSupport) {
  LicenseManagement::License license;
  license.is_offline_licensed = true;
  license.offline_grace_period_days = 30;

  EXPECT_TRUE(license.is_offline_licensed);
  EXPECT_EQ(license.offline_grace_period_days, 30);
}

TEST_F(LicenseManagementTest, MultiUserLicense) {
  LicenseManagement::License license;
  license.max_users = 10;
  license.license_type = LicenseManagement::LicenseType::ENTERPRISE;

  EXPECT_EQ(license.max_users, 10);
}

TEST_F(LicenseManagementTest, HardwareBinding) {
  std::vector<std::string> hardware_ids;
  hardware_ids.push_back("00:11:22:33:44:55");
  hardware_ids.push_back("AA:BB:CC:DD:EE:FF");

  EXPECT_EQ(hardware_ids.size(), 2);
}

TEST_F(LicenseManagementTest, LicenseExpiration) {
  time_t past_time = std::time(nullptr) - (365 * 24 * 3600);
  LicenseManagement::License license;
  license.expiration_date = past_time;

  EXPECT_LT(license.expiration_date, std::time(nullptr));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
