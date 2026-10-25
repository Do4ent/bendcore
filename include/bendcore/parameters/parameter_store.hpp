#pragma once

#include "../document/entity_id.hpp"
#include <string>
#include <map>
#include <variant>
#include <cmath>

namespace BendCore::CadCore {

/**
 * @brief Parameter storage with validation and change history
 */
class ParameterStore {
public:
    using Value = std::variant<double, bool, std::string>;

    struct Parameter {
        std::string name;
        Value value;
        std::string units;
        std::string formula;
        uint64_t revision = 0;
    };

    ParameterStore() noexcept = default;

    /**
     * @brief Set parameter value
     */
    void set(const std::string& name, const Value& value) noexcept;

    /**
     * @brief Get parameter value
     */
    Value get(const std::string& name) const;

    /**
     * @brief Check if parameter exists
     */
    bool has(const std::string& name) const noexcept;

    /**
     * @brief Get all parameters
     */
    std::map<std::string, Parameter> getAll() const;

    /**
     * @brief Get parameter count
     */
    size_t size() const noexcept { return params_.size(); }

    /**
     * @brief Clear all parameters
     */
    void clear() noexcept { params_.clear(); }

    /**
     * @brief Get parameter by name
     */
    const Parameter* getParameter(const std::string& name) const noexcept;

private:
    std::map<std::string, Parameter> params_;
};

/**
 * @brief Unit converter
 */
class Units {
public:
    static double toMM(double value, const std::string& unit) noexcept;
    static double fromMM(double value, const std::string& unit) noexcept;
    static bool isValid(const std::string& unit) noexcept;
};

} // namespace BendCore::CadCore
