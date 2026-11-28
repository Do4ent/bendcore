#include "bendcore/simulation/structural_solver.hpp"
#include <cmath>
#include <algorithm>
#include <limits>
#include <numeric>
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <Eigen/SparseLU>

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
    return principalStresses()[0];
}

double StressTensor::principalStress2() const noexcept {
    return principalStresses()[1];
}

double StressTensor::principalStress3() const noexcept {
    return principalStresses()[2];
}

std::array<double, 3> StressTensor::principalStresses() const noexcept {
    // Eigenvalues of the symmetric Cauchy stress tensor, sigma1 >= sigma2 >= sigma3 (signed).
    Eigen::Matrix3d s;
    s << xx(), xy(), xz(),
         xy(), yy(), yz(),
         xz(), yz(), zz();
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> eig(s, Eigen::EigenvaluesOnly);
    const Eigen::Vector3d v = eig.eigenvalues();  // ascending
    return {v[2], v[1], v[0]};
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
      min_safety_factor_(std::numeric_limits<double>::infinity()), converged_(false) {

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

namespace {

using Matrix6x12 = Eigen::Matrix<double, 6, 12>;

// Strain-displacement matrix of a linear 4-node tetrahedron (constant strain), Voigt order
// xx, yy, zz, xy, yz, xz with engineering shear strains. Returns the volume (0 if degenerate).
double tetB(const std::array<Vector3D, 4>& x, Matrix6x12& B) {
    Eigen::Matrix4d M;
    for (int i = 0; i < 4; ++i) M.row(i) << 1.0, x[i].x, x[i].y, x[i].z;
    const double det = M.determinant();
    const double volume = std::abs(det) / 6.0;
    const double scale = std::max({(x[1] - x[0]).magnitude(), (x[2] - x[0]).magnitude(),
                                   (x[3] - x[0]).magnitude()});
    if (!(volume > 1e-12 * scale * scale * scale)) return 0.0;
    const Eigen::Matrix4d C = M.inverse();  // N_i = C(0,i) + C(1,i) x + C(2,i) y + C(3,i) z
    B.setZero();
    for (int i = 0; i < 4; ++i) {
        const double bx = C(1, i), by = C(2, i), bz = C(3, i);
        B(0, 3 * i) = bx;
        B(1, 3 * i + 1) = by;
        B(2, 3 * i + 2) = bz;
        B(3, 3 * i) = by;  B(3, 3 * i + 1) = bx;
        B(4, 3 * i + 1) = bz;  B(4, 3 * i + 2) = by;
        B(5, 3 * i) = bz;  B(5, 3 * i + 2) = bx;
    }
    return volume;
}

Eigen::Matrix<double, 6, 6> isotropicD(double E, double nu) {
    const double lambda = E * nu / ((1.0 + nu) * (1.0 - 2.0 * nu));
    const double mu = E / (2.0 * (1.0 + nu));
    Eigen::Matrix<double, 6, 6> D = Eigen::Matrix<double, 6, 6>::Zero();
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) D(i, j) = lambda;
        D(i, i) = lambda + 2.0 * mu;
        D(3 + i, 3 + i) = mu;
    }
    return D;
}

}  // namespace

