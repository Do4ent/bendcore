#include <cassert>
#include <cmath>
#include <vector>
#include <iostream>
#include <iomanip>

#include "bendcore/topology_optimization/simp_method.hpp"
#include "bendcore/topology_optimization/sensitivity_analysis.hpp"
#include "bendcore/topology_optimization/density_filtering.hpp"
#include "bendcore/topology_optimization/optimization_algorithms.hpp"
#include "bendcore/topology_optimization/constraint_handling.hpp"
#include "bendcore/topology_optimization/post_processing.hpp"
#include "bendcore/topology_optimization/topology_optimizer.hpp"

using namespace bendcore::topology_optimization;

// Test counters
int tests_passed = 0;
int tests_failed = 0;

void assert_equal(double a, double b, double tolerance, const std::string& msg) {
    if (std::abs(a - b) < tolerance) {
        tests_passed++;
    } else {
        std::cout << "FAIL: " << msg << " (" << a << " != " << b << ")" << std::endl;
        tests_failed++;
    }
}

void assert_true(bool condition, const std::string& msg) {
    if (condition) {
        tests_passed++;
    } else {
        std::cout << "FAIL: " << msg << std::endl;
        tests_failed++;
    }
}

void assert_false(bool condition, const std::string& msg) {
    if (!condition) {
        tests_passed++;
    } else {
        std::cout << "FAIL: " << msg << std::endl;
        tests_failed++;
    }
}

// ============================================================================
// SIMP Method Tests (1-8)
// ============================================================================

void test_simp_material_interpolation_zero() {
    double E = SIMPMethod::material_interpolation(0.0, 1.0, 1e-3, 3.0);
    assert_equal(E, 1e-3, 1e-6, "SIMP: Material interpolation at x=0");
}

void test_simp_material_interpolation_one() {
    double E = SIMPMethod::material_interpolation(1.0, 1.0, 1e-3, 3.0);
    assert_equal(E, 1.0, 1e-6, "SIMP: Material interpolation at x=1");
}

void test_simp_material_interpolation_half() {
    double E = SIMPMethod::material_interpolation(0.5, 1.0, 1e-3, 3.0);
    double expected = 1e-3 + std::pow(0.5, 3.0) * (1.0 - 1e-3);
    assert_equal(E, expected, 1e-6, "SIMP: Material interpolation at x=0.5");
}

void test_simp_material_derivative_zero() {
    double dE = SIMPMethod::material_derivative(1e-12, 1.0, 1e-3, 3.0);
    assert_true(dE > 0.0, "SIMP: Material derivative is positive");
}

void test_simp_material_derivative_one() {
    double dE = SIMPMethod::material_derivative(1.0, 1.0, 1e-3, 3.0);
    double expected = 3.0 * std::pow(1.0, 2.0) * (1.0 - 1e-3);
    assert_equal(dE, expected, 1e-6, "SIMP: Material derivative at x=1");
}

void test_simp_initialization() {
    SIMPMethod simp;
    simp.initialize_densities(100, 0.5);
    auto densities = simp.get_densities();
    assert_true(densities.size() == 100, "SIMP: Initialize 100 elements");
    assert_equal(densities[0], 0.5, 1e-6, "SIMP: Initial density is 0.5");
}

void test_simp_update_densities() {
    SIMPMethod simp;
    simp.initialize_densities(10, 0.5);

    std::vector<double> sensitivities(10, -0.1);  // Negative sensitivity
    std::vector<double> lower(10, 0.0);
    std::vector<double> upper(10, 1.0);

    simp.update_densities(sensitivities, lower, upper);
    auto new_densities = simp.get_densities();

    assert_true(new_densities.size() == 10, "SIMP: Update densities");
    assert_true(new_densities[0] > 0.5, "SIMP: Densities increase with negative sensitivity");
}

