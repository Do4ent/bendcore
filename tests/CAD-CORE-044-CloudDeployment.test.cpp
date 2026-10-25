#include <gtest/gtest.h>
#include "../src/deployment/CloudDeployment.hpp"

using namespace bendcore::deployment;

class CloudDeploymentTest : public ::testing::Test {
 protected:
  CloudDeployment cloud_;
};

TEST_F(CloudDeploymentTest, CloudProviderVariants) {
  std::vector<CloudDeployment::CloudProvider> providers = {
    CloudDeployment::CloudProvider::AWS,
    CloudDeployment::CloudProvider::AZURE,
    CloudDeployment::CloudProvider::GCP,
    CloudDeployment::CloudProvider::DIGITALOCEAN,
    CloudDeployment::CloudProvider::DOCKER
  };

  EXPECT_EQ(providers.size(), 5);
}

TEST_F(CloudDeploymentTest, DeploymentEnvironments) {
  std::vector<CloudDeployment::DeploymentEnvironment> envs = {
    CloudDeployment::DeploymentEnvironment::STAGING,
    CloudDeployment::DeploymentEnvironment::PRODUCTION,
    CloudDeployment::DeploymentEnvironment::CANARY,
    CloudDeployment::DeploymentEnvironment::BLUE_GREEN
  };

  EXPECT_EQ(envs.size(), 4);
}

TEST_F(CloudDeploymentTest, CloudConfigStructure) {
  CloudDeployment::CloudConfig config;
  config.provider = CloudDeployment::CloudProvider::AWS;
  config.region = "us-east-1";
  config.num_instances = 2;
  config.enable_auto_scaling = true;

  EXPECT_EQ(config.provider, CloudDeployment::CloudProvider::AWS);
  EXPECT_EQ(config.region, "us-east-1");
  EXPECT_EQ(config.num_instances, 2);
  EXPECT_TRUE(config.enable_auto_scaling);
}

TEST_F(CloudDeploymentTest, DeploymentStatusStructure) {
  CloudDeployment::DeploymentStatus status;
  status.success = true;
  status.environment = CloudDeployment::DeploymentEnvironment::PRODUCTION;
  status.endpoint_url = "https://api.example.com";

  EXPECT_TRUE(status.success);
  EXPECT_FALSE(status.endpoint_url.empty());
}

TEST_F(CloudDeploymentTest, InstanceConfiguration) {
  CloudDeployment::CloudConfig config;
  config.min_instances = 1;
  config.max_instances = 10;
  config.num_instances = 3;

  EXPECT_LE(config.min_instances, config.num_instances);
  EXPECT_LE(config.num_instances, config.max_instances);
}

TEST_F(CloudDeploymentTest, HealthCheckStructure) {
  CloudDeployment::HealthCheck check;
  check.is_healthy = true;
  check.response_time_ms = 50.0;
  check.http_status = 200;

  EXPECT_TRUE(check.is_healthy);
  EXPECT_GT(check.response_time_ms, 0.0);
  EXPECT_EQ(check.http_status, 200);
}

TEST_F(CloudDeploymentTest, MetricsCollection) {
  CloudDeployment::HealthCheck check;
  check.metrics["cpu"] = 25.0;
  check.metrics["memory"] = 512.0;
  check.metrics["disk"] = 80.0;

  EXPECT_EQ(check.metrics.size(), 3);
  EXPECT_EQ(check.metrics["cpu"], 25.0);
}

TEST_F(CloudDeploymentTest, AutoScalingConfiguration) {
  CloudDeployment::CloudConfig config;
  config.enable_auto_scaling = true;
  config.min_instances = 2;
  config.max_instances = 20;

  EXPECT_TRUE(config.enable_auto_scaling);
  EXPECT_GE(config.max_instances, config.min_instances);
}

TEST_F(CloudDeploymentTest, DatabaseConfiguration) {
  CloudDeployment::CloudConfig config;
  config.database_type = "PostgreSQL";
  config.cache_type = "Redis";

  EXPECT_EQ(config.database_type, "PostgreSQL");
  EXPECT_EQ(config.cache_type, "Redis");
}

TEST_F(CloudDeploymentTest, SecurityConfiguration) {
  CloudDeployment::CloudConfig config;
  config.enable_ssl = true;
  config.enable_cdn = true;
  config.domain_name = "bendcore.com";

  EXPECT_TRUE(config.enable_ssl);
  EXPECT_TRUE(config.enable_cdn);
  EXPECT_EQ(config.domain_name, "bendcore.com");
}

TEST_F(CloudDeploymentTest, DeploymentRegions) {
  std::vector<std::string> regions = {
    "us-east-1",
    "eu-west-1",
    "ap-southeast-1",
    "us-west-2"
  };

  EXPECT_EQ(regions.size(), 4);
}

TEST_F(CloudDeploymentTest, InstanceTypes) {
  std::vector<std::string> types = {
    "t3.micro",
    "t3.small",
    "t3.medium",
    "t3.large",
    "m5.xlarge"
  };

  EXPECT_EQ(types.size(), 5);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
