#include "bendcore/ab_testing.hpp"
#include <cassert>
#include <iostream>
#include <map>

using namespace bendcore;

void test_50_50_split() {
  std::cout << "TEST: 50/50 split distribution\n";

  ABTestManager& manager = ABTestManager::getInstance();

  ABTestManager::ExperimentConfig config;
  config.experiment_id = "test_exp_50_50";
  config.control_percentage = 50.0;
  config.force_variant = false;

  // Count how many users get control vs treatment
  int control_count = 0;
  int treatment_count = 0;
  const int total_users = 1000;

  for (int i = 0; i < total_users; ++i) {
    std::string user_id = "user_" + std::to_string(i);
    auto variant = manager.get_variant(config, user_id);

    if (variant == ABTestManager::Variant::CONTROL) {
      control_count++;
    } else {
      treatment_count++;
    }
  }

  // Allow 10% variance in split (should be close to 50/50)
  int expected = total_users / 2;
  int tolerance = total_users / 10;  // 10% tolerance

  assert(control_count >= expected - tolerance && control_count <= expected + tolerance);
  assert(treatment_count >= expected - tolerance && treatment_count <= expected + tolerance);

  std::cout << "PASS: 50/50 split (Control: " << control_count << ", Treatment: " << treatment_count << ")\n";
}

void test_consistent_variant_per_user() {
  std::cout << "TEST: Consistent variant per user\n";

  ABTestManager& manager = ABTestManager::getInstance();

  ABTestManager::ExperimentConfig config;
  config.experiment_id = "test_exp_consistent";
  config.control_percentage = 50.0;
  config.force_variant = false;

  std::string user_id = "user_consistent_123";

  // Get variant multiple times
  auto variant1 = manager.get_variant(config, user_id);
  auto variant2 = manager.get_variant(config, user_id);
  auto variant3 = manager.get_variant(config, user_id);

  // Should always get the same variant
  assert(variant1 == variant2);
  assert(variant2 == variant3);

  std::cout << "PASS: Consistent variant per user\n";
}

void test_user_id_hashing() {
  std::cout << "TEST: User ID hashing\n";

  ABTestManager& manager = ABTestManager::getInstance();

  ABTestManager::ExperimentConfig config;
  config.experiment_id = "test_exp_hashing";
  config.control_percentage = 50.0;
  config.force_variant = false;

  // Different user IDs should potentially get different variants
  auto variant_user1 = manager.get_variant(config, "user_1");
  auto variant_user2 = manager.get_variant(config, "user_2");
  auto variant_user3 = manager.get_variant(config, "user_3");

  // Not all should be the same (extremely unlikely with proper hashing)
  bool all_same = (variant_user1 == variant_user2) && (variant_user2 == variant_user3);

  // With proper hashing, this should be false 99.9% of the time
  assert(!all_same);

  std::cout << "PASS: User ID hashing\n";
}

void test_variant_assignment() {
  std::cout << "TEST: Variant assignment\n";

  ABTestManager& manager = ABTestManager::getInstance();

  ABTestManager::ExperimentConfig config;
  config.experiment_id = "test_exp_assignment";
  config.control_percentage = 30.0;  // 30% control, 70% treatment
  config.force_variant = false;

  auto variant = manager.get_variant(config, "user_test");

  assert(variant == ABTestManager::Variant::CONTROL ||
         variant == ABTestManager::Variant::TREATMENT);

  std::cout << "PASS: Variant assignment\n";
}

void test_force_variant_for_testing() {
  std::cout << "TEST: Force variant for testing\n";

  ABTestManager& manager = ABTestManager::getInstance();

  ABTestManager::ExperimentConfig config;
  config.experiment_id = "test_exp_force";
  config.control_percentage = 50.0;
  config.force_variant = true;
  config.forced_variant = ABTestManager::Variant::TREATMENT;

  // Should always return the forced variant
  for (int i = 0; i < 10; ++i) {
    auto variant = manager.get_variant(config, "user_" + std::to_string(i));
    assert(variant == ABTestManager::Variant::TREATMENT);
  }

  std::cout << "PASS: Force variant for testing\n";
}

void test_experiment_configuration() {
  std::cout << "TEST: Experiment configuration\n";

  ABTestManager& manager = ABTestManager::getInstance();

  ABTestManager::ExperimentConfig config;
  config.experiment_id = "template_match_v1";
  config.control_percentage = 50.0;
  config.force_variant = false;

  manager.register_experiment(config);

  auto retrieved = manager.get_experiment("template_match_v1");
  assert(retrieved != nullptr);
  assert(retrieved->experiment_id == "template_match_v1");
  assert(retrieved->control_percentage == 50.0);

  std::cout << "PASS: Experiment configuration\n";
}

void test_variant_string_representation() {
  std::cout << "TEST: Variant string representation\n";

  ABTestManager& manager = ABTestManager::getInstance();

  ABTestManager::ExperimentConfig config;
  config.experiment_id = "test_exp_string";
  config.control_percentage = 50.0;
  config.force_variant = false;

  auto variant_str = manager.get_variant_string(config, "user_test");

  assert(variant_str == "control" || variant_str == "treatment");

  std::cout << "PASS: Variant string representation\n";
}

int main() {
  std::cout << "\n=== A/B Testing Manager Tests ===\n\n";

  test_50_50_split();
  test_consistent_variant_per_user();
  test_user_id_hashing();
  test_variant_assignment();
  test_force_variant_for_testing();
  test_experiment_configuration();
  test_variant_string_representation();

  std::cout << "\n=== All A/B Testing Tests Passed ===\n\n";

  return 0;
}
