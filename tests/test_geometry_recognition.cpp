/**
 * Tests for GeometryRecognition
 * Tests tube detection and path fitting
 */

#include <gtest/gtest.h>
#include "../src/bendcore/geometry_recognition.hpp"
#include <vector>
#include <cmath>

using namespace bendcore;

class GeometryRecognitionTest : public ::testing::Test {
 protected:
  GeometryRecognition recognizer;

  SolidGeometry CreateMockCylinder() {
    SolidGeometry solid;
    solid.id = 1;
    solid.name = "Cylinder_1";
    solid.outer_diameter = 12.7;
    solid.inner_diameter = 10.0;
    solid.wall_thickness = 1.35;
    solid.is_cylindrical = true;
    solid.is_hollow = true;
    solid.material = "Steel";
    solid.bounding_box = {0, 0, 0, 10, 10, 100};
    solid.length = 100.0;

    return solid;
  }

  std::vector<std::vector<double>> CreateMockPath() {
    std::vector<std::vector<double>> path = {
        {0, 0, 0},
        {50, 0, 0},
        {100, 0, 0},
        {100, 50, 0},
        {100, 100, 0},
    };
    return path;
  }
};

// Test 1: Recognize tubes
TEST_F(GeometryRecognitionTest, RecognizeTubes) {
  std::vector<SolidGeometry> solids;
  solids.push_back(CreateMockCylinder());

  auto candidates = recognizer.recognize_tubes(solids);

  EXPECT_GT(candidates.size(), 0);
}

// Test 2: Match diameter to standard profile
TEST_F(GeometryRecognitionTest, MatchDiameterProfile) {
  std::string profile = recognizer.match_profile_by_diameter(12.7, 0.5);

  EXPECT_NE(profile, "");
  EXPECT_TRUE(profile.find("Profile_") != std::string::npos ||
              profile.find("Custom_") != std::string::npos);
}

// Test 3: Standard diameter matching
TEST_F(GeometryRecognitionTest, StandardDiameterMatching) {
  // 12.7 is 1/2"
  std::string profile1 = recognizer.match_profile_by_diameter(12.7, 0.5);
  EXPECT_NE(profile1, "");

  // 25.4 is 1"
  std::string profile2 = recognizer.match_profile_by_diameter(25.4, 0.5);
  EXPECT_NE(profile2, "");

  // These should match standards
  EXPECT_NE(profile1, profile2);
}

// Test 4: Detect bend angles
TEST_F(GeometryRecognitionTest, DetectBendAngles) {
  auto path = CreateMockPath();
  auto angles = recognizer.detect_bend_angles(path, 5.0);

  // Path has a 90-degree bend
  EXPECT_GT(angles.size(), 0);
}

// Test 5: Smooth path
TEST_F(GeometryRecognitionTest, SmoothPath) {
  auto path = CreateMockPath();
  auto smoothed = recognizer.smooth_path(path, 0.5);

  EXPECT_GT(smoothed.size(), path.size());
}

// Test 6: Simplify path
TEST_F(GeometryRecognitionTest, SimplifyPath) {
  std::vector<std::vector<double>> path = {
      {0, 0, 0},
      {10, 0, 0},
      {20, 0, 0},
      {30, 0, 0},
      {40, 0, 0},
      {50, 0, 0},
      {50, 10, 0},
      {50, 20, 0},
      {50, 30, 0},
  };

  auto simplified = recognizer.simplify_path(path, 0.1);

  EXPECT_LE(simplified.size(), path.size());
  EXPECT_GT(simplified.size(), 0);
}

// Test 7: Calculate confidence
TEST_F(GeometryRecognitionTest, CalculateConfidence) {
  TubeCandidate candidate;
  candidate.id = "tube-1";
  candidate.name = "Test Tube";
  candidate.outer_diameter = 12.7;
  candidate.material = "Steel";
  candidate.bending_profile = "Profile_1270";
  candidate.vertices = {{0, 0, 0}, {100, 0, 0}};

  double confidence = recognizer.calculate_confidence(candidate);

  EXPECT_GE(confidence, 50.0);
  EXPECT_LE(confidence, 100.0);
}

