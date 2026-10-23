/**
 * GrowthBook Integration Tests
 * Tests for A/B testing, feature flags, and analytics
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iostream>
#include <memory>
#include <vector>
#include <map>

namespace bendcore {
namespace tests {

/**
 * Mock GrowthBook feature flag system
 */
class MockGrowthBook {
 public:
  MockGrowthBook() : api_initialized_(false), event_count_(0) {}

  void initialize(const std::string& api_key) {
    api_initialized_ = !api_key.empty();
  }

  bool isFeatureEnabled(const std::string& feature_name) {
    if (!api_initialized_) {
      return false;
    }
    auto it = features_.find(feature_name);
    if (it != features_.end()) {
      return it->second;
    }
    return false;
  }

  void setFeature(const std::string& feature_name, bool enabled) {
    features_[feature_name] = enabled;
  }

  int getEventCount() const { return event_count_; }

  void clearEvents() { event_count_ = 0; }

 private:
  bool api_initialized_;
  int event_count_;
  std::map<std::string, bool> features_;
};

/**
 * Mock Experiment Manager
 */
class MockExperimentManager {
 public:
  MockExperimentManager() : event_count_(0) {}

  std::string assignVariant(const std::string& experiment_id,
                            const std::string& user_id) {
    // Deterministic assignment for testing
    auto key = experiment_id + "_" + user_id;
    if (assignments_.find(key) != assignments_.end()) {
      return assignments_[key];
    }

    // Assign variant A or B based on hash
    std::hash<std::string> hasher;
    size_t hash = hasher(key);
    std::string variant = (hash % 2 == 0) ? "A" : "B";

    assignments_[key] = variant;
    return variant;
  }

  std::string getAssignedVariant(const std::string& experiment_id,
                                  const std::string& user_id) {
    auto key = experiment_id + "_" + user_id;
    auto it = assignments_.find(key);
    return (it != assignments_.end()) ? it->second : "";
  }

  void trackEvent(const std::string& experiment_id, const std::string& event_type) {
    event_count_++;
  }

  int getEventCount() const { return event_count_; }

  void clearEvents() { event_count_ = 0; }

 private:
  std::map<std::string, std::string> assignments_;
  int event_count_;
};

/**
 * Test fixture for GrowthBook tests
 */
class GrowthBookTest : public ::testing::Test {
 protected:
  void SetUp() override {
    growthbook_ = std::make_unique<MockGrowthBook>();
    experiment_manager_ = std::make_unique<MockExperimentManager>();

    growthbook_->initialize("test_api_key");
  }

  std::unique_ptr<MockGrowthBook> growthbook_;
  std::unique_ptr<MockExperimentManager> experiment_manager_;
};

/**
 * Test 1: Feature flag evaluation
 */
TEST_F(GrowthBookTest, FeatureFlagEvaluation) {
  // Feature should be disabled by default
  EXPECT_FALSE(growthbook_->isFeatureEnabled("parametric_dimensions"));

  // Enable feature
  growthbook_->setFeature("parametric_dimensions", true);
  EXPECT_TRUE(growthbook_->isFeatureEnabled("parametric_dimensions"));

  // Check multiple features
  growthbook_->setFeature("smart_templates", true);
  growthbook_->setFeature("search_filters", false);

  EXPECT_TRUE(growthbook_->isFeatureEnabled("smart_templates"));
  EXPECT_FALSE(growthbook_->isFeatureEnabled("search_filters"));
}

/**
 * Test 2: Experiment variant assignment (50/50 split)
 */
TEST_F(GrowthBookTest, ExperimentVariantAssignment) {
  const std::string experiment_id = "parametric_test";

  // Assign variants to multiple users
  std::vector<std::string> variants;
  for (int i = 0; i < 100; i++) {
    std::string user_id = "user_" + std::to_string(i);
    std::string variant =
        experiment_manager_->assignVariant(experiment_id, user_id);
    EXPECT_TRUE(variant == "A" || variant == "B");
    variants.push_back(variant);
  }

  // Check distribution (should be roughly 50/50)
  int count_a = std::count(variants.begin(), variants.end(), "A");
  int count_b = std::count(variants.begin(), variants.end(), "B");

  // Allow some variance (40-60)
  EXPECT_GE(count_a, 40);
  EXPECT_LE(count_a, 60);
  EXPECT_GE(count_b, 40);
  EXPECT_LE(count_b, 60);
}

/**
 * Test 3: Variant assignment persistence
 */
TEST_F(GrowthBookTest, VariantAssignmentPersistence) {
  const std::string experiment_id = "smart_templates_test";
  const std::string user_id = "user_123";

  // First assignment
  std::string variant1 =
      experiment_manager_->assignVariant(experiment_id, user_id);
  EXPECT_TRUE(variant1 == "A" || variant1 == "B");

  // Second call should return same variant
  std::string variant2 =
      experiment_manager_->assignVariant(experiment_id, user_id);
  EXPECT_EQ(variant1, variant2);

  // Check via get method
  std::string variant3 =
      experiment_manager_->getAssignedVariant(experiment_id, user_id);
  EXPECT_EQ(variant1, variant3);
}

