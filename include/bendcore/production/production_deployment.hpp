#ifndef BENDCORE_PRODUCTION_DEPLOYMENT_HPP
#define BENDCORE_PRODUCTION_DEPLOYMENT_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <optional>

namespace bendcore::production {

/**
 * Deployment configuration
 */
struct DeploymentConfig {
  std::string version;
  std::string environment;  // dev, staging, production
  std::string deployment_id;
  std::map<std::string, std::string> environment_vars;
  std::vector<std::string> services_to_deploy;
  bool enable_canary = false;
  double canary_traffic_percent = 5.0;
  int health_check_timeout_sec = 30;
  int rollback_timeout_sec = 300;
};

/**
 * Deployment status
 */
enum class DeploymentStatus {
  PENDING,
  IN_PROGRESS,
  VERIFYING,
  SUCCESS,
  FAILED,
  ROLLED_BACK
};

/**
 * Deployment record
 */
struct DeploymentRecord {
  std::string deployment_id;
  std::string version;
  std::string environment;
  DeploymentStatus status;
  std::chrono::system_clock::time_point start_time;
  std::chrono::system_clock::time_point end_time;
  std::string deployed_by;
  std::vector<std::string> changed_files;
  std::vector<std::string> deployment_steps;
  std::optional<std::string> rollback_id;
  std::map<std::string, std::string> metrics;
};

/**
 * Automated deployment pipeline
 */
class DeploymentPipeline {
 public:
  using PreDeployCallback = std::function<bool(const DeploymentConfig&)>;
  using PostDeployCallback = std::function<void(const DeploymentRecord&)>;

  DeploymentPipeline();
  ~DeploymentPipeline();

  // Register pre-deploy hook
  void register_pre_deploy_hook(PreDeployCallback hook);

  // Register post-deploy hook
  void register_post_deploy_hook(PostDeployCallback hook);

  // Execute deployment
  DeploymentRecord deploy(const DeploymentConfig& config);

  // Get deployment history
  std::vector<DeploymentRecord> get_deployment_history(int limit = 50) const;

  // Get current deployment status
  DeploymentStatus get_deployment_status(const std::string& deployment_id) const;

  // Rollback deployment
  bool rollback(const std::string& deployment_id);

 private:
  std::vector<PreDeployCallback> pre_deploy_hooks_;
  std::vector<PostDeployCallback> post_deploy_hooks_;
  std::vector<DeploymentRecord> deployment_history_;
  mutable std::shared_mutex mutex_;
};

/**
 * Blue-Green deployment strategy
 */
class BlueGreenDeployer {
 public:
  BlueGreenDeployer();
  ~BlueGreenDeployer() = default;

  // Deploy to green environment
  bool deploy_to_green(const DeploymentConfig& config);

  // Run health checks on green
  bool health_check_green() const;

  // Switch traffic from blue to green
  bool switch_traffic() const;

  // Validate new deployment
  bool validate_deployment() const;

  // Rollback to blue
  bool rollback_to_blue() const;

  // Get current active environment
  std::string get_active_environment() const;

 private:
  std::string active_environment_ = "blue";
  std::map<std::string, DeploymentConfig> environments_;
  mutable std::shared_mutex mutex_;
};

/**
 * Canary deployment strategy
 */
class CanaryDeployer {
 public:
  CanaryDeployer();
  ~CanaryDeployer() = default;

  // Start canary deployment
  bool start_canary(const DeploymentConfig& config);

  // Increase canary traffic
  bool increase_traffic(double new_percentage);

  // Monitor canary metrics
  std::map<std::string, double> get_canary_metrics() const;

  // Complete canary deployment
  bool complete_canary();

  // Abort canary deployment
  bool abort_canary();

  // Get canary status
  struct CanaryStatus {
    bool active;
    double traffic_percent;
    int errors;
    double error_rate;
    double response_time_ms;
  };

  CanaryStatus get_status() const;

 private:
  bool canary_active_ = false;
  double current_traffic_percent_ = 0.0;
  std::map<std::string, double> metrics_;
  mutable std::shared_mutex mutex_;
};

/**
 * Deployment verification and validation
 */
class DeploymentValidator {
 public:
  DeploymentValidator();
  ~DeploymentValidator() = default;

  // Pre-deployment checklist
  struct PreDeployChecklist {
    bool all_tests_passing = false;
    bool code_review_approved = false;
    bool security_scan_clean = false;
    bool performance_acceptable = false;
    bool compatibility_verified = false;
    bool backup_created = false;
  };

  // Verify pre-deployment readiness
  PreDeployChecklist verify_pre_deployment(const DeploymentConfig& config);

  // Verify post-deployment health
  bool verify_post_deployment() const;

  // Run smoke tests
  bool run_smoke_tests() const;

  // Validate configuration
  bool validate_configuration(const DeploymentConfig& config) const;

 private:
  std::vector<std::string> test_results_;
};

} // namespace bendcore::production

#endif // BENDCORE_PRODUCTION_DEPLOYMENT_HPP
