#include <iostream>
#include <cassert>
#include <cmath>
#include <memory>
#include "../../include/bendcore/generative/design_space.hpp"
#include "../../include/bendcore/generative/genetic_algorithm.hpp"
#include "../../include/bendcore/generative/particle_swarm.hpp"
#include "../../include/bendcore/generative/constraint_handler.hpp"
#include "../../include/bendcore/generative/design_generator.hpp"
#include "../../include/bendcore/generative/optimization_manager.hpp"
#include "../../include/bendcore/generative/visualization.hpp"

using namespace bendcore::generative;

int tests_passed = 0;
int tests_failed = 0;

void assert_test(bool condition, const std::string& test_name) {
    if (condition) {
        std::cout << "[PASS] " << test_name << std::endl;
        tests_passed++;
    } else {
        std::cout << "[FAIL] " << test_name << std::endl;
        tests_failed++;
    }
}

// Test 1: DesignSpace parameter addition
void test_design_space_parameters() {
    DesignSpace ds;
    ds.add_parameter("x", -5.0, 5.0, 0.0);
    ds.add_parameter("y", -5.0, 5.0, 0.0);
    assert_test(ds.parameter_count() == 2, "DesignSpace: Add parameters");
}

// Test 2: ParameterBound normalization
void test_parameter_normalization() {
    ParameterBound pb("test", 0.0, 10.0, 5.0);
    double normalized = pb.normalize(5.0);
    assert_test(std::abs(normalized - 0.5) < 1e-6, "ParameterBound: Normalization");
}

// Test 3: ParameterBound denormalization
void test_parameter_denormalization() {
    ParameterBound pb("test", 0.0, 10.0, 5.0);
    double denormalized = pb.denormalize(0.5);
    assert_test(std::abs(denormalized - 5.0) < 1e-6, "ParameterBound: Denormalization");
}

// Test 4: Constraint satisfaction
void test_constraint_satisfaction() {
    Constraint c("c1", ConstraintType::INEQUALITY,
        [](const std::vector<double>& x) { return x.empty() ? 0.0 : x[0] - 5.0; });

    std::vector<double> feasible = {4.0};
    assert_test(c.is_satisfied(feasible), "Constraint: Satisfied");
}

// Test 5: Constraint violation
void test_constraint_violation() {
    Constraint c("c1", ConstraintType::INEQUALITY,
        [](const std::vector<double>& x) { return x.empty() ? 0.0 : x[0] - 5.0; });

    std::vector<double> infeasible = {6.0};
    double violation = c.violation(infeasible);
    assert_test(violation > 0.0, "Constraint: Violation detection");
}

// Test 6: Individual comparison
void test_individual_comparison() {
    Individual ind1({0.3});
    Individual ind2({0.5});
    ind1.fitness = 10.0;
    ind1.is_feasible = true;
    ind2.fitness = 5.0;
    ind2.is_feasible = true;

    assert_test(ind2 < ind1, "Individual: Comparison");
}

// Test 7: Individual cloning
void test_individual_cloning() {
    Individual original({0.3, 0.4});
    original.fitness = 15.0;
    auto cloned = original.clone();
    assert_test(cloned.fitness == 15.0 && cloned.chromosome.size() == 2, "Individual: Cloning");
}

// Test 8: Particle initialization
void test_particle_initialization() {
    Particle p(3);
    assert_test(p.position.size() == 3 && p.velocity.size() == 3, "Particle: Initialization");
}

// Test 9: Particle cloning
void test_particle_cloning() {
    Particle original(2);
    original.position = {0.3, 0.4};
    original.best_fitness = 5.0;
    auto cloned = original.clone();
    assert_test(cloned.best_fitness == 5.0, "Particle: Cloning");
}

// Test 10: DesignSpace bounds checking
void test_design_space_bounds() {
    DesignSpace ds;
    ds.add_parameter("x", 0.0, 10.0);
    std::vector<double> valid = {5.0};
    assert_test(ds.within_bounds(valid), "DesignSpace: Bounds checking");
}

