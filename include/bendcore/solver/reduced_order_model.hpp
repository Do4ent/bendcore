// Reduced Order Modeling (ROM)
// M7 Phase: Real-time Solver (GAP-030)
#pragma once

#include "bendcore/solver/real_time_solver.hpp"
#include <vector>
#include <cstdint>
#include <memory>
#include <optional>

namespace bendcore::solver {

// POD (Proper Orthogonal Decomposition) basis
class PODBasis {
public:
    explicit PODBasis(int32_t basis_size);

    // Build POD basis from snapshot matrix (column vectors are snapshots)
    void build_from_snapshots(
        const std::vector<std::vector<double>>& snapshots,
        double energy_threshold = 0.95);

    // Get POD basis vectors
    const std::vector<std::vector<double>>& get_basis_vectors() const { return basis_; }

    // Project high-dimensional vector onto POD subspace
    std::vector<double> project(const std::vector<double>& u) const;

    // Reconstruct high-dimensional vector from reduced coefficients
    std::vector<double> reconstruct(const std::vector<double>& coeffs) const;

    // Get singular values (energy content)
    const std::vector<double>& get_singular_values() const { return singular_values_; }

    // Get cumulative energy percentage
    std::vector<double> get_cumulative_energy() const;

    // Number of basis vectors
    int32_t get_num_basis() const { return static_cast<int32_t>(basis_.size()); }

    // Dimension of high-dimensional space
    int32_t get_dimension() const { return dimension_; }

    // Relative information content
    double get_relative_information(int32_t num_modes) const;

    void print_info() const;

private:
    int32_t basis_size_;
    int32_t dimension_;
    std::vector<std::vector<double>> basis_;
    std::vector<double> singular_values_;

    // SVD computation (simplified)
    void compute_svd(const std::vector<std::vector<double>>& snapshots);
};

// ROM for FEA systems
// Offline phase: build reduced system
// Online phase: fast solving in reduced space
class ReducedOrderModel {
public:
    explicit ReducedOrderModel(int32_t basis_size);

    // OFFLINE PHASE: Build ROM from full system
    // This is called once with training data
    void offline_phase(
        const SparseMatrix& K_full,
        const std::vector<std::vector<double>>& displacement_snapshots,
        const std::vector<std::vector<double>>& load_snapshots);

    // Get POD basis
    const PODBasis& get_basis() const { return basis_; }
    PODBasis& get_basis_mutable() { return basis_; }

    // ONLINE PHASE: Fast solving with reduced system
    std::vector<double> solve_reduced(
        const std::vector<double>& load_reduced,
        double tolerance = 1e-6,
        int32_t max_iterations = 100);

    // Reconstruct full solution from reduced coefficients
    std::vector<double> reconstruct_full_solution(
        const std::vector<double>& coeffs_reduced) const;

    // Project full solution to reduced space
    std::vector<double> project_to_reduced(
        const std::vector<double>& u_full) const;

    // Check if ROM is ready
    bool is_ready() const { return rom_ready_; }

    // Error estimation
    double estimate_error(
        const std::vector<double>& solution_reduced,
        const std::vector<double>& solution_full) const;

    // Speedup information
    struct SpeedupInfo {
        int32_t full_dofs;
        int32_t reduced_dofs;
        double speedup_factor;
        double accuracy_loss;
    };

    SpeedupInfo get_speedup_info() const;

    // Get statistics
    int32_t get_reduced_size() const { return reduced_size_; }
    int32_t get_full_size() const { return full_size_; }

    void print_statistics() const;

private:
    PODBasis basis_;
    SparseMatrix K_reduced_{0};
    std::vector<double> F_reduced_;

    int32_t reduced_size_;
    int32_t full_size_;
    bool rom_ready_ = false;

    // Projection helper
    SparseMatrix project_matrix(
        const SparseMatrix& K_full) const;

    std::vector<double> project_vector(
        const std::vector<double>& F_full) const;
};

// Adaptive ROM with error control
class AdaptiveROM {
public:
    explicit AdaptiveROM(int32_t initial_basis_size);

    // Build ROM incrementally
    void add_snapshot(
        const std::vector<double>& displacement,
        const std::vector<double>& load);

    // Adaptive basis expansion
    void expand_basis(int32_t new_basis_size);

    // Solve with error estimation
    struct AdaptiveSolution {
        std::vector<double> solution;
        double error_estimate;
        bool error_within_tolerance;
        int32_t actual_basis_size;
    };

    AdaptiveSolution solve_adaptive(
        const std::vector<double>& load,
        double error_tolerance = 1e-3);

    // Automatic basis size selection
    int32_t select_basis_size(double error_tolerance) const;

    // Statistics
    int32_t get_num_snapshots() const { return num_snapshots_; }
    int32_t get_current_basis_size() const { return current_basis_size_; }

private:
    std::vector<std::vector<double>> snapshots_;
    std::vector<std::vector<double>> loads_;
    int32_t current_basis_size_;
    int32_t max_basis_size_;
    int32_t num_snapshots_ = 0;
    std::unique_ptr<ReducedOrderModel> rom_;
};

// ROM with parameter dependency
class ParametricROM {
public:
    explicit ParametricROM(int32_t basis_size, int32_t num_parameters);

    // Build parametric ROM
    void build_parametric_model(
        const std::vector<std::vector<double>>& parameters,
        const std::vector<SparseMatrix>& K_samples,
        const std::vector<std::vector<double>>& displacement_snapshots);

    // Solve for new parameters by interpolation
    std::vector<double> solve_for_parameters(
        const std::vector<double>& parameters);

    // Get ROMs at parameter values
    const std::vector<ReducedOrderModel>& get_rom_samples() const;

private:
    int32_t num_parameters_;
    std::vector<std::vector<double>> parameter_samples_;
    std::vector<ReducedOrderModel> rom_samples_;
    bool parametric_ready_ = false;
};

}  // namespace bendcore::solver
