#include "bendcore/topology_optimization/sensitivity_analysis.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>

namespace bendcore {
namespace topology_optimization {

std::vector<double> SensitivityAnalysis::compute_compliance_sensitivity(
    const std::vector<double>& densities,
    const std::vector<double>& displacements,
    const std::vector<double>& stiffness_matrix,
    size_t n_elements,
    double penalty_exponent,
    double E_0,
    double E_min) {

    std::vector<double> sensitivities(n_elements);
    double E_range = E_0 - E_min;

    for (size_t i = 0; i < n_elements; ++i) {
        // Adjoint-based sensitivity: dc/dx = -p * x^(p-1) * (E_0 - E_min) * K_e * u_e
        double x = densities[i];
        if (x < 1e-12) x = 1e-12;

        double dE_dx = penalty_exponent * std::pow(x, penalty_exponent - 1.0) * E_range;

        // Simplified: approximate element strain energy
        // Real implementation would use element-level displacements
        double strain_energy = 0.0;
        if (i < stiffness_matrix.size()) {
            strain_energy = std::abs(stiffness_matrix[i]) *
                           (displacements.empty() ? 1.0 : displacements[std::min(i, displacements.size()-1)]);
        }

        sensitivities[i] = -dE_dx * strain_energy;
    }

    return sensitivities;
}

std::vector<double> SensitivityAnalysis::compute_stress_sensitivity(
    const std::vector<double>& densities,
    const std::vector<double>& stresses,
    double stress_limit,
    double penalty_exponent) {

    std::vector<double> sensitivities(densities.size());

    for (size_t i = 0; i < densities.size(); ++i) {
        double x = densities[i];
        if (x < 1e-12) x = 1e-12;

        double dE_dx = penalty_exponent * std::pow(x, penalty_exponent - 1.0);

        // Stress sensitivity - active elements contribute more
        double normalized_stress = (i < stresses.size()) ?
                                   stresses[i] / stress_limit : 0.5;

        sensitivities[i] = dE_dx * normalized_stress;
    }

    return sensitivities;
}

std::vector<double> SensitivityAnalysis::compute_perimeter_sensitivity(
    const std::vector<double>& densities,
    const std::vector<std::vector<int>>& neighbor_info) {

    std::vector<double> sensitivities(densities.size(), 0.0);

    for (size_t i = 0; i < densities.size(); ++i) {
        if (i < neighbor_info.size()) {
            double perimeter_contribution = 0.0;
            for (int neighbor : neighbor_info[i]) {
                if (neighbor >= 0 && neighbor < static_cast<int>(densities.size())) {
                    // Count density discontinuities
                    if (std::abs(densities[i] - densities[neighbor]) > 0.1) {
                        perimeter_contribution += 1.0;
                    }
                }
            }
            sensitivities[i] = perimeter_contribution;
        }
    }

    return sensitivities;
}

SensitivityAnalysis::SensitivityAnalysis(const ConvergenceCriteria& criteria)
    : criteria_(criteria) {}

bool SensitivityAnalysis::check_convergence_density(
    const std::vector<double>& current_densities,
    const std::vector<double>& previous_densities) {

    if (current_densities.size() != previous_densities.size()) {
        return false;
    }

    double max_change = compute_max_change(current_densities, previous_densities);
    return max_change < criteria_.density_change_tolerance;
}

bool SensitivityAnalysis::check_convergence_objective(double current_objective) {
    objective_history_.push_back(current_objective);

    if (objective_history_.size() < 2) {
        return false;
    }

    double prev_objective = objective_history_[objective_history_.size() - 2];
    double change = std::abs(current_objective - prev_objective) /
                   (std::abs(prev_objective) + 1e-12);

    if (change < criteria_.objective_change_tolerance) {
        stall_count_++;
        return stall_count_ >= criteria_.max_stall_iterations;
    } else {
        stall_count_ = 0;
        return false;
    }
}

bool SensitivityAnalysis::check_constraints_satisfied(
    const std::vector<double>& constraint_values,
    double tolerance) {

    for (double c : constraint_values) {
        if (c > tolerance) {
            return false;
        }
    }
    return true;
}

double SensitivityAnalysis::compute_gradient_norm(const std::vector<double>& sensitivities) {
    double norm = 0.0;
    for (double s : sensitivities) {
        norm += s * s;
    }
    return std::sqrt(norm);
}

double SensitivityAnalysis::compute_max_change(const std::vector<double>& current,
                                               const std::vector<double>& previous) {
    if (current.size() != previous.size()) {
        return 1.0;
    }

    double max_change = 0.0;
    for (size_t i = 0; i < current.size(); ++i) {
        max_change = std::max(max_change, std::abs(current[i] - previous[i]));
    }
    return max_change;
}

} // namespace topology_optimization
} // namespace bendcore
