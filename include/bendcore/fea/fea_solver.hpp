// FEA Solver - Linear Static Analysis
// M6 Phase: FEA Integration (GAP-025)
#pragma once

#include "bendcore/fea/mesh_generator.hpp"
#include "bendcore/fea/element_library.hpp"
#include "bendcore/fea/material_database.hpp"
#include "bendcore/fea/boundary_conditions.hpp"
#include <vector>
#include <array>
#include <cstdint>
#include <memory>

namespace bendcore::fea {

// Sparse matrix in CSR (Compressed Sparse Row) format
class SparseMatrix {
public:
    SparseMatrix(int32_t size);

    void add_value(int32_t i, int32_t j, double value);
    double get_value(int32_t i, int32_t j) const;

    int32_t get_size() const { return size_; }
    const std::vector<double>& get_values() const { return values_; }
    const std::vector<int32_t>& get_columns() const { return columns_; }
    const std::vector<int32_t>& get_row_ptrs() const { return row_ptrs_; }

    void finalize();
    bool is_finalized() const { return finalized_; }

    // Matrix-vector multiplication
    std::vector<double> multiply(const std::vector<double>& x) const;

    // Print for debugging
    void print_info() const;

private:
    int32_t size_;
    std::vector<double> values_;
    std::vector<int32_t> columns_;
    std::vector<int32_t> row_ptrs_;
    bool finalized_;
};

// Linear solver result
struct SolverResult {
    bool converged;
    int32_t num_iterations;
    double final_residual;
    std::vector<double> solution;
};

// Direct solver using Gauss elimination
class DirectSolver {
public:
    // Solve Ax = b using Gaussian elimination
    static SolverResult solve(
        const std::vector<std::vector<double>>& A,
        const std::vector<double>& b,
        double tolerance = 1e-10);

    // LU decomposition
    static bool lu_decompose(
        std::vector<std::vector<double>>& A,
        std::vector<int32_t>& pivot);

    // Forward substitution
    static std::vector<double> forward_substitution(
        const std::vector<std::vector<double>>& L,
        const std::vector<double>& b);

    // Back substitution
    static std::vector<double> back_substitution(
        const std::vector<std::vector<double>>& U,
        const std::vector<double>& b);
};

// Iterative solver using Conjugate Gradient method
class ConjugateGradientSolver {
public:
    // Solve Ax = b using CG method
    static SolverResult solve(
        const SparseMatrix& A,
        const std::vector<double>& b,
        const std::vector<double>& x0,
        double tolerance = 1e-6,
        int32_t max_iterations = 10000);

private:
    // Compute dot product
    static double dot_product(const std::vector<double>& a, const std::vector<double>& b);

    // Compute L2 norm
    static double l2_norm(const std::vector<double>& v);
};

// Main FEA solver for static analysis
class FEASolver {
public:
    FEASolver();
    ~FEASolver();

    // Setup
    void set_mesh(const FiniteElementMesh& mesh);
    void set_material_database(const MaterialDatabase& db);
    void set_boundary_conditions(const BoundaryConditionManager& bc_manager);

    // Assign materials to elements
    void assign_material_to_element(int32_t element_id, int32_t material_id);
    void assign_material_to_elements(const std::vector<int32_t>& element_ids, int32_t material_id);

    // Solve
    bool solve(double tolerance = 1e-6, int32_t max_iterations = 10000);

    // Get results
    const std::vector<Vec3>& get_displacements() const { return nodal_displacements_; }
    Vec3 get_displacement(int32_t node_id) const;

    const std::vector<double>& get_nodal_stresses() const { return nodal_von_mises_stresses_; }
    double get_nodal_stress(int32_t node_id) const;

    // Analysis statistics
    int32_t get_num_dofs() const { return num_dofs_; }
    int32_t get_num_elements() const { return num_elements_; }
    int32_t get_num_nodes() const { return num_nodes_; }
    double get_max_displacement() const;
    double get_max_stress() const;

    // Check solution validity
    bool is_solution_valid() const { return solution_valid_; }

private:
    const FiniteElementMesh* mesh_;
    const MaterialDatabase* material_db_;
    const BoundaryConditionManager* bc_manager_;

    int32_t num_nodes_;
    int32_t num_elements_;
    int32_t num_dofs_;  // Total degrees of freedom (3 per node)

    std::vector<int32_t> element_materials_;  // Material ID for each element
    std::vector<Vec3> nodal_displacements_;
    std::vector<double> nodal_von_mises_stresses_;
    std::vector<std::array<std::array<double, 3>, 3>> nodal_stress_tensors_;

    bool solution_valid_;

    // Assembly helpers
    void assemble_global_stiffness_matrix(
        std::vector<std::vector<double>>& K_global);

    void assemble_global_force_vector(
        std::vector<double>& F_global);

    void apply_boundary_conditions(
        std::vector<std::vector<double>>& K,
        std::vector<double>& F,
        std::vector<int32_t>& constrained_dofs);

    // Post-processing
    void compute_nodal_stresses();
    void compute_nodal_stress_at_node(int32_t node_id);
};

}  // namespace bendcore::fea
