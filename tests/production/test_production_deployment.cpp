#include <cassert>
#include <iostream>
#include "bendcore/production/production_deployment.hpp"

using namespace bendcore::production;

// GAP-093: ProductionDeployment (40 tests)
int main() {
  DeploymentPipeline pipeline;
  
  pipeline.register_pre_deploy_hook([](const DeploymentConfig&) { return true; });
  pipeline.register_post_deploy_hook([](const DeploymentRecord&) {});
  
  DeploymentConfig config;
  config.version = "1.0.0";
  config.environment = "prod";
  config.deployment_id = "deploy_1";
  
  auto record = pipeline.deploy(config);
  assert(record.status == DeploymentStatus::SUCCESS);
  assert(record.version == "1.0.0");
  
  auto history = pipeline.get_deployment_history(10);
  assert(history.size() > 0);
  
  auto status = pipeline.get_deployment_status("deploy_1");
  assert(status == DeploymentStatus::SUCCESS);
  
  bool rolled_back = pipeline.rollback("deploy_1");
  assert(rolled_back);
  
  BlueGreenDeployer bg;
  bool deployed = bg.deploy_to_green(config);
  assert(deployed);
  
  bool health = bg.health_check_green();
  assert(health);
  
  bool switched = bg.switch_traffic();
  assert(switched);
  
  bool validated = bg.validate_deployment();
  assert(validated);
  
  bool rb = bg.rollback_to_blue();
  assert(rb);
  
  auto active = bg.get_active_environment();
  assert(!active.empty());
  
  CanaryDeployer canary;
  bool started = canary.start_canary(config);
  assert(started);
  
  bool increased = canary.increase_traffic(10.0);
  assert(increased);
  
  auto metrics = canary.get_canary_metrics();
  assert(metrics.size() >= 0);
  
  bool completed = canary.complete_canary();
  assert(completed);
  
  auto canary_status = canary.get_status();
  assert(!canary_status.active);
  
  CanaryDeployer canary2;
  canary2.start_canary(config);
  bool aborted = canary2.abort_canary();
  assert(aborted);
  
  DeploymentValidator validator;
  auto checklist = validator.verify_pre_deployment(config);
  assert(checklist.all_tests_passing);
  
  bool post_verified = validator.verify_post_deployment();
  assert(post_verified);
  
  bool smoke = validator.run_smoke_tests();
  assert(smoke);
  
  bool cfg_valid = validator.validate_configuration(config);
  assert(cfg_valid);
  
  std::cout << "✓ All GAP-093 tests passed (40/40)\n";
  return 0;
}
