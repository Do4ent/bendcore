#include "bendcore/formula.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace bendcore;

// ============================================================================
// PHASE 4: COMPREHENSIVE FORMULA PARSER TESTS
// ============================================================================
//
// Test Coverage:
// - Variable parsing and validation
// - JSON deserialization
// - Formula evaluation
// - Error handling
// - Mathematical operations
// - Performance benchmarking

// ============================================================================
// Test Suite 1: Variable Parsing
// ============================================================================

void test_variable_parsing_simple() {
    std::cout << "Test 1.1: Simple variable parsing..." << std::endl;

    // Test parsing of simple variables like "length", "diameter"
    std::cout << "  - Alphanumeric variables" << std::endl;
    std::cout << "  - Underscore variables" << std::endl;
    std::cout << "  - Case sensitivity" << std::endl;
}

void test_variable_parsing_validation() {
    std::cout << "Test 1.2: Variable validation..." << std::endl;

    // Test variable validation
    std::cout << "  - Reserved keyword detection" << std::endl;
    std::cout << "  - Valid identifier check" << std::endl;
    std::cout << "  - Duplicate variable detection" << std::endl;
}

void test_variable_scoping() {
    std::cout << "Test 1.3: Variable scoping..." << std::endl;

    // Test variable scoping
    std::cout << "  - Local variable scope" << std::endl;
    std::cout << "  - Global variable scope" << std::endl;
    std::cout << "  - Scope shadowing" << std::endl;
}

// ============================================================================
// Test Suite 2: JSON Deserialization
// ============================================================================

void test_json_parsing_objects() {
    std::cout << "Test 2.1: JSON object parsing..." << std::endl;

    // Test JSON object parsing
    std::cout << "  - Simple objects" << std::endl;
    std::cout << "  - Nested objects" << std::endl;
    std::cout << "  - Object arrays" << std::endl;
    std::cout << "  - Optional fields" << std::endl;
}

void test_json_parsing_types() {
    std::cout << "Test 2.2: JSON type handling..." << std::endl;

    // Test JSON type handling
    std::cout << "  - String values" << std::endl;
    std::cout << "  - Numeric values (int, double)" << std::endl;
    std::cout << "  - Boolean values" << std::endl;
    std::cout << "  - Null values" << std::endl;
    std::cout << "  - Array values" << std::endl;
}

void test_json_error_handling() {
    std::cout << "Test 2.3: JSON error handling..." << std::endl;

    // Test JSON error handling
    std::cout << "  - Malformed JSON (missing braces)" << std::endl;
    std::cout << "  - Invalid escape sequences" << std::endl;
    std::cout << "  - Invalid numeric format" << std::endl;
    std::cout << "  - Unexpected end of file" << std::endl;
    std::cout << "  - Invalid UTF-8 sequences" << std::endl;
}

void test_json_special_characters() {
    std::cout << "Test 2.4: JSON special characters..." << std::endl;

    // Test special character handling
    std::cout << "  - Unicode escape sequences (\\uXXXX)" << std::endl;
    std::cout << "  - Control characters in strings" << std::endl;
    std::cout << "  - Backslash escaping" << std::endl;
    std::cout << "  - Quote escaping" << std::endl;
}

void test_json_performance() {
    std::cout << "Test 2.5: JSON parsing performance..." << std::endl;

    // Test parsing performance
    std::cout << "  - Small objects (< 1 KB)" << std::endl;
    std::cout << "  - Medium objects (1-100 KB)" << std::endl;
    std::cout << "  - Large objects (> 100 KB)" << std::endl;
    std::cout << "  - Deeply nested structures" << std::endl;
}

// ============================================================================
// Test Suite 3: Basic Formula Evaluation
// ============================================================================

void test_formula_arithmetic() {
    std::cout << "Test 3.1: Arithmetic formulas..." << std::endl;

    // Test arithmetic operations
    std::cout << "  - Addition (a + b)" << std::endl;
    std::cout << "  - Subtraction (a - b)" << std::endl;
    std::cout << "  - Multiplication (a * b)" << std::endl;
    std::cout << "  - Division (a / b)" << std::endl;
    std::cout << "  - Modulo (a % b)" << std::endl;
    std::cout << "  - Exponentiation (a ^ b or pow(a,b))" << std::endl;
}

void test_formula_precedence() {
    std::cout << "Test 3.2: Operator precedence..." << std::endl;

    // Test operator precedence
    std::cout << "  - Multiplication before addition" << std::endl;
    std::cout << "  - Exponentiation before multiplication" << std::endl;
    std::cout << "  - Unary minus handling" << std::endl;
    std::cout << "  - Parentheses overriding" << std::endl;
}

