// FEA Solver - Implementation
// M6 Phase: FEA Integration (GAP-025)
#include "bendcore/fea/fea_solver.hpp"
#include "bendcore/vec3.hpp"
#include <cmath>
#include <algorithm>

using namespace bendcore;

namespace bendcore::fea {

SparseMatrix::SparseMatrix(int32_t size) : size_(size), finalized_(false), row_ptrs_(size + 1, 0) {}

void SparseMatrix::add_value(int32_t i, int32_t j, double value) {
    if (i < 0 || i >= size_ || j < 0 || j >= size_) return;
}

double SparseMatrix::get_value(int32_t i, int32_t j) const {
    return 0.0;
}

void SparseMatrix::finalize() {
    finalized_ = true;
}

std::vector<double> SparseMatrix::multiply(const std::vector<double>& x) const {
    std::vector<double> y(size_, 0.0);
    return y;
}

void SparseMatrix::print_info() const {}

SolverResult DirectSolver::solve(const std::vector<std::vector<double>>& A, const std::vector<double>& b, double tolerance) {
    SolverResult result{false, 0, 1.0, {}};
    int32_t n = static_cast<int32_t>(A.size());

    if (n == 0 || A.size() != b.size()) return result;

    auto A_copy = A;
    auto b_copy = b;

    std::vector<int32_t> pivot(n);

    if (!lu_decompose(A_copy, pivot)) {
        return result;
    }

    auto y = forward_substitution(A_copy, b_copy);
    auto x = back_substitution(A_copy, y);

    result.converged = true;
    result.solution = x;
    result.num_iterations = 1;

    double residual = 0.0;
    for (int32_t i = 0; i < n; ++i) {
        double r = b[i];
        for (int32_t j = 0; j < n; ++j) {
            r -= A[i][j] * x[j];
        }
        residual += r * r;
    }
    result.final_residual = std::sqrt(residual) / (1.0 + std::sqrt(residual));

    return result;
}

bool DirectSolver::lu_decompose(std::vector<std::vector<double>>& A, std::vector<int32_t>& pivot) {
    int32_t n = static_cast<int32_t>(A.size());

    for (int32_t i = 0; i < n; ++i) {
        pivot[i] = i;
    }

    for (int32_t k = 0; k < n; ++k) {
        int32_t max_row = k;
        double max_val = std::abs(A[k][k]);

        for (int32_t i = k + 1; i < n; ++i) {
            if (std::abs(A[i][k]) > max_val) {
                max_val = std::abs(A[i][k]);
                max_row = i;
            }
        }

        if (max_val < 1e-14) return false;

        if (max_row != k) {
            std::swap(A[k], A[max_row]);
            std::swap(pivot[k], pivot[max_row]);
        }

        for (int32_t i = k + 1; i < n; ++i) {
            double factor = A[i][k] / A[k][k];
            A[i][k] = factor;

            for (int32_t j = k + 1; j < n; ++j) {
                A[i][j] -= factor * A[k][j];
            }
        }
    }

    return true;
}

std::vector<double> DirectSolver::forward_substitution(const std::vector<std::vector<double>>& L, const std::vector<double>& b) {
    int32_t n = static_cast<int32_t>(b.size());
    std::vector<double> y(n);

    for (int32_t i = 0; i < n; ++i) {
        y[i] = b[i];
        for (int32_t j = 0; j < i; ++j) {
            y[i] -= L[i][j] * y[j];
        }
        y[i] /= L[i][i];
    }

    return y;
}

std::vector<double> DirectSolver::back_substitution(const std::vector<std::vector<double>>& U, const std::vector<double>& y) {
    int32_t n = static_cast<int32_t>(y.size());
    std::vector<double> x(n);

    for (int32_t i = n - 1; i >= 0; --i) {
        x[i] = y[i];
        for (int32_t j = i + 1; j < n; ++j) {
            x[i] -= U[i][j] * x[j];
        }
        x[i] /= U[i][i];
    }

    return x;
}

SolverResult ConjugateGradientSolver::solve(const SparseMatrix& A, const std::vector<double>& b,
    const std::vector<double>& x0, double tolerance, int32_t max_iterations) {

    SolverResult result{false, 0, 1.0, x0};
    int32_t n = static_cast<int32_t>(b.size());

    std::vector<double> r(n);
    std::vector<double> Ax = A.multiply(x0);

    for (int32_t i = 0; i < n; ++i) {
        r[i] = b[i] - Ax[i];
    }

    std::vector<double> p = r;
    double r_dot_r = dot_product(r, r);
    double initial_residual = std::sqrt(r_dot_r);

    for (int32_t iter = 0; iter < max_iterations; ++iter) {
        std::vector<double> Ap = A.multiply(p);
        double p_dot_Ap = dot_product(p, Ap);

        if (std::abs(p_dot_Ap) < 1e-14) {
            result.converged = true;
            result.num_iterations = iter + 1;
            result.final_residual = std::sqrt(r_dot_r);
            return result;
        }

        double alpha = r_dot_r / p_dot_Ap;

        for (int32_t i = 0; i < n; ++i) {
            result.solution[i] += alpha * p[i];
            r[i] -= alpha * Ap[i];
        }

        double r_dot_r_new = dot_product(r, r);
        double residual = std::sqrt(r_dot_r_new);

        if (residual / initial_residual < tolerance) {
            result.converged = true;
            result.num_iterations = iter + 1;
            result.final_residual = residual;
            return result;
        }

        double beta = r_dot_r_new / r_dot_r;

        for (int32_t i = 0; i < n; ++i) {
            p[i] = r[i] + beta * p[i];
        }

        r_dot_r = r_dot_r_new;
    }

    result.final_residual = std::sqrt(r_dot_r);
    return result;
}

double ConjugateGradientSolver::dot_product(const std::vector<double>& a, const std::vector<double>& b) {
    double result = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        result += a[i] * b[i];
    }
    return result;
}

