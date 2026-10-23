#pragma once

#include <string>
#include <map>
#include <cstdint>

namespace bendcore {

/**
 * A/B Test Manager
 *
 * Provides hash-based variant assignment for feature experimentation.
 * Ensures consistent variant assignment per user (same user always gets same variant).
 */
class ABTestManager {
public:
  enum class Variant {
    CONTROL = 0,      // Original/control behavior
    TREATMENT = 1,    // New/treatment feature
  };

  struct ExperimentConfig {
    std::string experiment_id;    // e.g., "template_match_v1", "search_ui_v2"
    double control_percentage;    // Percentage in control (50.0 for 50/50 split)
    bool force_variant = false;   // Force variant for testing
    Variant forced_variant = Variant::CONTROL;
  };

  /**
   * Get singleton instance
   */
  static ABTestManager& getInstance();

  /**
   * Get variant for user in an experiment
   *
   * Uses hash-based assignment: consistent per user/experiment pair
   * @param config Experiment configuration
   * @param user_id User identifier
   * @return CONTROL or TREATMENT variant
   */
  Variant get_variant(const ExperimentConfig& config, const std::string& user_id);

  /**
   * Get variant as string
   */
  std::string get_variant_string(const ExperimentConfig& config, const std::string& user_id);

  /**
   * Register an active experiment
   */
  void register_experiment(const ExperimentConfig& config);

  /**
   * Get registered experiment config
   */
  ExperimentConfig* get_experiment(const std::string& experiment_id);

  /**
   * Get all registered experiments
   */
  std::map<std::string, ExperimentConfig> get_experiments() const;

private:
  ABTestManager() = default;

  /**
   * Hash user ID + experiment ID to a consistent value
   * @return Hash value modulo 100 (0-99)
   */
  uint32_t hash_user_to_variant(const std::string& user_id, const std::string& exp_id);

  std::map<std::string, ExperimentConfig> experiments_;
};

}  // namespace bendcore
