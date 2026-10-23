// Реализация парсера формул, вычислителя и решателя Ньютона-Рафсона
#include "bendcore/formula.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <sstream>

namespace bendcore::formula {

// ---------- Tokenizer implementation ----------

using TokenType = FormulaParser::Tokenizer::TokenType;
using Token = FormulaParser::Tokenizer::Token;

void FormulaParser::Tokenizer::skip_whitespace() {
    while (pos_ < text_.size() && std::isspace(text_[pos_])) {
        ++pos_;
    }
}

bool FormulaParser::Tokenizer::is_digit(char c) const {
    return c >= '0' && c <= '9';
}

bool FormulaParser::Tokenizer::is_alpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool FormulaParser::Tokenizer::is_alnum(char c) const {
    return is_alpha(c) || is_digit(c);
}

Token FormulaParser::Tokenizer::peek_token() {
    if (peeked_) {
        return *peeked_;
    }
    peeked_ = next_token();
    return *peeked_;
}

Token FormulaParser::Tokenizer::next_token() {
    if (peeked_) {
        Token result = *peeked_;
        peeked_ = std::nullopt;
        return result;
    }

    skip_whitespace();

    if (pos_ >= text_.size()) {
        return {TokenType::Eof, "", pos_};
    }

    size_t start_pos = pos_;
    char c = text_[pos_];

    // Number
    if (is_digit(c) || (c == '.' && pos_ + 1 < text_.size() && is_digit(text_[pos_ + 1]))) {
        while (pos_ < text_.size() && (is_digit(text_[pos_]) || text_[pos_] == '.')) {
            ++pos_;
        }
        return {TokenType::Number, text_.substr(start_pos, pos_ - start_pos), start_pos};
    }

    // Identifier or keyword
    if (is_alpha(c)) {
        while (pos_ < text_.size() && is_alnum(text_[pos_])) {
            ++pos_;
        }
        return {TokenType::Identifier, text_.substr(start_pos, pos_ - start_pos), start_pos};
    }

    // Single character tokens
    ++pos_;
    switch (c) {
        case '+':
            return {TokenType::Plus, "+", start_pos};
        case '-':
            return {TokenType::Minus, "-", start_pos};
        case '*':
            return {TokenType::Star, "*", start_pos};
        case '/':
            return {TokenType::Slash, "/", start_pos};
        case '^':
            return {TokenType::Caret, "^", start_pos};
        case '(':
            return {TokenType::LParen, "(", start_pos};
        case ')':
            return {TokenType::RParen, ")", start_pos};
        case ',':
            return {TokenType::Comma, ",", start_pos};
        case '$':
            return {TokenType::Dollar, "$", start_pos};
        case '=':
            return {TokenType::Equal, "=", start_pos};
        case '.':
            return {TokenType::Dot, ".", start_pos};
        default:
            throw std::invalid_argument(std::string("Unknown character: ") + c);
    }
}

// ---------- FormulaParser implementation ----------

ASTNodePtr FormulaParser::parse(const std::string& formula_text) {
    FormulaParser parser(formula_text);
    auto result = parser.parse_expression();
    if (!parser.check(TokenType::Eof)) {
        throw std::invalid_argument("Unexpected tokens at end of formula");
    }
    return result;
}

void FormulaParser::advance() {
    current_token_ = tokenizer_.next_token();
}

bool FormulaParser::check(TokenType type) const {
    return current_token_.type == type;
}

bool FormulaParser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

void FormulaParser::expect(TokenType type) {
    if (!check(type)) {
        throw std::invalid_argument(
            std::string("Expected token type ") + std::to_string(static_cast<int>(type)) +
            ", got " + std::to_string(static_cast<int>(current_token_.type))
        );
    }
    advance();
}

ASTNodePtr FormulaParser::parse_expression() {
    return parse_additive();
}

ASTNodePtr FormulaParser::parse_additive() {
    auto left = parse_multiplicative();

    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        BinaryOp op;
        if (match(TokenType::Plus)) {
            op = BinaryOp::Add;
        } else {
            match(TokenType::Minus);
            op = BinaryOp::Sub;
        }
        auto right = parse_multiplicative();
        left = ASTNode::make_binary(op, left, right);
    }

    return left;
}

ASTNodePtr FormulaParser::parse_multiplicative() {
    auto left = parse_power();

    while (check(TokenType::Star) || check(TokenType::Slash)) {
        BinaryOp op;
        if (match(TokenType::Star)) {
            op = BinaryOp::Mul;
        } else {
            match(TokenType::Slash);
            op = BinaryOp::Div;
        }
        auto right = parse_power();
        left = ASTNode::make_binary(op, left, right);
    }

    return left;
}

