#include <gtest/gtest.h>
#include "../src/manufacturing/FEAIntegration.hpp"

using namespace bendcore::manufacturing;

class FEAIntegrationTest : public ::testing::Test {
 protected:
  FEAIntegration fea_;

  Tube createSimpleTube() {
    Tube tube;
    tube.id = "TUBE-001";
    tube.material = "Steel";
    tube.length = 100.0;
    tube.outer_diameter = 12.0;
    tube.wall_thickness = 1.0;

    Bend bend1;
    bend1.position = 25.0;
    bend1.angle = 45.0;
    bend1.radius = 15.0;
    bend1.dwell_time = 1.0;
    tube.bends.push_back(bend1);

    Bend bend2;
    bend2.position = 75.0;
    bend2.angle = 45.0;
    bend2.radius = 15.0;
    bend2.dwell_time = 1.0;
    tube.bends.push_back(bend2);

    return tube;
  }
};

TEST_F(FEAIntegrationTest, AnalyzeTubeValidInput) {
  auto tube = createSimpleTube();
  auto material = FEAIntegration::getMaterialProperties("Steel");
  FEAIntegration::AnalysisSettings settings;
  settings.mesh_refinement = 1;

  auto result = fea_.analyzeTube(tube, material, settings);

  EXPECT_TRUE(!result.failure_mode.empty() || result.is_valid);
  EXPECT_GT(result.max_stress_mpa, 0.0);
  EXPECT_GE(result.max_deflection_mm, 0.0);
  EXPECT_GE(result.stress_utilization, 0.0);
}

TEST_F(FEAIntegrationTest, AnalyzeTubeInvalidGeometry) {
  Tube tube;
  tube.id = "TUBE-INVALID";
  tube.material = "Steel";
  tube.length = 100.0;
  tube.outer_diameter = -1.0;  // Invalid
  tube.wall_thickness = 1.0;

  auto material = FEAIntegration::getMaterialProperties("Steel");
  FEAIntegration::AnalysisSettings settings;

  auto result = fea_.analyzeTube(tube, material, settings);

  EXPECT_FALSE(result.is_valid);
  EXPECT_FALSE(result.failure_mode.empty());
}

TEST_F(FEAIntegrationTest, GetMaterialProperties) {
  auto steel = FEAIntegration::getMaterialProperties("Steel");
  EXPECT_GT(steel.youngs_modulus, 0.0);
  EXPECT_EQ(steel.name, "Steel");

  auto aluminum = FEAIntegration::getMaterialProperties("Aluminum");
  EXPECT_GT(aluminum.youngs_modulus, 0.0);
  EXPECT_EQ(aluminum.name, "Aluminum");

  auto titanium = FEAIntegration::getMaterialProperties("Titanium");
  EXPECT_GT(titanium.youngs_modulus, 0.0);
  EXPECT_EQ(titanium.name, "Titanium");
}

TEST_F(FEAIntegrationTest, ValidateBendLimits) {
  auto tube = createSimpleTube();
  auto material = FEAIntegration::getMaterialProperties("Steel");

  bool valid = fea_.validateBendLimits(tube, material, 0.15);
  EXPECT_TRUE(valid);
}

TEST_F(FEAIntegrationTest, ValidateBendLimitsExcessive) {
  Tube tube;
  tube.id = "TUBE-EXCESSIVE";
  tube.material = "Steel";
  tube.length = 100.0;
  tube.outer_diameter = 12.0;
  tube.wall_thickness = 1.0;

  Bend bend;
  bend.position = 50.0;
  bend.angle = 180.0;
  bend.radius = 2.0;  // Very tight radius
  bend.dwell_time = 1.0;
  tube.bends.push_back(bend);

  auto material = FEAIntegration::getMaterialProperties("Steel");

  bool valid = fea_.validateBendLimits(tube, material, 0.15);
  EXPECT_FALSE(valid);
}

TEST_F(FEAIntegrationTest, AnalyzeAssembly) {
  std::vector<Tube> tubes;
  tubes.push_back(createSimpleTube());
  tubes.push_back(createSimpleTube());

  std::vector<fea::Material> materials;
  materials.push_back(FEAIntegration::getMaterialProperties("Steel"));
  materials.push_back(FEAIntegration::getMaterialProperties("Aluminum"));

  FEAIntegration::AnalysisSettings settings;
  settings.mesh_refinement = 1;

  auto results = fea_.analyzeAssembly(tubes, materials, settings);

  EXPECT_EQ(results.size(), 2);
  for (const auto& result : results) {
    EXPECT_GE(result.max_stress_mpa, 0.0);
  }
}