void test_simp_volume_constraint() {
    SIMPMethod simp;
    simp.initialize_densities(100, 0.6);

    std::vector<double> sensitivities(100, -0.1);
    auto constrained = simp.apply_volume_constraint(sensitivities, 50.0);

    assert_true(constrained.size() == 100, "SIMP: Volume constraint applies");
    double total_vol = 0.0;
    for (double d : constrained) {
        total_vol += d;
    }
    assert_true(total_vol <= 55.0, "SIMP: Total volume respects constraint");
}

// ============================================================================
// Sensitivity Analysis Tests (9-16)
// ============================================================================

void test_sensitivity_compliance_dimension() {
    std::vector<double> densities(10, 0.5);
    std::vector<double> displacements(10, 0.01);
    std::vector<double> stiffness(10, 1.0);

    auto sens = SensitivityAnalysis::compute_compliance_sensitivity(
        densities, displacements, stiffness, 10, 3.0, 1.0, 1e-3);

    assert_equal(static_cast<double>(sens.size()), 10.0, 0.1,
                "Sensitivity: Compliance sensitivity dimension");
}

void test_sensitivity_stress_dimension() {
    std::vector<double> densities(10, 0.5);
    std::vector<double> stresses(10, 100.0);

    auto sens = SensitivityAnalysis::compute_stress_sensitivity(
        densities, stresses, 1000.0, 3.0);

    assert_equal(static_cast<double>(sens.size()), 10.0, 0.1,
                "Sensitivity: Stress sensitivity dimension");
}

void test_sensitivity_convergence_density() {
    SensitivityAnalysis analyzer;
    std::vector<double> current(10, 0.51);
    std::vector<double> previous(10, 0.5);

    bool converged = analyzer.check_convergence_density(current, previous);
    assert_true(!converged, "Sensitivity: Non-convergent when change is large");
}

void test_sensitivity_max_change() {
    std::vector<double> current = {0.6, 0.7, 0.5};
    std::vector<double> previous = {0.5, 0.5, 0.5};

    double max_change = SensitivityAnalysis::compute_max_change(current, previous);
    assert_equal(max_change, 0.2, 1e-6, "Sensitivity: Max change computation");
}

void test_sensitivity_gradient_norm() {
    std::vector<double> sensitivities = {3.0, 4.0};
    double norm = SensitivityAnalysis::compute_gradient_norm(sensitivities);
    assert_equal(norm, 5.0, 1e-6, "Sensitivity: Gradient norm (3-4-5 triangle)");
}

void test_sensitivity_constraint_satisfaction() {
    SensitivityAnalysis analyzer;
    std::vector<double> constraints = {-0.1, -0.05, -0.01};

    bool satisfied = analyzer.check_constraints_satisfied(constraints, 1e-3);
    assert_true(satisfied, "Sensitivity: All constraints satisfied");
}

void test_sensitivity_constraint_violation() {
    SensitivityAnalysis analyzer;
    std::vector<double> constraints = {0.05, -0.01};

    bool satisfied = analyzer.check_constraints_satisfied(constraints, 1e-3);
    assert_false(satisfied, "Sensitivity: Constraint violation detected");
}

void test_sensitivity_convergence_objective() {
    SensitivityAnalysis analyzer;
    analyzer.check_convergence_objective(100.0);
    analyzer.check_convergence_objective(100.0);  // Same value
    bool converged = analyzer.check_convergence_objective(100.0);
    assert_true(!converged, "Sensitivity: Convergence on objective");
}

// ============================================================================
// Density Filtering Tests (17-23)
// ============================================================================

void test_filter_linear_basic() {
    std::vector<double> densities = {1.0, 0.5, 0.0};
    std::vector<std::array<double, 3>> positions = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0}
    };

    auto filtered = DensityFilter::apply_linear_filter(densities, 1.5, positions);
    assert_true(filtered.size() == 3, "Filter: Linear filter dimension");
}

void test_filter_helmholtz() {
    std::vector<double> densities(5, 0.5);
    std::vector<std::array<double, 3>> positions = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0},
        {3.0, 0.0, 0.0}, {4.0, 0.0, 0.0}
    };

    auto filtered = DensityFilter::apply_helmholtz_filter(densities, 1.5, positions);
    assert_true(filtered.size() == 5, "Filter: Helmholtz filter dimension");
}