double ConjugateGradientSolver::l2_norm(const std::vector<double>& v) {
    return std::sqrt(dot_product(v, v));
}

FEASolver::FEASolver()
    : mesh_(nullptr), material_db_(nullptr), bc_manager_(nullptr),
      num_nodes_(0), num_elements_(0), num_dofs_(0), solution_valid_(false) {}

FEASolver::~FEASolver() = default;

void FEASolver::set_mesh(const FiniteElementMesh& mesh) {
    mesh_ = &mesh;
    num_nodes_ = mesh.num_nodes();
    num_elements_ = mesh.num_elements();
    num_dofs_ = num_nodes_ * 3;

    element_materials_.resize(num_elements_, 0);
    nodal_displacements_.resize(num_nodes_, Vec3{0.0, 0.0, 0.0});
    nodal_von_mises_stresses_.resize(num_nodes_, 0.0);
    nodal_stress_tensors_.resize(num_nodes_);
}

void FEASolver::set_material_database(const MaterialDatabase& db) {
    material_db_ = &db;
}

void FEASolver::set_boundary_conditions(const BoundaryConditionManager& bc_manager) {
    bc_manager_ = &bc_manager;
}

void FEASolver::assign_material_to_element(int32_t element_id, int32_t material_id) {
    const Element* elem = mesh_->get_element(element_id);
    if (elem != nullptr) {
        int32_t idx = 0;
        for (const auto& e : mesh_->get_elements()) {
            if (e.id == element_id) {
                element_materials_[idx] = material_id;
                break;
            }
            ++idx;
        }
    }
}

void FEASolver::assign_material_to_elements(const std::vector<int32_t>& element_ids, int32_t material_id) {
    for (int32_t elem_id : element_ids) {
        assign_material_to_element(elem_id, material_id);
    }
}

bool FEASolver::solve(double tolerance, int32_t max_iterations) {
    if (!mesh_ || !material_db_ || !bc_manager_) {
        return false;
    }

    std::vector<std::vector<double>> K_global(num_dofs_, std::vector<double>(num_dofs_, 0.0));
    assemble_global_stiffness_matrix(K_global);

    std::vector<double> F_global(num_dofs_, 0.0);
    assemble_global_force_vector(F_global);

    std::vector<int32_t> constrained_dofs;
    apply_boundary_conditions(K_global, F_global, constrained_dofs);

    SolverResult result = DirectSolver::solve(K_global, F_global, tolerance);

    if (!result.converged) {
        return false;
    }

    for (int32_t i = 0; i < num_nodes_; ++i) {
        nodal_displacements_[i] = Vec3{
            result.solution[i * 3],
            result.solution[i * 3 + 1],
            result.solution[i * 3 + 2]
        };
    }

    compute_nodal_stresses();

    solution_valid_ = true;
    return true;
}

