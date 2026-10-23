/**
 * BendCore Formula Engine Tests - Q3 2027 Task 1
 */

#include <gtest/gtest.h>
#include "bendcore/formula_engine.hpp"

using namespace bendcore;

// ============================================================================
// Parser Tests
// ============================================================================

TEST(FormulaParserTest, ParseSimpleNumber) {
    FormulaParser parser;
    auto ast = parser.parse("42");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NodeType::LITERAL);
    EXPECT_DOUBLE_EQ(ast->literal_value, 42.0);
}

TEST(FormulaParserTest, ParseVariable) {
    FormulaParser parser;
    auto ast = parser.parse("$OD");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NodeType::VARIABLE);
    EXPECT_EQ(ast->var_name, "$OD");
}

TEST(FormulaParserTest, ParseAddition) {
    FormulaParser parser;
    auto ast = parser.parse("5 + 3");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NodeType::BINARY_OP);
    EXPECT_EQ(ast->binary_op, '+');
}

TEST(FormulaParserTest, ParseComplexExpression) {
    FormulaParser parser;
    auto ast = parser.parse("$OD * 2 + 5");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NodeType::BINARY_OP);
    EXPECT_EQ(ast->binary_op, '+');
}

TEST(FormulaParserTest, ParseFunctionCall) {
    FormulaParser parser;
    auto ast = parser.parse("sqrt(16)");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NodeType::FUNCTION_CALL);
    EXPECT_EQ(ast->func_name, "sqrt");
}

TEST(FormulaParserTest, ParsePower) {
    FormulaParser parser;
    auto ast = parser.parse("2 ^ 3");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, NodeType::BINARY_OP);
    EXPECT_EQ(ast->binary_op, '^');
}

// ============================================================================
// Evaluator Tests
// ============================================================================

TEST(FormulaEvaluatorTest, EvaluateLiteral) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("42");
    double result = evaluator.evaluate(ast, {});
    EXPECT_DOUBLE_EQ(result, 42.0);
}

TEST(FormulaEvaluatorTest, EvaluateVariable) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("$OD");
    std::map<std::string, double> vars = {{"$OD", 25.0}};
    double result = evaluator.evaluate(ast, vars);
    EXPECT_DOUBLE_EQ(result, 25.0);
}

TEST(FormulaEvaluatorTest, EvaluateAddition) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("5 + 3");
    double result = evaluator.evaluate(ast, {});
    EXPECT_DOUBLE_EQ(result, 8.0);
}

TEST(FormulaEvaluatorTest, EvaluateMultiplication) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("6 * 7");
    double result = evaluator.evaluate(ast, {});
    EXPECT_DOUBLE_EQ(result, 42.0);
}

TEST(FormulaEvaluatorTest, EvaluateComplexFormula) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("$OD * 2 + 5");
    std::map<std::string, double> vars = {{"$OD", 25.0}};
    double result = evaluator.evaluate(ast, vars);
    EXPECT_DOUBLE_EQ(result, 55.0);  // 25 * 2 + 5 = 55
}

TEST(FormulaEvaluatorTest, EvaluateSqrt) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("sqrt(16)");
    double result = evaluator.evaluate(ast, {});
    EXPECT_DOUBLE_EQ(result, 4.0);
}

TEST(FormulaEvaluatorTest, EvaluateAbs) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("abs(-5)");
    double result = evaluator.evaluate(ast, {});
    EXPECT_DOUBLE_EQ(result, 5.0);
}

TEST(FormulaEvaluatorTest, EvaluateMin) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("min(3, 7, 2)");
    double result = evaluator.evaluate(ast, {});
    EXPECT_DOUBLE_EQ(result, 2.0);
}

TEST(FormulaEvaluatorTest, EvaluateMax) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("max(3, 7, 2)");
    double result = evaluator.evaluate(ast, {});
    EXPECT_DOUBLE_EQ(result, 7.0);
}

TEST(FormulaEvaluatorTest, EvaluatePower) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("2 ^ 3");
    double result = evaluator.evaluate(ast, {});
    EXPECT_DOUBLE_EQ(result, 8.0);
}

TEST(FormulaEvaluatorTest, EvaluateUndefinedVariable) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("$UNDEFINED");
    EXPECT_THROW(
        evaluator.evaluate(ast, {}),
        std::runtime_error
    );
}

// ============================================================================
// Dimension Formula Tests
// ============================================================================