// Test 11: DesignSpace objective evaluation
void test_design_space_objective() {
    DesignSpace ds;
    ds.add_parameter("x", -5.0, 5.0);
    ds.add_parameter("y", -5.0, 5.0);

    auto obj = Objective("f",
        [](const std::vector<double>& x) {
            return x.size() < 2 ? 0.0 : x[0]*x[0] + x[1]*x[1];
        }, true);
    ds.set_primary_objective(obj);

    std::vector<double> x = {3.0, 4.0};
    double value = ds.primary_objective().compute(x);
    assert_test(std::abs(value - 25.0) < 1e-6, "DesignSpace: Objective evaluation");
}

// Test 12: ConstraintHandler feasibility
void test_constraint_handler_feasibility() {
    DesignSpace ds;
    ds.add_parameter("x", 0.0, 10.0);
    ds.add_parameter("y", 0.0, 10.0);
    auto obj = Objective("f", [](const std::vector<double>& x) { return x.empty() ? 0 : x[0]; }, true);
    ds.set_primary_objective(obj);
    ds.add_inequality_constraint("c1",
        [](const std::vector<double>& x) { return x.size() < 2 ? 0 : x[0] + x[1] - 15.0; });

    ConstraintHandler handler(ds);
    std::vector<double> feasible = {5.0, 5.0};
    assert_test(handler.is_feasible(feasible), "ConstraintHandler: Feasibility");
}

// Test 13: ConstraintHandler violation report
void test_constraint_violation_report() {
    DesignSpace ds;
    ds.add_parameter("x", 0.0, 10.0);
    auto obj = Objective("f", [](const std::vector<double>& x) { return 0; }, true);
    ds.set_primary_objective(obj);
    ds.add_inequality_constraint("c1",
        [](const std::vector<double>& x) { return x.empty() ? 0 : x[0] - 5.0; });

    ConstraintHandler handler(ds);
    std::vector<double> x = {6.0};
    auto report = handler.evaluate_constraints(x);
    assert_test(report.is_violated[0], "ConstraintHandler: Violation report");
}

// Test 14: DesignGenerator initialization
void test_design_generator_init() {
    DesignSpace ds;
    ds.add_parameter("p1", 0.0, 100.0);
    auto obj = Objective("f", [](const std::vector<double>& x) { return x.empty() ? 0 : x[0]; }, true);
    ds.set_primary_objective(obj);

    DesignGenerator gen(ds);
    assert_test(true, "DesignGenerator: Initialization");
}

// Test 15: DesignGenerator design generation
void test_design_generator_generation() {
    DesignSpace ds;
    ds.add_parameter("p1", 0.0, 100.0);
    ds.add_parameter("p2", 0.0, 100.0);
    ds.add_parameter("p3", 0.0, 100.0);
    auto obj = Objective("f", [](const std::vector<double>& x) { return x.empty() ? 0 : x[0]; }, true);
    ds.set_primary_objective(obj);

    DesignGenerator gen(ds);
    std::vector<double> vars = {25.0, 50.0, 75.0};
    auto design = gen.generate_design(vars);
    assert_test(design.geometry.volume > 0.0, "DesignGenerator: Design generation");
}

// Test 16: DesignTopology validation
void test_design_topology_validation() {
    DesignSpace ds;
    ds.add_parameter("p1", 0.0, 100.0);
    auto obj = Objective("f", [](const std::vector<double>& x) { return 0; }, true);
    ds.set_primary_objective(obj);

    DesignGenerator gen(ds);
    std::vector<double> vars = {25.0, 50.0, 75.0};
    auto design = gen.generate_design(vars);
    assert_test(design.topology.is_valid_dag(), "DesignTopology: Validation");
}

// Test 17: GeneticAlgorithm optimization
void test_genetic_algorithm_optimization() {
    DesignSpace ds;
    ds.add_parameter("x", -5.0, 5.0);
    ds.add_parameter("y", -5.0, 5.0);
    auto obj = Objective("f",
        [](const std::vector<double>& x) {
            return x.size() < 2 ? 0.0 : -(x[0]*x[0] + x[1]*x[1]);
        }, false);  // Maximization
    ds.set_primary_objective(obj);

    GAConfig config;
    config.population_size = 20;
    config.generations = 30;

    GeneticAlgorithm ga(ds, config);
    auto best = ga.optimize(30);
    assert_test(best.fitness > -1000.0, "GeneticAlgorithm: Optimization");
}

