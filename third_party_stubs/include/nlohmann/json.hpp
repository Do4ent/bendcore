#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include <optional>
#include <iostream>

namespace nlohmann {

// Forward declaration
class json;

// Type aliases
using json_value = std::variant<
    std::nullptr_t,
    bool,
    double,
    std::string,
    std::vector<json>,
    std::map<std::string, json>
>;

class json {
public:
    // Constructors
    json() : value_(nullptr) {}
    json(nullptr_t) : value_(nullptr) {}
    json(bool b) : value_(b) {}
    json(int i) : value_(static_cast<double>(i)) {}
    json(double d) : value_(d) {}
    json(const std::string& s) : value_(s) {}
    json(const char* s) : value_(std::string(s)) {}
    json(const std::vector<json>& arr) : value_(arr) {}
    json(const std::map<std::string, json>& obj) : value_(obj) {}
    
    // Copy and move
    json(const json&) = default;
    json(json&&) noexcept = default;
    json& operator=(const json&) = default;
    json& operator=(json&&) noexcept = default;
    
    // Type checks
    bool is_null() const { return std::holds_alternative<std::nullptr_t>(value_); }
    bool is_boolean() const { return std::holds_alternative<bool>(value_); }
    bool is_number() const { return std::holds_alternative<double>(value_); }
    bool is_string() const { return std::holds_alternative<std::string>(value_); }
    bool is_array() const { return std::holds_alternative<std::vector<json>>(value_); }
    bool is_object() const { return std::holds_alternative<std::map<std::string, json>>(value_); }
    
    // Conversions
    bool get<bool>() const { return std::get<bool>(value_); }
    double get<double>() const { return std::get<double>(value_); }
    int get<int>() const { return static_cast<int>(std::get<double>(value_)); }
    std::string get<std::string>() const { return std::get<std::string>(value_); }
    
    // Array/Object access
    json& operator[](size_t idx) {
        if (!is_array()) value_ = std::vector<json>();
        return std::get<std::vector<json>>(value_)[idx];
    }
    
    json& operator[](const std::string& key) {
        if (!is_object()) value_ = std::map<std::string, json>();
        return std::get<std::map<std::string, json>>(value_)[key];
    }
    
    const json& operator[](size_t idx) const {
        return std::get<std::vector<json>>(value_)[idx];
    }
    
    const json& operator[](const std::string& key) const {
        return std::get<std::map<std::string, json>>(value_).at(key);
    }
    
    // Size
    size_t size() const {
        if (is_array()) return std::get<std::vector<json>>(value_).size();
        if (is_object()) return std::get<std::map<std::string, json>>(value_).size();
        return 0;
    }
    
    // Parsing
    static json parse(const std::string& str) {
        // Minimal JSON parser for stub
        json result;
        if (str == "{}") result.value_ = std::map<std::string, json>();
        else if (str == "[]") result.value_ = std::vector<json>();
        else if (str == "null") result.value_ = nullptr;
        else if (str == "true") result.value_ = true;
        else if (str == "false") result.value_ = false;
        else {
            try { result.value_ = std::stod(str); }
            catch (...) { result.value_ = str; }
        }
        return result;
    }
    
    // Serialization
    std::string dump(int indent = -1) const {
        if (is_null()) return "null";
        if (is_boolean()) return std::get<bool>(value_) ? "true" : "false";
        if (is_number()) return std::to_string(std::get<double>(value_));
        if (is_string()) return "\"" + std::get<std::string>(value_) + "\"";
        if (is_array()) {
            std::string result = "[";
            auto& arr = std::get<std::vector<json>>(value_);
            for (size_t i = 0; i < arr.size(); ++i) {
                result += arr[i].dump(indent);
                if (i < arr.size() - 1) result += ",";
            }
            return result + "]";
        }
        if (is_object()) {
            std::string result = "{";
            auto& obj = std::get<std::map<std::string, json>>(value_);
            bool first = true;
            for (const auto& [k, v] : obj) {
                if (!first) result += ",";
                result += "\"" + k + "\":" + v.dump(indent);
                first = false;
            }
            return result + "}";
        }
        return "";
    }
    
private:
    json_value value_;
};

} // namespace nlohmann
