#pragma once

#include <variant>
#include <string>
#include <cmath>
#include <stdexcept>

namespace bendcore::parameters {

/**
 * Значение параметра - может быть double, bool или string
 */
using ParameterValue = std::variant<double, bool, std::string>;

/**
 * Вспомогательные функции для работы с ParameterValue
 */
class ParameterValueUtils {
public:
    /**
     * Получить значение как double
     */
    static double as_double(const ParameterValue& value) {
        if (std::holds_alternative<double>(value)) {
            return std::get<double>(value);
        }
        throw std::runtime_error("ParameterValue is not a double");
    }

    /**
     * Получить значение как bool
     */
    static bool as_bool(const ParameterValue& value) {
        if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value);
        }
        throw std::runtime_error("ParameterValue is not a bool");
    }

    /**
     * Получить значение как string
     */
    static std::string as_string(const ParameterValue& value) {
        if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        }
        throw std::runtime_error("ParameterValue is not a string");
    }

    /**
     * Проверить, является ли значение double
     */
    static bool is_double(const ParameterValue& value) {
        return std::holds_alternative<double>(value);
    }

    /**
     * Проверить, является ли значение bool
     */
    static bool is_bool(const ParameterValue& value) {
        return std::holds_alternative<bool>(value);
    }

    /**
     * Проверить, является ли значение string
     */
    static bool is_string(const ParameterValue& value) {
        return std::holds_alternative<std::string>(value);
    }

    /**
     * Преобразовать значение в строку
     */
    static std::string to_string(const ParameterValue& value) {
        if (is_double(value)) {
            double d = as_double(value);
            if (std::fmod(d, 1.0) == 0.0) {
                return std::to_string(static_cast<long long>(d));
            }
            return std::to_string(d);
        } else if (is_bool(value)) {
            return as_bool(value) ? "true" : "false";
        } else {
            return as_string(value);
        }
    }
};

inline double get_double(const ParameterValue& value) {
    return ParameterValueUtils::as_double(value);
}

inline bool get_bool(const ParameterValue& value) {
    return ParameterValueUtils::as_bool(value);
}

inline std::string get_string(const ParameterValue& value) {
    return ParameterValueUtils::as_string(value);
}

}  // namespace bendcore::parameters
