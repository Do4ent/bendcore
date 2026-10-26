#include "bendcore/simulation/structural_solver.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace BendCore::CadCore::Simulation {

// ==================== MaterialProperties ====================

void MaterialProperties::computeShearModulus() noexcept {
    if (shear_modulus < 1e-10 && young_modulus > 1e-10 && poisson_ratio > 0.0) {
        // G = E / (2(1 + nu))
        shear_modulus = young_modulus / (2.0 * (1.0 + poisson_ratio));
    }
}

// ==================== StressTensor ====================

double StressTensor::vonMisesStress() const noexcept {
    // VM = sqrt((σxx - σyy)² + (σyy - σzz)² + (σzz - σxx)² + 6(τxy² + τyz² + τxz²)) / sqrt(2)
    double diff_xy = xx() - yy();
    double diff_yz = yy() - zz();
    double diff_zx = zz() - xx();

    double tau_sq = xy() * xy() + yz() * yz() + xz() * xz();

    double vm = std::sqrt(
        (diff_xy * diff_xy + diff_yz * diff_yz + diff_zx * diff_zx + 6.0 * tau_sq) / 2.0
    );

    return vm;
}

double StressTensor::principalStress1() const noexcept {
    // For now, return maximum normal stress as approximation
    return std::max({std::abs(xx()), std::abs(yy()), std::abs(zz())});
}

double StressTensor::principalStress2() const noexcept {
    return std::sqrt(principalStress1());
}

double StressTensor::principalStress3() const noexcept {
    return std::min({std::abs(xx()), std::abs(yy()), std::abs(zz())});
}

// ==================== StrainTensor ====================

double StrainTensor::equivalentStrain() const noexcept {
    // εeq = sqrt((εxx - εyy)² + (εyy - εzz)² + (εzz - εxx)² + 6(γxy² + γyz² + γxz²)) / sqrt(2)
    double diff_xy = xx() - yy();
    double diff_yz = yy() - zz();
    double diff_zx = zz() - xx();

    double gamma_sq = xy() * xy() + yz() * yz() + xz() * xz();

    double eq = std::sqrt(
        (diff_xy * diff_xy + diff_yz * diff_yz + diff_zx * diff_zx + 6.0 * gamma_sq) / 2.0
    );

    return eq;
}

// ==================== StructuralSolver ====================

StructuralSolver::StructuralSolver()
    : num_nodes_(0), num_elements_(0),
      max_stress_(0.0), max_displacement_(0.0),
      min_safety_factor_(1.0e10), converged_(false) {

    // Default material: Steel
    material_.name = "Steel";
    material_.young_modulus = 2.1e11;      // Pa
    material_.poisson_ratio = 0.3;
    material_.yield_strength = 2.5e8;      // 250 MPa
    material_.tensile_strength = 4.0e8;    // 400 MPa
    material_.density = 7850.0;            // kg/m^3
    material_.computeShearModulus();
}

void StructuralSolver::setMaterialProperties(const MaterialProperties& props) {
    material_ = props;
    material_.computeShearModulus();
}

void StructuralSolver::setNumNodes(size_t num_nodes) {
    num_nodes_ = num_nodes;
    node_coordinates_.resize(num_nodes);
    displacements_.resize(num_nodes);
}

void StructuralSolver::setNumElements(size_t num_elements) {
    num_elements_ = num_elements;
    elements_.resize(num_elements);
    element_results_.resize(num_elements);
}

void StructuralSolver::addNodeCoordinates(size_t node_id, const Vector3D& coords) {
    if (node_id < node_coordinates_.size()) {
        node_coordinates_[node_id] = coords;
    }
}

void StructuralSolver::addElement(size_t elem_id, const std::vector<size_t>& node_indices) {
    if (elem_id < elements_.size()) {
        elements_[elem_id] = node_indices;
    }
}

void StructuralSolver::addLoad(const Load& load) {
    loads_.push_back(load);
}

void StructuralSolver::addConstraint(const Constraint& constraint) {
    constraints_.push_back(constraint);
}

bool StructuralSolver::solve() {
    if (num_nodes_ == 0 || num_elements_ == 0) {
        return false;
    }

    initializeDisplacements();
    assembleStiffnessMatrix();
    applyBoundaryConditions();
    solveLinearSystem();
    computeElementStress();
    computeSafetyFactors();

    converged_ = true;
    return converged_;
}

void StructuralSolver::initializeDisplacements() {
    std::fill(displacements_.begin(), displacements_.end(), Vector3D(0, 0, 0));
    max_displacement_ = 0.0;
}

void StructuralSolver::assembleStiffnessMatrix() {
    // Simplified assembly: compute element stiffness matrices
    for (size_t elem_id = 0; elem_id < num_elements_; elem_id++) {
        if (elements_[elem_id].empty()) continue;

        // For each element, compute approximate stiffness contribution
        const auto& node_ids = elements_[elem_id];
        if (node_ids.size() < 3) continue;

        // Element volume approximation
        Vector3D v0 = node_coordinates_[node_ids[0]];
        Vector3D v1 = node_coordinates_[node_ids[1]];
        Vector3D v2 = node_coordinates_[node_ids[2]];

        Vector3D edge1 = v1 - v0;
        Vector3D edge2 = v2 - v0;
        double elem_size = edge1.magnitude() + edge2.magnitude();

        // Store element size for later use
        element_results_[elem_id].element_id = elem_id;
        element_results_[elem_id].displacement = Vector3D(0, 0, 0);
    }
}

