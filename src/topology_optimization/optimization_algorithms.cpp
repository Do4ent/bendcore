#include "bendcore/topology_optimization/optimization_algorithms.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace bendcore {
namespace topology_optimization {

std::vector<double> OptimizationAlgorithm::optimality_criteria_update(
    const std::vector<double>& densities,
    const std::vector<double>& sensitivities,
    const std::vector<double>& lower_bounds,
    const std::vector<double>& upper_bounds,
    double move_limit) {

    std::vector<double> new_densities(densities.size());

    for (size_t i = 0; i < densities.size(); ++i) {
        // OC update rule with move limit
        double x = densities[i];
        double lower = std::max(lower_bounds[i], x - move_limit);
        double upper = std::min(upper_bounds[i], x + move_limit);

        // Move based on sensitivity direction
        // Negative sensitivity means increase density, positive means decrease
        if (sensitivities[i] < 0.0) {
            // Decrease sensitivity - increase density
            new_densities[i] = std::min(upper, x * (1.0 + move_limit * 0.1));
        } else {
            // Increase sensitivity - decrease density
            new_densities[i] = std::max(lower, x * (1.0 - move_limit * 0.1));
        }
    }

    return new_densities;
}

std::vector<double> OptimizationAlgorithm::optimality_criteria_with_volume_constraint(
    const std::vector<double>& densities,
    const std::vector<double>& sensitivities,
    const std::vector<double>& lower_bounds,
    const std::vector<double>& upper_bounds,
    double move_limit,
    double target_volume,
    double tolerance) {

    // Bisection on Lagrange multiplier
    double lambda_low = -1e6;
    double lambda_high = 1e6;
    double target_fraction = target_volume / static_cast<double>(densities.size());

    for (int iter = 0; iter < 50; ++iter) {
        double lambda = 0.5 * (lambda_low + lambda_high);

        // Evaluate volume with current lambda
        double volume = 0.0;
        for (size_t i = 0; i < densities.size(); ++i) {
            double criterion = sensitivities[i] + lambda;
            double x_new;

            if (criterion < 0.0) {
                x_new = std::min(upper_bounds[i],
                                densities[i] * (1.0 + move_limit * 0.1));
            } else {
                x_new = std::max(lower_bounds[i],
                                densities[i] * (1.0 - move_limit * 0.1));
            }

            volume += x_new;
        }

        volume /= static_cast<double>(densities.size());

        if (volume > target_fraction + tolerance) {
            lambda_low = lambda;
        } else {
            lambda_high = lambda;
        }

        if (std::abs(volume - target_fraction) < tolerance * 0.1) {
            break;
        }
    }

    double lambda = 0.5 * (lambda_low + lambda_high);
    std::vector<double> new_densities(densities.size());

    for (size_t i = 0; i < densities.size(); ++i) {
        double criterion = sensitivities[i] + lambda;

        if (criterion < 0.0) {
            new_densities[i] = std::min(upper_bounds[i],
                                       densities[i] * (1.0 + move_limit * 0.1));
        } else {
            new_densities[i] = std::max(lower_bounds[i],
                                       densities[i] * (1.0 - move_limit * 0.1));
        }
    }

    return new_densities;
}

std::vector<double> OptimizationAlgorithm::moving_asymptotes_update(
    const std::vector<double>& densities,
    const std::vector<double>& sensitivities,
    const std::vector<double>& lower_bounds,
    const std::vector<double>& upper_bounds,
    double step_size) {

    std::vector<double> new_densities(densities.size());

    // MMA-like update with asymptotic constraint
    for (size_t i = 0; i < densities.size(); ++i) {
        double x = densities[i];
        double gradient = sensitivities[i];

        // Simple projected gradient step
        double step = -step_size * gradient / (std::abs(gradient) + 1e-12);
        double x_new = x + step;

        // Project onto bounds
        new_densities[i] = std::max(lower_bounds[i],
                                   std::min(upper_bounds[i], x_new));
    }

    return new_densities;
}

double OptimizationAlgorithm::backtracking_line_search(
    double current_value,
    const std::vector<double>& gradient,
    const std::vector<double>& direction,
    std::function<double(const std::vector<double>&)> objective_function,
    int max_iterations,
    double reduction_factor) {

    double alpha = 1.0;
    double grad_dir_product = 0.0;

    for (size_t i = 0; i < gradient.size(); ++i) {
        grad_dir_product += gradient[i] * direction[i];
    }

    // Backtracking: reduce alpha until sufficient decrease
    for (int iter = 0; iter < max_iterations; ++iter) {
        std::vector<double> candidate(direction.size());
        for (size_t i = 0; i < direction.size(); ++i) {
            candidate[i] = direction[i] + alpha * direction[i];
        }

        double new_value = objective_function(candidate);

        // Armijo condition
        if (new_value <= current_value + 0.0001 * alpha * grad_dir_product) {
            return alpha;
        }

        alpha *= reduction_factor;
    }

    return alpha;
}

double OptimizationAlgorithm::golden_section_search(
    double lower_bound,
    double upper_bound,
    std::function<double(double)> objective_function,
    double tolerance) {

    const double phi = (1.0 + std::sqrt(5.0)) / 2.0;  // Golden ratio
    const double resphi = 2.0 - phi;

    double x1 = lower_bound + resphi * (upper_bound - lower_bound);
    double x2 = upper_bound - resphi * (upper_bound - lower_bound);

    double f1 = objective_function(x1);
    double f2 = objective_function(x2);

    while (std::abs(upper_bound - lower_bound) > tolerance) {
        if (f1 < f2) {
            upper_bound = x2;
            x2 = x1;
            f2 = f1;
            x1 = lower_bound + resphi * (upper_bound - lower_bound);
            f1 = objective_function(x1);
        } else {
            lower_bound = x1;
            x1 = x2;
            f1 = f2;
            x2 = upper_bound - resphi * (upper_bound - lower_bound);
            f2 = objective_function(x2);
        }
    }

    return 0.5 * (lower_bound + upper_bound);
}

OptimizationAlgorithm::OptimizationAlgorithm(const Parameters& params)
    : parameters_(params) {}

std::vector<double> OptimizationAlgorithm::step(
    const std::vector<double>& densities,
    const std::vector<double>& sensitivities,
    const std::vector<double>& lower_bounds,
    const std::vector<double>& upper_bounds) {

    switch (parameters_.algorithm) {
        case AlgorithmType::OPTIMALITY_CRITERIA:
            return optimality_criteria_update(densities, sensitivities,
                                             lower_bounds, upper_bounds,
                                             parameters_.move_limit);
        case AlgorithmType::METHOD_OF_MOVING_ASYMPTOTES:
            return moving_asymptotes_update(densities, sensitivities,
                                           lower_bounds, upper_bounds,
                                           parameters_.step_size);
    }

    return densities;
}

std::vector<double> OptimizationAlgorithm::step_with_volume_constraint(
    const std::vector<double>& densities,
    const std::vector<double>& sensitivities,
    const std::vector<double>& lower_bounds,
    const std::vector<double>& upper_bounds,
    double target_volume) {

    switch (parameters_.algorithm) {
        case AlgorithmType::OPTIMALITY_CRITERIA:
            return optimality_criteria_with_volume_constraint(
                densities, sensitivities, lower_bounds, upper_bounds,
                parameters_.move_limit, target_volume);
        case AlgorithmType::METHOD_OF_MOVING_ASYMPTOTES:
            // MMA with constraints would require more complex implementation
            return moving_asymptotes_update(densities, sensitivities,
                                           lower_bounds, upper_bounds,
                                           parameters_.step_size);
    }

    return densities;
}

} // namespace topology_optimization
} // namespace bendcore