ASTNodePtr FormulaParser::parse_power() {
    auto left = parse_unary();

    if (check(TokenType::Caret)) {
        match(TokenType::Caret);
        auto right = parse_power();  // Right associative
        left = ASTNode::make_binary(BinaryOp::Pow, left, right);
    }

    return left;
}

ASTNodePtr FormulaParser::parse_unary() {
    if (match(TokenType::Minus)) {
        auto operand = parse_unary();
        return ASTNode::make_unary(UnaryOp::Neg, operand);
    }

    if (match(TokenType::Plus)) {
        return parse_unary();
    }

    return parse_postfix();
}

ASTNodePtr FormulaParser::parse_postfix() {
    return parse_primary();
}

ASTNodePtr FormulaParser::parse_primary() {
    // Spec reference: =BOM.B5
    if (check(TokenType::Equal)) {
        match(TokenType::Equal);
        return parse_spec_reference();
    }

    // Variable: $OD, $Wall, etc.
    if (match(TokenType::Dollar)) {
        expect(TokenType::Identifier);
        std::string var_name = "$" + current_token_.value;
        advance();
        return ASTNode::make_variable(var_name);
    }

    // Number
    if (check(TokenType::Number)) {
        double value = std::stod(current_token_.value);
        advance();
        return ASTNode::make_number(value);
    }

    // Function call or variable
    if (check(TokenType::Identifier)) {
        std::string name = current_token_.value;
        advance();

        // Check for function call
        if (check(TokenType::LParen)) {
            return parse_function_call(name);
        }

        // Otherwise, it's a variable without $ prefix
        return ASTNode::make_variable(name);
    }

    // Parenthesized expression
    if (match(TokenType::LParen)) {
        auto expr = parse_expression();
        expect(TokenType::RParen);
        return expr;
    }

    throw std::invalid_argument("Unexpected token in primary expression");
}

ASTNodePtr FormulaParser::parse_spec_reference() {
    // Parse table name
    expect(TokenType::Identifier);
    std::string table_name = current_token_.value;
    advance();

    expect(TokenType::Dot);

    // Parse cell address (e.g., B5)
    expect(TokenType::Identifier);
    std::string address = current_token_.value;
    advance();

    if (address.empty() || !std::isalpha(address[0])) {
        throw std::invalid_argument("Invalid specification cell address");
    }

    char column = std::toupper(address[0]);
    int row = 0;

    try {
        row = std::stoi(address.substr(1));
    } catch (...) {
        throw std::invalid_argument("Invalid specification cell row number");
    }

    return ASTNode::make_spec_reference(table_name, column, row);
}

ASTNodePtr FormulaParser::parse_function_call(const std::string& func_name) {
    expect(TokenType::LParen);

    std::vector<ASTNodePtr> args;
    if (!check(TokenType::RParen)) {
        args.push_back(parse_expression());
        while (match(TokenType::Comma)) {
            args.push_back(parse_expression());
        }
    }

    expect(TokenType::RParen);
    return ASTNode::make_function(func_name, args);
}

// ---------- SpecificationReference helpers ----------

std::string SpecificationReference::address() const {
    return std::string(1, column) + std::to_string(row);
}

SpecificationReference parse_spec_address(const std::string& address) {
    if (address.empty() || !std::isalpha(address[0])) {
        throw std::invalid_argument("Invalid specification cell address");
    }

    SpecificationReference ref;
    ref.column = std::toupper(address[0]);

    try {
        ref.row = std::stoi(address.substr(1));
    } catch (...) {
        throw std::invalid_argument("Invalid specification cell row number");
    }

    return ref;
}

// ---------- FormulaEvaluator implementation ----------

double FormulaEvaluator::evaluate(const ASTNodePtr& node, EvaluationContext& context) {
    return eval_node(node, context);
}

double FormulaEvaluator::evaluate_formula(const std::string& formula_text,
                                        EvaluationContext& context) {
    try {
        auto ast = FormulaParser::parse(formula_text);
        return evaluate(ast, context);
    } catch (const std::exception& e) {
        context.last_error = e.what();
        throw;
    }
}

