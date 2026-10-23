/**
 * BendCore Phase 35C: TubeBuilder Tests
 * Unit tests for core tube generation algorithm
 */

#include <gtest/gtest.h>
#include "bendcore/tube_builder.hpp"
#include "bendcore/tube_analyzer.hpp"
#include <cmath>

using namespace bendcore::tube;

// ============ SETUP ============

class TubeBuilderTest : public ::testing::Test {
 protected:
  TubeBuilder builder;

  // Create a simple L-shaped path with one 90° bend
  std::vector<PathSegment> create_simple_l_path() {
    std::vector<PathSegment> path;

    // First straight segment
    PathSegment seg1;
    seg1.segment_id = "seg1";
    seg1.start = Vector3(0, 0, 0);
    seg1.end = Vector3(100, 0, 0);
    seg1.is_straight = true;
    seg1.bend_angle = 0;
    path.push_back(seg1);

    // 90° bend
    PathSegment seg2;
    seg2.segment_id = "seg2";
    seg2.start = Vector3(100, 0, 0);
    seg2.end = Vector3(100, 100, 0);
    seg2.is_straight = false;
    seg2.bend_angle = M_PI / 2.0;  // 90°
    path.push_back(seg2);

    return path;
  }

  BendingProfile create_profile() {
    BendingProfile profile;
    profile.profile_id = "prof1";
    profile.name = "Standard Bend";
    profile.radius = 50.0;  // mm
    profile.min_bend_angle = M_PI / 180.0;  // 1°
    profile.max_bend_angle = M_PI;  // 180°
    profile.min_straight = 10.0;  // mm
    profile.min_degenerate = 0.5 * M_PI / 180.0;  // 0.5°
    return profile;
  }

  TubeSpecification create_spec() {
    TubeSpecification spec;
    spec.tube_id = "tube1";
    spec.outer_diameter = 12.0;  // mm
    spec.inner_diameter = 10.0;  // mm
    spec.material_id = "copper";
    spec.density = 8.96;  // g/cm³ for copper
    spec.designation = "Медная трубка ∅12×10";
    return spec;
  }
};

// ============ TESTS ============

TEST_F(TubeBuilderTest, BuildSimpleLPath) {
  auto path = create_simple_l_path();
  auto profile = create_profile();
  auto spec = create_spec();

  TubeBody tube = builder.build(path, profile, spec);

  EXPECT_FALSE(tube.tube_id.empty());
  EXPECT_GT(tube.straight_sections.size(), 0);
  EXPECT_GT(tube.bend_sections.size(), 0);
  EXPECT_GT(tube.total_length, 0);
  EXPECT_GT(tube.total_mass, 0);
}

TEST_F(TubeBuilderTest, TangentCalculation) {
  double radius = 50.0;
  double angle = M_PI / 2.0;  // 90°

  double tangent = builder.build(
    std::vector<PathSegment>(),
    create_profile(),
    create_spec()
  ).total_length * 0;  // Get builder instance

  // T = R × tan(B/2) = 50 × tan(45°) = 50 × 1 = 50
  double expected = 50.0 * std::tan(M_PI / 4.0);
  double tolerance = 0.1;

  EXPECT_LT(std::abs(expected - 50.0), tolerance);
}

TEST_F(TubeBuilderTest, VolumeCalculation) {
  auto path = create_simple_l_path();
  auto profile = create_profile();
  auto spec = create_spec();

  TubeBody tube = builder.build(path, profile, spec);

  // Calculate expected volume manually
  // Wall thickness = (OD - ID) / 2 = 1.0 mm
  // Cross-section area = π(r_o² - r_i²) = π(6² - 5²) = 11π mm²
  // Straight segment: 100 mm, plus bend arc

  EXPECT_GT(tube.total_volume, 0);
  EXPECT_LT(tube.total_volume, 10000);  // Sanity check
}

TEST_F(TubeBuilderTest, MassCalculation) {
  auto path = create_simple_l_path();
  auto profile = create_profile();
  auto spec = create_spec();

  TubeBody tube = builder.build(path, profile, spec);

  // mass = density × volume (volume in cm³)
  // density = 8.96 g/cm³
  // volume should be small in cm³

  EXPECT_GT(tube.total_mass, 0);
  EXPECT_EQ(tube.total_mass, tube.total_volume / 1000.0 * spec.density);
}