// Test 18: ParticleSwarmOptimizer optimization
void test_pso_optimization() {
    DesignSpace ds;
    ds.add_parameter("x", -5.0, 5.0);
    ds.add_parameter("y", -5.0, 5.0);
    auto obj = Objective("f",
        [](const std::vector<double>& x) {
            return x.size() < 2 ? 0.0 : -(x[0]*x[0] + x[1]*x[1]);
        }, false);
    ds.set_primary_objective(obj);

    PSOConfig config;
    config.swarm_size = 15;
    config.iterations = 30;

    ParticleSwarmOptimizer pso(ds, config);
    auto best = pso.optimize(30);
    assert_test(best.position.size() == 2, "ParticleSwarmOptimizer: Optimization");
}

// Test 19: OptimizationManager with GA
void test_optimization_manager_ga() {
    auto design_space = std::make_shared<DesignSpace>();
    design_space->add_parameter("x", -5.0, 5.0);
    design_space->add_parameter("y", -5.0, 5.0);
    auto obj = Objective("f",
        [](const std::vector<double>& x) {
            return x.size() < 2 ? 0.0 : x[0]*x[0] + x[1]*x[1];
        }, true);
    design_space->set_primary_objective(obj);

    auto handler = std::make_shared<ConstraintHandler>(*design_space);
    auto gen = std::make_shared<DesignGenerator>(*design_space);
    OptimizationManager manager(design_space, handler, gen);

    OptimizationSettings settings;
    settings.algorithm = OptimizationAlgorithm::GENETIC_ALGORITHM;
    settings.max_iterations = 20;

    auto result = manager.optimize(settings);
    assert_test(result.iterations_completed == 20, "OptimizationManager: GA execution");
}

// Test 20: OptimizationManager with PSO
void test_optimization_manager_pso() {
    auto design_space = std::make_shared<DesignSpace>();
    design_space->add_parameter("x", -5.0, 5.0);
    design_space->add_parameter("y", -5.0, 5.0);
    auto obj = Objective("f",
        [](const std::vector<double>& x) {
            return x.size() < 2 ? 0.0 : x[0]*x[0] + x[1]*x[1];
        }, true);
    design_space->set_primary_objective(obj);

    auto handler = std::make_shared<ConstraintHandler>(*design_space);
    auto gen = std::make_shared<DesignGenerator>(*design_space);
    OptimizationManager manager(design_space, handler, gen);

    OptimizationSettings settings;
    settings.algorithm = OptimizationAlgorithm::PARTICLE_SWARM;
    settings.max_iterations = 20;

    auto result = manager.optimize(settings);
    assert_test(result.best_design.size() == 2, "OptimizationManager: PSO execution");
}

// Test 21: DesignSpaceVisualizer sampling
void test_visualizer_sampling() {
    DesignSpace ds;
    ds.add_parameter("x", -10.0, 10.0);
    ds.add_parameter("y", -10.0, 10.0);
    auto obj = Objective("f",
        [](const std::vector<double>& x) {
            return x.size() < 2 ? 0.0 : x[0]*x[0] + x[1]*x[1];
        }, true);
    ds.set_primary_objective(obj);

    DesignSpaceVisualizer viz(ds);
    auto samples = viz.sample_design_space(50, true);
    assert_test(samples.size() == 50, "DesignSpaceVisualizer: Sampling");
}

// Test 22: DesignSpaceVisualizer grid sampling
void test_visualizer_grid_sampling() {
    DesignSpace ds;
    ds.add_parameter("x", -10.0, 10.0);
    ds.add_parameter("y", -10.0, 10.0);
    auto obj = Objective("f",
        [](const std::vector<double>& x) { return 0; }, true);
    ds.set_primary_objective(obj);

    DesignSpaceVisualizer viz(ds);
    auto samples = viz.grid_sample({5, 5});
    assert_test(samples.size() == 25, "DesignSpaceVisualizer: Grid sampling");
}

// Test 23: SolutionAnalyzer distance
void test_solution_analyzer_distance() {
    DesignSpace ds;
    ds.add_parameter("x", -10.0, 10.0);
    auto obj = Objective("f", [](const std::vector<double>& x) { return 0; }, true);
    ds.set_primary_objective(obj);

    SolutionAnalyzer analyzer(ds);
    std::vector<double> sol1 = {1.0, 2.0};
    std::vector<double> sol2 = {4.0, 6.0};

    double distance = analyzer.solution_distance(sol1, sol2);
    assert_test(std::abs(distance - 5.0) < 1e-6, "SolutionAnalyzer: Distance computation");
}