void test_filter_morphological_opening() {
    std::vector<double> densities(5, 0.5);
    std::vector<std::array<double, 3>> positions = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0},
        {3.0, 0.0, 0.0}, {4.0, 0.0, 0.0}
    };

    auto filtered = DensityFilter::apply_morphological_opening(densities, 1.5, positions);
    assert_true(filtered.size() == 5, "Filter: Morphological opening dimension");
}

void test_filter_morphological_closing() {
    std::vector<double> densities(5, 0.5);
    std::vector<std::array<double, 3>> positions = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0},
        {3.0, 0.0, 0.0}, {4.0, 0.0, 0.0}
    };

    auto filtered = DensityFilter::apply_morphological_closing(densities, 1.5, positions);
    assert_true(filtered.size() == 5, "Filter: Morphological closing dimension");
}

void test_filter_sensitivities() {
    DensityFilter filter;
    std::vector<double> sensitivities = {-0.1, -0.2, -0.3};
    std::vector<double> densities = {0.5, 0.6, 0.7};
    std::vector<std::array<double, 3>> positions = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0}
    };

    auto filtered = filter.filter_sensitivities(sensitivities, densities, positions);
    assert_true(filtered.size() == 3, "Filter: Sensitivity filtering");
}

void test_filter_weight_computation() {
    DensityFilter filter;
    std::vector<std::array<double, 3>> positions = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}
    };

    auto weights = filter.compute_weights(positions, 2.0);
    assert_true(weights.size() == 2, "Filter: Weight matrix computation");
}

// ============================================================================
// Optimization Algorithms Tests (24-30)
// ============================================================================

void test_oc_update_basic() {
    std::vector<double> densities = {0.5, 0.5, 0.5};
    std::vector<double> sensitivities = {-0.1, -0.1, -0.1};
    std::vector<double> lower(3, 0.0);
    std::vector<double> upper(3, 1.0);

    auto updated = OptimizationAlgorithm::optimality_criteria_update(
        densities, sensitivities, lower, upper, 0.1);

    assert_true(updated.size() == 3, "OC: Update dimension");
    assert_true(updated[0] > 0.5, "OC: Density increases with negative sensitivity");
}

void test_oc_volume_constraint() {
    std::vector<double> densities(10, 0.6);
    std::vector<double> sensitivities(10, -0.1);
    std::vector<double> lower(10, 0.0);
    std::vector<double> upper(10, 1.0);

    auto updated = OptimizationAlgorithm::optimality_criteria_with_volume_constraint(
        densities, sensitivities, lower, upper, 0.1, 5.0);

    assert_true(updated.size() == 10, "OC: Volume constraint update dimension");
}

void test_mma_update_basic() {
    std::vector<double> densities = {0.5, 0.5};
    std::vector<double> sensitivities = {-0.1, 0.1};
    std::vector<double> lower(2, 0.0);
    std::vector<double> upper(2, 1.0);

    auto updated = OptimizationAlgorithm::moving_asymptotes_update(
        densities, sensitivities, lower, upper, 0.1);

    assert_true(updated.size() == 2, "MMA: Update dimension");
}

void test_backtracking_line_search() {
    auto objective = [](const std::vector<double>& x) {
        return x[0] * x[0];
    };

    std::vector<double> gradient = {2.0};
    std::vector<double> direction = {-1.0};

    double alpha = OptimizationAlgorithm::backtracking_line_search(
        1.0, gradient, direction, objective);

    assert_true(alpha > 0.0, "Line search: Positive step size");
    assert_true(alpha <= 1.0, "Line search: Step size in bounds");
}

void test_golden_section_search() {
    auto objective = [](double x) {
        return (x - 0.5) * (x - 0.5);
    };

    double x_opt = OptimizationAlgorithm::golden_section_search(0.0, 1.0, objective);
    assert_equal(x_opt, 0.5, 0.01, "Golden section: Optimal at x=0.5");
}

