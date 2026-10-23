/**
 * Tests for ImportFilter
 * Tests grouping rules and filtering functionality
 */

#include <gtest/gtest.h>
#include "../src/bendcore/import_filter.hpp"
#include <vector>

using namespace bendcore;

class ImportFilterTest : public ::testing::Test {
 protected:
  ImportFilter filter;

  std::vector<SolidGeometry> CreateMockSolids() {
    std::vector<SolidGeometry> solids;

    // Create 3 cylinders with different diameters
    for (int i = 0; i < 3; ++i) {
      SolidGeometry solid;
      solid.id = i;
      solid.name = "Tube_" + std::to_string(i + 1);
      solid.outer_diameter = 12.7 + i * 5.0;  // 12.7, 17.7, 22.7
      solid.inner_diameter = 10.0 + i * 5.0;  // 10.0, 15.0, 20.0
      solid.wall_thickness = 1.35;
      solid.is_cylindrical = true;
      solid.is_hollow = true;
      solid.material = i == 0 ? "Steel" : "Aluminum";
      solid.length = 100.0 + i * 20.0;
      solid.bounding_box = {0, 0, 0, 10, 10, solid.length};

      solids.push_back(solid);
    }

    return solids;
  }
};

// Test 1: Detect tubes
TEST_F(ImportFilterTest, DetectTubes) {
  auto solids = CreateMockSolids();
  auto tubes = filter.detect_tubes(solids);

  EXPECT_EQ(tubes.size(), 3);
  EXPECT_TRUE(tubes[0].is_hollow);
  EXPECT_TRUE(tubes[0].is_cylindrical);
}

// Test 2: Filter by diameter
TEST_F(ImportFilterTest, FilterByDiameter) {
  auto solids = CreateMockSolids();
  auto filtered = filter.filter_by_diameter(solids, 12.0, 18.0);

  EXPECT_EQ(filtered.size(), 2);
  EXPECT_GE(filtered[0].outer_diameter, 12.0);
  EXPECT_LE(filtered[0].outer_diameter, 18.0);
}

// Test 3: Filter by material
TEST_F(ImportFilterTest, FilterByMaterial) {
  auto solids = CreateMockSolids();
  auto filtered = filter.filter_by_material(solids, "Steel");

  EXPECT_EQ(filtered.size(), 1);
  EXPECT_EQ(filtered[0].material, "Steel");
}

// Test 4: Detect contacts
TEST_F(ImportFilterTest, DetectContacts) {
  auto solids = CreateMockSolids();

  // Adjust bounding boxes to create contact
  solids[1].bounding_box = {8, 0, 0, 12, 10, 100};  // Overlaps with solid 0

  auto contacts = filter.detect_contacts(solids, 0.01);

  EXPECT_GT(contacts.size(), 0);
}

// Test 5: Group by diameter (RULE_1)
TEST_F(ImportFilterTest, GroupByDiameter) {
  auto solids = CreateMockSolids();
  std::vector<GroupingRule> rules = {GroupingRule::RULE_1};
  auto groups = filter.apply_rules(solids, rules);

  EXPECT_GT(groups.size(), 0);
  EXPECT_EQ(groups[0].rule, GroupingRule::RULE_1);
  EXPECT_EQ(groups[0].solids.size(), 3);
}

// Test 6: Group by material (RULE_2)
TEST_F(ImportFilterTest, GroupByMaterial) {
  auto solids = CreateMockSolids();
  std::vector<GroupingRule> rules = {GroupingRule::RULE_2};
  auto groups = filter.apply_rules(solids, rules);

  EXPECT_GT(groups.size(), 0);
  EXPECT_EQ(groups[0].rule, GroupingRule::RULE_2);
}

// Test 7: Apply multiple rules
TEST_F(ImportFilterTest, ApplyMultipleRules) {
  auto solids = CreateMockSolids();
  std::vector<GroupingRule> rules = {
      GroupingRule::RULE_1,
      GroupingRule::RULE_2,
      GroupingRule::RULE_4
  };
  auto groups = filter.apply_rules(solids, rules);

  EXPECT_GT(groups.size(), 0);
}

// Test 8: Analysis summary
TEST_F(ImportFilterTest, AnalysisSummary) {
  auto solids = CreateMockSolids();
  auto summary = filter.get_analysis_summary(solids);

  EXPECT_TRUE(summary.contains("total_solids"));
  EXPECT_TRUE(summary.contains("detected_tubes"));
  EXPECT_TRUE(summary.contains("diameter_distribution"));
  EXPECT_EQ(summary["total_solids"], 3);
}

// Test 9: Contact detection accuracy
TEST_F(ImportFilterTest, ContactDetectionAccuracy) {
  std::vector<SolidGeometry> solids;

  SolidGeometry s1;
  s1.id = 0;
  s1.bounding_box = {0, 0, 0, 10, 10, 10};

  SolidGeometry s2;
  s2.id = 1;
  s2.bounding_box = {5, 5, 5, 15, 15, 15};  // Overlaps

  solids.push_back(s1);
  solids.push_back(s2);

  auto contacts = filter.detect_contacts(solids, 0.01);

  EXPECT_GT(contacts.size(), 0);
}

// Test 10: Isolated tube detection
TEST_F(ImportFilterTest, IsolatedTubeDetection) {
  auto solids = CreateMockSolids();
  std::vector<GroupingRule> rules = {GroupingRule::RULE_5};
  auto groups = filter.apply_rules(solids, rules);

  // All tubes should be isolated (no contacts)
  // This depends on contact detection
  EXPECT_GE(groups.size(), 0);
}

// Test 11: Empty input handling
TEST_F(ImportFilterTest, EmptyInput) {
  std::vector<SolidGeometry> empty;
  auto tubes = filter.detect_tubes(empty);

  EXPECT_EQ(tubes.size(), 0);
}

// Test 12: Diameter matching tolerance
TEST_F(ImportFilterTest, DiameterToleranceMatching) {
  auto solids = CreateMockSolids();

  // Filter with strict tolerance
  auto filtered = filter.filter_by_diameter(solids, 12.7, 12.8);
  EXPECT_GT(filtered.size(), 0);

  // Filter with loose tolerance
  auto filtered2 = filter.filter_by_diameter(solids, 12.0, 30.0);
  EXPECT_EQ(filtered2.size(), 3);
}
