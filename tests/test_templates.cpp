/**
 * Template System Unit Tests
 * Tests for template data model, auto-selection, and CRUD operations
 * Task 2.7: Testing (8+ unit tests)
 */

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include "bendcore/templates.hpp"

using namespace bendcore;
namespace fs = std::filesystem;

class TemplateManagerTest : public ::testing::Test {
 protected:
  TemplateManager manager;
  std::string test_file = "/tmp/test_templates.json";

  void SetUp() override {
    // Clean up test file if it exists
    if (fs::exists(test_file)) {
      fs::remove(test_file);
    }
  }

  void TearDown() override {
    // Clean up test file
    if (fs::exists(test_file)) {
      fs::remove(test_file);
    }
  }
};

// ============ UNIT TESTS ============

/**
 * Test 1: Load templates from JSON
 */
TEST_F(TemplateManagerTest, LoadTemplatesFromJSON) {
  // Use the actual templates.json from data directory
  const std::string templates_path = "/home/claude/bendcore/data/templates.json";

  bool result = manager.load_templates(templates_path);
  ASSERT_TRUE(result) << "Failed to load templates from " << templates_path;

  const auto& templates = manager.get_all_templates();
  EXPECT_GE(templates.size(), 3) << "Should have at least 3 predefined templates";

  // Check for the three main spec templates
  auto standard_90 = manager.get_template("std-90");
  auto reinforced_small = manager.get_template("rfd-small");
  auto large_asm = manager.get_template("lg-asm");

  EXPECT_TRUE(standard_90.has_value()) << "Should have Standard 90° template";
  EXPECT_TRUE(reinforced_small.has_value()) << "Should have Reinforced Small template";
  EXPECT_TRUE(large_asm.has_value()) << "Should have Large Assembly template";
}

/**
 * Test 2: Auto-selection for Standard 90° tube (OD=25, Wall=2.0)
 */
TEST_F(TemplateManagerTest, AutoSelectStandard90Tube) {
  const std::string templates_path = "/home/claude/bendcore/data/templates.json";
  manager.load_templates(templates_path);

  const auto& library = manager.get_library();

  // Test standard parameters: OD=25, Wall=2.0, Bend=90°, Radius=2.0
  auto matches = TemplateMatcher::find_best_matches(
      library, 25.0, 2.0, 90.0, 2.0, 20.0, 3);

  EXPECT_GE(matches.size(), 1) << "Should return at least one match";
  EXPECT_LE(matches.size(), 3) << "Should return at most 3 matches";

  // Best match should be Standard 90° template
  EXPECT_EQ(matches[0].template_id, "std-90");
  EXPECT_GE(matches[0].confidence, 85.0) << "Should have high confidence for standard 90°";
}

/**
 * Test 3: Auto-selection for Small-OD tube (OD=12, Wall=1.8)
 */
TEST_F(TemplateManagerTest, AutoSelectSmallODTube) {
  const std::string templates_path = "/home/claude/bendcore/data/templates.json";
  manager.load_templates(templates_path);

  const auto& library = manager.get_library();

  // Test small-OD parameters: OD=12, Wall=1.8, Bend=90°, Radius=2.5
  auto matches = TemplateMatcher::find_best_matches(
      library, 12.0, 1.8, 90.0, 2.5, 25.0, 3);

  EXPECT_GE(matches.size(), 1) << "Should return at least one match";

  // Best match should be Reinforced Small template
  EXPECT_EQ(matches[0].template_id, "rfd-small");
  EXPECT_GE(matches[0].confidence, 80.0) << "Should have high confidence for small-OD";
}

/**
 * Test 4: Auto-selection for Large Assembly (OD=100, Wall=4.0)
 */
TEST_F(TemplateManagerTest, AutoSelectLargeAssemblyTube) {
  const std::string templates_path = "/home/claude/bendcore/data/templates.json";
  manager.load_templates(templates_path);

  const auto& library = manager.get_library();

  // Test large assembly parameters: OD=100, Wall=4.0, Bend=90°, Radius=4.0
  auto matches = TemplateMatcher::find_best_matches(
      library, 100.0, 4.0, 90.0, 4.0, 50.0, 3);

  EXPECT_GE(matches.size(), 1) << "Should return at least one match";

  // Best match should be Large Assembly template
  EXPECT_EQ(matches[0].template_id, "lg-asm");
  EXPECT_GE(matches[0].confidence, 80.0) << "Should have high confidence for large assembly";
}

/**
 * Test 5: Template CRUD - Create, Read, Update, Delete
 */