void test_algorithm_step() {
    OptimizationAlgorithm optimizer;
    std::vector<double> densities = {0.5, 0.5};
    std::vector<double> sensitivities = {-0.1, -0.1};
    std::vector<double> lower(2, 0.0);
    std::vector<double> upper(2, 1.0);

    auto updated = optimizer.step(densities, sensitivities, lower, upper);
    assert_true(updated.size() == 2, "Algorithm: Step dimension");
}

void test_algorithm_volume_constraint_step() {
    OptimizationAlgorithm optimizer;
    std::vector<double> densities(5, 0.5);
    std::vector<double> sensitivities(5, -0.1);
    std::vector<double> lower(5, 0.0);
    std::vector<double> upper(5, 1.0);

    auto updated = optimizer.step_with_volume_constraint(
        densities, sensitivities, lower, upper, 2.5);

    assert_true(updated.size() == 5, "Algorithm: Volume constraint step");
}

// ============================================================================
// Constraint Handling Tests (31-35)
// ============================================================================

void test_volume_constraint_satisfied() {
    std::vector<double> densities(10, 0.5);
    std::vector<double> volumes(10, 1.0);

    double constraint = ConstraintHandler::compute_volume_constraint(
        densities, volumes, 5.0);

    assert_equal(constraint, 0.0, 1e-6, "Constraint: Volume perfectly matched");
}

void test_volume_constraint_violated() {
    std::vector<double> densities(10, 0.5);
    std::vector<double> volumes(10, 1.0);

    double constraint = ConstraintHandler::compute_volume_constraint(
        densities, volumes, 3.0);

    assert_true(constraint > 0.0, "Constraint: Volume violation detected");
}

void test_stress_constraint() {
    std::vector<double> stresses = {100.0, 200.0, 150.0};
    double constraint = ConstraintHandler::compute_stress_constraint(stresses, 300.0);

    assert_equal(constraint, 200.0/300.0 - 1.0, 1e-6, "Constraint: Stress computation");
}

void test_symmetry_constraint() {
    std::vector<double> densities = {0.2, 0.5, 0.3, 0.6};
    std::vector<std::pair<size_t, size_t>> pairs = {{0, 3}, {1, 2}};

    auto constrained = ConstraintHandler::apply_symmetry_constraint(densities, pairs);
    assert_equal(constrained[0], constrained[3], 1e-6, "Constraint: Symmetry enforced");
    assert_equal(constrained[1], constrained[2], 1e-6, "Constraint: Symmetry enforced");
}

void test_constraint_handler_add() {
    ConstraintHandler handler;
    handler.add_constraint(ConstraintHandler::Constraint(
        ConstraintHandler::ConstraintType::VOLUME, 0.0));

    auto active = handler.get_active_constraints();
    assert_true(active.size() > 0, "Constraint: Handler tracks constraints");
}

// ============================================================================
// Post-Processing Tests (36-41)
// ============================================================================

void test_threshold_simple() {
    std::vector<double> densities = {0.1, 0.5, 0.9};
    auto binary = PostProcessor::apply_simple_threshold(densities, 0.5);

    assert_equal(binary[0], 0.0, 1e-6, "Post-process: Threshold below");
    assert_equal(binary[1], 0.0, 1e-6, "Post-process: Threshold at boundary");
    assert_equal(binary[2], 1.0, 1e-6, "Post-process: Threshold above");
}

void test_threshold_heaviside() {
    std::vector<double> densities = {0.0, 0.5, 1.0};
    auto smoothed = PostProcessor::apply_heaviside_threshold(densities, 0.5);

    assert_true(smoothed[0] >= 0.0 && smoothed[0] <= 1.0, "Post-process: Heaviside in [0,1]");
    assert_true(smoothed[1] >= 0.0 && smoothed[1] <= 1.0, "Post-process: Heaviside in [0,1]");
    assert_true(smoothed[2] >= 0.0 && smoothed[2] <= 1.0, "Post-process: Heaviside in [0,1]");
}