bool StructuralSolver::solve() {
    converged_ = false;
    max_stress_ = 0.0;
    max_displacement_ = 0.0;
    min_safety_factor_ = std::numeric_limits<double>::infinity();
    std::fill(displacements_.begin(), displacements_.end(), Vector3D(0, 0, 0));
    for (size_t e = 0; e < element_results_.size(); ++e) {
        element_results_[e] = StructuralResults{};
        element_results_[e].element_id = e;
    }

    const size_t n = num_nodes_;
    if (n == 0 || num_elements_ == 0) return false;
    const double E = material_.young_modulus;
    const double nu = material_.poisson_ratio;
    if (!(E > 0.0) || !(nu > -1.0 && nu < 0.5)) return false;

    // Element geometry: only linear tetrahedra.
    std::vector<Matrix6x12> Bs(num_elements_);
    std::vector<double> volumes(num_elements_, 0.0);
    for (size_t e = 0; e < num_elements_; ++e) {
        const auto& ids = elements_[e];
        if (ids.size() != 4) return false;
        std::array<Vector3D, 4> x;
        for (int i = 0; i < 4; ++i) {
            if (ids[i] >= n) return false;
            x[i] = node_coordinates_[ids[i]];
        }
        volumes[e] = tetB(x, Bs[e]);
        if (volumes[e] == 0.0) return false;
    }

    // Loads: point forces only (pressure, distributed and thermal loads need face/element
    // data the Load structure does not carry, so they are rejected rather than guessed).
    const size_t ndof = 3 * n;
    Eigen::VectorXd F = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(ndof));
    for (const auto& load : loads_) {
        if (load.type != LoadType::FORCE || load.node_id >= n) return false;
        F[3 * load.node_id] += load.force.x;
        F[3 * load.node_id + 1] += load.force.y;
        F[3 * load.node_id + 2] += load.force.z;
    }

    // Prescribed displacements (value from Constraint::displacement, default 0).
    std::vector<bool> fixed(ndof, false);
    std::vector<double> value(ndof, 0.0);
    auto fix = [&](size_t node, int axis, double v) {
        fixed[3 * node + axis] = true;
        value[3 * node + axis] = v;
    };
    for (const auto& c : constraints_) {
        if (c.node_id >= n) return false;
        const Vector3D& d = c.displacement;
        switch (c.type) {
            case ConstraintType::FIXED: fix(c.node_id, 0, d.x); fix(c.node_id, 1, d.y); fix(c.node_id, 2, d.z); break;
            case ConstraintType::FIXED_X: fix(c.node_id, 0, d.x); break;
            case ConstraintType::FIXED_Y: fix(c.node_id, 1, d.y); break;
            case ConstraintType::FIXED_Z: fix(c.node_id, 2, d.z); break;
            case ConstraintType::PINNED: fix(c.node_id, 0, d.x); fix(c.node_id, 1, d.y); break;
            case ConstraintType::ROLLER:
                return false;  // needs a surface normal, which Constraint does not carry
        }
    }

    // Assembly K = sum V B^T D B, Dirichlet rows replaced by identity.
    const Eigen::Matrix<double, 6, 6> D = isotropicD(E, nu);
    std::vector<Eigen::Triplet<double>> triplets;
    Eigen::VectorXd rhs = F;
    Eigen::SparseMatrix<double> K_full(static_cast<Eigen::Index>(ndof), static_cast<Eigen::Index>(ndof));
    {
        std::vector<Eigen::Triplet<double>> all;
        all.reserve(num_elements_ * 144);
        for (size_t e = 0; e < num_elements_; ++e) {
            const Eigen::Matrix<double, 12, 12> Ke = volumes[e] * Bs[e].transpose() * D * Bs[e];
            for (int a = 0; a < 12; ++a) {
                for (int b = 0; b < 12; ++b) {
                    const size_t r = 3 * elements_[e][a / 3] + a % 3;
                    const size_t c = 3 * elements_[e][b / 3] + b % 3;
                    all.emplace_back(r, c, Ke(a, b));
                }
            }
        }
        K_full.setFromTriplets(all.begin(), all.end());
    }
    for (int k = 0; k < K_full.outerSize(); ++k) {
        for (Eigen::SparseMatrix<double>::InnerIterator it(K_full, k); it; ++it) {
            const auto r = static_cast<size_t>(it.row());
            const auto c = static_cast<size_t>(it.col());
            if (fixed[r]) continue;
            if (fixed[c]) {
                rhs[it.row()] -= it.value() * value[c];
            } else {
                triplets.emplace_back(it.row(), it.col(), it.value());
            }
        }
    }
    for (size_t d = 0; d < ndof; ++d) {
        if (fixed[d]) {
            triplets.emplace_back(d, d, 1.0);
            rhs[static_cast<Eigen::Index>(d)] = value[d];
        }
    }
    Eigen::SparseMatrix<double> K(static_cast<Eigen::Index>(ndof), static_cast<Eigen::Index>(ndof));
    K.setFromTriplets(triplets.begin(), triplets.end());
    Eigen::SparseLU<Eigen::SparseMatrix<double>> lu;
    lu.compute(K);
    if (lu.info() != Eigen::Success) return false;  // mechanism: rigid-body motion not restrained
    const Eigen::VectorXd u = lu.solve(rhs);
    if (lu.info() != Eigen::Success || !u.allFinite()) return false;
    // A nearly singular system (insufficient constraints) shows up as a large residual.
    const double rhs_norm = std::max(rhs.norm(), 1e-300);
    if ((K * u - rhs).norm() > 1e-8 * rhs_norm) return false;

    for (size_t i = 0; i < n; ++i) {
        displacements_[i] = Vector3D(u[3 * i], u[3 * i + 1], u[3 * i + 2]);
        max_displacement_ = std::max(max_displacement_, displacements_[i].magnitude());
    }

    // Element strain = B u_e, stress = D strain (constant per element).
    for (size_t e = 0; e < num_elements_; ++e) {
        Eigen::Matrix<double, 12, 1> ue;
        Vector3D avg(0, 0, 0);
        for (int a = 0; a < 4; ++a) {
            const size_t node = elements_[e][a];
            ue.segment<3>(3 * a) << u[3 * node], u[3 * node + 1], u[3 * node + 2];
            avg = avg + displacements_[node];
        }
        const Eigen::Matrix<double, 6, 1> strain = Bs[e] * ue;
        const Eigen::Matrix<double, 6, 1> stress = D * strain;
        auto& r = element_results_[e];
        r.element_id = e;
        r.displacement = avg * 0.25;
        for (int k = 0; k < 6; ++k) {
            r.strain.components[k] = strain[k];
            r.stress.components[k] = stress[k];
        }
        const double vm = r.stress.vonMisesStress();
        max_stress_ = std::max(max_stress_, vm);
        r.safety_factor = vm > 0.0 ? material_.yield_strength / vm
                                   : std::numeric_limits<double>::infinity();
        min_safety_factor_ = std::min(min_safety_factor_, r.safety_factor);
    }

    converged_ = true;
    return true;
}

}  // namespace BendCore::CadCore::Simulation
