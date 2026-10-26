// Real-time Simulation Solver Framework
// M7 Phase: Real-time Solver (GAP-030)
#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <memory>
#include <chrono>
#include <optional>

namespace bendcore::solver {

// Forward declarations
struct SolverMetrics;
struct SolverConfig;
class IterativeSolver;
class Preconditioner;
class ReducedOrderModel;

// Real-time solution result
struct RealtimeSolution {
    bool converged;
    std::vector<double> displacement;
    std::vector<double> stress;
    double max_displacement;
    double max_stress;
    int32_t iterations;
    double residual;
    std::chrono::milliseconds solve_time;
    double error_estimate;
};

// Solver configuration for real-time execution
struct RealtimeSolverConfig {
    // Time budget (milliseconds)
    double time_budget_ms = 16.0;  // ~60 FPS

    // Convergence criteria
    double residual_tolerance = 1e-4;
    int32_t max_iterations = 100;

    // Incremental solving
    bool use_incremental = true;
    double load_step = 0.1;

    // Quality control
    bool use_error_bounds = true;
    double error_tolerance = 1e-2;

    // Adaptive refinement
    bool use_adaptive_refinement = true;
    int32_t min_quality_level = 1;
    int32_t max_quality_level = 5;

    // ROM (Reduced Order Modeling)
    bool use_rom = false;
    int32_t rom_basis_size = 10;

    // Scheduling
    int32_t priority = 0;
    bool use_scheduler = true;
};

// Sparse matrix format (CSR - Compressed Sparse Row)
class SparseMatrix {
public:
    explicit SparseMatrix(int32_t size);

    void add_value(int32_t i, int32_t j, double value);
    double get_value(int32_t i, int32_t j) const;

    int32_t get_size() const { return size_; }
    const std::vector<double>& get_values() const { return values_; }
    const std::vector<int32_t>& get_columns() const { return columns_; }
    const std::vector<int32_t>& get_row_ptrs() const { return row_ptrs_; }

    void finalize();
    bool is_finalized() const { return finalized_; }

    // Matrix-vector multiplication (y = A*x)
    std::vector<double> multiply(const std::vector<double>& x) const;

    // Matrix-matrix multiplication
    SparseMatrix multiply(const SparseMatrix& B) const;

    // Get row (for preconditioner setup)
    void get_row(int32_t row, std::vector<int32_t>& cols, std::vector<double>& vals) const;

    // Statistics
    int32_t num_nonzeros() const { return static_cast<int32_t>(values_.size()); }

    void print_info() const;

private:
    int32_t size_;
    std::vector<double> values_;
    std::vector<int32_t> columns_;
    std::vector<int32_t> row_ptrs_;
    bool finalized_;

    // Temporary storage during assembly
    std::vector<std::vector<std::pair<int32_t, double>>> temp_rows_;
};

// Main real-time solver interface
class RealTimeSolver {
public:
    explicit RealTimeSolver(int32_t system_size);
    ~RealTimeSolver();

    // Setup
    void set_stiffness_matrix(const SparseMatrix& K);
    void set_force_vector(const std::vector<double>& F);
    void set_boundary_conditions(const std::vector<int32_t>& fixed_dofs);

    // Configuration
    void set_config(const RealtimeSolverConfig& config) { config_ = config; }
    const RealtimeSolverConfig& get_config() const { return config_; }

    // Incremental solving (for real-time interaction)
    RealtimeSolution solve_increment(
        const std::vector<double>& delta_load,
        const std::vector<double>* initial_guess = nullptr);

    // Full solve
    RealtimeSolution solve(
        const std::vector<double>* initial_guess = nullptr);

    // Adaptive quality control
    RealtimeSolution solve_adaptive(
        const std::vector<double>& target_load,
        int32_t quality_level = 3);

    // Parameter sensitivity analysis
    std::vector<double> compute_sensitivity(
        int32_t parameter_id,
        double delta = 1e-6);

    // Get current solution
    const std::vector<double>& get_displacement() const { return current_displacement_; }
    const std::vector<double>& get_stress() const { return current_stress_; }

    // Metrics and diagnostics
    const SolverMetrics& get_metrics() const { return *metrics_; }
    void print_metrics() const;

    // Memory management
    void clear_cache();
    int64_t get_memory_usage() const;

private:
    int32_t system_size_;
    RealtimeSolverConfig config_;

    SparseMatrix stiffness_matrix_{0};
    std::vector<double> force_vector_;
    std::vector<int32_t> fixed_dofs_;

    std::vector<double> current_displacement_;
    std::vector<double> current_stress_;
    std::vector<double> previous_displacement_;

    std::unique_ptr<IterativeSolver> iterative_solver_;
    std::unique_ptr<Preconditioner> preconditioner_;
    std::unique_ptr<ReducedOrderModel> rom_;
    std::unique_ptr<SolverMetrics> metrics_;

    // Helper methods
    void apply_boundary_conditions(
        SparseMatrix& K,
        std::vector<double>& F,
        const std::vector<int32_t>& fixed_dofs);

    RealtimeSolution solve_with_solver(
        const std::vector<double>* initial_guess = nullptr);

    void compute_stress_field(const std::vector<double>& displacement);

    void update_metrics(
        bool converged,
        int32_t iterations,
        double residual,
        std::chrono::milliseconds solve_time,
        double error_estimate);
};

}  // namespace bendcore::solver