TEST_F(FEAIntegrationTest, GetYieldStrength) {
  auto material = FEAIntegration::getMaterialProperties("Steel");
  double yield_strength = FEAIntegration::getYieldStrength(material);

  EXPECT_GT(yield_strength, 0.0);
  EXPECT_NEAR(yield_strength, material.youngs_modulus * 0.001, 1e-6);
}

TEST_F(FEAIntegrationTest, MeshGeneration) {
  auto tube = createSimpleTube();
  auto material = FEAIntegration::getMaterialProperties("Steel");
  FEAIntegration::AnalysisSettings settings;
  settings.mesh_refinement = 1;

  // This is tested indirectly through analyzeTube
  auto result = fea_.analyzeTube(tube, material, settings);

  // If mesh was generated successfully, we should have results
  if (result.is_valid) {
    EXPECT_GT(result.element_stresses.size(), 0);
  }
}

// Test different analysis settings
TEST_F(FEAIntegrationTest, AnalysisSettingsMeshRefinement) {
  auto tube = createSimpleTube();
  auto material = FEAIntegration::getMaterialProperties("Steel");

  FEAIntegration::AnalysisSettings coarse;
  coarse.mesh_refinement = 1;

  FEAIntegration::AnalysisSettings fine;
  fine.mesh_refinement = 3;

  auto result_coarse = fea_.analyzeTube(tube, material, coarse);
  auto result_fine = fea_.analyzeTube(tube, material, fine);

  // Fine mesh should be more accurate (different results)
  EXPECT_NE(result_coarse.max_stress_mpa, result_fine.max_stress_mpa);
}

// Test safety factor enforcement
TEST_F(FEAIntegrationTest, SafetyFactorEnforcement) {
  auto tube = createSimpleTube();
  auto material = FEAIntegration::getMaterialProperties("Steel");

  FEAIntegration::AnalysisSettings safe;
  safe.safety_factor = 3.0;

  auto result = fea_.analyzeTube(tube, material, safe);

  if (!result.is_valid) {
    EXPECT_FALSE(result.failure_mode.empty());
  }
}

// Test multiple materials
TEST_F(FEAIntegrationTest, MultipleMaterials) {
  std::vector<std::string> material_names = {"Steel", "Aluminum", "Titanium"};
  std::vector<fea::Material> materials;

  for (const auto& name : material_names) {
    materials.push_back(FEAIntegration::getMaterialProperties(name));
  }

  EXPECT_EQ(materials.size(), 3);
  EXPECT_NE(materials[0].youngs_modulus, materials[1].youngs_modulus);
  EXPECT_NE(materials[1].youngs_modulus, materials[2].youngs_modulus);
}

// Stress concentration test
TEST_F(FEAIntegrationTest, StressConcentrationFactors) {
  auto tube = createSimpleTube();
  auto material = FEAIntegration::getMaterialProperties("Steel");
  FEAIntegration::AnalysisSettings settings;

  auto result1 = fea_.analyzeTube(tube, material, settings);

  // The stress values should reflect concentration factors
  EXPECT_GT(result1.max_stress_mpa, 0.0);
}

// Test with no bends (straight tube)
TEST_F(FEAIntegrationTest, StraightTubeNoBends) {
  Tube tube;
  tube.id = "STRAIGHT";
  tube.material = "Steel";
  tube.length = 100.0;
  tube.outer_diameter = 12.0;
  tube.wall_thickness = 1.0;
  // No bends

  auto material = FEAIntegration::getMaterialProperties("Steel");
  FEAIntegration::AnalysisSettings settings;

  auto result = fea_.analyzeTube(tube, material, settings);

  // Straight tube should have minimal stress and deflection
  if (result.is_valid) {
    EXPECT_LE(result.max_deflection_mm, 1.0);
  }
}

// Boundary condition test
TEST_F(FEAIntegrationTest, BoundaryConditions) {
  auto tube = createSimpleTube();
  auto material = FEAIntegration::getMaterialProperties("Steel");
  FEAIntegration::AnalysisSettings settings;

  // Should apply fixed BC at one end
  auto result = fea_.analyzeTube(tube, material, settings);

  // With proper BC, analysis should complete
  EXPECT_TRUE(!result.failure_mode.empty() || result.is_valid);
}

// Load application test
TEST_F(FEAIntegrationTest, LoadApplication) {
  auto tube = createSimpleTube();
  auto material = FEAIntegration::getMaterialProperties("Steel");
  FEAIntegration::AnalysisSettings settings;

  // Analysis applies loads at bend locations
  auto result = fea_.analyzeTube(tube, material, settings);

  // With loads, stress should be > 0
  if (result.is_valid) {
    EXPECT_GT(result.max_stress_mpa, 0.0);
  }
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