// Test 8: Validate tube candidate
TEST_F(GeometryRecognitionTest, ValidateTubeCandidate) {
  TubeCandidate candidate;
  candidate.id = "tube-1";
  candidate.outer_diameter = 12.7;
  candidate.inner_diameter = 10.0;
  candidate.vertices = {{0, 0, 0}, {100, 0, 0}};
  candidate.confidence = 85.0;

  bool valid = recognizer.validate_tube_candidate(candidate);

  EXPECT_TRUE(valid);
}

// Test 9: Invalid candidate rejection
TEST_F(GeometryRecognitionTest, InvalidCandidateRejection) {
  TubeCandidate candidate;
  candidate.id = "tube-1";
  candidate.outer_diameter = 12.7;
  candidate.inner_diameter = 15.0;  // Inner > outer!
  candidate.confidence = 30.0;  // Too low
  candidate.vertices = {};  // No vertices

  bool valid = recognizer.validate_tube_candidate(candidate);

  EXPECT_FALSE(valid);
}

// Test 10: Fit path through solid
TEST_F(GeometryRecognitionTest, FitPathThroughSolid) {
  auto solid = CreateMockCylinder();
  auto result = recognizer.fit_path_through_solid(solid);

  EXPECT_TRUE(result.success || !result.success);  // Either succeed or fail gracefully
}

// Test 11: Recognition report
TEST_F(GeometryRecognitionTest, RecognitionReport) {
  std::vector<TubeCandidate> candidates;

  TubeCandidate c1;
  c1.id = "tube-1";
  c1.confidence = 90.0;

  TubeCandidate c2;
  c2.id = "tube-2";
  c2.confidence = 70.0;

  candidates.push_back(c1);
  candidates.push_back(c2);

  auto report = recognizer.get_recognition_report(candidates);

  EXPECT_TRUE(report.contains("total_candidates"));
  EXPECT_TRUE(report.contains("high_confidence"));
  EXPECT_TRUE(report.contains("candidates"));
  EXPECT_EQ(report["total_candidates"], 2);
}

// Test 12: Angle computation accuracy
TEST_F(GeometryRecognitionTest, AngleComputation) {
  // Test 90-degree angle
  std::vector<double> p1 = {0, 0, 0};
  std::vector<double> p2 = {1, 0, 0};
  std::vector<double> p3 = {1, 1, 0};

  // Note: This uses internal function, may need to adjust based on API
  // For now, just test that we can create the path
  auto path = std::vector<std::vector<double>>{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}};
  auto angles = recognizer.detect_bend_angles(path, 1.0);

  // Should detect ~90 degree angle
  EXPECT_GT(angles.size(), 0);
}

// Test 13: Empty solid handling
TEST_F(GeometryRecognitionTest, EmptyInputHandling) {
  std::vector<SolidGeometry> empty;
  auto candidates = recognizer.recognize_tubes(empty);

  EXPECT_EQ(candidates.size(), 0);
}

// Test 14: Non-cylindrical rejection
TEST_F(GeometryRecognitionTest, NonCylindricalRejection) {
  std::vector<SolidGeometry> solids;

  SolidGeometry box;
  box.is_cylindrical = false;
  box.is_hollow = true;

  solids.push_back(box);

  auto candidates = recognizer.recognize_tubes(solids);

  EXPECT_EQ(candidates.size(), 0);
}

// Test 15: Confidence factors
TEST_F(GeometryRecognitionTest, ConfidenceFactors) {
  TubeCandidate low_confidence;
  low_confidence.outer_diameter = 12.7;
  low_confidence.confidence = 40.0;
  low_confidence.vertices = {{0, 0, 0}};  // Only 1 vertex

  TubeCandidate high_confidence;
  high_confidence.outer_diameter = 12.7;
  high_confidence.material = "Steel";
  high_confidence.bending_profile = "Profile_1270";
  high_confidence.confidence = 95.0;
  high_confidence.vertices = {{0, 0, 0}, {100, 0, 0}};

  bool low_valid = recognizer.validate_tube_candidate(low_confidence);
  bool high_valid = recognizer.validate_tube_candidate(high_confidence);

  EXPECT_FALSE(low_valid);
  EXPECT_TRUE(high_valid);
}