TEST(DimensionFormulaTest, CreateFormula) {
    DimensionFormulaManager manager;

    auto formula = manager.create_formula(
        "dim_001",
        "$OD * 2 + 5",
        "user@example.com"
    );

    EXPECT_EQ(formula.id, "dim_001");
    EXPECT_EQ(formula.formula_text, "$OD * 2 + 5");
    EXPECT_EQ(formula.author, "user@example.com");
}

TEST(DimensionFormulaTest, UpdateFormula) {
    DimensionFormulaManager manager;

    auto formula = manager.create_formula(
        "dim_001",
        "$OD * 2",
        "user@example.com"
    );

    manager.update_formula(formula, "$OD * 3");
    EXPECT_EQ(formula.formula_text, "$OD * 3");
}

TEST(DimensionFormulaTest, EvaluateFormula) {
    DimensionFormulaManager manager;

    auto formula = manager.create_formula(
        "dim_001",
        "$OD * 2 + 5",
        "user@example.com"
    );
    formula.variables_map = {{"$OD", 25.0}};

    double result = manager.evaluate_formula(formula);
    EXPECT_DOUBLE_EQ(result, 55.0);
}

TEST(DimensionFormulaTest, Serialization) {
    DimensionFormula original;
    original.id = "dim_001";
    original.formula_text = "$OD * 2 + 5";
    original.result = 55.0;
    original.author = "user@example.com";
    original.created_at = "2026-10-23T21:30:00Z";
    original.variables_map = {{"$OD", 25.0}};

    json j = original.to_json();
    auto restored = DimensionFormula::from_json(j);

    EXPECT_EQ(restored.id, original.id);
    EXPECT_EQ(restored.formula_text, original.formula_text);
    EXPECT_DOUBLE_EQ(restored.result, original.result);
    EXPECT_EQ(restored.author, original.author);
}

TEST(DimensionFormulaTest, RecalculateAll) {
    DimensionFormulaManager manager;

    std::vector<DimensionFormula> formulas;

    auto f1 = manager.create_formula("dim_001", "$OD * 2", "user@example.com");
    f1.variables_map = {{"$OD", 20.0}};
    formulas.push_back(f1);

    auto f2 = manager.create_formula("dim_002", "$OD + 10", "user@example.com");
    f2.variables_map = {{"$OD", 20.0}};
    formulas.push_back(f2);

    // Update OD to 25
    manager.recalculate_all(formulas, {{"$OD", 25.0}});

    EXPECT_DOUBLE_EQ(
        manager.evaluate_formula(formulas[0]),
        50.0  // 25 * 2
    );
    EXPECT_DOUBLE_EQ(
        manager.evaluate_formula(formulas[1]),
        35.0  // 25 + 10
    );
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(FormulaErrorTest, DivisionByZero) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("10 / 0");
    EXPECT_THROW(
        evaluator.evaluate(ast, {}),
        std::runtime_error
    );
}

TEST(FormulaErrorTest, SqrtNegative) {
    FormulaParser parser;
    FormulaEvaluator evaluator;

    auto ast = parser.parse("sqrt(-1)");
    EXPECT_THROW(
        evaluator.evaluate(ast, {}),
        std::runtime_error
    );
}

TEST(FormulaErrorTest, InvalidSyntax) {
    FormulaParser parser;

    EXPECT_THROW(
        parser.parse("5 +* 3"),
        std::runtime_error
    );
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(FormulaIntegrationTest, FullWorkflow) {
    DimensionFormulaManager manager;

    // Create parametric dimension
    auto formula = manager.create_formula(
        "dim_001",
        "$OD * 2 + $Wall * 4",
        "user@example.com"
    );

    // Set variables
    formula.variables_map = {
        {"$OD", 25.0},
        {"$Wall", 2.5}
    };

    // Evaluate
    double result = manager.evaluate_formula(formula);
    EXPECT_DOUBLE_EQ(result, 60.0);  // 25*2 + 2.5*4 = 50 + 10 = 60

    // Update variables
    formula.variables_map["$OD"] = 30.0;
    result = manager.evaluate_formula(formula);
    EXPECT_DOUBLE_EQ(result, 70.0);  // 30*2 + 2.5*4 = 60 + 10 = 70

    // Serialize
    json j = formula.to_json();
    EXPECT_TRUE(j.contains("id"));
    EXPECT_TRUE(j.contains("formula_text"));
    EXPECT_TRUE(j.contains("result"));
}
