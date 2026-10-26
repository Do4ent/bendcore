#include "../../include/bendcore/generative/design_space.hpp"
#include <cmath>
#include <sstream>

namespace bendcore::generative {

// ParameterBound methods
double ParameterBound::normalize(double value) const {
    if (range() == 0.0) return 0.0;
    return (value - lower_bound) / range();
}

double ParameterBound::denormalize(double normalized) const {
    return lower_bound + normalized * range();
}

// Constraint methods
bool Constraint::is_satisfied(const std::vector<double>& x) const {
    if (!evaluate) return true;

    double violation = evaluate(x);
    switch (type) {
        case ConstraintType::INEQUALITY:
            return violation <= tolerance;
        case ConstraintType::EQUALITY:
            return std::fabs(violation) <= tolerance;
        case ConstraintType::BOUND:
        case ConstraintType::CUSTOM:
            return violation <= tolerance;
        default:
            return true;
    }
}

double Constraint::violation(const std::vector<double>& x) const {
    if (!evaluate) return 0.0;

    double value = evaluate(x);
    switch (type) {
        case ConstraintType::INEQUALITY:
            return std::max(0.0, value - tolerance);
        case ConstraintType::EQUALITY:
            return std::fabs(value);
        case ConstraintType::BOUND:
        case ConstraintType::CUSTOM:
            return std::max(0.0, value - tolerance);
        default:
            return 0.0;
    }
}

// DesignSpace methods
void DesignSpace::add_parameter(const ParameterBound& param) {
    parameters_.push_back(param);
}

void DesignSpace::add_parameter(const std::string& name, double lower, double upper, double def) {
    parameters_.emplace_back(name, lower, upper, def);
}

const ParameterBound& DesignSpace::get_parameter(size_t index) const {
    if (index >= parameters_.size()) {
        throw std::out_of_range("Parameter index out of range");
    }
    return parameters_[index];
}

void DesignSpace::add_constraint(const Constraint& constraint) {
    constraints_.push_back(constraint);
}

void DesignSpace::add_inequality_constraint(const std::string& name,
                                           std::function<double(const std::vector<double>&)> g,
                                           double tolerance, double weight) {
    constraints_.emplace_back(name, ConstraintType::INEQUALITY, g, tolerance, weight);
}

void DesignSpace::add_equality_constraint(const std::string& name,
                                         std::function<double(const std::vector<double>&)> h,
                                         double tolerance, double weight) {
    constraints_.emplace_back(name, ConstraintType::EQUALITY, h, tolerance, weight);
}

const Constraint& DesignSpace::get_constraint(size_t index) const {
    if (index >= constraints_.size()) {
        throw std::out_of_range("Constraint index out of range");
    }
    return constraints_[index];
}

void DesignSpace::set_primary_objective(const Objective& objective) {
    primary_objective_ = objective;
}

void DesignSpace::add_secondary_objective(const Objective& objective) {
    secondary_objectives_.push_back(objective);
}

bool DesignSpace::is_feasible(const std::vector<double>& x) const {
    return constraint_violation(x) < 1e-6;
}

double DesignSpace::constraint_violation(const std::vector<double>& x) const {
    if (!within_bounds(x)) return std::numeric_limits<double>::max();

    double total_violation = 0.0;
    for (const auto& constraint : constraints_) {
        total_violation += constraint.violation(x);
    }
    return total_violation;
}

std::vector<double> DesignSpace::normalize(const std::vector<double>& x) const {
    std::vector<double> normalized(x.size());
    for (size_t i = 0; i < x.size() && i < parameters_.size(); ++i) {
        normalized[i] = parameters_[i].normalize(x[i]);
    }
    return normalized;
}

std::vector<double> DesignSpace::denormalize(const std::vector<double>& normalized) const {
    std::vector<double> x(normalized.size());
    for (size_t i = 0; i < normalized.size() && i < parameters_.size(); ++i) {
        x[i] = parameters_[i].denormalize(normalized[i]);
    }
    return x;
}

bool DesignSpace::within_bounds(const std::vector<double>& x) const {
    if (x.size() != parameters_.size()) return false;

    for (size_t i = 0; i < x.size(); ++i) {
        if (!parameters_[i].is_valid(x[i])) return false;
    }
    return true;
}

void DesignSpace::enforce_bounds(std::vector<double>& x) const {
    if (x.size() != parameters_.size()) {
        x.resize(parameters_.size());
    }

    for (size_t i = 0; i < x.size(); ++i) {
        x[i] = std::max(parameters_[i].lower_bound,
                       std::min(x[i], parameters_[i].upper_bound));
    }
}

} // namespace bendcore::generative
