#pragma once

#include <vector>
#include <memory>
#include <array>
#include <cmath>

namespace bendcore {
namespace topology_optimization {

// Forward declarations
class SIMPMethod;
class SensitivityAnalysis;
class DensityFilter;
class OptimizationAlgorithm;
class ConstraintHandler;
class PostProcessor;

/**
 * Comprehensive Topology Optimization Framework
 *
 * Main orchestrator integrating all components:
 * - SIMP material interpolation
 * - Sensitivity analysis
 * - Density filtering
 * - Optimization algorithms
 * - Constraint handling
 * - Post-processing
 */
class TopologyOptimizer {
public:
    /**
     * Problem definition for topology optimization
     */
    struct ProblemDefinition {
        // Domain definition
        size_t n_elements = 0;
        std::vector<double> element_volumes;
        std::vector<std::array<double, 3>> element_positions;
        std::vector<std::array<double, 3>> element_dimensions;

        // Material properties
        double E_0 = 1.0;                     // Reference Young's modulus
        double poisson_ratio = 0.3;           // Poisson's ratio
        double density = 1.0;                 // Material density

        // Loads and boundary conditions
        std::vector<std::array<double, 3>> loads;          // Applied forces
        std::vector<std::pair<size_t, int>> fixed_dofs;    // Fixed DOFs (element, local_dof)
        std::vector<double> displacements;                  // Computed displacements
        std::vector<double> stresses;                       // Computed stresses

        // Problem type
        bool is_minimum_compliance = true;   // True for compliance minimization
        double target_volume_fraction = 0.5; // Target volume fraction
    };

    /**
     * Optimization parameters
     */
    struct OptimizationParameters {
        // Stopping criteria
        int max_iterations = 100;
        double convergence_tolerance = 1e-3;
        int stall_iterations = 5;

        // SIMP parameters
        double simp_penalty = 3.0;
        double simp_move_limit = 0.1;

        // Filter parameters
        double filter_radius = 1.5;
        bool use_density_filtering = true;

        // Constraint parameters
        double volume_constraint_tolerance = 1e-4;
        bool apply_symmetry = false;
        bool apply_manufacturing_constraints = false;
        double min_feature_size = 1.0;
    };

    /**
     * Optimization result
     */
    struct OptimizationResult {
        bool converged = false;
        int iterations = 0;
        double final_compliance = 0.0;
        double final_volume = 0.0;
        std::vector<double> final_densities;
        std::vector<double> objective_history;
        std::vector<double> constraint_history;

        // Post-processing results
        bool design_manufacturable = false;
        double manufacturability_score = 0.0;
    };

public:
    /**
     * Construct optimizer
     * @param problem Problem definition
     * @param params Optimization parameters
     */
    explicit TopologyOptimizer(const ProblemDefinition& problem,
                              const OptimizationParameters& params = OptimizationParameters());

    /**
     * Run topology optimization
     * @return Optimization result
     */
    OptimizationResult optimize();

    /**
     * Run single optimization iteration
     * @param iteration Iteration number
     * @return True if converged
     */
    bool optimize_iteration(int iteration);

    /**
     * Compute finite element analysis (FEA)
     * Should be implemented by user or integrated with FEA solver
     *
     * @param densities Current density field
     * @param loads Applied loads
     * @return Displacements and stresses
     */
    std::pair<std::vector<double>, std::vector<double>>
    compute_fea(const std::vector<double>& densities,
                const std::vector<std::array<double, 3>>& loads);

private:
    ProblemDefinition problem_;
    OptimizationParameters parameters_;

    // Optimization components
    std::unique_ptr<SIMPMethod> simp_method_;
    std::unique_ptr<SensitivityAnalysis> sensitivity_analyzer_;
    std::unique_ptr<DensityFilter> density_filter_;
    std::unique_ptr<OptimizationAlgorithm> optimizer_;
    std::unique_ptr<ConstraintHandler> constraint_handler_;
    std::unique_ptr<PostProcessor> post_processor_;

    // State variables
    std::vector<double> current_densities_;
    std::vector<double> sensitivities_;
    std::vector<double> objective_history_;
    std::vector<double> constraint_history_;
    int iteration_count_ = 0;

    /**
     * Initialize optimization
     */
    void initialize();

    /**
     * Compute compliance for current design
     * @param densities Current densities
     * @param displacements Nodal displacements
     * @return Total compliance
     */
    double compute_compliance(const std::vector<double>& densities,
                             const std::vector<double>& displacements);

    /**
     * Compute sensitivities
     * @param densities Current densities
     * @param displacements Nodal displacements
     * @return Sensitivity values
     */
    std::vector<double> compute_sensitivities(const std::vector<double>& densities,
                                             const std::vector<double>& displacements);
};

} // namespace topology_optimization
} // namespace bendcore