void StructuralSolver::applyBoundaryConditions() {
    // Apply displacement constraints
    for (const auto& constraint : constraints_) {
        if (constraint.node_id < displacements_.size()) {
            switch (constraint.type) {
                case ConstraintType::FIXED:
                    displacements_[constraint.node_id] = Vector3D(0, 0, 0);
                    break;
                case ConstraintType::FIXED_X:
                    displacements_[constraint.node_id].x = 0.0;
                    break;
                case ConstraintType::FIXED_Y:
                    displacements_[constraint.node_id].y = 0.0;
                    break;
                case ConstraintType::FIXED_Z:
                    displacements_[constraint.node_id].z = 0.0;
                    break;
                case ConstraintType::PINNED:
                    displacements_[constraint.node_id].x = 0.0;
                    displacements_[constraint.node_id].y = 0.0;
                    break;
                case ConstraintType::ROLLER:
                    // Simplified: same as pinned
                    displacements_[constraint.node_id].x = 0.0;
                    displacements_[constraint.node_id].y = 0.0;
                    break;
            }
        }
    }
}

void StructuralSolver::solveLinearSystem() {
    // Simplified solution: apply loads and compute displacements
    // In a real FEA, this would solve K*u = F

    for (const auto& load : loads_) {
        if (load.node_id < displacements_.size()) {
            // Simplified displacement calculation: u = F / K (approximate)
            // K_approx ~ E*A/L
            double stiffness_approx = material_.young_modulus * 0.01 / 1.0;  // Rough approximation

            switch (load.type) {
                case LoadType::FORCE: {
                    Vector3D u = load.force * (1.0 / stiffness_approx);
                    displacements_[load.node_id] = displacements_[load.node_id] + u;
                    break;
                }
                case LoadType::DISPLACEMENT: {
                    // Displacement loads are handled through constraints, not loads
                    // This case is for completeness
                    break;
                }
                case LoadType::PRESSURE: {
                    // Simplified pressure load conversion
                    Vector3D force = Vector3D(0, 0, 1) * (load.pressure * 0.01);
                    Vector3D u = force * (1.0 / stiffness_approx);
                    displacements_[load.node_id] = displacements_[load.node_id] + u;
                    break;
                }
                default:
                    break;
            }
        }
    }

    // Calculate max displacement
    max_displacement_ = 0.0;
    for (const auto& disp : displacements_) {
        double mag = disp.magnitude();
        max_displacement_ = std::max(max_displacement_, mag);
    }
}

void StructuralSolver::computeElementStress() {
    max_stress_ = 0.0;

    for (size_t elem_id = 0; elem_id < num_elements_; elem_id++) {
        auto& result = element_results_[elem_id];

        // Compute element strain from nodal displacements
        const auto& node_ids = elements_[elem_id];
        if (node_ids.empty()) continue;

        // Average displacement at element center
        Vector3D avg_displacement = Vector3D(0, 0, 0);
        for (size_t node_id : node_ids) {
            if (node_id < displacements_.size()) {
                avg_displacement = avg_displacement + displacements_[node_id];
            }
        }
        avg_displacement = avg_displacement * (1.0 / std::max(size_t(1), node_ids.size()));

        result.displacement = avg_displacement;

        // Compute strain (simplified)
        double elem_size = 1.0;
        double strain = avg_displacement.magnitude() / elem_size;
        result.strain.components[0] = strain;
        result.strain.components[1] = strain * 0.3;  // Account for Poisson's effect
        result.strain.components[2] = strain * 0.3;

        // Compute stress using Hooke's law: σ = E*ε
        double stress = material_.young_modulus * strain;
        result.stress.components[0] = stress;
        result.stress.components[1] = stress * 0.3;  // σyy
        result.stress.components[2] = stress * 0.3;  // σzz

        // von Mises stress
        double vm_stress = result.stress.vonMisesStress();
        max_stress_ = std::max(max_stress_, vm_stress);
    }
}

void StructuralSolver::computeSafetyFactors() {
    min_safety_factor_ = 1.0e10;

    for (auto& result : element_results_) {
        double vm_stress = result.stress.vonMisesStress();

        if (vm_stress > 1e-10) {
            result.safety_factor = material_.yield_strength / vm_stress;
        } else {
            result.safety_factor = 100.0;  // No stress, high safety factor
        }

        min_safety_factor_ = std::min(min_safety_factor_, result.safety_factor);
    }

    // Ensure min_safety_factor is reasonable
    if (min_safety_factor_ > 1e9) {
        min_safety_factor_ = 100.0;
    }
}

}  // namespace BendCore::CadCore::Simulation