double FormulaEvaluator::eval_node(const ASTNodePtr& node, EvaluationContext& context) {
    if (!node) {
        throw std::logic_error("Null AST node");
    }

    switch (node->type) {
        case ASTNode::Number:
            return node->number_value;

        case ASTNode::Variable: {
            auto it = context.variables.find(node->variable_name);
            if (it == context.variables.end()) {
                throw std::invalid_argument(
                    "Undefined variable: " + node->variable_name
                );
            }
            return it->second;
        }

        case ASTNode::UnaryOpNode:
            return eval_unary(node->unary_op, eval_node(node->unary_operand, context));

        case ASTNode::BinaryOpNode: {
            double left = eval_node(node->left_operand, context);
            double right = eval_node(node->right_operand, context);
            return eval_binary(node->binary_op, left, right);
        }

        case ASTNode::FunctionCall: {
            std::vector<double> args;
            for (const auto& arg : node->function_args) {
                args.push_back(eval_node(arg, context));
            }

            const auto& fname = node->function_name;
            if (fname == "sqrt" && args.size() == 1) {
                if (args[0] < 0) {
                    throw std::invalid_argument("sqrt of negative number");
                }
                return std::sqrt(args[0]);
            } else if (fname == "abs" && args.size() == 1) {
                return std::abs(args[0]);
            } else if (fname == "sin" && args.size() == 1) {
                return std::sin(args[0] * M_PI / 180.0);  // Convert from degrees
            } else if (fname == "cos" && args.size() == 1) {
                return std::cos(args[0] * M_PI / 180.0);
            } else if (fname == "tan" && args.size() == 1) {
                return std::tan(args[0] * M_PI / 180.0);
            } else if (fname == "min" && args.size() >= 2) {
                return *std::min_element(args.begin(), args.end());
            } else if (fname == "max" && args.size() >= 2) {
                return *std::max_element(args.begin(), args.end());
            } else if (fname == "floor" && args.size() == 1) {
                return std::floor(args[0]);
            } else if (fname == "ceil" && args.size() == 1) {
                return std::ceil(args[0]);
            } else if (fname == "round" && args.size() == 1) {
                return std::round(args[0]);
            } else {
                throw std::invalid_argument("Unknown function: " + fname);
            }
        }

        case ASTNode::SpecReference: {
            const auto& table_name = node->spec_table_name;
            auto table_it = context.specification_tables.find(table_name);
            if (table_it == context.specification_tables.end()) {
                throw std::invalid_argument(
                    "Unknown specification table: " + table_name
                );
            }

            const auto& table = table_it->second;
            auto row_it = table.find(node->spec_row);
            if (row_it == table.end()) {
                throw std::invalid_argument(
                    "Row " + std::to_string(node->spec_row) + " not found in table " + table_name
                );
            }

            const auto& row_data = row_it->second;
            auto col_it = row_data.find(node->spec_column);
            if (col_it == row_data.end()) {
                throw std::invalid_argument(
                    "Cell " + table_name + "." + node->spec_column +
                    std::to_string(node->spec_row) + " not found"
                );
            }

            return col_it->second;
        }

        default:
            throw std::logic_error("Unknown AST node type");
    }
}

double FormulaEvaluator::eval_unary(UnaryOp op, double operand) {
    switch (op) {
        case UnaryOp::Neg:
            return -operand;
        case UnaryOp::Abs:
            return std::abs(operand);
        case UnaryOp::Sqrt:
            if (operand < 0) {
                throw std::invalid_argument("sqrt of negative number");
            }
            return std::sqrt(operand);
        case UnaryOp::Sin:
            return std::sin(operand * M_PI / 180.0);
        case UnaryOp::Cos:
            return std::cos(operand * M_PI / 180.0);
        case UnaryOp::Tan:
            return std::tan(operand * M_PI / 180.0);
        case UnaryOp::Floor:
            return std::floor(operand);
        case UnaryOp::Ceil:
            return std::ceil(operand);
        default:
            throw std::logic_error("Unknown unary operator");
    }
}

double FormulaEvaluator::eval_binary(BinaryOp op, double left, double right) {
    switch (op) {
        case BinaryOp::Add:
            return left + right;
        case BinaryOp::Sub:
            return left - right;
        case BinaryOp::Mul:
            return left * right;
        case BinaryOp::Div:
            if (right == 0) {
                throw std::invalid_argument("Division by zero");
            }
            return left / right;
        case BinaryOp::Pow:
            return std::pow(left, right);
        case BinaryOp::Min:
            return std::min(left, right);
        case BinaryOp::Max:
            return std::max(left, right);
        default:
            throw std::logic_error("Unknown binary operator");
    }
}

bool FormulaEvaluator::has_circular_reference(const ASTNodePtr& node,
                                             const std::vector<SpecificationReference>& already_evaluating) {
    if (!node) {
        return false;
    }

    switch (node->type) {
        case ASTNode::SpecReference: {
            SpecificationReference ref;
            ref.table_name = node->spec_table_name;
            ref.column = node->spec_column;
            ref.row = node->spec_row;

            for (const auto& evaluating : already_evaluating) {
                if (evaluating.table_name == ref.table_name &&
                    evaluating.column == ref.column &&
                    evaluating.row == ref.row) {
                    return true;
                }
            }
            return false;
        }

        case ASTNode::UnaryOpNode:
            return has_circular_reference(node->unary_operand, already_evaluating);

        case ASTNode::BinaryOpNode:
            return has_circular_reference(node->left_operand, already_evaluating) ||
                   has_circular_reference(node->right_operand, already_evaluating);

        case ASTNode::FunctionCall:
            for (const auto& arg : node->function_args) {
                if (has_circular_reference(arg, already_evaluating)) {
                    return true;
                }
            }
            return false;

        default:
            return false;
    }
}