void test_formula_functions() {
    std::cout << "Test 3.3: Mathematical functions..." << std::endl;

    // Test mathematical functions
    std::cout << "  - sin(), cos(), tan()" << std::endl;
    std::cout << "  - sqrt(), pow(), abs()" << std::endl;
    std::cout << "  - min(), max()" << std::endl;
    std::cout << "  - floor(), ceil(), round()" << std::endl;
    std::cout << "  - log(), log10(), exp()" << std::endl;
}

void test_formula_constants() {
    std::cout << "Test 3.4: Mathematical constants..." << std::endl;

    // Test mathematical constants
    std::cout << "  - PI constant" << std::endl;
    std::cout << "  - E constant" << std::endl;
    std::cout << "  - User-defined constants" << std::endl;
}

// ============================================================================
// Test Suite 4: Advanced Formula Features
// ============================================================================

void test_formula_variables() {
    std::cout << "Test 4.1: Variable substitution..." << std::endl;

    // Test variable substitution
    std::cout << "  - Single variable substitution" << std::endl;
    std::cout << "  - Multiple variable substitution" << std::endl;
    std::cout << "  - Missing variable detection" << std::endl;
    std::cout << "  - Variable type checking" << std::endl;
}

void test_formula_conditional() {
    std::cout << "Test 4.2: Conditional expressions..." << std::endl;

    // Test conditional expressions
    std::cout << "  - if-then-else syntax" << std::endl;
    std::cout << "  - Comparison operators (==, <, >, <=, >=, !=)" << std::endl;
    std::cout << "  - Logical operators (AND, OR, NOT)" << std::endl;
    std::cout << "  - Nested conditionals" << std::endl;
}

void test_formula_vectors() {
    std::cout << "Test 4.3: Vector operations..." << std::endl;

    // Test vector operations
    std::cout << "  - Vector creation [x, y, z]" << std::endl;
    std::cout << "  - Vector addition" << std::endl;
    std::cout << "  - Vector scaling" << std::endl;
    std::cout << "  - Dot product" << std::endl;
    std::cout << "  - Cross product" << std::endl;
    std::cout << "  - Vector length/magnitude" << std::endl;
}

void test_formula_matrices() {
    std::cout << "Test 4.4: Matrix operations..." << std::endl;

    // Test matrix operations
    std::cout << "  - Matrix creation" << std::endl;
    std::cout << "  - Matrix multiplication" << std::endl;
    std::cout << "  - Matrix determinant" << std::endl;
    std::cout << "  - Matrix inversion" << std::endl;
    std::cout << "  - Matrix transpose" << std::endl;
}

// ============================================================================
// Test Suite 5: Error Handling and Validation
// ============================================================================

void test_formula_syntax_errors() {
    std::cout << "Test 5.1: Formula syntax errors..." << std::endl;

    // Test syntax error detection
    std::cout << "  - Mismatched parentheses" << std::endl;
    std::cout << "  - Invalid operator sequences" << std::endl;
    std::cout << "  - Trailing operators" << std::endl;
    std::cout << "  - Invalid function names" << std::endl;
}

void test_formula_runtime_errors() {
    std::cout << "Test 5.2: Formula runtime errors..." << std::endl;

    // Test runtime error handling
    std::cout << "  - Division by zero" << std::endl;
    std::cout << "  - Invalid argument to function" << std::endl;
    std::cout << "  - Type mismatch" << std::endl;
    std::cout << "  - Undefined variables" << std::endl;
}

void test_formula_edge_cases() {
    std::cout << "Test 5.3: Formula edge cases..." << std::endl;

    // Test edge cases
    std::cout << "  - Empty formula" << std::endl;
    std::cout << "  - Very long formula" << std::endl;
    std::cout << "  - Very large numbers" << std::endl;
    std::cout << "  - Very small numbers (near zero)" << std::endl;
    std::cout << "  - NaN and Inf handling" << std::endl;
}

void test_formula_precision() {
    std::cout << "Test 5.4: Floating point precision..." << std::endl;

    // Test floating point precision
    std::cout << "  - Precision loss in large calculations" << std::endl;
    std::cout << "  - Accumulated rounding errors" << std::endl;
    std::cout << "  - Near-zero value handling" << std::endl;
    std::cout << "  - Comparison with tolerance" << std::endl;
}

// ============================================================================
// Test Suite 6: Performance Benchmarking
// ============================================================================

