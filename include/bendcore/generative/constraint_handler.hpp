#pragma once

#include <vector>
#include <string>
#include <optional>
#include <algorithm>
#include "design_space.hpp"

namespace bendcore::generative {

/**
 * Constraint Handling Module
 * Manages constraint violations and feasibility restoration
 */

// Constraint violation report
struct ConstraintViolationReport {
    std::vector<bool> is_violated;           // Per-constraint violation status
    std::vector<double> violation_amounts;   // Per-constraint violation magnitude
    double total_violation = 0.0;            // Sum of all violations
    double penalty_value = 0.0;              // Total penalty for infeasible designs
    bool is_feasible = false;

    [[nodiscard]] size_t violated_constraint_count() const;
    [[nodiscard]] std::string to_string() const;
};

// Feasibility restoration strategy
enum class FeasibilityStrategy {
    PENALTY_FUNCTION,        // Add penalty to objective
    REPAIR_OPERATOR,         // Modify solution to become feasible
    STOCHASTIC_RANKING,      // Rank by feasibility then fitness
    HYBRID                   // Combine multiple strategies
};

// Constraint handling method
enum class ConstraintMethod {
    PENALTY,                 // Simple penalty function
    ADAPTIVE_PENALTY,        // Adapt penalty weights during optimization
    EPSILON_CONSTRAINT,      // Relax constraints gradually
    THRESHOLD               // Multi-threshold approach
};

// Active constraint tracking
struct ActiveConstraintSet {
    std::vector<size_t> constraint_indices;  // Indices of active constraints
    std::vector<double> multipliers;         // Lagrange multiplier estimates

    [[nodiscard]] bool is_empty() const { return constraint_indices.empty(); }
    [[nodiscard]] size_t count() const { return constraint_indices.size(); }
};

// Constraint violation history for adaptive methods
struct ConstraintHistory {
    std::vector<std::vector<double>> violation_history;  // Per-constraint violations over time
    std::vector<double> average_violation;               // Moving average
    std::vector<double> penalty_weights;                 // Adaptive penalty weights
    size_t update_frequency = 10;

    void record_violation(const ConstraintViolationReport& report);
    void update_penalties();
    [[nodiscard]] std::vector<double> get_current_weights() const { return penalty_weights; }
};

// Constraint handler
class ConstraintHandler {
public:
    explicit ConstraintHandler(const DesignSpace& design_space);

    // Constraint evaluation
    [[nodiscard]] ConstraintViolationReport evaluate_constraints(const std::vector<double>& x) const;

    // Feasibility checking
    [[nodiscard]] bool is_feasible(const std::vector<double>& x) const;
    [[nodiscard]] double feasibility_measure(const std::vector<double>& x) const;

    // Penalty computation
    [[nodiscard]] double compute_penalty(const std::vector<double>& x,
                                        const ConstraintViolationReport& violation) const;
    [[nodiscard]] double penalty_with_weights(const std::vector<double>& x,
                                             const std::vector<double>& weights) const;

    // Active constraint set
    [[nodiscard]] ActiveConstraintSet identify_active_constraints(const std::vector<double>& x,
                                                                 double tolerance = 1e-3) const;

    // Feasibility restoration
    [[nodiscard]] std::optional<std::vector<double>> repair_solution(const std::vector<double>& x,
                                                                    size_t max_iterations = 100);

    // Configuration
    void set_method(ConstraintMethod method) { method_ = method; }
    void set_strategy(FeasibilityStrategy strategy) { strategy_ = strategy; }
    void set_penalty_weight(double weight) { base_penalty_weight_ = weight; }
    void enable_adaptive_penalty(bool enable) { use_adaptive_penalty_ = enable; }

    // History tracking for adaptive methods
    void record_violation(const ConstraintViolationReport& report);
    void update_adaptive_weights();
    [[nodiscard]] const ConstraintHistory& get_history() const { return history_; }

    // Multi-constraint resolution
    [[nodiscard]] std::vector<size_t> find_redundant_constraints(const std::vector<double>& x,
                                                                 double tolerance = 1e-6) const;
    [[nodiscard]] bool resolve_conflicting_constraints(std::vector<double>& x,
                                                      size_t max_iterations = 50);

private:
    const DesignSpace& design_space_;
    ConstraintMethod method_ = ConstraintMethod::PENALTY;
    FeasibilityStrategy strategy_ = FeasibilityStrategy::PENALTY_FUNCTION;
    double base_penalty_weight_ = 100.0;
    bool use_adaptive_penalty_ = false;
    ConstraintHistory history_;

    // Penalty methods
    [[nodiscard]] double penalty_function(const std::vector<double>& x) const;
    [[nodiscard]] double adaptive_penalty(const std::vector<double>& x);
    [[nodiscard]] double epsilon_constraint_penalty(const std::vector<double>& x, size_t iteration) const;

    // Repair operators
    [[nodiscard]] std::vector<double> gradient_based_repair(const std::vector<double>& x,
                                                           size_t max_iterations) const;
    [[nodiscard]] std::vector<double> orthogonal_repair(const std::vector<double>& x) const;
};

} // namespace bendcore::generative
