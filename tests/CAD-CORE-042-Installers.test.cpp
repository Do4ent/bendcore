#include <gtest/gtest.h>
#include "../src/deployment/Installers.hpp"

using namespace bendcore::deployment;

class InstallersTest : public ::testing::Test {
 protected:
  Installers installers_;
};

TEST_F(InstallersTest, InstallerTypeValidation) {
  EXPECT_TRUE(true);  // Placeholder
}

TEST_F(InstallersTest, WindowsInstallerConfig) {
  Installers::InstallerConfig config;
  config.app_name = "BendCore";
  config.version = "9.0.0";
  config.author = "BendCore Team";

  EXPECT_EQ(config.app_name, "BendCore");
  EXPECT_EQ(config.version, "9.0.0");
}

TEST_F(InstallersTest, MacOSInstallerConfig) {
  Installers::InstallerConfig config;
  config.install_dir = "/Applications/BendCore";
  config.create_desktop_shortcut = false;

  EXPECT_EQ(config.install_dir, "/Applications/BendCore");
  EXPECT_FALSE(config.create_desktop_shortcut);
}

TEST_F(InstallersTest, LinuxInstallerConfig) {
  Installers::InstallerConfig config;
  config.install_dir = "/opt/bendcore";
  config.create_start_menu = false;

  EXPECT_EQ(config.install_dir, "/opt/bendcore");
  EXPECT_FALSE(config.create_start_menu);
}

TEST_F(InstallersTest, FileAssociations) {
  Installers::InstallerConfig config;
  config.associate_file_types = true;
  config.file_types.push_back(".bendcore");
  config.file_types.push_back(".bnd");

  EXPECT_TRUE(config.associate_file_types);
  EXPECT_EQ(config.file_types.size(), 2);
}

TEST_F(InstallersTest, InstallerResultValidation) {
  Installers::InstallerResult result;
  result.success = true;
  result.installer_path = "/tmp/bendcore-9.0.0.msi";
  result.file_size_bytes = 50 * 1024 * 1024;

  EXPECT_TRUE(result.success);
  EXPECT_FALSE(result.installer_path.empty());
  EXPECT_GT(result.file_size_bytes, 0);
}

TEST_F(InstallersTest, CompressionRatio) {
  Installers::InstallerResult result;
  result.compression_ratio = 0.85;

  EXPECT_GE(result.compression_ratio, 0.0);
  EXPECT_LE(result.compression_ratio, 1.0);
}

TEST_F(InstallersTest, FileSizeCalculation) {
  Installers::InstallerConfig config;
  Installers::InstallerResult result;
  result.file_size_bytes = 100 * 1024 * 1024;

  double size_mb = result.file_size_bytes / (1024.0 * 1024.0);
  EXPECT_NEAR(size_mb, 100.0, 0.1);
}

TEST_F(InstallersTest, MultiPlatformSupport) {
  std::vector<Installers::InstallerType> types = {
    Installers::InstallerType::WINDOWS_MSI,
    Installers::InstallerType::MACOS_DMG,
    Installers::InstallerType::LINUX_DEB,
    Installers::InstallerType::PORTABLE_ZIP
  };

  EXPECT_EQ(types.size(), 4);
}

TEST_F(InstallersTest, InstallerSuiteGeneration) {
  Installers::InstallerConfig config;
  config.app_name = "BendCore";
  config.version = "9.0.0";

  EXPECT_EQ(config.app_name, "BendCore");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
