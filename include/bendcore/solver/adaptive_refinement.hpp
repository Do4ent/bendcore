// Adaptive Refinement and Error Control
// M7 Phase: Real-time Solver (GAP-030)
#pragma once

#include "bendcore/solver/real_time_solver.hpp"
#include <vector>
#include <cstdint>
#include <memory>

namespace bendcore::solver {

// Error estimation result
struct ErrorEstimate {
    double global_error;
    std::vector<double> element_errors;
    double max_element_error;
    int32_t num_elements_exceeding_tol;
    bool meets_tolerance;
};

// Solution quality metrics
struct QualityMetrics {
    double residual_norm;
    double solution_norm;
    double relative_residual;
    double energy_norm;
    int32_t iterations_to_convergence;
    double convergence_rate;
};

// A posteriori error estimator
class ErrorEstimator {
public:
    // Set reference solution for error computation
    void set_reference_solution(const std::vector<double>& ref_solution) {
        reference_solution_ = ref_solution;
    }

    // Estimate error using residual-based method
    ErrorEstimate estimate_error_residual(
        const SparseMatrix& K,
        const std::vector<double>& u,
        const std::vector<double>& F);

    // Estimate error using recovery-based method (Zienkiewicz-Zhu)
    ErrorEstimate estimate_error_recovery(
        const std::vector<double>& nodal_stress,
        const std::vector<double>& recovered_stress);

    // Estimate error using reference solution (if available)
    ErrorEstimate estimate_error_exact(
        const std::vector<double>& computed_solution);

    // Global error measure
    double compute_energy_norm(
        const SparseMatrix& K,
        const std::vector<double>& u);

    // Local error indicators per element
    std::vector<double> compute_local_errors(
        const std::vector<double>& global_error_indicator,
        int32_t num_elements);

private:
    std::vector<double> reference_solution_;
};

// Convergence monitor
class ConvergenceMonitor {
public:
    // Add iteration data
    void add_iteration(double residual, double step_size);

    // Check convergence criteria
    bool is_converged(
        double tolerance,
        int32_t min_iterations = 1) const;

    // Get convergence rate
    double get_convergence_rate() const;

    // Detect stagnation
    bool is_stagnating(int32_t window_size = 5) const;

    // Get residual history
    const std::vector<double>& get_residual_history() const { return residual_history_; }

    // Reset monitor
    void reset();

    // Get statistics
    struct ConvergenceStats {
        int32_t num_iterations;
        double initial_residual;
        double final_residual;
        double convergence_rate;
        bool converged;
    };

    ConvergenceStats get_statistics() const;

private:
    std::vector<double> residual_history_;
    std::vector<double> step_sizes_;
};

// Mesh refinement indicator
class RefinementIndicator {
public:
    // Compute refinement indicator
    std::vector<bool> compute_refinement_flags(
        const std::vector<double>& element_errors,
        double error_tolerance,
        double refinement_fraction = 0.3);

    // Adaptive refinement strategy
    enum class RefinementStrategy {
        FixedFraction,   // Refine fraction with largest errors
        ErrorThreshold,  // Refine elements exceeding threshold
        HierarchicalBasis // Use hierarchical basis functions
    };

    void set_strategy(RefinementStrategy strategy) { strategy_ = strategy; }

    // Compute optimal mesh size
    std::vector<double> compute_optimal_mesh_size(
        const std::vector<double>& element_errors,
        double target_error,
        double convergence_rate);

private:
    RefinementStrategy strategy_ = RefinementStrategy::FixedFraction;
};

// Adaptive solver with error control
class AdaptiveSolver {
public:
    explicit AdaptiveSolver(RealTimeSolver& base_solver);

    // Solve with guaranteed error bound
    struct AdaptiveSolution {
        std::vector<double> displacement;
        std::vector<double> stress;
        ErrorEstimate error;
        int32_t refinement_steps;
        double total_time_ms;
    };

    AdaptiveSolution solve_with_error_control(
        double target_error = 1e-2,
        int32_t max_refinements = 5);

    // Get quality assessment
    QualityMetrics assess_quality(
        const std::vector<double>& solution,
        const SparseMatrix& K,
        const std::vector<double>& F);

    // Automatic quality level selection
    int32_t select_quality_level(double error_tolerance);

private:
    RealTimeSolver& base_solver_;
    std::unique_ptr<ErrorEstimator> error_estimator_;
    std::unique_ptr<RefinementIndicator> refinement_indicator_;
    std::unique_ptr<ConvergenceMonitor> convergence_monitor_;
};

// Solution verification
class SolutionVerifier {
public:
    // Check solution bounds
    struct BoundsCheck {
        bool displacements_within_bounds;
        bool stresses_within_bounds;
        bool energy_positive;
        std::vector<std::string> warnings;
    };

    BoundsCheck verify_bounds(
        const std::vector<double>& displacement,
        const std::vector<double>& stress,
        double max_displacement,
        double max_stress);

    // Check physical validity
    bool check_physical_validity(
        const std::vector<double>& displacement,
        const std::vector<double>& stress,
        const SparseMatrix& K);

    // Verify equilibrium
    double compute_equilibrium_residual(
        const SparseMatrix& K,
        const std::vector<double>& u,
        const std::vector<double>& F);

    // Check energy conservation
    bool check_energy_conservation(
        const std::vector<double>& displacement,
        const std::vector<double>& external_load,
        double tolerance = 1e-3);

private:
    double compute_strain_energy(
        const SparseMatrix& K,
        const std::vector<double>& u);
};

}  // namespace bendcore::solver
