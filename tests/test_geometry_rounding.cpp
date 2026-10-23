/**
 * Tests for GeometryRounding
 * Tests geometry snapping to standard values
 */

#include <gtest/gtest.h>
#include "../src/bendcore/geometry_rounding.hpp"
#include <cmath>

using namespace bendcore;

class GeometryRoundingTest : public ::testing::Test {
 protected:
  GeometryRounder rounder;

  TubeBody CreateMockTube() {
    TubeBody tube;
    tube.id = "tube-1";
    tube.name = "Test Tube";
    tube.outer_diameter = 12.65;  // Close to 12.7
    tube.inner_diameter = 10.05;
    tube.bend_radius = 14.8;  // Close to 15
    tube.material = "Steel";
    tube.segment_lengths = {100.2, 50.1, 75.3};
    tube.bend_angles = {88.5, 92.1};

    return tube;
  }
};

// Test 1: Round diameter
TEST_F(GeometryRoundingTest, RoundDiameter) {
  double od = rounder.round_diameter(12.65, 0.5);

  EXPECT_GT(od, 0.0);
  // Should snap to nearest standard (12.7)
  EXPECT_LE(std::abs(od - 12.7), 0.1);
}

// Test 2: Round radius
TEST_F(GeometryRoundingTest, RoundRadius) {
  double radius = rounder.round_radius(14.8, 0.1);

  EXPECT_GT(radius, 0.0);
  // Should snap to a standard radius
  auto standards = rounder.get_standard_radii();
  bool found = false;
  for (double std_r : standards) {
    if (std::abs(radius - std_r) < 0.01) {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found);
}

// Test 3: Round length
TEST_F(GeometryRoundingTest, RoundLength) {
  double length = rounder.round_length(100.23, 0.1);

  EXPECT_NEAR(length, 100.2, 0.01);
}

// Test 4: Round angle
TEST_F(GeometryRoundingTest, RoundAngle) {
  double angle = rounder.round_angle(88.7, 5.0);

  EXPECT_NEAR(angle, 90.0, 1.0);
}

// Test 5: Round multiple angles
TEST_F(GeometryRoundingTest, RoundMultipleAngles) {
  double angle1 = rounder.round_angle(45.2, 5.0);
  double angle2 = rounder.round_angle(88.9, 5.0);
  double angle3 = rounder.round_angle(180.1, 5.0);

  EXPECT_NEAR(angle1, 45.0, 1.0);
  EXPECT_NEAR(angle2, 90.0, 1.0);
  EXPECT_NEAR(angle3, 180.0, 1.0);
}

// Test 6: Round complete tube
TEST_F(GeometryRoundingTest, RoundCompleteTube) {
  auto tube = CreateMockTube();

  RoundingPreferences prefs;
  prefs.round_diameters = true;
  prefs.round_radii = true;
  prefs.round_lengths = true;
  prefs.round_angles = true;

  auto result = rounder.round_tube(tube, prefs);

  EXPECT_TRUE(result.success);
  EXPECT_NE(result.rounded_tube.id, "");
}

// Test 7: Round with preferences
TEST_F(GeometryRoundingTest, RoundWithPreferences) {
  auto tube = CreateMockTube();

  RoundingPreferences prefs;
  prefs.round_diameters = true;
  prefs.round_radii = false;
  prefs.round_lengths = true;
  prefs.round_angles = false;

  auto result = rounder.round_tube(tube, prefs);

  EXPECT_TRUE(result.success);
}

// Test 8: Round multiple tubes
TEST_F(GeometryRoundingTest, RoundMultipleTubes) {
  std::vector<TubeBody> tubes;
  tubes.push_back(CreateMockTube());
  tubes.push_back(CreateMockTube());
  tubes.push_back(CreateMockTube());

  RoundingPreferences prefs;
  prefs.round_diameters = true;

  auto results = rounder.round_tubes(tubes, prefs);

  EXPECT_EQ(results.size(), 3);
  for (const auto& result : results) {
    EXPECT_TRUE(result.success);
  }
}

// Test 9: Get standard diameters
TEST_F(GeometryRoundingTest, GetStandardDiameters) {
  auto diameters = rounder.get_standard_diameters();

  EXPECT_GT(diameters.size(), 0);
  // Check for common diameters
  auto it = std::find(diameters.begin(), diameters.end(), 12.7);
  EXPECT_NE(it, diameters.end());
}

// Test 10: Get standard radii
TEST_F(GeometryRoundingTest, GetStandardRadii) {
  auto radii = rounder.get_standard_radii();

  EXPECT_GT(radii.size(), 0);
  // Check for common radii
  EXPECT_TRUE(std::find(radii.begin(), radii.end(), 10.0) != radii.end() ||
              std::find(radii.begin(), radii.end(), 15.0) != radii.end());
}

// Test 11: Match standard
TEST_F(GeometryRoundingTest, MatchStandard) {
  auto standards = rounder.get_standard_diameters();
  bool matches = rounder.matches_standard(12.7, standards, 0.1);

  EXPECT_TRUE(matches);
}

// Test 12: Find nearest standard
TEST_F(GeometryRoundingTest, FindNearestStandard) {
  auto standards = rounder.get_standard_diameters();
  double nearest = rounder.find_nearest_standard(12.65, standards);

  // Should be 12.7
  EXPECT_LE(std::abs(nearest - 12.7), 0.15);
}

// Test 13: No rounding when all enabled
TEST_F(GeometryRoundingTest, NoChangeRounding) {
  auto tube = CreateMockTube();
  // Use values that are already on standards
  tube.outer_diameter = 12.7;
  tube.bend_radius = 15.0;
  tube.segment_lengths = {100.0, 50.0};

  RoundingPreferences prefs;
  prefs.round_diameters = true;
  prefs.round_radii = true;
  prefs.round_lengths = true;

  auto result = rounder.round_tube(tube, prefs);

  EXPECT_TRUE(result.success);
  // Changes should be minimal
}

// Test 14: Rounding report
TEST_F(GeometryRoundingTest, RoundingReport) {
  std::vector<TubeBody> tubes;
  tubes.push_back(CreateMockTube());
  tubes.push_back(CreateMockTube());

  RoundingPreferences prefs;
  prefs.round_diameters = true;

  auto results = rounder.round_tubes(tubes, prefs);
  auto report = rounder.get_rounding_report(results);

  EXPECT_TRUE(report.contains("total_tubes"));
  EXPECT_TRUE(report.contains("tubes_modified"));
  EXPECT_TRUE(report.contains("total_changes"));
  EXPECT_EQ(report["total_tubes"], 2);
}

// Test 15: Precision control
TEST_F(GeometryRoundingTest, PrecisionControl) {
  double length = 100.12345;

  double rounded_0_1 = rounder.round_length(length, 0.1);
  double rounded_1_0 = rounder.round_length(length, 1.0);
  double rounded_0_5 = rounder.round_length(length, 0.5);

  EXPECT_LT(std::abs(rounded_0_1 - length), std::abs(rounded_1_0 - length));
  EXPECT_NEAR(rounded_0_1, 100.1, 0.01);
  EXPECT_NEAR(rounded_1_0, 100.0, 0.01);
}

// Test 16: Angle step precision
TEST_F(GeometryRoundingTest, AngleStepPrecision) {
  double angle = 87.6;

  double rounded_5 = rounder.round_angle(angle, 5.0);
  double rounded_15 = rounder.round_angle(angle, 15.0);

  EXPECT_NEAR(rounded_5, 90.0, 1.0);
  EXPECT_NEAR(rounded_15, 90.0, 1.0);
}

// Test 17: Diameter tolerance matching
TEST_F(GeometryRoundingTest, DiameterToleranceMatching) {
  auto standards = rounder.get_standard_diameters();

  // Exact match
  EXPECT_TRUE(rounder.matches_standard(12.7, standards, 0.1));

  // Within tolerance
  EXPECT_TRUE(rounder.matches_standard(12.72, standards, 0.1));

  // Outside tolerance
  EXPECT_FALSE(rounder.matches_standard(12.72, standards, 0.01));
}

// Test 18: Zero precision handling
TEST_F(GeometryRoundingTest, ZeroPrecisionHandling) {
  double length = 100.5;
  double rounded = rounder.round_length(length, 0.0);

  // With zero precision, should return original
  EXPECT_EQ(rounded, length);
}

// Test 19: Large value rounding
TEST_F(GeometryRoundingTest, LargeValueRounding) {
  double large_length = 5000.25;
  double rounded = rounder.round_length(large_length, 0.5);

  EXPECT_NEAR(rounded, 5000.0, 1.0);
}

// Test 20: Change tracking
TEST_F(GeometryRoundingTest, ChangeTracking) {
  auto tube = CreateMockTube();

  RoundingPreferences prefs;
  prefs.round_diameters = true;
  prefs.round_radii = false;
  prefs.round_lengths = true;

  auto result = rounder.round_tube(tube, prefs);

  EXPECT_TRUE(result.success);
  // Check if changes are recorded
  EXPECT_NOT_EQUAL(result.changes.size(), 0);
}