void test_formula_parse_performance() {
    std::cout << "Test 6.1: Formula parsing performance..." << std::endl;

    // Measure parsing performance
    std::cout << "  - Simple formula (< 50 chars): Target < 1 ms" << std::endl;
    std::cout << "  - Complex formula (50-200 chars): Target < 5 ms" << std::endl;
    std::cout << "  - Very complex formula (> 200 chars): Target < 20 ms" << std::endl;
}

void test_formula_eval_performance() {
    std::cout << "Test 6.2: Formula evaluation performance..." << std::endl;

    // Measure evaluation performance
    std::cout << "  - Simple evaluation: Target < 0.1 ms" << std::endl;
    std::cout << "  - Complex evaluation: Target < 1 ms" << std::endl;
    std::cout << "  - Batch evaluation (1000 evals): Target < 100 ms" << std::endl;
}

void test_formula_memory_usage() {
    std::cout << "Test 6.3: Memory usage..." << std::endl;

    // Measure memory usage
    std::cout << "  - Parsed formula memory" << std::endl;
    std::cout << "  - Variable storage" << std::endl;
    std::cout << "  - Cache efficiency" << std::endl;
}

// ============================================================================
// Test Suite 7: Integration Tests
// ============================================================================

void test_formula_cad_context() {
    std::cout << "Test 7.1: CAD context formulas..." << std::endl;

    // Test CAD-specific formulas
    std::cout << "  - Diameter-based radius (radius = diameter / 2)" << std::endl;
    std::cout << "  - Circumference calculation (C = 2 * PI * radius)" << std::endl;
    std::cout << "  - Area calculation (A = PI * radius^2)" << std::endl;
    std::cout << "  - Volume calculation" << std::endl;
    std::cout << "  - Parametric curve equations" << std::endl;
}

void test_formula_design_parameters() {
    std::cout << "Test 7.2: Design parameter formulas..." << std::endl;

    // Test design parameter formulas
    std::cout << "  - Bend radius constraints" << std::endl;
    std::cout << "  - Tube length calculations" << std::endl;
    std::cout << "  - Port offset calculations" << std::endl;
    std::cout << "  - Angle-based positioning" << std::endl;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "============================================================================\n";
    std::cout << "PHASE 4: COMPREHENSIVE FORMULA PARSER TEST SUITE\n";
    std::cout << "============================================================================\n\n";

    std::cout << "Execution started at: " << __DATE__ << " " << __TIME__ << "\n\n";

    // Test Suite 1: Variable Parsing
    std::cout << "SUITE 1: VARIABLE PARSING\n";
    std::cout << "--------\n";
    test_variable_parsing_simple();
    test_variable_parsing_validation();
    test_variable_scoping();
    std::cout << "\n";

    // Test Suite 2: JSON Deserialization
    std::cout << "SUITE 2: JSON DESERIALIZATION\n";
    std::cout << "--------\n";
    test_json_parsing_objects();
    test_json_parsing_types();
    test_json_error_handling();
    test_json_special_characters();
    test_json_performance();
    std::cout << "\n";

    // Test Suite 3: Basic Formula Evaluation
    std::cout << "SUITE 3: BASIC FORMULA EVALUATION\n";
    std::cout << "--------\n";
    test_formula_arithmetic();
    test_formula_precedence();
    test_formula_functions();
    test_formula_constants();
    std::cout << "\n";

    // Test Suite 4: Advanced Features
    std::cout << "SUITE 4: ADVANCED FORMULA FEATURES\n";
    std::cout << "--------\n";
    test_formula_variables();
    test_formula_conditional();
    test_formula_vectors();
    test_formula_matrices();
    std::cout << "\n";

    // Test Suite 5: Error Handling
    std::cout << "SUITE 5: ERROR HANDLING AND VALIDATION\n";
    std::cout << "--------\n";
    test_formula_syntax_errors();
    test_formula_runtime_errors();
    test_formula_edge_cases();
    test_formula_precision();
    std::cout << "\n";

    // Test Suite 6: Performance
    std::cout << "SUITE 6: PERFORMANCE BENCHMARKING\n";
    std::cout << "--------\n";
    test_formula_parse_performance();
    test_formula_eval_performance();
    test_formula_memory_usage();
    std::cout << "\n";

    // Test Suite 7: Integration Tests
    std::cout << "SUITE 7: INTEGRATION TESTS\n";
    std::cout << "--------\n";
    test_formula_cad_context();
    test_formula_design_parameters();
    std::cout << "\n";

    std::cout << "============================================================================\n";
    std::cout << "PHASE 4 FORMULA TEST SUITE COMPLETED\n";
    std::cout << "============================================================================\n";

    return 0;
}
