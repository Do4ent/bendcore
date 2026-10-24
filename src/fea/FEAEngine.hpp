#pragma once

#include <vector>
#include <Eigen/Dense>
#include <Eigen/Sparse>

namespace bendcore::fea {

using Point3 = Eigen::Vector3d;
using StrainTensor = Eigen::Matrix3d;
using StressTensor = Eigen::Matrix3d;

enum class BoundaryConditionType {
  FIXED,      // Zero displacement
  PINNED,     // Specified displacement
  ROLLER,     // Constrained in one direction
};

struct Material {
  double youngs_modulus;    // Elastic modulus (Pa)
  double poissons_ratio;    // Poisson's ratio (0-0.5)
  double density;           // Density (kg/m^3)
  std::string name;
};

struct Tetrahedron {
  std::array<int, 4> nodes;  // Node indices
  Material material;
  double volume = 0.0;
};

struct BoundaryCondition {
  int node;
  BoundaryConditionType type;
  double value = 0.0;  // For pinned BC
};

struct Load {
  int node;
  int direction;  // 0=x, 1=y, 2=z
  double magnitude;
};

struct FEAResult {
  std::vector<Eigen::Vector3d> displacements;  // u per node
  std::vector<StrainTensor> strains;           // ε per element
  std::vector<StressTensor> stresses;          // σ per element
  double max_stress = 0.0;
  double max_displacement = 0.0;
  bool solver_converged = false;
};

class LinearElasticFEA {
 public:
  LinearElasticFEA();

  // Main solver
  FEAResult solve(
    const std::vector<Point3>& nodes,
    const std::vector<Tetrahedron>& elements,
    const std::vector<BoundaryCondition>& bcs,
    const std::vector<Load>& loads
  );

  // Material library
  static Material getSteel() {
    return Material{
      .youngs_modulus = 200e9,    // 200 GPa
      .poissons_ratio = 0.3,
      .density = 7850.0,
      .name = "Steel"
    };
  }

  static Material getAluminum() {
    return Material{
      .youngs_modulus = 70e9,     // 70 GPa
      .poissons_ratio = 0.33,
      .density = 2700.0,
      .name = "Aluminum"
    };
  }

  static Material getTitanium() {
    return Material{
      .youngs_modulus = 103e9,    // 103 GPa
      .poissons_ratio = 0.34,
      .density = 4506.0,
      .name = "Titanium"
    };
  }

  // Getters
  int getElementCount() const { return element_count_; }

 private:
  // Element stiffness computation
  Eigen::Matrix<double, 12, 12> computeTetrahedronStiffness(
    const Tetrahedron& elem,
    const std::vector<Point3>& nodes
  );

  // Material matrix
  Eigen::Matrix<double, 6, 6> computeMaterialMatrix(
    const Material& material
  );

  // Post-processing
  StrainTensor computeStrain(
    const Tetrahedron& elem,
    const Eigen::VectorXd& u_elem,
    const std::vector<Point3>& nodes
  );

  StressTensor computeStress(
    const Tetrahedron& elem,
    const StrainTensor& strain,
    const Material& material
  );

  // Stress invariants
  double computeVonMisesStress(const StressTensor& stress);

  // State
  int element_count_ = 0;
};

}  // namespace bendcore::fea
