#pragma once

#include <vector>
#include <memory>
#include <string>
#include <optional>
#include "../fea/FEAEngine.hpp"
#include "ManufacturingPlanner.hpp"

namespace bendcore::manufacturing {

// FEA Integration for Manufacturing Validation
class FEAIntegration {
 public:
  FEAIntegration();

  struct AnalysisSettings {
    bool analyze_stress = true;
    bool analyze_deflection = true;
    bool analyze_strain = true;
    double safety_factor = 1.5;
    int mesh_refinement = 2;  // 1=coarse, 2=normal, 3=fine
  };

  struct TubeAnalysisResult {
    bool is_valid = true;
    double max_stress_mpa = 0.0;
    double max_deflection_mm = 0.0;
    double max_strain = 0.0;
    double stress_utilization = 0.0;  // max_stress / yield_strength
    std::vector<double> element_stresses;
    std::vector<double> element_strains;
    std::string failure_mode;  // Empty if no failure
  };

  // Analyze single tube
  TubeAnalysisResult analyzeTube(
    const Tube& tube,
    const fea::Material& material,
    const AnalysisSettings& settings
  );

  // Analyze complete assembly (all tubes)
  std::vector<TubeAnalysisResult> analyzeAssembly(
    const std::vector<Tube>& tubes,
    const std::vector<fea::Material>& materials,
    const AnalysisSettings& settings
  );

  // Get material properties from standard library
  static fea::Material getMaterialProperties(const std::string& material_name);

  // Validate bend angles and radii
  bool validateBendLimits(
    const Tube& tube,
    const fea::Material& material,
    double max_strain = 0.15
  );

  // Get yield strength for material
  static double getYieldStrength(const fea::Material& material);

 private:
  // Generate tetrahedral mesh for tube
  void generateTubeMesh(
    const Tube& tube,
    const AnalysisSettings& settings,
    std::vector<fea::Point3>& nodes,
    std::vector<fea::Tetrahedron>& elements
  );

  // Apply loads for bend analysis
  void applyBendLoads(
    const Tube& tube,
    const fea::Material& material,
    std::vector<fea::Load>& loads,
    std::vector<fea::BoundaryCondition>& bcs
  );

  // Extract stress concentration factors
  double getStressConcentration(
    double bend_angle,
    double bend_radius,
    double tube_od,
    double wall_thickness
  );

  fea::LinearElasticFEA fea_solver_;
  int mesh_element_count_ = 0;
};

}  // namespace bendcore::manufacturing
