#include "../../include/bendcore/generative/constraint_handler.hpp"
#include <numeric>
#include <sstream>
#include <cmath>

namespace bendcore::generative {

// ConstraintViolationReport methods
size_t ConstraintViolationReport::violated_constraint_count() const {
    return std::count(is_violated.begin(), is_violated.end(), true);
}

std::string ConstraintViolationReport::to_string() const {
    std::ostringstream oss;
    oss << "Constraints: " << violated_constraint_count() << "/" << is_violated.size() << " violated\n";
    oss << "Total violation: " << total_violation << "\n";
    oss << "Penalty: " << penalty_value << "\n";
    oss << "Feasible: " << (is_feasible ? "yes" : "no");
    return oss.str();
}

// ConstraintHistory methods
void ConstraintHistory::record_violation(const ConstraintViolationReport& report) {
    if (violation_history.empty()) {
        violation_history.resize(report.is_violated.size());
    }

    for (size_t i = 0; i < report.violation_amounts.size() && i < violation_history.size(); ++i) {
        violation_history[i].push_back(report.violation_amounts[i]);
    }
}

void ConstraintHistory::update_penalties() {
    if (violation_history.empty()) return;

    average_violation.resize(violation_history.size());
    penalty_weights.resize(violation_history.size());

    for (size_t i = 0; i < violation_history.size(); ++i) {
        if (!violation_history[i].empty()) {
            average_violation[i] = std::accumulate(violation_history[i].begin(),
                                                   violation_history[i].end(), 0.0) /
                                  violation_history[i].size();

            penalty_weights[i] = average_violation[i] > 0.0 ? 1.0 / average_violation[i] : 1.0;
        }
    }
}

// ConstraintHandler methods
ConstraintHandler::ConstraintHandler(const DesignSpace& design_space)
    : design_space_(design_space) {

    history_.penalty_weights.resize(design_space.constraint_count(), 1.0);
    history_.average_violation.resize(design_space.constraint_count(), 0.0);
}

ConstraintViolationReport ConstraintHandler::evaluate_constraints(const std::vector<double>& x) const {
    ConstraintViolationReport report;
    report.is_violated.resize(design_space_.constraint_count());
    report.violation_amounts.resize(design_space_.constraint_count());
    report.total_violation = 0.0;
    report.penalty_value = 0.0;

    for (size_t i = 0; i < design_space_.constraint_count(); ++i) {
        const auto& constraint = design_space_.get_constraint(i);
        double violation = constraint.violation(x);

        report.is_violated[i] = (violation > constraint.tolerance);
        report.violation_amounts[i] = violation;
        report.total_violation += violation;
        report.penalty_value += violation * constraint.penalty_weight;
    }

    report.is_feasible = (report.total_violation < 1e-6);
    return report;
}

bool ConstraintHandler::is_feasible(const std::vector<double>& x) const {
    auto report = evaluate_constraints(x);
    return report.is_feasible;
}

double ConstraintHandler::feasibility_measure(const std::vector<double>& x) const {
    auto report = evaluate_constraints(x);
    return 1.0 - std::min(1.0, report.total_violation);
}

double ConstraintHandler::compute_penalty(const std::vector<double>& x,
                                         const ConstraintViolationReport& violation) const {
    return violation.penalty_value;
}

double ConstraintHandler::penalty_with_weights(const std::vector<double>& x,
                                              const std::vector<double>& weights) const {
    auto report = evaluate_constraints(x);
    double penalty = 0.0;

    for (size_t i = 0; i < report.violation_amounts.size() && i < weights.size(); ++i) {
        penalty += report.violation_amounts[i] * weights[i];
    }

    return penalty;
}

ActiveConstraintSet ConstraintHandler::identify_active_constraints(const std::vector<double>& x,
                                                                  double tolerance) const {
    ActiveConstraintSet active;

    for (size_t i = 0; i < design_space_.constraint_count(); ++i) {
        const auto& constraint = design_space_.get_constraint(i);
        double violation = constraint.violation(x);

        if (violation > tolerance) {
            active.constraint_indices.push_back(i);
            active.multipliers.push_back(violation);
        }
    }

    return active;
}

std::optional<std::vector<double>> ConstraintHandler::repair_solution(const std::vector<double>& x,
                                                                      size_t max_iterations) {
    if (is_feasible(x)) {
        return x;
    }

    switch (strategy_) {
        case FeasibilityStrategy::REPAIR_OPERATOR:
            return gradient_based_repair(x, max_iterations);
        case FeasibilityStrategy::STOCHASTIC_RANKING:
            return orthogonal_repair(x);
        default:
            return {};
    }
}

void ConstraintHandler::record_violation(const ConstraintViolationReport& report) {
    history_.record_violation(report);
}

void ConstraintHandler::update_adaptive_weights() {
    history_.update_penalties();
}

std::vector<size_t> ConstraintHandler::find_redundant_constraints(const std::vector<double>& x,
                                                                 double tolerance) const {
    std::vector<size_t> redundant;

    for (size_t i = 0; i < design_space_.constraint_count(); ++i) {
        const auto& constraint = design_space_.get_constraint(i);

        if (constraint.violation(x) < tolerance) {
            bool is_redundant = true;

            for (size_t j = 0; j < design_space_.constraint_count(); ++j) {
                if (i != j) {
                    const auto& other_constraint = design_space_.get_constraint(j);
                    if (other_constraint.violation(x) > tolerance) {
                        is_redundant = false;
                        break;
                    }
                }
            }

            if (is_redundant) {
                redundant.push_back(i);
            }
        }
    }

    return redundant;
}

bool ConstraintHandler::resolve_conflicting_constraints(std::vector<double>& x,
                                                       size_t max_iterations) {
    for (size_t iter = 0; iter < max_iterations; ++iter) {
        if (is_feasible(x)) {
            return true;
        }

        auto active = identify_active_constraints(x, 1e-3);
        if (active.is_empty()) {
            return true;
        }
    }

    return is_feasible(x);
}

double ConstraintHandler::penalty_function(const std::vector<double>& x) const {
    auto report = evaluate_constraints(x);
    return report.penalty_value;
}

double ConstraintHandler::adaptive_penalty(const std::vector<double>& x) {
    auto report = evaluate_constraints(x);
    record_violation(report);

    double penalty = 0.0;
    for (size_t i = 0; i < report.violation_amounts.size() && i < history_.penalty_weights.size(); ++i) {
        penalty += report.violation_amounts[i] * history_.penalty_weights[i];
    }

    return penalty;
}

double ConstraintHandler::epsilon_constraint_penalty(const std::vector<double>& x, size_t iteration) const {
    auto report = evaluate_constraints(x);

    // Gradually relax constraints
    double epsilon_factor = 1.0 - (iteration / 100.0);
    epsilon_factor = std::max(0.1, epsilon_factor);

    return report.penalty_value * epsilon_factor;
}

std::vector<double> ConstraintHandler::gradient_based_repair(const std::vector<double>& x,
                                                            size_t max_iterations) const {
    auto repaired = x;
    design_space_.enforce_bounds(repaired);

    for (size_t iter = 0; iter < max_iterations; ++iter) {
        if (is_feasible(repaired)) {
            return repaired;
        }

        // Simple gradient-based adjustment
        for (size_t i = 0; i < repaired.size(); ++i) {
            repaired[i] += 0.01 * (0.5 - repaired[i]);
            repaired[i] = std::max(0.0, std::min(1.0, repaired[i]));
        }
    }

    return repaired;
}

std::vector<double> ConstraintHandler::orthogonal_repair(const std::vector<double>& x) const {
    return x;  // Placeholder for orthogonal repair operator
}

} // namespace bendcore::generative
