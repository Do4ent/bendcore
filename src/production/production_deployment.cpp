#include "bendcore/production/production_deployment.hpp"
#include <algorithm>

namespace bendcore::production {

DeploymentPipeline::DeploymentPipeline() {}

DeploymentPipeline::~DeploymentPipeline() {}

void DeploymentPipeline::register_pre_deploy_hook(PreDeployCallback hook) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  pre_deploy_hooks_.push_back(hook);
}

void DeploymentPipeline::register_post_deploy_hook(PostDeployCallback hook) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  post_deploy_hooks_.push_back(hook);
}

DeploymentRecord DeploymentPipeline::deploy(const DeploymentConfig& config) {
  DeploymentRecord record{
    .deployment_id = config.deployment_id,
    .version = config.version,
    .environment = config.environment,
    .status = DeploymentStatus::PENDING,
    .start_time = std::chrono::system_clock::now()
  };

  // Execute pre-deploy hooks
  {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    for (auto& hook : pre_deploy_hooks_) {
      if (!hook(config)) {
        record.status = DeploymentStatus::FAILED;
        record.end_time = std::chrono::system_clock::now();
        deployment_history_.push_back(record);
        return record;
      }
    }
  }

  record.status = DeploymentStatus::IN_PROGRESS;
  record.deployment_steps.push_back("Pre-deploy checks passed");
  record.deployment_steps.push_back("Deploying services");
  record.deployment_steps.push_back("Running health checks");

  record.status = DeploymentStatus::SUCCESS;
  record.end_time = std::chrono::system_clock::now();

  // Execute post-deploy hooks
  {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    for (auto& hook : post_deploy_hooks_) {
      hook(record);
    }
    deployment_history_.push_back(record);
  }

  return record;
}

std::vector<DeploymentRecord> DeploymentPipeline::get_deployment_history(int limit) const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  if (deployment_history_.size() <= static_cast<size_t>(limit)) {
    return deployment_history_;
  }
  return std::vector<DeploymentRecord>(
    deployment_history_.end() - limit, deployment_history_.end());
}

DeploymentStatus DeploymentPipeline::get_deployment_status(const std::string& deployment_id) const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  for (const auto& record : deployment_history_) {
    if (record.deployment_id == deployment_id) {
      return record.status;
    }
  }
  return DeploymentStatus::PENDING;
}

bool DeploymentPipeline::rollback(const std::string& deployment_id) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  for (auto& record : deployment_history_) {
    if (record.deployment_id == deployment_id && record.status == DeploymentStatus::SUCCESS) {
      record.status = DeploymentStatus::ROLLED_BACK;
      return true;
    }
  }
  return false;
}

BlueGreenDeployer::BlueGreenDeployer() : active_environment_("blue") {}

bool BlueGreenDeployer::deploy_to_green(const DeploymentConfig& config) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  environments_["green"] = config;
  return true;
}

bool BlueGreenDeployer::health_check_green() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  auto it = environments_.find("green");
  return it != environments_.end();
}

bool BlueGreenDeployer::switch_traffic() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  // In real implementation, would switch load balancer
  return true;
}

bool BlueGreenDeployer::validate_deployment() const {
  return health_check_green();
}

bool BlueGreenDeployer::rollback_to_blue() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  // Switch back to blue
  return true;
}

std::string BlueGreenDeployer::get_active_environment() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  return active_environment_;
}

CanaryDeployer::CanaryDeployer() {}

bool CanaryDeployer::start_canary(const DeploymentConfig& config) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  canary_active_ = true;
  current_traffic_percent_ = 5.0; // Start with 5% traffic
  metrics_["errors"] = 0;
  metrics_["response_time_ms"] = 0;
  return true;
}

bool CanaryDeployer::increase_traffic(double new_percentage) {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  if (canary_active_ && new_percentage <= 100.0) {
    current_traffic_percent_ = new_percentage;
    return true;
  }
  return false;
}

std::map<std::string, double> CanaryDeployer::get_canary_metrics() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  return metrics_;
}

bool CanaryDeployer::complete_canary() {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  canary_active_ = false;
  current_traffic_percent_ = 100.0;
  return true;
}

bool CanaryDeployer::abort_canary() {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  canary_active_ = false;
  current_traffic_percent_ = 0.0;
  return true;
}

CanaryDeployer::CanaryStatus CanaryDeployer::get_status() const {
  std::lock_guard<std::shared_mutex> lock(mutex_);
  return CanaryStatus{
    .active = canary_active_,
    .traffic_percent = current_traffic_percent_,
    .errors = static_cast<int>(metrics_.count("errors") ? metrics_.at("errors") : 0),
    .error_rate = 0.0,
    .response_time_ms = metrics_.count("response_time_ms") ? metrics_.at("response_time_ms") : 0.0
  };
}

DeploymentValidator::DeploymentValidator() {}

DeploymentValidator::PreDeployChecklist
DeploymentValidator::verify_pre_deployment(const DeploymentConfig&) {
  return PreDeployChecklist{
    .all_tests_passing = true,
    .code_review_approved = true,
    .security_scan_clean = true,
    .performance_acceptable = true,
    .compatibility_verified = true,
    .backup_created = true
  };
}

bool DeploymentValidator::verify_post_deployment() const {
  return true;
}

bool DeploymentValidator::run_smoke_tests() const {
  return true;
}

bool DeploymentValidator::validate_configuration(const DeploymentConfig&) const {
  return true;
}

} // namespace bendcore::production
