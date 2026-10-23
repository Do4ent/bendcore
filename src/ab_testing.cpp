#include "bendcore/ab_testing.hpp"
#include <functional>
#include <sstream>

namespace bendcore {

ABTestManager& ABTestManager::getInstance() {
  static ABTestManager instance;
  return instance;
}

ABTestManager::Variant ABTestManager::get_variant(const ExperimentConfig& config,
                                                   const std::string& user_id) {
  // If variant is forced for testing, use that
  if (config.force_variant) {
    return config.forced_variant;
  }

  // Hash user + experiment to consistent value
  uint32_t hash = hash_user_to_variant(user_id, config.experiment_id);

  // If hash < control_percentage, assign CONTROL
  if (hash < static_cast<uint32_t>(config.control_percentage)) {
    return Variant::CONTROL;
  }

  return Variant::TREATMENT;
}

std::string ABTestManager::get_variant_string(const ExperimentConfig& config,
                                              const std::string& user_id) {
  auto variant = get_variant(config, user_id);
  return (variant == Variant::CONTROL) ? "control" : "treatment";
}

void ABTestManager::register_experiment(const ExperimentConfig& config) {
  experiments_[config.experiment_id] = config;
}

ABTestManager::ExperimentConfig* ABTestManager::get_experiment(const std::string& experiment_id) {
  auto it = experiments_.find(experiment_id);
  if (it != experiments_.end()) {
    return &it->second;
  }
  return nullptr;
}

std::map<std::string, ABTestManager::ExperimentConfig> ABTestManager::get_experiments() const {
  return experiments_;
}

uint32_t ABTestManager::hash_user_to_variant(const std::string& user_id,
                                             const std::string& exp_id) {
  // Combine user_id and exp_id for hashing
  std::string combined = user_id + "::" + exp_id;

  // Use std::hash to get a hash value
  std::hash<std::string> hasher;
  uint64_t hash_value = hasher(combined);

  // Convert to 0-99 range for variant assignment
  return static_cast<uint32_t>(hash_value % 100);
}

}  // namespace bendcore
