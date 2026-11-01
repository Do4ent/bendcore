#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <optional>

namespace bendcore::parameters {

/// Parameter type enumeration
enum class ParameterType {
    FLOAT,
    INTEGER,
    BOOLEAN,
    STRING,
    DIMENSION,
    ANGLE
};

/// Parameter constraint type
enum class ConstraintType {
    MIN_VALUE,
    MAX_VALUE,
    EQUALS,
    NOT_EQUALS,
    RANGE,
    CUSTOM
};

/// Represents a single constraint on a parameter
struct ParameterConstraint {
    ConstraintType type;
    double value1 = 0.0;      // For MIN, MAX, EQUALS, NOT_EQUALS
    double value2 = 0.0;      // For RANGE (min, max)
    std::string description;
    std::function<bool(double)> custom_validator;

    /// Check if a value satisfies this constraint
    bool validate(double value) const;
};

/// Represents a single parameter in the parametric design system
class Parameter {
public:
    /// Constructor
    Parameter(
        const std::string& id,
        const std::string& name,
        ParameterType type,
        double default_value = 0.0
    );

    // Getters
    std::string get_id() const { return id_; }
    std::string get_name() const { return name_; }
    ParameterType get_type() const { return type_; }
    double get_value() const { return value_; }
    double get_default_value() const { return default_value_; }
    std::string get_description() const { return description_; }

    // Setters
    void set_value(double value);
    void set_description(const std::string& desc) { description_ = desc; }

    /// Add a constraint to this parameter
    void add_constraint(const ParameterConstraint& constraint);

    /// Get all constraints
    const std::vector<ParameterConstraint>& get_constraints() const {
        return constraints_;
    }

    /// Check if value satisfies all constraints
    bool validate(double value) const;

    /// Register callback for value changes
    void on_value_changed(std::function<void(double)> callback);

    /// Get callbacks registered for value changes
    const std::vector<std::function<void(double)>>& get_callbacks() const {
        return callbacks_;
    }

private:
    std::string id_;
    std::string name_;
    ParameterType type_;
    double value_;
    double default_value_;
    std::string description_;
    std::vector<ParameterConstraint> constraints_;
    std::vector<std::function<void(double)>> callbacks_;
};

/// Parameter dependency between two parameters
struct ParameterDependency {
    std::string source_param_id;
    std::string target_param_id;
    std::string expression;  // Math expression linking them
    std::function<double(double)> compute_func;  // Function to compute target from source
};

/// The main Parametric Design Engine
class ParameterEngine {
public:
    ParameterEngine() = default;

    /// Create and register a new parameter
    std::shared_ptr<Parameter> create_parameter(
        const std::string& id,
        const std::string& name,
        ParameterType type,
        double default_value = 0.0
    );

    /// Get a parameter by ID
    std::shared_ptr<Parameter> get_parameter(const std::string& id) const;

    /// Get all parameters
    const std::map<std::string, std::shared_ptr<Parameter>>& get_all_parameters() const {
        return parameters_;
    }

    /// Remove a parameter
    bool remove_parameter(const std::string& id);

    /// Create a dependency between two parameters
    void link_parameters(
        const std::string& source_id,
        const std::string& target_id,
        const std::string& expression,
        std::function<double(double)> compute_func
    );

    /// Get all dependencies
    const std::vector<ParameterDependency>& get_dependencies() const {
        return dependencies_;
    }

    /// Update a parameter value with automatic propagation through dependencies
    bool update_parameter(const std::string& id, double new_value);

    /// Get parameter value (with type safety)
    template<typename T>
    std::optional<T> get_typed_value(const std::string& id) const;

    /// Validate all parameters
    bool validate_all() const;

    /// Get number of parameters
    size_t parameter_count() const { return parameters_.size(); }

    /// Reset all parameters to default values
    void reset_to_defaults();

    /// Enable/disable real-time updates
    void set_real_time_updates(bool enabled) { real_time_updates_enabled_ = enabled; }
    bool is_real_time_updates_enabled() const { return real_time_updates_enabled_; }

    /// Perform sensitivity analysis
    struct SensitivityResult {
        std::string parameter_id;
        double sensitivity_value;  // How much target changes when this param changes by 1%
    };

    std::vector<SensitivityResult> analyze_sensitivity(
        const std::string& target_param_id,
        double variation_percent = 1.0
    );

private:
    std::map<std::string, std::shared_ptr<Parameter>> parameters_;
    std::vector<ParameterDependency> dependencies_;
    bool real_time_updates_enabled_ = true;

    /// Helper to propagate changes through dependency chain
    void propagate_changes(const std::string& param_id);

    /// Helper to find dependent parameters
    std::vector<std::string> find_dependents(const std::string& param_id) const;
};

/// Template specialization for type-safe value retrieval
template<>
inline std::optional<double> ParameterEngine::get_typed_value(const std::string& id) const {
    auto param = get_parameter(id);
    if (param && param->get_type() == ParameterType::FLOAT) {
        return param->get_value();
    }
    return std::nullopt;
}

template<>
inline std::optional<int> ParameterEngine::get_typed_value(const std::string& id) const {
    auto param = get_parameter(id);
    if (param && param->get_type() == ParameterType::INTEGER) {
        return static_cast<int>(param->get_value());
    }
    return std::nullopt;
}

}  // namespace bendcore::parameters
