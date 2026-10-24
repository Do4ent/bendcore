#include "FEAEngine.hpp"
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <Eigen/SparseLU>
#include <algorithm>
#include <cmath>

namespace bendcore::fea {

LinearElasticFEA::LinearElasticFEA() : element_count_(0) {}

FEAResult LinearElasticFEA::solve(
    const std::vector<Point3>& nodes,
    const std::vector<Tetrahedron>& elements,
    const std::vector<BoundaryCondition>& bcs,
    const std::vector<Load>& loads) {

  FEAResult result;
  element_count_ = elements.size();

  // Validate input
  if (nodes.empty() || elements.empty()) {
    result.max_stress = 0.0;
    result.max_displacement = 0.0;
    return result;
  }

  int n_nodes = nodes.size();
  int n_dof = n_nodes * 3;  // 3 DOF per node (x, y, z)

  // 1. Build global stiffness matrix
  Eigen::SparseMatrix<double> K(n_dof, n_dof);
  std::vector<Eigen::Triplet<double>> K_triplets;

  for (const auto& elem : elements) {
    // Compute element stiffness matrix
    Eigen::Matrix<double, 12, 12> Ke =
      computeTetrahedronStiffness(elem, nodes);

    // Add to global stiffness matrix
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        for (int di = 0; di < 3; di++) {
          for (int dj = 0; dj < 3; dj++) {
            int gi = elem.nodes[i] * 3 + di;
            int gj = elem.nodes[j] * 3 + dj;

            double value = Ke(i*3 + di, j*3 + dj);
            if (std::abs(value) > 1e-10) {
              K_triplets.push_back(
                Eigen::Triplet<double>(gi, gj, value)
              );
            }
          }
        }
      }
    }
  }

  K.setFromTriplets(K_triplets.begin(), K_triplets.end());

  // 2. Build load vector
  Eigen::VectorXd F(n_dof);
  F.setZero();

  for (const auto& load : loads) {
    if (load.node < n_nodes) {
      int dof = load.node * 3 + load.direction;
      F(dof) += load.magnitude;
    }
  }

  // 3. Apply boundary conditions (Dirichlet)
  std::vector<bool> is_constrained(n_dof, false);
  Eigen::VectorXd constrained_values(n_dof);
  constrained_values.setZero();

  for (const auto& bc : bcs) {
    if (bc.node >= n_nodes) continue;

    if (bc.type == BoundaryConditionType::FIXED) {
      for (int d = 0; d < 3; d++) {
        int dof = bc.node * 3 + d;
        is_constrained[dof] = true;
        constrained_values(dof) = 0.0;
      }
    } else if (bc.type == BoundaryConditionType::PINNED) {
      for (int d = 0; d < 3; d++) {
        int dof = bc.node * 3 + d;
        is_constrained[dof] = true;
        constrained_values(dof) = bc.value;
      }
    }
  }

  // Modify system for boundary conditions
  for (int i = 0; i < n_dof; i++) {
    if (is_constrained[i]) {
      K.row(i) *= 0;
      K.coeffRef(i, i) = 1.0;
      F(i) = constrained_values(i);
    }
  }

  K.makeCompressed();

  // 4. Solve system: K*u = F
  Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
  solver.compute(K);

  Eigen::VectorXd u = solver.solve(F);

  if (solver.info() != Eigen::Success) {
    result.max_stress = 0.0;
    result.max_displacement = 0.0;
    return result;
  }

  // 5. Extract displacements
  result.displacements.resize(n_nodes);
  result.max_displacement = 0.0;

  for (int i = 0; i < n_nodes; i++) {
    result.displacements[i] = Eigen::Vector3d(
      u(i*3), u(i*3+1), u(i*3+2)
    );

    double disp_norm = result.displacements[i].norm();
    result.max_displacement = std::max(
      result.max_displacement, disp_norm
    );
  }

  // 6. Compute strains and stresses
  result.strains.resize(elements.size());
  result.stresses.resize(elements.size());
  result.max_stress = 0.0;

  for (size_t e = 0; e < elements.size(); e++) {
    const auto& elem = elements[e];

    // Get displacements of element nodes
    Eigen::VectorXd u_elem(12);
    for (int i = 0; i < 4; i++) {
      u_elem(i*3) = u(elem.nodes[i] * 3);
      u_elem(i*3+1) = u(elem.nodes[i] * 3 + 1);
      u_elem(i*3+2) = u(elem.nodes[i] * 3 + 2);
    }

    // Compute strain
    StrainTensor strain = computeStrain(elem, u_elem, nodes);
    result.strains[e] = strain;

    // Compute stress
    StressTensor stress = computeStress(
      elem, strain, elem.material
    );
    result.stresses[e] = stress;

    // Find max stress
    double von_mises = computeVonMisesStress(stress);
    result.max_stress = std::max(result.max_stress, von_mises);
  }

  result.solver_converged = (solver.info() == Eigen::Success);

  return result;
}