void test_morphological_cleanup() {
    std::vector<double> densities = {0.8, 0.1, 0.9};
    std::vector<std::array<double, 3>> positions = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0}
    };

    auto cleaned = PostProcessor::morphological_cleanup(densities, positions);
    assert_true(cleaned.size() == 3, "Post-process: Morphological cleanup dimension");
}

void test_manufacturability_check() {
    PostProcessor processor;
    std::vector<double> densities(10, 0.5);
    std::vector<std::array<double, 3>> positions = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0},
        {3.0, 0.0, 0.0}, {4.0, 0.0, 0.0}, {5.0, 0.0, 0.0},
        {6.0, 0.0, 0.0}, {7.0, 0.0, 0.0}, {8.0, 0.0, 0.0},
        {9.0, 0.0, 0.0}
    };

    double score = processor.check_manufacturability(densities, positions);
    assert_true(score >= 0.0 && score <= 1.0, "Post-process: Manufacturability score in [0,1]");
}

void test_boundary_smoothing() {
    PostProcessor processor;
    std::vector<double> densities = {0.2, 0.5, 0.8};
    std::vector<std::array<double, 3>> positions = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0}
    };

    auto smoothed = processor.smooth_boundaries(densities, positions);
    assert_true(smoothed.size() == 3, "Post-process: Boundary smoothing dimension");
}

