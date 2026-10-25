#include "FEAIntegration.hpp"
#include <cmath>
#include <algorithm>

namespace bendcore::manufacturing {

FEAIntegration::FEAIntegration() {}

FEAIntegration::TubeAnalysisResult FEAIntegration::analyzeTube(
    const Tube& tube,
    const fea::Material& material,
    const AnalysisSettings& settings) {

  TubeAnalysisResult result;

  // Validate input
  if (tube.outer_diameter <= 0 || tube.wall_thickness <= 0) {
    result.is_valid = false;
    result.failure_mode = "Invalid tube geometry";
    return result;
  }

  // Generate mesh
  std::vector<fea::Point3> nodes;
  std::vector<fea::Tetrahedron> elements;
  generateTubeMesh(tube, settings, nodes, elements);

  if (nodes.empty() || elements.empty()) {
    result.is_valid = false;
    result.failure_mode = "Mesh generation failed";
    return result;
  }

  // Apply boundary conditions
  std::vector<fea::BoundaryCondition> bcs;
  std::vector<fea::Load> loads;
  applyBendLoads(tube, material, loads, bcs);

  // Solve
  auto fea_result = fea_solver_.solve(nodes, elements, bcs, loads);
  mesh_element_count_ = elements.size();

  result.max_deflection_mm = fea_result.max_displacement;
  result.max_stress_mpa = fea_result.max_stress / 1e6;  // Convert Pa to MPa

  // Calculate stress utilization
  double yield_strength = getYieldStrength(material);
  result.stress_utilization = result.max_stress_mpa / yield_strength;

  // Validate against safety factor
  if (result.stress_utilization > (1.0 / settings.safety_factor)) {
    result.is_valid = false;
    result.failure_mode = "Stress exceeds safe limits";
  }

  // Check deflection limits
  double max_allowed_deflection = tube.length * 0.01;  // 1% of tube length
  if (result.max_deflection_mm > max_allowed_deflection) {
    result.is_valid = false;
    result.failure_mode = "Deflection exceeds limits";
  }

  // Store element stresses
  result.element_stresses = fea_result.strains;

  return result;
}

std::vector<FEAIntegration::TubeAnalysisResult> FEAIntegration::analyzeAssembly(
    const std::vector<Tube>& tubes,
    const std::vector<fea::Material>& materials,
    const AnalysisSettings& settings) {

  std::vector<TubeAnalysisResult> results;
  results.reserve(tubes.size());

  for (size_t i = 0; i < tubes.size() && i < materials.size(); ++i) {
    auto result = analyzeTube(tubes[i], materials[i], settings);
    results.push_back(result);
  }

  return results;
}

fea::Material FEAIntegration::getMaterialProperties(
    const std::string& material_name) {

  if (material_name == "Steel" || material_name == "steel") {
    return fea::LinearElasticFEA::getSteel();
  } else if (material_name == "Aluminum" || material_name == "aluminum") {
    return fea::LinearElasticFEA::getAluminum();
  } else if (material_name == "Titanium" || material_name == "titanium") {
    return fea::LinearElasticFEA::getTitanium();
  }

  // Default to steel
  return fea::LinearElasticFEA::getSteel();
}

bool FEAIntegration::validateBendLimits(
    const Tube& tube,
    const fea::Material& material,
    double max_strain) {

  for (const auto& bend : tube.bends) {
    // Calculate strain from bend radius
    double inner_radius = bend.radius - tube.wall_thickness / 2;
    double outer_radius = bend.radius + tube.wall_thickness / 2;

    if (inner_radius <= 0) {
      return false;  // Invalid bend radius
    }

    // Maximum strain in outer fiber
    double strain = (outer_radius - inner_radius) / (2.0 * inner_radius);

    if (strain > max_strain) {
      return false;  // Excessive strain
    }

    // Minimum bend radius check
    double min_radius = tube.outer_diameter / 2;  // Simple minimum radius rule
    if (bend.radius < min_radius) {
      return false;
    }
  }

  return true;
}

double FEAIntegration::getYieldStrength(const fea::Material& material) {
  // Approximate yield strength based on Young's modulus
  // Yield = E * 0.001 (typical ratio for metals)
  return material.youngs_modulus * 0.001;
}

void FEAIntegration::generateTubeMesh(
    const Tube& tube,
    const AnalysisSettings& settings,
    std::vector<fea::Point3>& nodes,
    std::vector<fea::Tetrahedron>& elements) {

  // Create simple cylindrical mesh for tube
  int segments = 12 * settings.mesh_refinement;  // Circumferential segments
  int length_divisions = 8 * settings.mesh_refinement;  // Along length
  int radial_divisions = 2;  // Radial divisions (inner and outer)

  double outer_radius = tube.outer_diameter / 2.0;
  double inner_radius = outer_radius - tube.wall_thickness;

  // Generate nodes
  for (int i = 0; i <= length_divisions; ++i) {
    double z = (tube.length / length_divisions) * i;
    for (int j = 0; j < segments; ++j) {
      double angle = 2.0 * M_PI * j / segments;
      for (int k = 0; k <= radial_divisions; ++k) {
        double radius = inner_radius + (outer_radius - inner_radius) * k / radial_divisions;
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        nodes.push_back(fea::Point3(x, y, z));
      }
    }
  }

  // Generate tetrahedra elements
  for (int i = 0; i < length_divisions; ++i) {
    for (int j = 0; j < segments; ++j) {
      int nodes_per_ring = segments * (radial_divisions + 1);
      int base = i * nodes_per_ring;

      for (int k = 0; k < radial_divisions; ++k) {
        int n1 = base + j * (radial_divisions + 1) + k;
        int n2 = base + ((j + 1) % segments) * (radial_divisions + 1) + k;
        int n3 = base + j * (radial_divisions + 1) + k + 1;
        int n4 = base + ((j + 1) % segments) * (radial_divisions + 1) + k + 1;

        int n5 = base + nodes_per_ring + j * (radial_divisions + 1) + k;
        int n6 = base + nodes_per_ring + ((j + 1) % segments) * (radial_divisions + 1) + k;
        int n7 = base + nodes_per_ring + j * (radial_divisions + 1) + k + 1;
        int n8 = base + nodes_per_ring + ((j + 1) % segments) * (radial_divisions + 1) + k + 1;

        // Create tetrahedra from hex elements
        if (n1 < nodes.size() && n8 < nodes.size()) {
          fea::Tetrahedron tet;
          tet.nodes = {n1, n2, n3, n5};
          tet.material = getMaterialProperties(tube.material);
          elements.push_back(tet);

          tet.nodes = {n2, n4, n3, n6};
          elements.push_back(tet);

          tet.nodes = {n3, n4, n7, n8};
          elements.push_back(tet);

          tet.nodes = {n5, n6, n7, n8};
          elements.push_back(tet);

          tet.nodes = {n2, n4, n6, n8};
          elements.push_back(tet);

          tet.nodes = {n3, n5, n7, n8};
          elements.push_back(tet);
        }
      }
    }
  }

  mesh_element_count_ = elements.size();
}

void FEAIntegration::applyBendLoads(
    const Tube& tube,
    const fea::Material& material,
    std::vector<fea::Load>& loads,
    std::vector<fea::BoundaryCondition>& bcs) {

  // Fix one end
  fea::BoundaryCondition fixed_bc;
  fixed_bc.type = fea::BoundaryConditionType::FIXED;
  fixed_bc.node = 0;
  bcs.push_back(fixed_bc);

  // Apply moment loads at bend locations
  for (size_t i = 0; i < tube.bends.size(); ++i) {
    const auto& bend = tube.bends[i];

    // Create distributed load perpendicular to tube
    double bend_moment = bend.angle * bend.radius * material.youngs_modulus / 180.0;

    fea::Load load;
    load.direction = 1;  // Y direction (perpendicular)
    load.magnitude = bend_moment / 100.0;  // Distribute over element
    loads.push_back(load);
  }
}

double FEAIntegration::getStressConcentration(
    double bend_angle,
    double bend_radius,
    double tube_od,
    double wall_thickness) {

  // Stress concentration factor from bend theory
  double ratio = bend_radius / tube_od;

  // Peterson's equation approximation
  if (ratio < 1.0) {
    return 2.5 - ratio;
  } else if (ratio < 3.0) {
    return 2.0;
  } else {
    return 1.5;
  }
}

}  // namespace bendcore::manufacturing
