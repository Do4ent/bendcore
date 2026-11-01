#include "parameter_engine.hpp"
#include <algorithm>
#include <cmath>

namespace bendcore::parameters {

// ==================== ParameterConstraint ====================

bool ParameterConstraint::validate(double value) const {
    switch (type) {
        case ConstraintType::MIN_VALUE:
            return value >= value1;
        case ConstraintType::MAX_VALUE:
            return value <= value1;
        case ConstraintType::EQUALS:
            return std::abs(value - value1) < 1e-9;
        case ConstraintType::NOT_EQUALS:
            return std::abs(value - value1) >= 1e-9;
        case ConstraintType::RANGE:
            return value >= value1 && value <= value2;
        case ConstraintType::CUSTOM:
            if (custom_validator) {
                return custom_validator(value);
            }
            return true;
        default:
            return true;
    }
}

// ==================== Parameter ====================

Parameter::Parameter(
    const std::string& id,
    const std::string& name,
    ParameterType type,
    double default_value
)
    : id_(id), name_(name), type_(type),
      value_(default_value), default_value_(default_value) {}

void Parameter::set_value(double value) {
    if (validate(value)) {
        value_ = value;
        for (const auto& callback : callbacks_) {
            if (callback) {
                callback(value_);
            }
        }
    }
}

void Parameter::add_constraint(const ParameterConstraint& constraint) {
    constraints_.push_back(constraint);
}

bool Parameter::validate(double value) const {
    for (const auto& constraint : constraints_) {
        if (!constraint.validate(value)) {
            return false;
        }
    }
    return true;
}

void Parameter::on_value_changed(std::function<void(double)> callback) {
    callbacks_.push_back(callback);
}

// ==================== ParameterEngine ====================

std::shared_ptr<Parameter> ParameterEngine::create_parameter(
    const std::string& id,
    const std::string& name,
    ParameterType type,
    double default_value
) {
    auto param = std::make_shared<Parameter>(id, name, type, default_value);
    parameters_[id] = param;
    return param;
}

std::shared_ptr<Parameter> ParameterEngine::get_parameter(const std::string& id) const {
    auto it = parameters_.find(id);
    if (it != parameters_.end()) {
        return it->second;
    }
    return nullptr;
}

bool ParameterEngine::remove_parameter(const std::string& id) {
    auto it = parameters_.find(id);
    if (it != parameters_.end()) {
        // Remove dependencies related to this parameter
        auto new_end = std::remove_if(
            dependencies_.begin(), dependencies_.end(),
            [&id](const ParameterDependency& dep) {
                return dep.source_param_id == id || dep.target_param_id == id;
            }
        );
        dependencies_.erase(new_end, dependencies_.end());

        parameters_.erase(it);
        return true;
    }
    return false;
}

void ParameterEngine::link_parameters(
    const std::string& source_id,
    const std::string& target_id,
    const std::string& expression,
    std::function<double(double)> compute_func
) {
    if (!get_parameter(source_id) || !get_parameter(target_id)) {
        return;  // Both parameters must exist
    }

    ParameterDependency dep;
    dep.source_param_id = source_id;
    dep.target_param_id = target_id;
    dep.expression = expression;
    dep.compute_func = compute_func;

    dependencies_.push_back(dep);
}

bool ParameterEngine::update_parameter(const std::string& id, double new_value) {
    auto param = get_parameter(id);
    if (!param) {
        return false;
    }

    if (!param->validate(new_value)) {
        return false;
    }

    param->set_value(new_value);

    if (real_time_updates_enabled_) {
        propagate_changes(id);
    }

    return true;
}

std::vector<std::string> ParameterEngine::find_dependents(const std::string& param_id) const {
    std::vector<std::string> dependents;
    for (const auto& dep : dependencies_) {
        if (dep.source_param_id == param_id) {
            dependents.push_back(dep.target_param_id);
        }
    }
    return dependents;
}

void ParameterEngine::propagate_changes(const std::string& param_id) {
    std::vector<std::string> to_process = { param_id };
    std::set<std::string> processed;

    while (!to_process.empty()) {
        std::string current = to_process.back();
        to_process.pop_back();

        if (processed.count(current)) {
            continue;  // Avoid cycles
        }
        processed.insert(current);

        auto source_param = get_parameter(current);
        if (!source_param) continue;

        for (const auto& dep : dependencies_) {
            if (dep.source_param_id == current && dep.compute_func) {
                auto target = get_parameter(dep.target_param_id);
                if (target) {
                    double new_value = dep.compute_func(source_param->get_value());
                    if (target->validate(new_value)) {
                        target->set_value(new_value);
                        to_process.push_back(dep.target_param_id);
                    }
                }
            }
        }
    }
}

bool ParameterEngine::validate_all() const {
    for (const auto& [id, param] : parameters_) {
        if (!param->validate(param->get_value())) {
            return false;
        }
    }
    return true;
}

void ParameterEngine::reset_to_defaults() {
    for (auto& [id, param] : parameters_) {
        param->set_value(param->get_default_value());
    }
    if (real_time_updates_enabled_) {
        for (auto& [id, param] : parameters_) {
            propagate_changes(id);
        }
    }
}

std::vector<ParameterEngine::SensitivityResult>
ParameterEngine::analyze_sensitivity(
    const std::string& target_param_id,
    double variation_percent
) {
    std::vector<SensitivityResult> results;

    auto target = get_parameter(target_param_id);
    if (!target) {
        return results;
    }

    double base_value = target->get_value();

    // For each parameter, vary it by variation_percent and measure impact on target
    for (const auto& [id, param] : parameters_) {
        if (id == target_param_id) {
            continue;  // Skip self-sensitivity
        }

        double original = param->get_value();
        double variation = original * (variation_percent / 100.0);

        if (std::abs(variation) < 1e-9) {
            variation = variation_percent / 100.0;  // Use absolute variation if relative is too small
        }

        // Vary parameter up
        param->set_value(original + variation);
        if (real_time_updates_enabled_) {
            propagate_changes(id);
        }
        double value_up = target->get_value();

        // Vary parameter down
        param->set_value(original - variation);
        if (real_time_updates_enabled_) {
            propagate_changes(id);
        }
        double value_down = target->get_value();

        // Restore original value
        param->set_value(original);
        if (real_time_updates_enabled_) {
            propagate_changes(id);
        }

        // Calculate sensitivity: (change in target) / (change in parameter)
        double sensitivity = 0.0;
        if (std::abs(2.0 * variation) > 1e-9) {
            sensitivity = (value_up - value_down) / (2.0 * variation);
        }

        results.push_back({id, sensitivity});
    }

    // Restore target to original value
    target->set_value(base_value);
    if (real_time_updates_enabled_) {
        propagate_changes(target_param_id);
    }

    return results;
}

}  // namespace bendcore::parameters
