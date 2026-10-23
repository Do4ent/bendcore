/**
 * BendCore Formula Engine - Q3 2027 Task 1
 * Parametric Dimensions with Formula Support
 *
 * Newton-Raphson solver for parametric constraints
 * Supports: +, -, *, /, ^, sqrt(), abs(), min(), max()
 * Variables: $OD, $Wall, $Length, $BendAngle, $BendRadius
 * Specifications: =BOM.B5, =Spec.C3
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cmath>
#include <nlohmann/json.hpp>

namespace bendcore {

using json = nlohmann::json;

// ============================================================================
// AST Nodes for Formula Parsing
// ============================================================================

enum class TokenType {
    NUMBER,
    VARIABLE,
    PLUS, MINUS, MULTIPLY, DIVIDE, POWER,
    LPAREN, RPAREN,
    FUNCTION,
    EOF_TOKEN
};

struct Token {
    TokenType type;
    std::string value;
    int position = 0;
};

// AST Node types
enum class NodeType {
    LITERAL,
    VARIABLE,
    BINARY_OP,
    UNARY_OP,
    FUNCTION_CALL
};

struct ASTNode {
    NodeType type;

    // For literals
    double literal_value = 0.0;

    // For variables
    std::string var_name;

    // For binary ops: +, -, *, /, ^
    char binary_op = '\0';
    std::shared_ptr<ASTNode> left;
    std::shared_ptr<ASTNode> right;

    // For functions: sqrt, abs, min, max
    std::string func_name;
    std::vector<std::shared_ptr<ASTNode>> args;
};

// ============================================================================
// Formula Parser
// ============================================================================

class FormulaParser {
public:
    /**
     * Parse formula string into AST
     * @param formula Formula text: "$OD * 2 + 5"
     * @return Root AST node
     * @throws std::runtime_error if syntax error
     */
    std::shared_ptr<ASTNode> parse(const std::string& formula);

    /**
     * Tokenize formula string
     */
    std::vector<Token> tokenize(const std::string& formula);

private:
    std::shared_ptr<ASTNode> parse_expression();
    std::shared_ptr<ASTNode> parse_term();
    std::shared_ptr<ASTNode> parse_factor();
    std::shared_ptr<ASTNode> parse_primary();

    size_t current_pos_ = 0;
    std::vector<Token> tokens_;

    Token current_token() const;
    Token peek() const;
    void advance();
    void expect(TokenType type);
};

// ============================================================================
// Formula Evaluator with Newton-Raphson Solver
// ============================================================================

class FormulaEvaluator {
public:
    /**
     * Evaluate formula with given variables
     * @param ast AST root node
     * @param variables Variable map ($OD, $Wall, etc.)
     * @return Computed result
     */
    double evaluate(
        const std::shared_ptr<ASTNode>& ast,
        const std::map<std::string, double>& variables);

    /**
     * Solve parametric constraint using Newton-Raphson
     * @param formula Formula text (may contain circular reference)
     * @param variables Initial values
     * @param max_iterations Convergence limit (default 100)
     * @param tolerance Convergence threshold (default 0.001mm)
     * @return Solved result or error
     */
    struct SolveResult {
        bool success = false;
        double result = 0.0;
        int iterations = 0;
        double final_error = 0.0;
        std::string error_message;
    };

    SolveResult solve_newton_raphson(
        const std::string& formula,
        std::map<std::string, double>& variables,
        int max_iterations = 100,
        double tolerance = 0.001);

    /**
     * Detect circular references
     * @param formula Formula text
     * @param variables Known variables
     * @return true if circular reference detected
     */
    bool has_circular_reference(
        const std::string& formula,
        const std::map<std::string, double>& variables);

private:
    double eval_node(
        const std::shared_ptr<ASTNode>& node,
        const std::map<std::string, double>& variables);

    double numeric_derivative(
        const std::string& formula,
        const std::map<std::string, double>& variables,
        double h = 1e-6);
};

// ============================================================================
// Dimension Formula Data Structure
// ============================================================================

struct DimensionFormula {
    std::string id;                                    // "dim_001"
    std::string formula_text;                          // "$OD * 2 + 5"
    std::map<std::string, double> variables_map;       // {OD: 25, Wall: 2.5}
    double result = 0.0;                               // Calculated value
    std::string author;                                // "user@example.com"
    std::string created_at;                            // ISO 8601
    std::map<std::string, double> cached_results;      // Performance cache

    // Serialization
    json to_json() const;
    static DimensionFormula from_json(const json& j);
};

// ============================================================================
// Formula Manager (Dimension Factory)
// ============================================================================

class DimensionFormulaManager {
public:
    /**
     * Create new parametric dimension
     */
    DimensionFormula create_formula(
        const std::string& id,
        const std::string& formula_text,
        const std::string& author);

    /**
     * Update formula
     */
    void update_formula(
        DimensionFormula& formula,
        const std::string& new_formula);

    /**
     * Evaluate formula with current variables
     */
    double evaluate_formula(
        const DimensionFormula& formula,
        const std::map<std::string, double>& overrides = {});

    /**
     * Auto-recalculate all dependent formulas
     */
    void recalculate_all(
        std::vector<DimensionFormula>& formulas,
        const std::map<std::string, double>& updated_variables);

private:
    FormulaParser parser_;
    FormulaEvaluator evaluator_;
};

}  // namespace bendcore