TEST_F(TemplateManagerTest, TemplateCRUDOperations) {
  // Create a custom template
  Template custom_template;
  custom_template.id = "custom-test";
  custom_template.name = "Custom Test Template";
  custom_template.description = "Test template";
  custom_template.category = "Custom";
  custom_template.use_case = "Testing";

  custom_template.outer_diameter = {20.0, 40.0, 30.0, "mm"};
  custom_template.wall_thickness = {1.5, 2.5, 2.0, "mm"};
  custom_template.length = {100.0, 600.0, 350.0, "mm"};
  custom_template.compatible_materials = {"steel-1020", "aluminum-6061"};

  // Create (Add)
  manager.add_template(custom_template);
  EXPECT_EQ(manager.get_all_templates().size(), 1);

  // Read
  auto retrieved = manager.get_template("custom-test");
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->name, "Custom Test Template");

  // Update
  custom_template.name = "Updated Custom Template";
  manager.remove_template("custom-test");
  manager.add_template(custom_template);

  auto updated = manager.get_template("custom-test");
  ASSERT_TRUE(updated.has_value());
  EXPECT_EQ(updated->name, "Updated Custom Template");

  // Delete
  bool deleted = manager.remove_template("custom-test");
  EXPECT_TRUE(deleted);
  EXPECT_FALSE(manager.get_template("custom-test").has_value());
}

/**
 * Test 6: Custom template creation and persistence
 */
TEST_F(TemplateManagerTest, CustomTemplatePersistence) {
  const std::string templates_path = "/home/claude/bendcore/data/templates.json";
  manager.load_templates(templates_path);

  // Create a custom template
  Template custom_template;
  custom_template.id = "custom-persistent";
  custom_template.name = "Persistent Custom";
  custom_template.description = "Should be saved";
  custom_template.category = "Custom";
  custom_template.use_case = "Persistence test";

  custom_template.outer_diameter = {12.0, 48.0, 30.0, "mm"};
  custom_template.wall_thickness = {1.0, 3.0, 2.0, "mm"};
  custom_template.length = {30.0, 900.0, 400.0, "mm"};
  custom_template.compatible_materials = {};

  manager.add_template(custom_template);
  manager.set_active_template("custom-persistent");

  // Save to file
  bool saved = manager.save_templates(test_file);
  EXPECT_TRUE(saved);
  EXPECT_TRUE(fs::exists(test_file));

  // Create new manager and load from file
  TemplateManager manager2;
  bool loaded = manager2.load_templates(test_file);
  EXPECT_TRUE(loaded);

  // Verify the custom template was persisted
  auto retrieved = manager2.get_template("custom-persistent");
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->name, "Persistent Custom");

  // Verify active template was persisted
  auto active = manager2.get_active_template();
  ASSERT_TRUE(active.has_value());
  EXPECT_EQ(active->id, "custom-persistent");
}

/**
 * Test 7: Template export/import
 */
TEST_F(TemplateManagerTest, TemplateExportImport) {
  const std::string templates_path = "/home/claude/bendcore/data/templates.json";
  manager.load_templates(templates_path);

  size_t original_count = manager.get_all_templates().size();

  // Export current templates
  bool exported = manager.save_templates(test_file);
  EXPECT_TRUE(exported);

  // Create new manager and import
  TemplateManager manager2;
  bool imported = manager2.load_templates(test_file);
  EXPECT_TRUE(imported);

  // Verify all templates were imported
  EXPECT_EQ(manager2.get_all_templates().size(), original_count);

  // Verify specific templates
  for (const auto& original : manager.get_all_templates()) {
    auto imported_template = manager2.get_template(original.id);
    ASSERT_TRUE(imported_template.has_value());
    EXPECT_EQ(imported_template->name, original.name);
  }
}

/**
 * Test 8: Top 3 candidates ranking
 */
TEST_F(TemplateManagerTest, Top3CandidatesRanking) {
  const std::string templates_path = "/home/claude/bendcore/data/templates.json";
  manager.load_templates(templates_path);

  const auto& library = manager.get_library();

  // Use an ambiguous parameter set that could match multiple templates
  auto matches = TemplateMatcher::find_best_matches(
      library, 32.0, 2.5, 90.0, 2.0, 20.0, 3);

  EXPECT_LE(matches.size(), 3) << "Should return at most 3 candidates";

  // Verify confidence scores are in descending order
  for (size_t i = 1; i < matches.size(); ++i) {
    EXPECT_GE(matches[i - 1].confidence, matches[i].confidence)
      << "Confidence scores should be in descending order";
  }

  // Verify all confidence scores are valid (0-100)
  for (const auto& match : matches) {
    EXPECT_GE(match.confidence, 0.0);
    EXPECT_LE(match.confidence, 100.0);
  }
}

