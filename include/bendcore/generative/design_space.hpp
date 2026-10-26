#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>
#include <stdexcept>
#include <cmath>

namespace bendcore::generative {

/**
 * M6 Phase: Generative Design System (GAP-026)
 * Design Space Definition - Parameter bounds and constraint specifications
 */

// Parameter bound for design variables
struct ParameterBound {
    std::string name;
    double lower_bound = 0.0;
    double upper_bound = 1.0;
    double default_value = 0.5;

    ParameterBound() = default;
    ParameterBound(const std::string& n, double lower, double upper, double def = 0.5)
        : name(n), lower_bound(lower), upper_bound(upper), default_value(def) {
        if (lower > upper) throw std::invalid_argument("Lower bound > upper bound");
        if (def < lower || def > upper) throw std::invalid_argument("Default value out of bounds");
    }

    [[nodiscard]] double range() const { return upper_bound - lower_bound; }
    [[nodiscard]] bool is_valid(double value) const { return value >= lower_bound && value <= upper_bound; }
    [[nodiscard]] double normalize(double value) const;
    [[nodiscard]] double denormalize(double normalized) const;
};

// Constraint violation type
enum class ConstraintType {
    INEQUALITY,      // g(x) <= 0
    EQUALITY,        // h(x) = 0
    BOUND,           // Box constraint (automatically handled)
    CUSTOM           // User-defined constraint
};

// Single constraint definition
struct Constraint {
    std::string name;
    ConstraintType type = ConstraintType::INEQUALITY;
    std::function<double(const std::vector<double>&)> evaluate;
    double tolerance = 1e-6;
    double penalty_weight = 100.0;

    Constraint() = default;
    Constraint(const std::string& n, ConstraintType t,
               std::function<double(const std::vector<double>&)> eval,
               double tol = 1e-6, double weight = 100.0)
        : name(n), type(t), evaluate(eval), tolerance(tol), penalty_weight(weight) {}

    [[nodiscard]] bool is_satisfied(const std::vector<double>& x) const;
    [[nodiscard]] double violation(const std::vector<double>& x) const;
};

// Objective function definition
struct Objective {
    std::string name;
    std::function<double(const std::vector<double>&)> evaluate;
    bool is_minimization = true;  // true for minimization, false for maximization

    Objective() = default;
    Objective(const std::string& n, std::function<double(const std::vector<double>&)> eval, bool minimize = true)
        : name(n), evaluate(eval), is_minimization(minimize) {}

    [[nodiscard]] double compute(const std::vector<double>& x) const { return evaluate(x); }
};

// Design space definition combining parameters, constraints, and objectives
class DesignSpace {
public:
    DesignSpace() = default;
    explicit DesignSpace(size_t dimension) : parameters_(dimension) {}

    // Parameter management
    void add_parameter(const ParameterBound& param);
    void add_parameter(const std::string& name, double lower, double upper, double def = 0.5);
    [[nodiscard]] const ParameterBound& get_parameter(size_t index) const;
    [[nodiscard]] size_t parameter_count() const { return parameters_.size(); }
    [[nodiscard]] const std::vector<ParameterBound>& parameters() const { return parameters_; }

    // Constraint management
    void add_constraint(const Constraint& constraint);
    void add_inequality_constraint(const std::string& name,
                                  std::function<double(const std::vector<double>&)> g,
                                  double tolerance = 1e-6, double weight = 100.0);
    void add_equality_constraint(const std::string& name,
                                std::function<double(const std::vector<double>&)> h,
                                double tolerance = 1e-6, double weight = 100.0);
    [[nodiscard]] size_t constraint_count() const { return constraints_.size(); }
    [[nodiscard]] const Constraint& get_constraint(size_t index) const;
    [[nodiscard]] const std::vector<Constraint>& constraints() const { return constraints_; }

    // Objective management
    void set_primary_objective(const Objective& objective);
    void add_secondary_objective(const Objective& objective);
    [[nodiscard]] const Objective& primary_objective() const { return primary_objective_; }
    [[nodiscard]] const std::vector<Objective>& secondary_objectives() const { return secondary_objectives_; }
    [[nodiscard]] size_t objective_count() const { return 1 + secondary_objectives_.size(); }

    // Design point validation
    [[nodiscard]] bool is_feasible(const std::vector<double>& x) const;
    [[nodiscard]] double constraint_violation(const std::vector<double>& x) const;
    [[nodiscard]] std::vector<double> normalize(const std::vector<double>& x) const;
    [[nodiscard]] std::vector<double> denormalize(const std::vector<double>& normalized) const;

    // Bounds checking
    [[nodiscard]] bool within_bounds(const std::vector<double>& x) const;
    void enforce_bounds(std::vector<double>& x) const;

private:
    std::vector<ParameterBound> parameters_;
    std::vector<Constraint> constraints_;
    Objective primary_objective_;
    std::vector<Objective> secondary_objectives_;
};

} // namespace bendcore::generative