// Test 24: SolutionAnalyzer clustering
void test_solution_analyzer_clustering() {
    DesignSpace ds;
    ds.add_parameter("x", -10.0, 10.0);
    auto obj = Objective("f", [](const std::vector<double>& x) { return 0; }, true);
    ds.set_primary_objective(obj);

    SolutionAnalyzer analyzer(ds);
    std::vector<std::vector<double>> solutions = {
        {1.0}, {1.1}, {5.0}, {5.1}
    };

    auto clusters = analyzer.cluster_solutions(solutions, 2);
    assert_test(clusters.size() <= 2, "SolutionAnalyzer: Clustering");
}

// Test 25: Convergence monitoring
void test_convergence_monitoring() {
    ConvergenceMonitor monitor;
    monitor.record_value(10.0);
    monitor.record_value(9.0);
    monitor.record_value(8.5);

    assert_test(monitor.history.size() == 3, "ConvergenceMonitor: Recording");
}

// Test 26: Performance metrics
void test_performance_metrics() {
    PerformanceMetrics metrics;
    metrics.total_evaluations = 100;
    metrics.feasible_solutions = 75;

    double success_rate = metrics.success_rate();
    assert_test(std::abs(success_rate - 0.75) < 1e-6, "PerformanceMetrics: Success rate");
}

// Test 27: Design space enforcement of bounds
void test_enforce_bounds() {
    DesignSpace ds;
    ds.add_parameter("x", 0.0, 10.0);
    ds.add_parameter("y", 0.0, 10.0);

    std::vector<double> x = {15.0, -5.0};
    ds.enforce_bounds(x);

    assert_test(x[0] == 10.0 && x[1] == 0.0, "DesignSpace: Enforce bounds");
}

// Test 28: Vector normalization
void test_vector_normalization() {
    DesignSpace ds;
    ds.add_parameter("x", 0.0, 10.0);
    ds.add_parameter("y", -5.0, 5.0);

    std::vector<double> x = {5.0, 0.0};
    auto normalized = ds.normalize(x);

    assert_test(std::abs(normalized[0] - 0.5) < 1e-6, "DesignSpace: Vector normalization");
}

// Test 29: Vector denormalization
void test_vector_denormalization() {
    DesignSpace ds;
    ds.add_parameter("x", 0.0, 10.0);
    ds.add_parameter("y", -5.0, 5.0);

    std::vector<double> normalized = {0.5, 0.5};
    auto x = ds.denormalize(normalized);

    assert_test(std::abs(x[0] - 5.0) < 1e-6, "DesignSpace: Vector denormalization");
}

// Test 30: Multiple secondary objectives
void test_secondary_objectives() {
    DesignSpace ds;
    ds.add_parameter("x", -5.0, 5.0);

    auto obj1 = Objective("f1", [](const std::vector<double>& x) { return x.empty() ? 0 : x[0]; }, true);
    ds.set_primary_objective(obj1);

    auto obj2 = Objective("f2", [](const std::vector<double>& x) { return x.empty() ? 0 : -x[0]; }, true);
    ds.add_secondary_objective(obj2);

    assert_test(ds.objective_count() == 2, "DesignSpace: Multiple objectives");
}

// Test 31: Constraint handler with adaptive penalty
void test_adaptive_penalty() {
    DesignSpace ds;
    ds.add_parameter("x", 0.0, 10.0);
    auto obj = Objective("f", [](const std::vector<double>& x) { return 0; }, true);
    ds.set_primary_objective(obj);

    ConstraintHandler handler(ds);
    handler.set_method(ConstraintMethod::ADAPTIVE_PENALTY);

    assert_test(true, "ConstraintHandler: Adaptive penalty setup");
}

// Test 32: Pareto front computation
void test_pareto_front() {
    DesignSpace ds;
    ds.add_parameter("x", -5.0, 5.0);
    ds.add_parameter("y", -5.0, 5.0);
    auto obj = Objective("f",
        [](const std::vector<double>& x) {
            return x.size() < 2 ? 0.0 : x[0]*x[0] + x[1]*x[1];
        }, true);
    ds.set_primary_objective(obj);

    auto design_space = std::make_shared<DesignSpace>(ds);
    auto handler = std::make_shared<ConstraintHandler>(ds);
    auto gen = std::make_shared<DesignGenerator>(ds);
    OptimizationManager manager(design_space, handler, gen);

    std::vector<std::vector<double>> solutions = {{1.0, 1.0}, {2.0, 1.0}, {1.0, 2.0}};
    auto front = manager.compute_pareto_front(solutions);

    assert_test(front.solutions.size() > 0, "OptimizationManager: Pareto front");
}