/**
 * Test 9: Constraint violations detection
 */
TEST_F(TemplateManagerTest, ConstraintViolationsDetection) {
  const std::string templates_path = "/home/claude/bendcore/data/templates.json";
  manager.load_templates(templates_path);

  auto std_90 = manager.get_template("std-90");
  ASSERT_TRUE(std_90.has_value());

  // Test OD violation (too large)
  auto violations = TemplateMatcher::check_constraint_violations(
      *std_90, 200.0, 2.0, 90.0, 2.0, 20.0);
  EXPECT_FALSE(violations.empty()) << "Should detect OD violation";

  // Test wall thickness violation (too thin)
  violations = TemplateMatcher::check_constraint_violations(
      *std_90, 25.0, 0.1, 90.0, 2.0, 20.0);
  EXPECT_FALSE(violations.empty()) << "Should detect wall thickness violation";

  // Test bend angle violation
  violations = TemplateMatcher::check_constraint_violations(
      *std_90, 25.0, 2.0, 200.0, 2.0, 20.0);
  EXPECT_FALSE(violations.empty()) << "Should detect bend angle violation";
}

/**
 * Test 10: Recommendations generation
 */
TEST_F(TemplateManagerTest, RecommendationsGeneration) {
  const std::string templates_path = "/home/claude/bendcore/data/templates.json";
  manager.load_templates(templates_path);

  auto std_90 = manager.get_template("std-90");
  ASSERT_TRUE(std_90.has_value());

  auto violations = TemplateMatcher::check_constraint_violations(
      *std_90, 5.0, 2.0, 90.0, 2.0, 20.0);  // OD too small

  auto recommendations = TemplateMatcher::generate_recommendations(
      *std_90, 5.0, 2.0, violations);
  EXPECT_FALSE(recommendations.empty()) << "Should generate recommendations for OD violation";
}

// ============ PARAMETER SCORE TESTS ============

/**
 * Test parameter score calculation
 */
TEST(TemplateParameterScoreTest, ParameterScoreWithinRange) {
  TemplateParameter param = {10.0, 20.0, 15.0, "mm"};

  // Within range - should return 1.0
  double score = TemplateSelector::calculate_parameter_score(15.0, param);
  EXPECT_EQ(score, 1.0);
}

TEST(TemplateParameterScoreTest, ParameterScoreBelowRange) {
  TemplateParameter param = {10.0, 20.0, 15.0, "mm"};

  // Below range - should return less than 1.0
  double score = TemplateSelector::calculate_parameter_score(5.0, param);
  EXPECT_LT(score, 1.0);
  EXPECT_GE(score, 0.0);
}

TEST(TemplateParameterScoreTest, ParameterScoreAboveRange) {
  TemplateParameter param = {10.0, 20.0, 15.0, "mm"};

  // Above range - should return less than 1.0
  double score = TemplateSelector::calculate_parameter_score(25.0, param);
  EXPECT_LT(score, 1.0);
  EXPECT_GE(score, 0.0);
}

// ============ EDGE CASE TESTS ============

/**
 * Test with very small/large values
 */
TEST_F(TemplateManagerTest, EdgeCaseExtremeValues) {
  const std::string templates_path = "/home/claude/bendcore/data/templates.json";
  manager.load_templates(templates_path);

  const auto& library = manager.get_library();

  // Test with very small values
  auto matches_small = TemplateSelector::select_template(library, 1.0, 0.1, 10.0);
  EXPECT_LE(matches_small.size(), 3);

  // Test with very large values
  auto matches_large = TemplateSelector::select_template(library, 100.0, 5.0, 2000.0);
  EXPECT_LE(matches_large.size(), 3);
}

/**
 * Test duplicate template prevention
 */
TEST_F(TemplateManagerTest, DuplicateTemplatePrevention) {
  Template template1;
  template1.id = "duplicate-test";
  template1.name = "First";
  template1.description = "First template";
  template1.category = "Custom";
  template1.outer_diameter = {10.0, 50.0, 30.0, "mm"};
  template1.wall_thickness = {1.0, 3.0, 2.0, "mm"};
  template1.length = {50.0, 500.0, 250.0, "mm"};

  manager.add_template(template1);

  // Trying to add duplicate should throw
  EXPECT_THROW({
    manager.add_template(template1);
  }, std::runtime_error);
}

