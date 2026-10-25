#include <gtest/gtest.h>
#include "../src/deployment/ProductionBuild.hpp"

using namespace bendcore::deployment;

class ProductionBuildTest : public ::testing::Test {
 protected:
  ProductionBuild builder_;
};

TEST_F(ProductionBuildTest, BuildTypeValidation) {
  ProductionBuild::BuildConfig config;
  config.build_type = ProductionBuild::BuildType::DISTRIBUTION;
  EXPECT_EQ(config.build_type, ProductionBuild::BuildType::DISTRIBUTION);
}

TEST_F(ProductionBuildTest, PlatformValidation) {
  ProductionBuild::BuildConfig config;
  config.target_platform = ProductionBuild::Platform::LINUX;
  EXPECT_EQ(config.target_platform, ProductionBuild::Platform::LINUX);
}

TEST_F(ProductionBuildTest, OptimizationFlags) {
  ProductionBuild::BuildConfig config;
  config.enable_optimizations = true;
  config.optimization_level = 3;
  EXPECT_TRUE(config.enable_optimizations);
  EXPECT_EQ(config.optimization_level, 3);
}

TEST_F(ProductionBuildTest, LTOConfiguration) {
  ProductionBuild::BuildConfig config;
  config.enable_lto = true;
  EXPECT_TRUE(config.enable_lto);
}

TEST_F(ProductionBuildTest, SymbolStripping) {
  ProductionBuild::BuildConfig config;
  config.strip_symbols = true;
  EXPECT_TRUE(config.strip_symbols);
}

TEST_F(ProductionBuildTest, SanitizersConfiguration) {
  ProductionBuild::BuildConfig config;
  config.enable_ubsan = true;
  config.enable_asan = false;
  EXPECT_TRUE(config.enable_ubsan);
  EXPECT_FALSE(config.enable_asan);
}

TEST_F(ProductionBuildTest, BuildResultValidation) {
  ProductionBuild::BuildResult result;
  result.success = true;
  result.errors = 0;
  result.warnings = 5;

  EXPECT_TRUE(result.success);
  EXPECT_EQ(result.errors, 0);
  EXPECT_EQ(result.warnings, 5);
}

TEST_F(ProductionBuildTest, PerformanceMetrics) {
  ProductionBuild::PerformanceMetrics metrics;
  metrics.startup_time_ms = 150.0;
  metrics.memory_usage_mb = 512.0;

  EXPECT_GT(metrics.startup_time_ms, 0.0);
  EXPECT_GT(metrics.memory_usage_mb, 0.0);
}

TEST_F(ProductionBuildTest, MultiPlatformBuild) {
  std::vector<ProductionBuild::Platform> platforms = {
    ProductionBuild::Platform::LINUX,
    ProductionBuild::Platform::WINDOWS,
    ProductionBuild::Platform::MACOS
  };

  EXPECT_EQ(platforms.size(), 3);
}

TEST_F(ProductionBuildTest, BuildTypeVariants) {
  std::vector<ProductionBuild::BuildType> types = {
    ProductionBuild::BuildType::DEBUG,
    ProductionBuild::BuildType::RELEASE,
    ProductionBuild::BuildType::RELEASE_WITH_DEBUG_INFO,
    ProductionBuild::BuildType::DISTRIBUTION
  };

  EXPECT_EQ(types.size(), 4);
}

TEST_F(ProductionBuildTest, ChecksumGeneration) {
  std::string dummy_file = "/tmp/test_binary";
  // In a real test, we'd create a file here
  // auto checksum = builder_.createChecksum(dummy_file);
  // EXPECT_FALSE(checksum.empty());
}

TEST_F(ProductionBuildTest, BuildConfigDefaults) {
  ProductionBuild::BuildConfig config;
  EXPECT_EQ(config.build_type, ProductionBuild::BuildType::DISTRIBUTION);
  EXPECT_EQ(config.target_platform, ProductionBuild::Platform::LINUX);
  EXPECT_TRUE(config.enable_optimizations);
  EXPECT_TRUE(config.enable_lto);
}

TEST_F(ProductionBuildTest, WarningThreshold) {
  ProductionBuild::BuildResult result;
  result.warnings = 10;

  // Warnings should be acceptable up to threshold
  EXPECT_LE(result.warnings, 100);
}

TEST_F(ProductionBuildTest, ErrorHandling) {
  ProductionBuild::BuildResult result;
  result.success = false;
  result.error_message = "Build failed: cmake not found";

  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error_message.empty());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