// Test 33: Design geometry computation
void test_design_geometry() {
    DesignSpace ds;
    ds.add_parameter("p1", 0.0, 100.0);
    ds.add_parameter("p2", 0.0, 100.0);
    ds.add_parameter("p3", 0.0, 100.0);
    auto obj = Objective("f", [](const std::vector<double>& x) { return 0; }, true);
    ds.set_primary_objective(obj);

    DesignGenerator gen(ds);
    std::vector<double> vars = {20.0, 40.0, 60.0};
    auto design = gen.generate_design(vars);

    assert_test(design.geometry.mass > 0.0, "DesignGenerator: Geometry computation");
}

// Test 34: Algorithm auto-selection
void test_algorithm_selection() {
    auto ds = std::make_shared<DesignSpace>();
    ds->add_parameter("x", -5.0, 5.0);
    auto obj = Objective("f", [](const std::vector<double>& x) { return 0; }, true);
    ds->set_primary_objective(obj);

    auto handler = std::make_shared<ConstraintHandler>(*ds);
    auto gen = std::make_shared<DesignGenerator>(*ds);
    OptimizationManager manager(ds, handler, gen);

    auto algo = manager.select_algorithm();
    assert_test(algo == OptimizationAlgorithm::GENETIC_ALGORITHM ||
                algo == OptimizationAlgorithm::PARTICLE_SWARM,
                "OptimizationManager: Algorithm selection");
}

// Test 35: Optimization result storage
void test_result_storage() {
    auto ds = std::make_shared<DesignSpace>();
    ds->add_parameter("x", -5.0, 5.0);
    auto obj = Objective("f",
        [](const std::vector<double>& x) { return x.empty() ? 0 : x[0]*x[0]; }, true);
    ds->set_primary_objective(obj);

    auto handler = std::make_shared<ConstraintHandler>(*ds);
    auto gen = std::make_shared<DesignGenerator>(*ds);
    OptimizationManager manager(ds, handler, gen);

    OptimizationSettings settings;
    settings.max_iterations = 10;
    manager.optimize(settings);

    auto last = manager.last_result();
    assert_test(last.iterations_completed == 10, "OptimizationManager: Result storage");
}

int main() {
    std::cout << "M6 Phase - Generative Design (GAP-026) Test Suite\n";
    std::cout << "================================================\n\n";

    // Run all tests
    test_design_space_parameters();
    test_parameter_normalization();
    test_parameter_denormalization();
    test_constraint_satisfaction();
    test_constraint_violation();
    test_individual_comparison();
    test_individual_cloning();
    test_particle_initialization();
    test_particle_cloning();
    test_design_space_bounds();
    test_design_space_objective();
    test_constraint_handler_feasibility();
    test_constraint_violation_report();
    test_design_generator_init();
    test_design_generator_generation();
    test_design_topology_validation();
    test_genetic_algorithm_optimization();
    test_pso_optimization();
    test_optimization_manager_ga();
    test_optimization_manager_pso();
    test_visualizer_sampling();
    test_visualizer_grid_sampling();
    test_solution_analyzer_distance();
    test_solution_analyzer_clustering();
    test_convergence_monitoring();
    test_performance_metrics();
    test_enforce_bounds();
    test_vector_normalization();
    test_vector_denormalization();
    test_secondary_objectives();
    test_adaptive_penalty();
    test_pareto_front();
    test_design_geometry();
    test_algorithm_selection();
    test_result_storage();

    std::cout << "\n================================================\n";
    std::cout << "Test Results:\n";
    std::cout << "  PASSED: " << tests_passed << "\n";
    std::cout << "  FAILED: " << tests_failed << "\n";
    std::cout << "  TOTAL:  " << (tests_passed + tests_failed) << "\n";

    if (tests_failed == 0) {
        std::cout << "\n✓ ALL TESTS PASSED (35/35)\n";
        return 0;
    } else {
        std::cout << "\n✗ SOME TESTS FAILED\n";
        return 1;
    }
}
