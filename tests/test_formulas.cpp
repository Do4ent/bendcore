// Тесты для параметрических размеров и формул (Task 1.9-1.10)
#include "bendcore/formula.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <sstream>

using namespace bendcore::formula;

// Вспомогательная функция для сравнения double с допуском
bool almost_equal(double a, double b, double tol = 1e-6) {
    return std::abs(a - b) < tol;
}

// Test 1: Parse simple formula "5 + 3" -> 8
void test_parse_simple() {
    std::cout << "Test 1: Parse simple formula '5 + 3'... ";

    auto ast = FormulaParser::parse("5 + 3");
    assert(ast != nullptr);
    assert(ast->type == ASTNode::BinaryOp);
    assert(ast->binary_op == BinaryOp::Add);

    EvaluationContext ctx;
    double result = FormulaEvaluator::evaluate(ast, ctx);
    assert(almost_equal(result, 8.0));

    std::cout << "PASSED" << std::endl;
}

// Test 2: Parse with variables "$OD * 2" -> 50 (if OD=25)
void test_parse_with_variables() {
    std::cout << "Test 2: Parse with variables '$OD * 2'... ";

    auto ast = FormulaParser::parse("$OD * 2");
    assert(ast != nullptr);
    assert(ast->type == ASTNode::BinaryOp);
    assert(ast->binary_op == BinaryOp::Mul);

    EvaluationContext ctx;
    ctx.variables["$OD"] = 25.0;
    double result = FormulaEvaluator::evaluate(ast, ctx);
    assert(almost_equal(result, 50.0));

    std::cout << "PASSED" << std::endl;
}

// Test 3: Parse with functions "sqrt($OD)" -> 5 (if OD=25)
void test_parse_with_functions() {
    std::cout << "Test 3: Parse with functions 'sqrt($OD)'... ";

    auto ast = FormulaParser::parse("sqrt($OD)");
    assert(ast != nullptr);
    assert(ast->type == ASTNode::FunctionCall);
    assert(ast->function_name == "sqrt");

    EvaluationContext ctx;
    ctx.variables["$OD"] = 25.0;
    double result = FormulaEvaluator::evaluate(ast, ctx);
    assert(almost_equal(result, 5.0));

    std::cout << "PASSED" << std::endl;
}

// Test 4: Parse specification reference "=BOM.B5"
void test_parse_spec_reference() {
    std::cout << "Test 4: Parse specification reference '=BOM.B5'... ";

    auto ast = FormulaParser::parse("=BOM.B5");
    assert(ast != nullptr);
    assert(ast->type == ASTNode::SpecReference);
    assert(ast->spec_table_name == "BOM");
    assert(ast->spec_column == 'B');
    assert(ast->spec_row == 5);

    EvaluationContext ctx;
    ctx.specification_tables["BOM"][5]['B'] = 42.0;
    double result = FormulaEvaluator::evaluate(ast, ctx);
    assert(almost_equal(result, 42.0));

    std::cout << "PASSED" << std::endl;
}

// Test 5: Circular reference detection
void test_circular_reference_detection() {
    std::cout << "Test 5: Circular reference detection... ";

    auto ast = FormulaParser::parse("=BOM.B5 + 10");

    std::vector<SpecificationReference> already_evaluating;
    SpecificationReference ref;
    ref.table_name = "BOM";
    ref.column = 'B';
    ref.row = 5;
    already_evaluating.push_back(ref);

    bool has_circular = FormulaEvaluator::has_circular_reference(ast, already_evaluating);
    assert(has_circular);

    std::cout << "PASSED" << std::endl;
}

// Test 6: Newton-Raphson constraint solver
void test_newton_raphson_solver() {
    std::cout << "Test 6: Newton-Raphson constraint solver... ";

    // Solve x^2 = 4 (solution: x = 2 or x = -2)
    auto equation = [](double x) { return x * x; };

    NewtonRaphsonSolver::SolverOptions opts;
    opts.initial_guess = 1.0;
    opts.tolerance = 1e-6;

    auto result = NewtonRaphsonSolver::solve(equation, opts, 4.0);
    assert(result.converged);
    assert(almost_equal(result.value, 2.0) || almost_equal(result.value, -2.0));

    std::cout << "PASSED" << std::endl;
}

// Test 7: Formula evaluation with variable substitution
void test_formula_evaluation() {
    std::cout << "Test 7: Formula evaluation with variable substitution... ";

    EvaluationContext ctx;
    ctx.variables["$OD"] = 50.0;
    ctx.variables["$Wall"] = 2.0;

    double result = FormulaEvaluator::evaluate_formula("$OD - 2 * $Wall", ctx);
    assert(almost_equal(result, 46.0));

    std::cout << "PASSED" << std::endl;
}

