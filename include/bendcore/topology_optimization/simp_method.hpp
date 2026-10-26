#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace bendcore {
namespace topology_optimization {

/**
 * SIMP Method (Solid Isotropic Material with Penalization)
 *
 * Implements density-based topology optimization using the SIMP method.
 * - Density variables represent material distribution (0-1)
 * - Material interpolation via power law
 * - Penalty formulation for binary-like solutions
 * - Compliance minimization
 */
class SIMPMethod {
public:
    /**
     * SIMP material interpolation: E(x) = E_min + x^p * (E_0 - E_min)
     * @param density Density variable [0, 1]
     * @param E_0 Young's modulus for solid material
     * @param E_min Minimum Young's modulus (typically E_0 * 1e-3)
     * @param penalty_exponent Penalty exponent (typically 3)
     * @return Interpolated Young's modulus
     */
    static double material_interpolation(double density, double E_0, double E_min,
                                        double penalty_exponent = 3.0);

    /**
     * Compute effective stiffness from density
     * @param density Density variable
     * @param K_0 Reference stiffness matrix
     * @param E_0 Reference Young's modulus
     * @param E_min Minimum Young's modulus
     * @param penalty_exponent Penalty exponent
     * @return Scaled stiffness value
     */
    static double compute_effective_stiffness(double density, double K_0, double E_0,
                                             double E_min, double penalty_exponent = 3.0);

    /**
     * Compute material derivative for sensitivity analysis
     * dE/dx = p * x^(p-1) * (E_0 - E_min)
     * @param density Density variable
     * @param E_0 Reference Young's modulus
     * @param E_min Minimum Young's modulus
     * @param penalty_exponent Penalty exponent
     * @return Material derivative
     */
    static double material_derivative(double density, double E_0, double E_min,
                                     double penalty_exponent = 3.0);

public:
    struct Parameters {
        double E_0 = 1.0;                      // Reference Young's modulus
        double E_min = 1e-3;                   // Minimum Young's modulus
        double penalty_exponent = 3.0;         // SIMP penalty exponent
        double volume_fraction = 0.5;          // Target volume fraction
        double move_limit = 0.1;               // Design variable move limit per iteration
        double convergence_tolerance = 1e-3;   // Convergence tolerance
        int max_iterations = 100;              // Maximum iterations
    };

    explicit SIMPMethod(const Parameters& params = Parameters());

    /**
     * Initialize density field
     * @param n_elements Number of finite elements
     * @param initial_value Initial density (typically 0.5)
     */
    void initialize_densities(size_t n_elements, double initial_value = 0.5);

    /**
     * Update densities based on sensitivities and optimization criterion
     * @param sensitivities Sensitivity values (dc/dx)
     * @param lower_bounds Lower bounds for densities
     * @param upper_bounds Upper bounds for densities
     * @return True if converged, false otherwise
     */
    bool update_densities(const std::vector<double>& sensitivities,
                         const std::vector<double>& lower_bounds,
                         const std::vector<double>& upper_bounds);

    /**
     * Apply volume constraint during update
     * @param sensitivities Sensitivity values
     * @param target_volume Target total volume
     * @return Updated densities respecting volume constraint
     */
    std::vector<double> apply_volume_constraint(
        const std::vector<double>& sensitivities,
        double target_volume);

    // Getters
    const std::vector<double>& get_densities() const { return densities_; }
    std::vector<double>& get_densities_mutable() { return densities_; }
    double get_current_volume() const;
    double get_volume_fraction() const { return parameters_.volume_fraction; }
    int get_iteration_count() const { return iteration_count_; }

    // Setters
    void set_densities(const std::vector<double>& densities);
    void set_parameters(const Parameters& params) { parameters_ = params; }

private:
    Parameters parameters_;
    std::vector<double> densities_;
    std::vector<double> densities_prev_;
    int iteration_count_ = 0;

    /**
     * Bisection method to enforce volume constraint
     */
    double find_lagrange_multiplier(const std::vector<double>& sensitivities,
                                   double target_volume, int max_iter = 50);
};

} // namespace topology_optimization
} // namespace bendcore
