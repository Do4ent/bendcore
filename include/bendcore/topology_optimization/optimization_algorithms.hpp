#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>

namespace bendcore {
namespace topology_optimization {

/**
 * Optimization Algorithms for Topology Optimization
 *
 * Implements:
 * - Optimality Criteria (OC) method
 * - Method of Moving Asymptotes (MMA)
 * - Line search strategies
 */
class OptimizationAlgorithm {
public:
    /**
     * Algorithm type enumeration
     */
    enum class AlgorithmType {
        OPTIMALITY_CRITERIA = 0,  // OC method (fast, suitable for volume constraint)
        METHOD_OF_MOVING_ASYMPTOTES = 1  // MMA (more robust, handles multiple constraints)
    };

    /**
     * Line search type enumeration
     */
    enum class LineSearchType {
        BACKTRACKING = 0,      // Backtracking line search
        GOLDEN_SECTION = 1,    // Golden section search
        NONE = 2               // No line search (constant step)
    };

public:
    /**
     * Optimality Criteria (OC) method
     * Fast first-order method optimized for volume constraint
     *
     * @param densities Current densities
     * @param sensitivities dc/dx sensitivity values
     * @param lower_bounds Lower density bounds
     * @param upper_bounds Upper density bounds
     * @param move_limit Maximum move per iteration
     * @return Updated densities
     */
    static std::vector<double> optimality_criteria_update(
        const std::vector<double>& densities,
        const std::vector<double>& sensitivities,
        const std::vector<double>& lower_bounds,
        const std::vector<double>& upper_bounds,
        double move_limit = 0.1);

    /**
     * OC with Lagrange multiplier for volume constraint
     * @param densities Current densities
     * @param sensitivities Sensitivities
     * @param lower_bounds Lower bounds
     * @param upper_bounds Upper bounds
     * @param move_limit Move limit
     * @param target_volume Target total volume
     * @param tolerance Constraint tolerance
     * @return Updated densities
     */
    static std::vector<double> optimality_criteria_with_volume_constraint(
        const std::vector<double>& densities,
        const std::vector<double>& sensitivities,
        const std::vector<double>& lower_bounds,
        const std::vector<double>& upper_bounds,
        double move_limit,
        double target_volume,
        double tolerance = 1e-4);

    /**
     * Method of Moving Asymptotes (MMA)
     * Robust for multiple constraints and nonlinear objectives
     *
     * @param densities Current densities
     * @param sensitivities Gradient vector
     * @param lower_bounds Lower density bounds
     * @param upper_bounds Upper density bounds
     * @param step_size Trust region size
     * @return Updated densities
     */
    static std::vector<double> moving_asymptotes_update(
        const std::vector<double>& densities,
        const std::vector<double>& sensitivities,
        const std::vector<double>& lower_bounds,
        const std::vector<double>& upper_bounds,
        double step_size = 0.1);

    /**
     * Backtracking line search
     * Finds suitable step size along search direction
     *
     * @param current_value Current objective value
     * @param gradient Gradient at current point
     * @param direction Search direction
     * @param objective_function Function to evaluate
     * @param max_iterations Maximum backtracking iterations
     * @param reduction_factor Step size reduction factor
     * @return Optimal step size
     */
    static double backtracking_line_search(
        double current_value,
        const std::vector<double>& gradient,
        const std::vector<double>& direction,
        std::function<double(const std::vector<double>&)> objective_function,
        int max_iterations = 10,
        double reduction_factor = 0.5);

    /**
     * Golden section line search
     * More expensive but more accurate
     *
     * @param lower_bound Lower step size bound
     * @param upper_bound Upper step size bound
     * @param objective_function Function to evaluate
     * @param tolerance Convergence tolerance
     * @return Optimal step size
     */
    static double golden_section_search(
        double lower_bound,
        double upper_bound,
        std::function<double(double)> objective_function,
        double tolerance = 1e-6);

public:
    struct Parameters {
        AlgorithmType algorithm = AlgorithmType::OPTIMALITY_CRITERIA;
        LineSearchType line_search = LineSearchType::NONE;
        double move_limit = 0.1;
        double step_size = 1.0;
        int max_iterations = 100;
        double convergence_tolerance = 1e-3;
    };

    explicit OptimizationAlgorithm(const Parameters& params = Parameters());

    /**
     * Perform optimization step
     * @param densities Current densities
     * @param sensitivities Gradient values
     * @param lower_bounds Lower bounds
     * @param upper_bounds Upper bounds
     * @return Updated densities
     */
    std::vector<double> step(const std::vector<double>& densities,
                            const std::vector<double>& sensitivities,
                            const std::vector<double>& lower_bounds,
                            const std::vector<double>& upper_bounds);

    /**
     * Perform step with volume constraint
     * @param densities Current densities
     * @param sensitivities Gradient values
     * @param lower_bounds Lower bounds
     * @param upper_bounds Upper bounds
     * @param target_volume Target total volume
     * @return Updated densities
     */
    std::vector<double> step_with_volume_constraint(
        const std::vector<double>& densities,
        const std::vector<double>& sensitivities,
        const std::vector<double>& lower_bounds,
        const std::vector<double>& upper_bounds,
        double target_volume);

private:
    Parameters parameters_;
};

} // namespace topology_optimization
} // namespace bendcore
