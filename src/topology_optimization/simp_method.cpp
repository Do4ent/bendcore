#include "bendcore/topology_optimization/simp_method.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace bendcore {
namespace topology_optimization {

double SIMPMethod::material_interpolation(double density, double E_0, double E_min,
                                          double penalty_exponent) {
    if (density < 0.0 || density > 1.0) {
        throw std::invalid_argument("Density must be in [0, 1]");
    }
    return E_min + std::pow(density, penalty_exponent) * (E_0 - E_min);
}

double SIMPMethod::compute_effective_stiffness(double density, double K_0, double E_0,
                                               double E_min, double penalty_exponent) {
    double E = material_interpolation(density, E_0, E_min, penalty_exponent);
    return K_0 * (E / E_0);
}

double SIMPMethod::material_derivative(double density, double E_0, double E_min,
                                      double penalty_exponent) {
    if (density < 1e-12) {
        return penalty_exponent * std::pow(1e-12, penalty_exponent - 1.0) * (E_0 - E_min);
    }
    return penalty_exponent * std::pow(density, penalty_exponent - 1.0) * (E_0 - E_min);
}

SIMPMethod::SIMPMethod(const Parameters& params) : parameters_(params) {}

void SIMPMethod::initialize_densities(size_t n_elements, double initial_value) {
    if (initial_value < 0.0 || initial_value > 1.0) {
        throw std::invalid_argument("Initial density must be in [0, 1]");
    }
    densities_.assign(n_elements, initial_value);
    densities_prev_.assign(n_elements, initial_value);
}

bool SIMPMethod::update_densities(const std::vector<double>& sensitivities,
                                  const std::vector<double>& lower_bounds,
                                  const std::vector<double>& upper_bounds) {
    if (sensitivities.size() != densities_.size()) {
        throw std::invalid_argument("Sensitivities size mismatch");
    }

    densities_prev_ = densities_;

    // OC-like update with move limit
    for (size_t i = 0; i < densities_.size(); ++i) {
        double lower = std::max(lower_bounds[i],
                               densities_[i] - parameters_.move_limit);
        double upper = std::min(upper_bounds[i],
                               densities_[i] + parameters_.move_limit);

        // Move densities opposite to gradient direction
        // (since sensitivities are negative for minimization)
        double direction = (sensitivities[i] < 0.0) ? 1.0 : -1.0;
        densities_[i] = std::clamp(densities_[i] + direction * parameters_.move_limit * 0.1,
                                  lower, upper);
    }

    iteration_count_++;

    // Check convergence
    double max_change = 0.0;
    for (size_t i = 0; i < densities_.size(); ++i) {
        max_change = std::max(max_change, std::abs(densities_[i] - densities_prev_[i]));
    }

    return max_change < parameters_.convergence_tolerance;
}

std::vector<double> SIMPMethod::apply_volume_constraint(
    const std::vector<double>& sensitivities,
    double target_volume) {

    // Compute current total volume
    double current_volume = get_current_volume();

    // Find Lagrange multiplier using bisection
    double lambda = find_lagrange_multiplier(sensitivities, target_volume);

    // Update densities with Lagrange multiplier
    std::vector<double> new_densities(densities_.size());
    for (size_t i = 0; i < densities_.size(); ++i) {
        // Update rule: x_new = max(0, x_old + move * sign(-dc/dx - lambda))
        double criterion = sensitivities[i] + lambda;
        new_densities[i] = std::max(0.0, std::min(1.0,
            densities_[i] - parameters_.move_limit * (criterion > 0 ? 1.0 : -1.0)));
    }

    return new_densities;
}

double SIMPMethod::get_current_volume() const {
    double volume = 0.0;
    for (double d : densities_) {
        volume += d;
    }
    return volume / static_cast<double>(densities_.size());
}

void SIMPMethod::set_densities(const std::vector<double>& densities) {
    if (densities.size() != densities_.size()) {
        throw std::invalid_argument("Densities size mismatch");
    }
    densities_ = densities;
}

double SIMPMethod::find_lagrange_multiplier(const std::vector<double>& sensitivities,
                                           double target_volume, int max_iter) {
    double lower = -1e6;
    double upper = 1e6;
    double target_volume_fraction = target_volume / static_cast<double>(densities_.size());

    for (int iter = 0; iter < max_iter; ++iter) {
        double lambda = 0.5 * (lower + upper);
        double volume = 0.0;

        for (size_t i = 0; i < densities_.size(); ++i) {
            double x = densities_[i] - parameters_.move_limit *
                       std::copysign(1.0, sensitivities[i] + lambda);
            x = std::max(0.0, std::min(1.0, x));
            volume += x;
        }

        volume /= static_cast<double>(densities_.size());

        if (volume > target_volume_fraction) {
            lower = lambda;
        } else {
            upper = lambda;
        }

        if (std::abs(volume - target_volume_fraction) < 1e-6) {
            break;
        }
    }

    return 0.5 * (lower + upper);
}

} // namespace topology_optimization
} // namespace bendcore
