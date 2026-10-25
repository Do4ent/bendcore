#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace bendcore::deployment {

// Cloud Deployment System (CAD-CORE-044)
class CloudDeployment {
 public:
  enum class CloudProvider {
    AWS,          // Amazon Web Services
    AZURE,        // Microsoft Azure
    GCP,          // Google Cloud Platform
    DIGITALOCEAN, // DigitalOcean
    DOCKER        // Docker containers
  };

  enum class DeploymentEnvironment {
    STAGING,
    PRODUCTION,
    CANARY,
    BLUE_GREEN
  };

  struct CloudConfig {
    CloudProvider provider = CloudProvider::AWS;
    std::string region = "us-east-1";
    std::string instance_type = "t3.medium";
    int num_instances = 2;
    bool enable_auto_scaling = true;
    int min_instances = 1;
    int max_instances = 10;
    std::string database_type = "PostgreSQL";
    std::string cache_type = "Redis";
    bool enable_cdn = true;
    bool enable_ssl = true;
    std::string domain_name;
  };

  struct DeploymentStatus {
    bool success = true;
    std::string deployment_id;
    DeploymentEnvironment environment = DeploymentEnvironment::STAGING;
    std::string endpoint_url;
    std::vector<std::string> instance_ids;
    std::string error_message;
    double deployment_time_seconds = 0.0;
  };

  struct HealthCheck {
    bool is_healthy = true;
    double response_time_ms = 0.0;
    int http_status = 200;
    std::string error_message;
    std::map<std::string, double> metrics;  // cpu, memory, disk, etc.
  };

  CloudDeployment();

  // Deploy application to cloud
  DeploymentStatus deployApplication(
    const std::string& binary_path,
    const CloudConfig& config,
    DeploymentEnvironment environment = DeploymentEnvironment::STAGING
  );

  // Generate Docker container
  bool generateDockerImage(
    const std::string& binary_path,
    const std::string& output_dir,
    const std::string& docker_repo = "bendcore"
  );

  // Deploy Docker container
  DeploymentStatus deployDockerContainer(
    const std::string& image_name,
    const CloudConfig& config
  );

  // Generate Kubernetes manifests
  bool generateKubernetesManifests(
    const std::string& output_dir,
    const CloudConfig& config,
    const std::string& image_name
  );

  // Deploy to Kubernetes
  DeploymentStatus deployToKubernetes(
    const std::string& manifests_dir,
    const std::string& namespace_name = "bendcore"
  );

  // Health check
  HealthCheck performHealthCheck(const std::string& endpoint_url);

  // Monitor deployment
  std::string monitorDeployment(const std::string& deployment_id);

  // Rollback deployment
  bool rollbackDeployment(const std::string& deployment_id);

  // Generate infrastructure-as-code (Terraform)
  bool generateTerraform(
    const std::string& output_dir,
    const CloudConfig& config
  );

  // Generate CloudFormation template (AWS)
  bool generateCloudFormation(
    const std::string& output_dir,
    const CloudConfig& config
  );

  // Generate Azure Resource Manager template
  bool generateAzureTemplate(
    const std::string& output_dir,
    const CloudConfig& config
  );

  // Configure CI/CD pipeline
  bool setupCIPipeline(
    const std::string& git_repo_url,
    CloudProvider provider
  );

  // Generate GitHub Actions workflow
  bool generateGitHubActions(const std::string& output_path);

  // Generate GitLab CI/CD
  bool generateGitLabCI(const std::string& output_path);

  // Configure auto-scaling
  bool configureAutoScaling(
    const CloudConfig& config,
    const std::string& deployment_id
  );

  // Set up monitoring and logging
  bool setupMonitoring(
    const std::string& deployment_id,
    bool enable_cloudwatch = true,
    bool enable_stackdriver = true,
    bool enable_elk = false
  );

  // Generate deployment report
  std::string generateDeploymentReport(const DeploymentStatus& status);

 private:
  // Cloud provider specific implementations
  DeploymentStatus deployToAWS(
    const std::string& binary_path,
    const CloudConfig& config
  );

  DeploymentStatus deployToAzure(
    const std::string& binary_path,
    const CloudConfig& config
  );

  DeploymentStatus deployToGCP(
    const std::string& binary_path,
    const CloudConfig& config
  );

  DeploymentStatus deployToDigitalOcean(
    const std::string& binary_path,
    const CloudConfig& config
  );

  // Docker operations
  bool buildDockerImage(
    const std::string& dockerfile_path,
    const std::string& image_name
  );

  bool pushDockerImage(
    const std::string& image_name,
    const std::string& registry
  );

  // Kubernetes operations
  bool applyKubernetesConfig(const std::string& config_file);

  // Helper functions
  std::string generateDeploymentId();

  std::pair<bool, std::string> executeCommand(
    const std::string& command
  );

  CloudConfig current_config_;
  std::map<std::string, DeploymentStatus> deployment_history_;
};

}  // namespace bendcore::deployment