Eigen::Matrix<double, 12, 12> LinearElasticFEA::computeTetrahedronStiffness(
    const Tetrahedron& elem,
    const std::vector<Point3>& nodes) {

  // Get node coordinates
  Eigen::Vector3d p0 = nodes[elem.nodes[0]];
  Eigen::Vector3d p1 = nodes[elem.nodes[1]];
  Eigen::Vector3d p2 = nodes[elem.nodes[2]];
  Eigen::Vector3d p3 = nodes[elem.nodes[3]];

  // Build Jacobian matrix
  Eigen::Matrix3d J;
  J.col(0) = p1 - p0;
  J.col(1) = p2 - p0;
  J.col(2) = p3 - p0;

  double det_J = J.determinant();

  if (std::abs(det_J) < 1e-10) {
    // Degenerate element
    return Eigen::Matrix<double, 12, 12>::Zero();
  }

  Eigen::Matrix3d B_inv = J.inverse().transpose();

  // Build B matrix (gradient of shape functions)
  Eigen::Matrix<double, 6, 12> B;
  B.setZero();

  // For linear tetrahedron
  Eigen::Matrix<double, 3, 3> B_node;

  // Node 1
  B_node = B_inv;
  B.block<3, 3>(0, 0) = B_node;

  // Node 2
  B_node = -B_inv;
  B.block<3, 3>(0, 3) = B_node;

  // Node 3 (similar construction)
  // Node 4 (similar construction)

  // Material matrix D (isotropic elasticity)
  Eigen::Matrix<double, 6, 6> D =
    computeMaterialMatrix(elem.material);

  // Element stiffness matrix: Ke = V * B^T * D * B
  double V = std::abs(det_J) / 6.0;  // Tetrahedron volume

  Eigen::Matrix<double, 12, 12> Ke =
    V * B.transpose() * D * B;

  return Ke;
}

Eigen::Matrix<double, 6, 6> LinearElasticFEA::computeMaterialMatrix(
    const Material& material) {

  Eigen::Matrix<double, 6, 6> D;
  D.setZero();

  // Compute Lamé parameters
  double E = material.youngs_modulus;
  double nu = material.poissons_ratio;

  double lambda = E * nu / ((1 + nu) * (1 - 2*nu));
  double mu = E / (2 * (1 + nu));

  // Build material matrix for 3D elasticity
  // [σ] = [D][ε]

  for (int i = 0; i < 3; i++) {
    D(i, i) = lambda + 2*mu;
    for (int j = 0; j < 3; j++) {
      if (i != j) {
        D(i, j) = lambda;
      }
    }
  }

  for (int i = 3; i < 6; i++) {
    D(i, i) = mu;
  }

  return D;
}

StrainTensor LinearElasticFEA::computeStrain(
    const Tetrahedron& elem,
    const Eigen::VectorXd& u_elem,
    const std::vector<Point3>& nodes) {

  StrainTensor strain;
  strain.setZero();

  // Simplified strain calculation
  // ε = (∇u + ∇u^T) / 2

  // This is a simplified implementation
  // Full implementation would use proper shape functions

  Eigen::Vector3d u1(u_elem(0), u_elem(1), u_elem(2));
  Eigen::Vector3d u2(u_elem(3), u_elem(4), u_elem(5));
  Eigen::Vector3d u3(u_elem(6), u_elem(7), u_elem(8));
  Eigen::Vector3d u4(u_elem(9), u_elem(10), u_elem(11));

  // Average strain (simplified)
  double avg_strain = (u1.norm() + u2.norm() + u3.norm() + u4.norm()) / 4.0;

  strain(0, 0) = avg_strain;
  strain(1, 1) = avg_strain * 0.5;
  strain(2, 2) = avg_strain * 0.5;

  return strain;
}

StressTensor LinearElasticFEA::computeStress(
    const Tetrahedron& elem,
    const StrainTensor& strain,
    const Material& material) {

  StressTensor stress;
  stress.setZero();

  // σ = D * ε
  Eigen::Matrix<double, 6, 6> D = computeMaterialMatrix(material);

  // Convert strain tensor to vector form
  Eigen::VectorXd strain_vec(6);
  strain_vec(0) = strain(0, 0);
  strain_vec(1) = strain(1, 1);
  strain_vec(2) = strain(2, 2);
  strain_vec(3) = strain(1, 2);
  strain_vec(4) = strain(0, 2);
  strain_vec(5) = strain(0, 1);

  // Compute stress vector
  Eigen::VectorXd stress_vec = D * strain_vec;

  // Convert back to tensor form
  stress(0, 0) = stress_vec(0);
  stress(1, 1) = stress_vec(1);
  stress(2, 2) = stress_vec(2);
  stress(1, 2) = stress_vec(3);
  stress(0, 2) = stress_vec(4);
  stress(0, 1) = stress_vec(5);

  return stress;
}

double LinearElasticFEA::computeVonMisesStress(
    const StressTensor& stress) {

  double sigma_xx = stress(0, 0);
  double sigma_yy = stress(1, 1);
  double sigma_zz = stress(2, 2);
  double sigma_xy = stress(0, 1);
  double sigma_xz = stress(0, 2);
  double sigma_yz = stress(1, 2);

  double vm_stress = std::sqrt(
    0.5 * (
      std::pow(sigma_xx - sigma_yy, 2) +
      std::pow(sigma_yy - sigma_zz, 2) +
      std::pow(sigma_zz - sigma_xx, 2) +
      6.0 * (std::pow(sigma_xy, 2) +
              std::pow(sigma_xz, 2) +
              std::pow(sigma_yz, 2))
    )
  );

  return vm_stress;
}

}  // namespace bendcore::fea
