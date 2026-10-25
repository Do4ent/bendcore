/**
 * BendCore Formula API Endpoint - Q3 2027 Task 1 Phase 2
 * REST API for formula evaluation
 * Integrates with React FormulaEditor
 *
 * Endpoint: POST /api/bendcore/evaluate-formula
 * Request: { formula: string, variables: Record<string, number> }
 * Response: { result: number } | { message: string, suggestions: string[] }
 */

#include "bendcore/formula_engine.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <map>

using json = nlohmann::json;
using namespace bendcore;

/**
 * Format available variables for error message
 */
std::string format_available_variables(const std::map<std::string, double>& variables);

/**
 * Evaluate formula from API request
 *
 * @param request_body JSON with formula and variables
 * @return JSON response with result or error
 */
json evaluate_formula_api(const json& request_body) {
    json response;

    try {
        // Extract formula and variables from request
        if (!request_body.contains("formula") || !request_body.is_object()) {
            response["success"] = false;
            response["message"] = "Missing 'formula' field";
            response["suggestions"] = {
                "Formula field is required",
                "Format: { formula: '$OD * 2', variables: { '$OD': 25 } }"
            };
            return response;
        }

        std::string formula = request_body.at("formula").get<std::string>();

        // Get variables (optional)
        std::map<std::string, double> variables;
        if (request_body.contains("variables") && request_body["variables"].is_object()) {
            for (const auto& [key, value] : request_body["variables"].items()) {
                if (value.is_number()) {
                    variables[key] = value.get<double>();
                }
            }
        }

        // Validate formula is not empty
        if (formula.empty()) {
            response["success"] = false;
            response["message"] = "Formula cannot be empty";
            return response;
        }

        // Parse and evaluate formula
        FormulaParser parser;
        FormulaEvaluator evaluator;

        try {
            auto ast = parser.parse(formula);
            double result = evaluator.evaluate(ast, variables);

            response["success"] = true;
            response["result"] = result;
        }
        catch (const std::runtime_error& e) {
            std::string error_msg = e.what();

            response["success"] = false;
            response["message"] = error_msg;

            // Provide context-specific suggestions
            response["suggestions"] = json::array();

            if (error_msg.find("Division by zero") != std::string::npos) {
                response["suggestions"].push_back("Check for division by zero");
                response["suggestions"].push_back("Ensure denominator is not zero");
            }
            else if (error_msg.find("sqrt of negative") != std::string::npos) {
                response["suggestions"].push_back("sqrt() only works with non-negative numbers");
                response["suggestions"].push_back("Consider using abs() to ensure positive input");
            }
            else if (error_msg.find("Undefined variable") != std::string::npos) {
                response["suggestions"].push_back("Check variable names (use $OD, $Wall, etc.)");
                response["suggestions"].push_back("Available variables: " + format_available_variables(variables));
            }
            else if (error_msg.find("Unexpected token") != std::string::npos) {
                response["suggestions"].push_back("Check formula syntax");
                response["suggestions"].push_back("Example: $OD * 2 + 5");
                response["suggestions"].push_back("Operators: +, -, *, /, ^");
            }
            else {
                response["suggestions"].push_back("Verify formula syntax");
                response["suggestions"].push_back("Check all variable names are defined");
            }
        }
    }
    catch (const std::exception& e) {
        response["success"] = false;
        response["message"] = std::string("API Error: ") + e.what();
        response["suggestions"] = {
            "Check request format",
            "Ensure JSON is valid"
        };
    }

    return response;
}

/**
 * Format available variables for error message
 */
std::string format_available_variables(const std::map<std::string, double>& variables) {
    if (variables.empty()) {
        return "(none provided)";
    }

    std::string result = "[";
    bool first = true;
    for (const auto& [key, value] : variables) {
        if (!first) result += ", ";
        result += key;
        first = false;
    }
    result += "]";
    return result;
}

/**
 * Validate formula syntax without evaluation
 * Useful for real-time syntax checking in UI
 *
 * @param formula_text Formula string to validate
 * @return JSON with validation status and error details
 */
json validate_formula_api(const json& request_body) {
    json response;

    try {
        if (!request_body.contains("formula")) {
            response["valid"] = false;
            response["message"] = "Formula field required";
            return response;
        }

        std::string formula = request_body.at("formula").get<std::string>();

        if (formula.empty()) {
            response["valid"] = false;
            response["message"] = "Formula cannot be empty";
            return response;
        }

        FormulaParser parser;
        try {
            parser.parse(formula);
            response["valid"] = true;
            response["message"] = "Formula syntax is valid";
        }
        catch (const std::exception& e) {
            response["valid"] = false;
            response["message"] = e.what();
        }
    }
    catch (const std::exception& e) {
        response["valid"] = false;
        response["message"] = std::string("Validation error: ") + e.what();
    }

    return response;
}

/**
 * Get formula info (variables, functions used, etc.)
 * Useful for formula builder UI
 */
json get_formula_info_api(const json& request_body) {
    json response;

    try {
        if (!request_body.contains("formula")) {
            response["error"] = "Formula field required";
            return response;
        }

        std::string formula = request_body.at("formula").get<std::string>();

        response["formula"] = formula;
        response["length"] = formula.length();

        // Extract variables (simple regex-based approach)
        json variables = json::array();
        size_t pos = 0;
        while ((pos = formula.find('$', pos)) != std::string::npos) {
            size_t end = pos + 1;
            while (end < formula.length() &&
                   (std::isalnum(formula[end]) || formula[end] == '_')) {
                end++;
            }
            std::string var = formula.substr(pos, end - pos);
            variables.push_back(var);
            pos = end;
        }

        response["variables"] = variables;

        // Detect functions
        json functions = json::array();
        std::vector<std::string> func_names = {"sqrt", "abs", "min", "max"};
        for (const auto& func : func_names) {
            if (formula.find(func) != std::string::npos) {
                functions.push_back(func);
            }
        }
        response["functions"] = functions;

        // Count operators
        int ops = 0;
        for (char c : formula) {
            if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^') {
                ops++;
            }
        }
        response["operator_count"] = ops;

        response["success"] = true;
    }
    catch (const std::exception& e) {
        response["success"] = false;
        response["error"] = e.what();
    }

    return response;
}
