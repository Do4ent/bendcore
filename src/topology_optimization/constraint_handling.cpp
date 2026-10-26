#include "bendcore/topology_optimization/constraint_handling.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace bendcore {
namespace topology_optimization {

double ConstraintHandler::compute_volume_constraint(
    const std::vector<double>& densities,
    const std::vector<double>& element_volumes,
    double target_volume) {

    double total_volume = 0.0;

    for (size_t i = 0; i < densities.size(); ++i) {
        double vol = (i < element_volumes.size()) ? element_volumes[i] : 1.0;
        total_volume += densities[i] * vol;
    }

    return total_volume / target_volume - 1.0;  // <= 0 for satisfaction
}

double ConstraintHandler::compute_stress_constraint(
    const std::vector<double>& stresses,
    double stress_limit) {

    double max_stress = *std::max_element(stresses.begin(), stresses.end());
    return max_stress / stress_limit - 1.0;  // <= 0 for satisfaction
}

std::vector<double> ConstraintHandler::apply_symmetry_constraint(
    const std::vector<double>& densities,
    const std::vector<std::pair<size_t, size_t>>& symmetry_pairs) {

    std::vector<double> constrained = densities;

    for (const auto& pair : symmetry_pairs) {
        size_t i = pair.first;
        size_t j = pair.second;

        if (i < constrained.size() && j < constrained.size()) {
            // Average the symmetric elements
            double avg = 0.5 * (constrained[i] + constrained[j]);
            constrained[i] = avg;
            constrained[j] = avg;
        }
    }

    return constrained;
}

std::vector<double> ConstraintHandler::apply_manufacturing_constraint(
    const std::vector<double>& densities,
    const std::vector<std::array<double, 3>>& element_positions,
    double min_feature_size,
    double threshold) {

    std::vector<double> constrained = densities;

    // Find connected components
    auto components = std::vector<std::vector<size_t>>();
    std::vector<bool> visited(densities.size(), false);

    for (size_t i = 0; i < densities.size(); ++i) {
        if (!visited[i] && densities[i] > threshold) {
            std::vector<size_t> component;

            // BFS-like traversal
            std::vector<size_t> queue;
            queue.push_back(i);
            visited[i] = true;

            while (!queue.empty()) {
                size_t current = queue.back();
                queue.pop_back();
                component.push_back(current);

                // Find neighbors within distance
                for (size_t j = 0; j < densities.size(); ++j) {
                    if (!visited[j] && densities[j] > threshold && current < element_positions.size() &&
                        j < element_positions.size()) {

                        double dist = 0.0;
                        for (int k = 0; k < 3; ++k) {
                            double diff = element_positions[current][k] - element_positions[j][k];
                            dist += diff * diff;
                        }
                        dist = std::sqrt(dist);

                        if (dist < min_feature_size * 2.0) {
                            visited[j] = true;
                            queue.push_back(j);
                        }
                    }
                }
            }

            components.push_back(component);
        }
    }

    // Remove components smaller than min_feature_size
    for (const auto& component : components) {
        if (component.size() < 5) {  // Arbitrary minimum component size
            for (size_t idx : component) {
                constrained[idx] = 0.0;
            }
        }
    }

    return constrained;
}

double ConstraintHandler::compute_displacement_constraint(
    const std::vector<double>& displacements,
    double displacement_limit) {

    double max_disp = 0.0;
    for (double d : displacements) {
        max_disp = std::max(max_disp, std::abs(d));
    }

    return max_disp / displacement_limit - 1.0;  // <= 0 for satisfaction
}

double ConstraintHandler::compute_frequency_constraint(
    double fundamental_frequency,
    double target_frequency) {

    return fundamental_frequency / target_frequency - 1.0;  // <= 0 for satisfaction
}

ConstraintHandler::ConstraintHandler(const Parameters& params)
    : parameters_(params) {}

void ConstraintHandler::add_constraint(const Constraint& constraint) {
    constraints_.push_back(constraint);
}

std::vector<double> ConstraintHandler::evaluate_constraints(
    const std::vector<double>& densities,
    const std::vector<double>& element_volumes,
    const std::vector<double>& stresses,
    const std::vector<std::array<double, 3>>& element_positions) {

    std::vector<double> constraint_values;

    for (const auto& constraint : constraints_) {
        if (!constraint.active) continue;

        double value = 0.0;

        switch (constraint.type) {
            case ConstraintType::VOLUME:
                value = compute_volume_constraint(densities, element_volumes,
                                                 densities.size() * parameters_.volume_fraction);
                break;

            case ConstraintType::STRESS:
                if (!stresses.empty()) {
                    value = compute_stress_constraint(stresses, parameters_.stress_limit);
                }
                break;

            case ConstraintType::SYMMETRY:
                // Symmetry constraint is handled differently
                value = 0.0;
                break;

            case ConstraintType::MANUFACTURING:
                {
                    auto constrained = apply_manufacturing_constraint(densities, element_positions,
                                                                      parameters_.min_feature_size);
                    // Check how much modification was needed
                    double modification = 0.0;
                    for (size_t i = 0; i < densities.size(); ++i) {
                        modification += std::abs(densities[i] - constrained[i]);
                    }
                    value = modification / densities.size();
                }
                break;

            case ConstraintType::DISPLACEMENT:
                // Not implemented here
                value = 0.0;
                break;

            case ConstraintType::FREQUENCY:
                // Not implemented here
                value = 0.0;
                break;
        }

        constraint_values.push_back(value * constraint.weight);
    }

    return constraint_values;
}

bool ConstraintHandler::all_constraints_satisfied(
    const std::vector<double>& constraint_values) const {

    for (size_t i = 0; i < constraint_values.size(); ++i) {
        if (i < constraints_.size()) {
            if (constraint_values[i] > constraints_[i].upper_bound + parameters_.constraint_tolerance) {
                return false;
            }
        }
    }
    return true;
}

double ConstraintHandler::compute_penalty(
    const std::vector<double>& constraint_values) const {

    double penalty = 0.0;

    for (size_t i = 0; i < constraint_values.size(); ++i) {
        if (i < constraints_.size()) {
            double violation = std::max(0.0,
                constraint_values[i] - constraints_[i].upper_bound);
            penalty += parameters_.penalty_parameter * violation * violation;
        }
    }

    return penalty;
}

std::vector<size_t> ConstraintHandler::get_active_constraints() const {
    std::vector<size_t> active;

    for (size_t i = 0; i < constraints_.size(); ++i) {
        if (constraints_[i].active) {
            active.push_back(i);
        }
    }

    return active;
}

void ConstraintHandler::set_symmetry_pairs(
    const std::vector<std::pair<size_t, size_t>>& pairs) {
    symmetry_pairs_ = pairs;
}

} // namespace topology_optimization
} // namespace bendcore