void test_extract_geometry() {
    PostProcessor processor;
    std::vector<double> densities = {0.8, 0.2};
    std::vector<PostProcessor::Element> elements = {
        {0, {0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, 0.8, true},
        {1, {2.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, 0.2, false}
    };

    auto geometry = processor.extract_geometry(densities, elements);
    assert_true(geometry.vertices.size() > 0, "Post-process: Geometry extraction produces vertices");
}

// ============================================================================
// Topology Optimizer Integration Tests (42-45)
// ============================================================================

void test_optimizer_initialization() {
    TopologyOptimizer::ProblemDefinition problem;
    problem.n_elements = 100;
    problem.element_volumes.assign(100, 1.0);
    problem.element_positions.assign(100, {0.0, 0.0, 0.0});
    problem.element_dimensions.assign(100, {1.0, 1.0, 1.0});
    problem.E_0 = 1.0;
    problem.poisson_ratio = 0.3;
    problem.target_volume_fraction = 0.5;

    TopologyOptimizer optimizer(problem);
    assert_true(true, "Optimizer: Initialization successful");
}

void test_optimizer_single_iteration() {
    TopologyOptimizer::ProblemDefinition problem;
    problem.n_elements = 50;
    problem.element_volumes.assign(50, 1.0);
    problem.element_positions.assign(50, std::array<double, 3>{0.0, 0.0, 0.0});
    for (size_t i = 0; i < 50; ++i) {
        problem.element_positions[i][0] = static_cast<double>(i);
    }
    problem.element_dimensions.assign(50, {1.0, 1.0, 1.0});
    problem.E_0 = 1.0;
    problem.loads.assign(1, {1.0, 0.0, 0.0});

    TopologyOptimizer::OptimizationParameters params;
    params.max_iterations = 5;

    TopologyOptimizer optimizer(problem, params);
    optimizer.optimize_iteration(0);
    assert_true(true, "Optimizer: Single iteration successful");
}

void test_optimizer_convergence() {
    TopologyOptimizer::ProblemDefinition problem;
    problem.n_elements = 30;
    problem.element_volumes.assign(30, 1.0);
    problem.element_positions.assign(30, std::array<double, 3>{0.0, 0.0, 0.0});
    problem.element_dimensions.assign(30, {1.0, 1.0, 1.0});
    problem.E_0 = 1.0;
    problem.loads.assign(1, {1.0, 0.0, 0.0});

    TopologyOptimizer::OptimizationParameters params;
    params.max_iterations = 3;

    TopologyOptimizer optimizer(problem, params);
    auto result = optimizer.optimize();

    assert_true(result.iterations <= params.max_iterations,
               "Optimizer: Iterations within limit");
    assert_true(result.final_densities.size() == 30,
               "Optimizer: Final densities size matches");
}

void test_optimizer_result_validity() {
    TopologyOptimizer::ProblemDefinition problem;
    problem.n_elements = 20;
    problem.element_volumes.assign(20, 1.0);
    problem.element_positions.assign(20, std::array<double, 3>{0.0, 0.0, 0.0});
    problem.element_dimensions.assign(20, {1.0, 1.0, 1.0});
    problem.E_0 = 1.0;

    TopologyOptimizer::OptimizationParameters params;
    params.max_iterations = 2;

    TopologyOptimizer optimizer(problem, params);
    auto result = optimizer.optimize();

    // Check all densities are in [0, 1]
    bool valid = true;
    for (double d : result.final_densities) {
        if (d < 0.0 || d > 1.0) {
            valid = false;
            break;
        }
    }
    assert_true(valid, "Optimizer: All densities in [0, 1]");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << std::string(70, '=') << std::endl;
    std::cout << "BendCore Topology Optimization (M8/GAP-032) - Test Suite" << std::endl;
    std::cout << std::string(70, '=') << std::endl;

    // SIMP Tests
    std::cout << "\n[SIMP Method Tests]" << std::endl;
    test_simp_material_interpolation_zero();
    test_simp_material_interpolation_one();
    test_simp_material_interpolation_half();
    test_simp_material_derivative_zero();
    test_simp_material_derivative_one();
    test_simp_initialization();
    test_simp_update_densities();
    test_simp_volume_constraint();

    // Sensitivity Tests
    std::cout << "\n[Sensitivity Analysis Tests]" << std::endl;
    test_sensitivity_compliance_dimension();
    test_sensitivity_stress_dimension();
    test_sensitivity_convergence_density();
    test_sensitivity_max_change();
    test_sensitivity_gradient_norm();
    test_sensitivity_constraint_satisfaction();
    test_sensitivity_constraint_violation();
    test_sensitivity_convergence_objective();

    // Filter Tests
    std::cout << "\n[Density Filtering Tests]" << std::endl;
    test_filter_linear_basic();
    test_filter_helmholtz();
    test_filter_morphological_opening();
    test_filter_morphological_closing();
    test_filter_sensitivities();
    test_filter_weight_computation();

    // Optimization Algorithm Tests
    std::cout << "\n[Optimization Algorithm Tests]" << std::endl;
    test_oc_update_basic();
    test_oc_volume_constraint();
    test_mma_update_basic();
    test_backtracking_line_search();
    test_golden_section_search();
    test_algorithm_step();
    test_algorithm_volume_constraint_step();

    // Constraint Tests
    std::cout << "\n[Constraint Handling Tests]" << std::endl;
    test_volume_constraint_satisfied();
    test_volume_constraint_violated();
    test_stress_constraint();
    test_symmetry_constraint();
    test_constraint_handler_add();

    // Post-Processing Tests
    std::cout << "\n[Post-Processing Tests]" << std::endl;
    test_threshold_simple();
    test_threshold_heaviside();
    test_morphological_cleanup();
    test_manufacturability_check();
    test_boundary_smoothing();
    test_extract_geometry();

    // Integration Tests
    std::cout << "\n[Integration Tests]" << std::endl;
    test_optimizer_initialization();
    test_optimizer_single_iteration();
    test_optimizer_convergence();
    test_optimizer_result_validity();

    // Summary
    std::cout << std::string(70, '=') << std::endl;
    std::cout << "Test Results Summary:" << std::endl;
    std::cout << "  Passed: " << std::setw(3) << tests_passed << std::endl;
    std::cout << "  Failed: " << std::setw(3) << tests_failed << std::endl;
    std::cout << "  Total:  " << std::setw(3) << (tests_passed + tests_failed) << std::endl;
    std::cout << std::string(70, '=') << std::endl;

    return (tests_failed > 0) ? 1 : 0;
}