// ---------- NewtonRaphsonSolver implementation ----------

NewtonRaphsonSolver::SolverResult NewtonRaphsonSolver::solve(
    const std::function<double(double)>& equation,
    const SolverOptions& options,
    double target) {

    SolverResult result;
    result.value = options.initial_guess;
    double x = options.initial_guess;

    for (int i = 0; i < options.max_iterations; ++i) {
        double fx = equation(x) - target;
        double fp = (equation(x + options.step_size) - equation(x)) / options.step_size;

        if (std::abs(fp) < 1e-12) {
            result.error = "Derivative is too small";
            return result;
        }

        double x_new = x - fx / fp;
        double delta = std::abs(x_new - x);

        if (delta < options.tolerance) {
            result.value = x_new;
            result.converged = true;
            result.iterations = i + 1;
            return result;
        }

        x = x_new;
    }

    result.error = "Did not converge after " + std::to_string(options.max_iterations) + " iterations";
    result.value = x;
    return result;
}

// ---------- DimensionFormula implementation ----------

void DimensionFormula::evaluate(EvaluationContext& context) {
    if (!formula_ast) {
        is_valid = false;
        error_message = "Formula not parsed";
        return;
    }

    try {
        // Update context with current variables
        for (const auto& [name, value] : variables_map) {
            context.variables[name] = value;
        }

        result = FormulaEvaluator::evaluate(formula_ast, context);
        is_valid = true;
        error_message.clear();
    } catch (const std::exception& e) {
        is_valid = false;
        error_message = e.what();
    }
}

void DimensionFormula::set_formula(const std::string& text) {
    formula_text = text;
    try {
        formula_ast = FormulaParser::parse(text);
        is_valid = true;
        error_message.clear();
    } catch (const std::exception& e) {
        is_valid = false;
        error_message = e.what();
        formula_ast = nullptr;
    }
}

void DimensionFormula::set_variable(const std::string& name, double value) {
    variables_map[name] = value;
}

std::vector<std::string> DimensionFormula::get_variables() const {
    std::vector<std::string> result_vars;

    if (!formula_ast) {
        return result_vars;
    }

    // Use a stack-based approach for traversal
    std::vector<ASTNodePtr> work_stack;
    work_stack.push_back(formula_ast);

    while (!work_stack.empty()) {
        ASTNodePtr current = work_stack.back();
        work_stack.pop_back();

        if (!current) {
            continue;
        }

        switch (current->type) {
            case ASTNode::Variable: {
                auto& var_name = current->variable_name;
                if (std::find(result_vars.begin(), result_vars.end(), var_name) == result_vars.end()) {
                    result_vars.push_back(var_name);
                }
                break;
            }

            case ASTNode::UnaryOpNode: {
                if (current->unary_operand) {
                    work_stack.push_back(current->unary_operand);
                }
                break;
            }

            case ASTNode::BinaryOpNode: {
                if (current->left_operand) {
                    work_stack.push_back(current->left_operand);
                }
                if (current->right_operand) {
                    work_stack.push_back(current->right_operand);
                }
                break;
            }

            case ASTNode::FunctionCall: {
                for (const auto& arg : current->function_args) {
                    if (arg) {
                        work_stack.push_back(arg);
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    return result_vars;
}

// ---------- JSON serialization (basic implementation) ----------

std::string dimension_formula_to_json(const DimensionFormula& dim_formula) {
    std::ostringstream oss;
    oss << "{"
        << "\"id\":\"" << dim_formula.id << "\","
        << "\"formula_text\":\"" << dim_formula.formula_text << "\","
        << "\"result\":" << dim_formula.result << ","
        << "\"is_valid\":" << (dim_formula.is_valid ? "true" : "false") << ","
        << "\"author\":\"" << dim_formula.author << "\","
        << "\"created_at\":\"" << dim_formula.created_at << "\","
        << "\"modified_at\":\"" << dim_formula.modified_at << "\""
        << "}";
    return oss.str();
}

DimensionFormula dimension_formula_from_json(const std::string& json_text) {
    DimensionFormula dim_formula;
    // TODO: Implement proper JSON parsing
    // For now, this is a placeholder
    return dim_formula;
}

}  // namespace bendcore::formula