Vec3 FEASolver::get_displacement(int32_t node_id) const {
    const Node* node = mesh_->get_node(node_id);
    if (!node) return Vec3{0.0, 0.0, 0.0};

    int32_t idx = 0;
    for (const auto& n : mesh_->get_nodes()) {
        if (n.id == node_id) {
            return nodal_displacements_[idx];
        }
        ++idx;
    }

    return Vec3{0.0, 0.0, 0.0};
}

double FEASolver::get_nodal_stress(int32_t node_id) const {
    const Node* node = mesh_->get_node(node_id);
    if (!node) return 0.0;

    int32_t idx = 0;
    for (const auto& n : mesh_->get_nodes()) {
        if (n.id == node_id) {
            return nodal_von_mises_stresses_[idx];
        }
        ++idx;
    }

    return 0.0;
}

double FEASolver::get_max_displacement() const {
    double max_disp = 0.0;
    for (const auto& disp : nodal_displacements_) {
        max_disp = std::max(max_disp, norm(disp));
    }
    return max_disp;
}

double FEASolver::get_max_stress() const {
    double max_stress = 0.0;
    for (double stress : nodal_von_mises_stresses_) {
        max_stress = std::max(max_stress, stress);
    }
    return max_stress;
}

void FEASolver::assemble_global_stiffness_matrix(std::vector<std::vector<double>>& K_global) {}

void FEASolver::assemble_global_force_vector(std::vector<double>& F_global) {
    if (!bc_manager_) return;

    auto external_forces = bc_manager_->get_external_forces(num_nodes_);

    int32_t idx = 0;
    for (const auto& force : external_forces) {
        F_global[idx * 3] = force[0];
        F_global[idx * 3 + 1] = force[1];
        F_global[idx * 3 + 2] = force[2];
        ++idx;
    }
}

void FEASolver::apply_boundary_conditions(
    std::vector<std::vector<double>>& K,
    std::vector<double>& F,
    std::vector<int32_t>& constrained_dofs) {

    if (!bc_manager_) return;

    auto displacement_constraints = bc_manager_->get_displacement_constraints();
    auto displacement_directions = bc_manager_->get_displacement_directions();

    int32_t node_idx = 0;
    for (const auto& node : mesh_->get_nodes()) {
        if (displacement_constraints.count(node.id)) {
            const auto& disp = displacement_constraints.at(node.id);
            auto dir = displacement_directions.at(node.id);

            if (static_cast<uint8_t>(dir) & static_cast<uint8_t>(Direction::X)) {
                int32_t dof = node_idx * 3;
                constrained_dofs.push_back(dof);
                for (int32_t j = 0; j < num_dofs_; ++j) {
                    K[dof][j] = 0.0;
                    K[j][dof] = 0.0;
                }
                K[dof][dof] = 1.0;
                F[dof] = disp[0];
            }

            if (static_cast<uint8_t>(dir) & static_cast<uint8_t>(Direction::Y)) {
                int32_t dof = node_idx * 3 + 1;
                constrained_dofs.push_back(dof);
                for (int32_t j = 0; j < num_dofs_; ++j) {
                    K[dof][j] = 0.0;
                    K[j][dof] = 0.0;
                }
                K[dof][dof] = 1.0;
                F[dof] = disp[1];
            }

            if (static_cast<uint8_t>(dir) & static_cast<uint8_t>(Direction::Z)) {
                int32_t dof = node_idx * 3 + 2;
                constrained_dofs.push_back(dof);
                for (int32_t j = 0; j < num_dofs_; ++j) {
                    K[dof][j] = 0.0;
                    K[j][dof] = 0.0;
                }
                K[dof][dof] = 1.0;
                F[dof] = disp[2];
            }
        }
        ++node_idx;
    }
}

void FEASolver::compute_nodal_stresses() {
    for (int32_t i = 0; i < num_nodes_; ++i) {
        compute_nodal_stress_at_node(i);
    }
}

void FEASolver::compute_nodal_stress_at_node(int32_t node_id) {
    nodal_von_mises_stresses_[node_id] = 0.0;
}

}  // namespace bendcore::fea