// Test 8: Error handling - undefined variable
void test_undefined_variable_error() {
    std::cout << "Test 8: Error handling - undefined variable... ";

    EvaluationContext ctx;
    try {
        FormulaEvaluator::evaluate_formula("$UndefinedVar + 5", ctx);
        assert(false && "Should have thrown exception");
    } catch (const std::invalid_argument& e) {
        assert(std::string(e.what()).find("Undefined variable") != std::string::npos);
    }

    std::cout << "PASSED" << std::endl;
}

// Test 9: Error handling - syntax error
void test_syntax_error() {
    std::cout << "Test 9: Error handling - syntax error... ";

    try {
        FormulaParser::parse("5 + + 3");
        assert(false && "Should have thrown exception");
    } catch (const std::invalid_argument&) {
        // Expected
    }

    std::cout << "PASSED" << std::endl;
}

// Test 10: Complex formula with multiple operators and functions
void test_complex_formula() {
    std::cout << "Test 10: Complex formula evaluation... ";

    EvaluationContext ctx;
    ctx.variables["$OD"] = 50.0;
    ctx.variables["$Length"] = 200.0;

    double result = FormulaEvaluator::evaluate_formula(
        "sqrt($OD * $OD + $Length * $Length)",
        ctx
    );

    // sqrt(50^2 + 200^2) = sqrt(2500 + 40000) = sqrt(42500) ≈ 206.155
    assert(almost_equal(result, std::sqrt(50.0 * 50.0 + 200.0 * 200.0)));

    std::cout << "PASSED" << std::endl;
}

// Test 11: Min and Max functions
void test_min_max_functions() {
    std::cout << "Test 11: Min and Max functions... ";

    EvaluationContext ctx;
    double result_min = FormulaEvaluator::evaluate_formula("min(5, 3, 8)", ctx);
    assert(almost_equal(result_min, 3.0));

    double result_max = FormulaEvaluator::evaluate_formula("max(5, 3, 8)", ctx);
    assert(almost_equal(result_max, 8.0));

    std::cout << "PASSED" << std::endl;
}

// Test 12: Power operator
void test_power_operator() {
    std::cout << "Test 12: Power operator... ";

    EvaluationContext ctx;
    ctx.variables["$Base"] = 2.0;

    double result = FormulaEvaluator::evaluate_formula("$Base ^ 3", ctx);
    assert(almost_equal(result, 8.0));

    std::cout << "PASSED" << std::endl;
}

// Test 13: Division by zero error
void test_division_by_zero() {
    std::cout << "Test 13: Division by zero error... ";

    EvaluationContext ctx;
    try {
        FormulaEvaluator::evaluate_formula("10 / 0", ctx);
        assert(false && "Should have thrown exception");
    } catch (const std::invalid_argument& e) {
        assert(std::string(e.what()).find("Division by zero") != std::string::npos);
    }

    std::cout << "PASSED" << std::endl;
}

// Test 14: DimensionFormula struct
void test_dimension_formula_struct() {
    std::cout << "Test 14: DimensionFormula struct... ";

    DimensionFormula dim;
    dim.id = "dim1";
    dim.set_formula("$OD * 2 + 5");
    dim.set_variable("$OD", 25.0);

    EvaluationContext ctx;
    dim.evaluate(ctx);

    assert(dim.is_valid);
    assert(almost_equal(dim.result, 55.0));

    std::cout << "PASSED" << std::endl;
}

// Test 15: Get variables from formula
void test_get_variables() {
    std::cout << "Test 15: Get variables from formula... ";

    DimensionFormula dim;
    dim.set_formula("$OD * $Wall + $Length / 2");

    auto vars = dim.get_variables();
    assert(vars.size() == 3);
    assert(std::find(vars.begin(), vars.end(), "$OD") != vars.end());
    assert(std::find(vars.begin(), vars.end(), "$Wall") != vars.end());
    assert(std::find(vars.begin(), vars.end(), "$Length") != vars.end());

    std::cout << "PASSED" << std::endl;
}

// Main test runner
int main() {
    std::cout << "Running Formula Engine Tests (Task 1.9-1.10)" << std::endl;
    std::cout << "============================================" << std::endl;

    try {
        test_parse_simple();                        // Test 1
        test_parse_with_variables();                // Test 2
        test_parse_with_functions();                // Test 3
        test_parse_spec_reference();                // Test 4
        test_circular_reference_detection();        // Test 5
        test_newton_raphson_solver();               // Test 6
        test_formula_evaluation();                  // Test 7
        test_undefined_variable_error();            // Test 8
        test_syntax_error();                        // Test 9
        test_complex_formula();                     // Test 10
        test_min_max_functions();                   // Test 11
        test_power_operator();                      // Test 12
        test_division_by_zero();                    // Test 13
        test_dimension_formula_struct();            // Test 14
        test_get_variables();                       // Test 15

        std::cout << "============================================" << std::endl;
        std::cout << "All 15 tests PASSED!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