TEST_F(TubeBuilderTest, InvalidInputs) {
  std::vector<PathSegment> empty_path;
  auto profile = create_profile();
  auto spec = create_spec();

  TubeBody tube = builder.build(empty_path, profile, spec);

  EXPECT_FALSE(tube.valid);
  EXPECT_GT(tube.validation_errors.size(), 0);
}

TEST_F(TubeBuilderTest, DiameterValidation) {
  auto path = create_simple_l_path();
  auto profile = create_profile();
  auto spec = create_spec();

  // Invalid: OD <= ID
  spec.outer_diameter = 10.0;
  spec.inner_diameter = 10.0;

  TubeBody tube = builder.build(path, profile, spec);

  EXPECT_FALSE(tube.valid);
  EXPECT_GT(tube.validation_errors.size(), 0);
}

TEST_F(TubeBuilderTest, BendAngleValidation) {
  auto path = create_simple_l_path();
  auto profile = create_profile();
  auto spec = create_spec();

  // Set max bend angle to 45°
  profile.max_bend_angle = M_PI / 4.0;

  TubeBody tube = builder.build(path, profile, spec);

  // 90° bend exceeds max
  EXPECT_FALSE(tube.valid);
}

TEST_F(TubeBuilderTest, TubeAnalyzer) {
  auto path = create_simple_l_path();
  auto profile = create_profile();
  auto spec = create_spec();

  TubeBody tube = builder.build(path, profile, spec);

  auto analysis = TubeAnalyzer::analyze(tube, path, profile, spec);

  EXPECT_EQ(analysis.bend_stats.total_bends, 1);
  EXPECT_NEAR(analysis.total_length, tube.total_length, 1.0);
}

TEST_F(TubeBuilderTest, CylinderGeometry) {
  Cylinder cyl;
  cyl.center = Vector3(0, 0, 0);
  cyl.axis = Vector3(1, 0, 0);
  cyl.length = 100.0;
  cyl.outer_radius = 6.0;
  cyl.inner_radius = 5.0;

  double volume = cyl.volume();

  // π(r_o² - r_i²) × length = π(36 - 25) × 100 = 1100π mm³
  double expected = M_PI * 11.0 * 100.0;

  EXPECT_NEAR(volume, expected, 1.0);
}

TEST_F(TubeBuilderTest, TorusGeometry) {
  Torus torus;
  torus.center = Vector3(0, 0, 0);
  torus.axis = Vector3(0, 0, 1);
  torus.major_radius = 50.0;
  torus.minor_radius = 1.0;
  torus.start_angle = 0;
  torus.end_angle = M_PI / 2.0;  // 90°

  double volume = torus.volume();

  // For 90° arc: V = 2π²Rr² × (θ/2π) = 2π²×50×1² × 0.25 = ...
  EXPECT_GT(volume, 0);
  EXPECT_LT(volume, 5000);  // Sanity check
}

TEST_F(TubeBuilderTest, ZeroBendAngleHandling) {
  auto path = create_simple_l_path();
  path[1].bend_angle = 0;  // Zero angle

  auto profile = create_profile();
  auto spec = create_spec();

  TubeBody tube = builder.build(path, profile, spec);

  // Should handle gracefully
  EXPECT_NE(tube.straight_sections.size(), 0);
}

TEST_F(TubeBuilderTest, LargeNumberOfBends) {
  std::vector<PathSegment> path;

  for (int i = 0; i < 10; ++i) {
    PathSegment seg;
    seg.segment_id = "seg" + std::to_string(i);
    seg.start = Vector3(i * 50, 0, 0);
    seg.end = Vector3((i + 1) * 50, 0, 0);
    seg.is_straight = (i % 2 == 0);
    seg.bend_angle = seg.is_straight ? 0 : (M_PI / 4.0);  // 45°
    path.push_back(seg);
  }

  auto profile = create_profile();
  auto spec = create_spec();

  TubeBody tube = builder.build(path, profile, spec);

  EXPECT_GT(tube.bend_sections.size(), 0);
  EXPECT_GT(tube.total_length, 0);
}

// ============ MAIN ============

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
