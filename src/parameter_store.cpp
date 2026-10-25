#include "bendcore/parameters/parameter_store.hpp"

namespace BendCore::CadCore {

void ParameterStore::set(const std::string& name, const ParameterStore::Value& value) noexcept {
    params_[name].name = name;
    params_[name].value = value;
    params_[name].revision++;
}

ParameterStore::Value ParameterStore::get(const std::string& name) const {
    auto it = params_.find(name);
    if (it != params_.end()) {
        return it->second.value;
    }
    return 0.0;
}

bool ParameterStore::has(const std::string& name) const noexcept {
    return params_.find(name) != params_.end();
}

std::map<std::string, ParameterStore::Parameter> ParameterStore::getAll() const {
    std::map<std::string, Parameter> result;
    for (const auto& [name, param] : params_) {
        result[name] = param;
    }
    return result;
}

const ParameterStore::Parameter* ParameterStore::getParameter(const std::string& name) const noexcept {
    auto it = params_.find(name);
    if (it != params_.end()) {
        return &it->second;
    }
    return nullptr;
}

double Units::toMM(double value, const std::string& unit) noexcept {
    if (unit == "mm") return value;
    if (unit == "cm") return value * 10.0;
    if (unit == "m") return value * 1000.0;
    if (unit == "in") return value * 25.4;
    if (unit == "ft") return value * 304.8;
    return value;
}

double Units::fromMM(double value, const std::string& unit) noexcept {
    if (unit == "mm") return value;
    if (unit == "cm") return value / 10.0;
    if (unit == "m") return value / 1000.0;
    if (unit == "in") return value / 25.4;
    if (unit == "ft") return value / 304.8;
    return value;
}

bool Units::isValid(const std::string& unit) noexcept {
    return unit == "mm" || unit == "cm" || unit == "m" ||
           unit == "in" || unit == "ft" || unit == "deg" || unit == "rad";
}

} // namespace BendCore::CadCore
