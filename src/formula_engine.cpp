/**
 * BendCore Formula Engine Implementation - Q3 2027 Task 1
 * Parametric Dimensions with Newton-Raphson Solver
 */

#include "bendcore/formula_engine.hpp"
#include <cctype>
#include <cmath>
#include <sstream>
#include <algorithm>

namespace bendcore {

// ============================================================================
// FormulaParser Implementation
// ============================================================================

std::vector<Token> FormulaParser::tokenize(const std::string& formula) {
    std::vector<Token> tokens;
    size_t i = 0;

    while (i < formula.length()) {
        // Skip whitespace
        if (std::isspace(formula[i])) {
            i++;
            continue;
        }

        Token token;
        token.position = i;

        // Numbers
        if (std::isdigit(formula[i]) || formula[i] == '.') {
            size_t start = i;
            while (i < formula.length() &&
                   (std::isdigit(formula[i]) || formula[i] == '.')) {
                i++;
            }
            token.type = TokenType::NUMBER;
            token.value = formula.substr(start, i - start);
            tokens.push_back(token);
        }
        // Variables ($OD, $Wall, etc.)
        else if (formula[i] == '$') {
            size_t start = i;
            i++;
            while (i < formula.length() &&
                   (std::isalnum(formula[i]) || formula[i] == '_')) {
                i++;
            }
            token.type = TokenType::VARIABLE;
            token.value = formula.substr(start, i - start);
            tokens.push_back(token);
        }
        // Functions and identifiers
        else if (std::isalpha(formula[i])) {
            size_t start = i;
            while (i < formula.length() && std::isalnum(formula[i])) {
                i++;
            }
            std::string word = formula.substr(start, i - start);

            if (word == "sqrt" || word == "abs" || word == "min" || word == "max") {
                token.type = TokenType::FUNCTION;
            } else {
                token.type = TokenType::VARIABLE;
            }
            token.value = word;
            tokens.push_back(token);
        }
        // Operators
        else if (formula[i] == '+') {
            token.type = TokenType::PLUS;
            token.value = "+";
            i++;
            tokens.push_back(token);
        }
        else if (formula[i] == '-') {
            token.type = TokenType::MINUS;
            token.value = "-";
            i++;
            tokens.push_back(token);
        }
        else if (formula[i] == '*') {
            token.type = TokenType::MULTIPLY;
            token.value = "*";
            i++;
            tokens.push_back(token);
        }
        else if (formula[i] == '/') {
            token.type = TokenType::DIVIDE;
            token.value = "/";
            i++;
            tokens.push_back(token);
        }
        else if (formula[i] == '^') {
            token.type = TokenType::POWER;
            token.value = "^";
            i++;
            tokens.push_back(token);
        }
        else if (formula[i] == '(') {
            token.type = TokenType::LPAREN;
            token.value = "(";
            i++;
            tokens.push_back(token);
        }
        else if (formula[i] == ')') {
            token.type = TokenType::RPAREN;
            token.value = ")";
            i++;
            tokens.push_back(token);
        }
        else if (formula[i] == ',') {
            i++;
            continue;
        }
        else {
            throw std::runtime_error(
                std::string("Unknown character at position ") +
                std::to_string(i) + ": " + formula[i]);
        }
    }

    Token eof_token;
    eof_token.type = TokenType::EOF_TOKEN;
    tokens.push_back(eof_token);

    return tokens;
}

std::shared_ptr<ASTNode> FormulaParser::parse(const std::string& formula) {
    tokens_ = tokenize(formula);
    current_pos_ = 0;
    return parse_expression();
}

std::shared_ptr<ASTNode> FormulaParser::parse_expression() {
    auto node = parse_term();

    while (current_token().type == TokenType::PLUS ||
           current_token().type == TokenType::MINUS) {
        auto op_node = std::make_shared<ASTNode>();
        op_node->type = NodeType::BINARY_OP;
        op_node->binary_op = current_token().value[0];
        op_node->left = node;

        advance();
        op_node->right = parse_term();
        node = op_node;
    }

    return node;
}

std::shared_ptr<ASTNode> FormulaParser::parse_term() {
    auto node = parse_factor();

    while (current_token().type == TokenType::MULTIPLY ||
           current_token().type == TokenType::DIVIDE) {
        auto op_node = std::make_shared<ASTNode>();
        op_node->type = NodeType::BINARY_OP;
        op_node->binary_op = current_token().value[0];
        op_node->left = node;

        advance();
        op_node->right = parse_factor();
        node = op_node;
    }

    return node;
}

std::shared_ptr<ASTNode> FormulaParser::parse_factor() {
    auto node = parse_primary();

    if (current_token().type == TokenType::POWER) {
        auto op_node = std::make_shared<ASTNode>();
        op_node->type = NodeType::BINARY_OP;
        op_node->binary_op = '^';
        op_node->left = node;

        advance();
        op_node->right = parse_factor();  // Right associative
        node = op_node;
    }

    return node;
}

std::shared_ptr<ASTNode> FormulaParser::parse_primary() {
    auto token = current_token();

    // Number literal
    if (token.type == TokenType::NUMBER) {
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::LITERAL;
        node->literal_value = std::stod(token.value);
        advance();
        return node;
    }

    // Variable
    if (token.type == TokenType::VARIABLE) {
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::VARIABLE;
        node->var_name = token.value;
        advance();
        return node;
    }

    // Function call
    if (token.type == TokenType::FUNCTION) {
        auto node = std::make_shared<ASTNode>();
        node->type = NodeType::FUNCTION_CALL;
        node->func_name = token.value;
        advance();

        expect(TokenType::LPAREN);
        advance();

        while (current_token().type != TokenType::RPAREN) {
            node->args.push_back(parse_expression());
            if (current_token().type == TokenType::RPAREN) break;
            advance();  // Skip comma or next element
        }

        expect(TokenType::RPAREN);
        advance();

        return node;
    }

    // Parenthesized expression
    if (token.type == TokenType::LPAREN) {
        advance();
        auto node = parse_expression();
        expect(TokenType::RPAREN);
        advance();
        return node;
    }

    throw std::runtime_error("Unexpected token: " + token.value);
}

Token FormulaParser::current_token() const {
    if (current_pos_ < tokens_.size()) {
        return tokens_[current_pos_];
    }
    return tokens_.back();  // EOF token
}

Token FormulaParser::peek() const {
    if (current_pos_ + 1 < tokens_.size()) {
        return tokens_[current_pos_ + 1];
    }
    return tokens_.back();
}

void FormulaParser::advance() {
    if (current_pos_ < tokens_.size()) {
        current_pos_++;
    }
}

void FormulaParser::expect(TokenType type) {
    if (current_token().type != type) {
        throw std::runtime_error(
            "Expected token type " + std::to_string(static_cast<int>(type)));
    }
}

// ============================================================================
// FormulaEvaluator Implementation
// ============================================================================

double FormulaEvaluator::evaluate(
    const std::shared_ptr<ASTNode>& ast,
    const std::map<std::string, double>& variables) {
    return eval_node(ast, variables);
}

double FormulaEvaluator::eval_node(
    const std::shared_ptr<ASTNode>& node,
    const std::map<std::string, double>& variables) {

    if (!node) {
        throw std::runtime_error("Null AST node");
    }

    switch (node->type) {
        case NodeType::LITERAL:
            return node->literal_value;

        case NodeType::VARIABLE: {
            auto it = variables.find(node->var_name);
            if (it == variables.end()) {
                throw std::runtime_error(
                    "Undefined variable: " + node->var_name);
            }
            return it->second;
        }

        case NodeType::BINARY_OP: {
            double left = eval_node(node->left, variables);
            double right = eval_node(node->right, variables);

            switch (node->binary_op) {
                case '+': return left + right;
                case '-': return left - right;
                case '*': return left * right;
                case '/': {
                    if (std::abs(right) < 1e-10) {
                        throw std::runtime_error("Division by zero");
                    }
                    return left / right;
                }
                case '^': return std::pow(left, right);
                default:
                    throw std::runtime_error("Unknown binary operator");
            }
        }

        case NodeType::FUNCTION_CALL: {
            if (node->func_name == "sqrt") {
                if (node->args.size() != 1) {
                    throw std::runtime_error("sqrt requires 1 argument");
                }
                double val = eval_node(node->args[0], variables);
                if (val < 0) {
                    throw std::runtime_error("sqrt of negative number");
                }
                return std::sqrt(val);
            }
            else if (node->func_name == "abs") {
                if (node->args.size() != 1) {
                    throw std::runtime_error("abs requires 1 argument");
                }
                return std::abs(eval_node(node->args[0], variables));
            }
            else if (node->func_name == "min") {
                if (node->args.size() < 2) {
                    throw std::runtime_error("min requires at least 2 arguments");
                }
                double result = eval_node(node->args[0], variables);
                for (size_t i = 1; i < node->args.size(); i++) {
                    result = std::min(result,
                                    eval_node(node->args[i], variables));
                }
                return result;
            }
            else if (node->func_name == "max") {
                if (node->args.size() < 2) {
                    throw std::runtime_error("max requires at least 2 arguments");
                }
                double result = eval_node(node->args[0], variables);
                for (size_t i = 1; i < node->args.size(); i++) {
                    result = std::max(result,
                                    eval_node(node->args[i], variables));
                }
                return result;
            }
            else {
                throw std::runtime_error(
                    "Unknown function: " + node->func_name);
            }
        }

        default:
            throw std::runtime_error("Unknown AST node type");
    }
}

FormulaEvaluator::SolveResult FormulaEvaluator::solve_newton_raphson(
    const std::string& formula,
    std::map<std::string, double>& variables,
    int max_iterations,
    double tolerance) {

    SolveResult result;

    try {
        FormulaParser parser;
        auto ast = parser.parse(formula);

        double x = 0.0;
        for (int i = 0; i < max_iterations; i++) {
            double f_x = eval_node(ast, variables);
            double df_x = numeric_derivative(formula, variables);

            if (std::abs(df_x) < 1e-10) {
                result.error_message = "Derivative too small (singular point)";
                return result;
            }

            double x_next = x - f_x / df_x;
            double error = std::abs(x_next - x);

            x = x_next;
            result.final_error = error;
            result.iterations = i + 1;

            if (error < tolerance) {
                result.success = true;
                result.result = x;
                return result;
            }
        }

        result.error_message = "Max iterations reached";
        result.result = x;
    }
    catch (const std::exception& e) {
        result.error_message = e.what();
    }

    return result;
}

double FormulaEvaluator::numeric_derivative(
    const std::string& formula,
    const std::map<std::string, double>& variables,
    double h) {

    FormulaParser parser;
    auto ast = parser.parse(formula);

    double f_x = eval_node(ast, variables);

    auto vars_plus = variables;
    // Approximate by perturbing first variable
    if (!variables.empty()) {
        vars_plus.begin()->second += h;
    }

    double f_x_plus = eval_node(ast, vars_plus);

    return (f_x_plus - f_x) / h;
}

bool FormulaEvaluator::has_circular_reference(
    const std::string& formula,
    const std::map<std::string, double>& variables) {

    // Simple heuristic: if formula references undefined variables,
    // could be circular
    try {
        FormulaParser parser;
        auto ast = parser.parse(formula);
        evaluate(ast, variables);
        return false;
    }
    catch (const std::runtime_error& e) {
        std::string msg = e.what();
        return msg.find("Undefined variable") != std::string::npos;
    }
}

// ============================================================================
// DimensionFormula Serialization
// ============================================================================

json DimensionFormula::to_json() const {
    json j;
    j["id"] = id;
    j["formula_text"] = formula_text;
    j["result"] = result;
    j["author"] = author;
    j["created_at"] = created_at;

    json vars;
    for (const auto& [k, v] : variables_map) {
        vars[k] = v;
    }
    j["variables_map"] = vars;

    return j;
}

DimensionFormula DimensionFormula::from_json(const json& j) {
    DimensionFormula df;
    df.id = j.at("id");
    df.formula_text = j.at("formula_text");
    df.result = j.at("result");
    df.author = j.at("author");
    df.created_at = j.at("created_at");

    for (const auto& [k, v] : j.at("variables_map").items()) {
        df.variables_map[k] = v;
    }

    return df;
}

// ============================================================================
// DimensionFormulaManager Implementation
// ============================================================================

DimensionFormula DimensionFormulaManager::create_formula(
    const std::string& id,
    const std::string& formula_text,
    const std::string& author) {

    DimensionFormula formula;
    formula.id = id;
    formula.formula_text = formula_text;
    formula.author = author;
    formula.created_at = "2026-10-23T21:30:00Z";  // TODO: Use actual timestamp

    return formula;
}

void DimensionFormulaManager::update_formula(
    DimensionFormula& formula,
    const std::string& new_formula) {

    // Validate new formula by parsing it
    try {
        parser_.parse(new_formula);
        formula.formula_text = new_formula;
    }
    catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Invalid formula: ") + e.what());
    }
}

double DimensionFormulaManager::evaluate_formula(
    const DimensionFormula& formula,
    const std::map<std::string, double>& overrides) {

    auto vars = formula.variables_map;
    for (const auto& [k, v] : overrides) {
        vars[k] = v;
    }

    try {
        auto ast = parser_.parse(formula.formula_text);
        return evaluator_.evaluate(ast, vars);
    }
    catch (const std::exception& e) {
        throw std::runtime_error(
            std::string("Evaluation error: ") + e.what());
    }
}

void DimensionFormulaManager::recalculate_all(
    std::vector<DimensionFormula>& formulas,
    const std::map<std::string, double>& updated_variables) {

    for (auto& formula : formulas) {
        // Update variables
        for (const auto& [k, v] : updated_variables) {
            formula.variables_map[k] = v;
        }

        // Recalculate
        try {
            formula.result = evaluate_formula(formula);
        }
        catch (const std::exception& e) {
            // Log error but continue with other formulas
        }
    }
}

}  // namespace bendcore
