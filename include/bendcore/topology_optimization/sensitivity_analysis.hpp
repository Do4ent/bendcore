#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace bendcore {
namespace topology_optimization {

/**
 * Sensitivity Analysis for Topology Optimization
 *
 * Implements adjoint method for efficient sensitivity computation
 * and gradient-based design variable sensitivity analysis.
 */
class SensitivityAnalysis {
public:
    /**
     * Compute compliance sensitivity using adjoint method
     * dc/dx = -p * x^(p-1) * (E_0 - E_min) * K_0 / E_0
     *
     * @param densities Current density values
     * @param displacements Finite element displacements
     * @param stiffness_matrix Global stiffness matrix (flattened)
     * @param n_elements Number of elements
     * @param penalty_exponent SIMP penalty exponent
     * @param E_0 Reference Young's modulus
     * @param E_min Minimum Young's modulus
     * @return Sensitivity values (dc/dx)
     */
    static std::vector<double> compute_compliance_sensitivity(
        const std::vector<double>& densities,
        const std::vector<double>& displacements,
        const std::vector<double>& stiffness_matrix,
        size_t n_elements,
        double penalty_exponent = 3.0,
        double E_0 = 1.0,
        double E_min = 1e-3);

    /**
     * Compute stress sensitivity
     * @param densities Current density values
     * @param stresses Von Mises stresses
     * @param stress_limit Maximum allowable stress
     * @param penalty_exponent SIMP penalty exponent
     * @return Sensitivity values for stress constraint
     */
    static std::vector<double> compute_stress_sensitivity(
        const std::vector<double>& densities,
        const std::vector<double>& stresses,
        double stress_limit,
        double penalty_exponent = 3.0);

    /**
     * Compute perimeter sensitivity for edge length control
     * @param densities Current density values
     * @param neighbor_info Neighbor connectivity information
     * @return Perimeter sensitivity values
     */
    static std::vector<double> compute_perimeter_sensitivity(
        const std::vector<double>& densities,
        const std::vector<std::vector<int>>& neighbor_info);

public:
    /**
     * Convergence criteria for iterative optimization
     */
    struct ConvergenceCriteria {
        double density_change_tolerance = 1e-3;
        double objective_change_tolerance = 1e-4;
        double constraint_tolerance = 1e-3;
        int max_stall_iterations = 5;
    };

    explicit SensitivityAnalysis(const ConvergenceCriteria& criteria = ConvergenceCriteria());

    /**
     * Check convergence based on density changes
     * @param current_densities Current density values
     * @param previous_densities Previous density values
     * @return True if converged
     */
    bool check_convergence_density(const std::vector<double>& current_densities,
                                   const std::vector<double>& previous_densities);

    /**
     * Check convergence based on objective function
     * @param current_objective Current objective value
     * @return True if converged
     */
    bool check_convergence_objective(double current_objective);

    /**
     * Check constraint satisfaction
     * @param constraint_values Current constraint values
     * @param tolerance Tolerance for constraint satisfaction
     * @return True if all constraints satisfied
     */
    bool check_constraints_satisfied(const std::vector<double>& constraint_values,
                                    double tolerance);

    /**
     * Compute gradient norm for convergence monitoring
     * @param sensitivities Gradient values
     * @return L2 norm of gradient
     */
    static double compute_gradient_norm(const std::vector<double>& sensitivities);

    /**
     * Compute maximum density change
     * @param current Current values
     * @param previous Previous values
     * @return Maximum absolute change
     */
    static double compute_max_change(const std::vector<double>& current,
                                     const std::vector<double>& previous);

private:
    ConvergenceCriteria criteria_;
    std::vector<double> objective_history_;
    int stall_count_ = 0;
};

} // namespace topology_optimization
} // namespace bendcore