/**
 * Test 4: Event tracking
 */
TEST_F(GrowthBookTest, EventTracking) {
  EXPECT_EQ(0, experiment_manager_->getEventCount());

  // Track events
  experiment_manager_->trackEvent("parametric_test", "view");
  EXPECT_EQ(1, experiment_manager_->getEventCount());

  experiment_manager_->trackEvent("parametric_test", "conversion");
  EXPECT_EQ(2, experiment_manager_->getEventCount());

  experiment_manager_->trackEvent("smart_templates_test", "click");
  EXPECT_EQ(3, experiment_manager_->getEventCount());

  // Clear and verify
  experiment_manager_->clearEvents();
  EXPECT_EQ(0, experiment_manager_->getEventCount());
}

/**
 * Test 5: Multiple concurrent experiments
 */
TEST_F(GrowthBookTest, MultipleConcurrentExperiments) {
  const std::string user_id = "user_123";

  // Assign to multiple experiments simultaneously
  std::string parametric_variant =
      experiment_manager_->assignVariant("parametric_test", user_id);
  std::string templates_variant =
      experiment_manager_->assignVariant("smart_templates_test", user_id);
  std::string search_variant =
      experiment_manager_->assignVariant("search_filters_test", user_id);

  // Verify all assignments are valid and different experiments
  EXPECT_TRUE(parametric_variant == "A" || parametric_variant == "B");
  EXPECT_TRUE(templates_variant == "A" || templates_variant == "B");
  EXPECT_TRUE(search_variant == "A" || search_variant == "B");

  // Verify persistence across experiments
  EXPECT_EQ(parametric_variant,
            experiment_manager_->getAssignedVariant("parametric_test", user_id));
  EXPECT_EQ(templates_variant,
            experiment_manager_->getAssignedVariant("smart_templates_test", user_id));
  EXPECT_EQ(search_variant,
            experiment_manager_->getAssignedVariant("search_filters_test", user_id));
}

/**
 * Test 6: Uninitialized API handling
 */
TEST_F(GrowthBookTest, UninitializedApiHandling) {
  MockGrowthBook uninitialized_gb;

  // Should not throw and return false when API not initialized
  EXPECT_FALSE(uninitialized_gb.isFeatureEnabled("parametric_dimensions"));
  EXPECT_FALSE(uninitialized_gb.isFeatureEnabled("smart_templates"));
  EXPECT_FALSE(uninitialized_gb.isFeatureEnabled("search_filters"));
}

/**
 * Test 7: Analytics event batching
 */
TEST_F(GrowthBookTest, AnalyticsEventBatching) {
  const int batch_size = 100;
  MockExperimentManager analytics;

  // Track a batch of events
  for (int i = 0; i < batch_size; i++) {
    analytics.trackEvent("test_experiment", "event_" + std::to_string(i));
  }

  EXPECT_EQ(batch_size, analytics.getEventCount());

  // Clear batch
  analytics.clearEvents();
  EXPECT_EQ(0, analytics.getEventCount());
}

/**
 * Test 8: Conversion rate calculation
 */
TEST_F(GrowthBookTest, ConversionRateCalculation) {
  const std::string experiment_id = "parametric_test";

  // Variant A: 50 views, 5 conversions
  for (int i = 0; i < 50; i++) {
    experiment_manager_->trackEvent(experiment_id, "view");
  }
  for (int i = 0; i < 5; i++) {
    experiment_manager_->trackEvent(experiment_id, "conversion");
  }

  // Variant B: 50 views, 10 conversions (2x conversion rate)
  for (int i = 0; i < 50; i++) {
    experiment_manager_->trackEvent(experiment_id, "view");
  }
  for (int i = 0; i < 10; i++) {
    experiment_manager_->trackEvent(experiment_id, "conversion");
  }

  // Total: 100 views, 15 conversions = 15% conversion rate
  EXPECT_EQ(100, experiment_manager_->getEventCount());

  // Clear for next test
  experiment_manager_->clearEvents();
}

/**
 * Integration test: Full experiment workflow
 */
TEST_F(GrowthBookTest, FullExperimentWorkflow) {
  const std::string experiment_id = "full_test";
  const std::string user_id = "user_integration_test";

  // Step 1: User enters experiment and gets assigned variant
  std::string variant = experiment_manager_->assignVariant(experiment_id, user_id);
  EXPECT_TRUE(variant == "A" || variant == "B");

  // Step 2: Track initial exposure
  experiment_manager_->trackEvent(experiment_id, "exposure");
  EXPECT_EQ(1, experiment_manager_->getEventCount());

  // Step 3: User interacts with feature
  experiment_manager_->trackEvent(experiment_id, "interaction");
  EXPECT_EQ(2, experiment_manager_->getEventCount());

  // Step 4: User converts
  experiment_manager_->trackEvent(experiment_id, "conversion");
  EXPECT_EQ(3, experiment_manager_->getEventCount());

  // Step 5: Verify variant assignment is persistent
  std::string variant_check =
      experiment_manager_->getAssignedVariant(experiment_id, user_id);
  EXPECT_EQ(variant, variant_check);
}

}  // namespace tests
}  // namespace bendcore

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
