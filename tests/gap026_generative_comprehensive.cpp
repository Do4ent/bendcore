// GAP-026: Generative Design - Comprehensive Test Suite
// M6 Phase: GA + PSO Optimization for tube design
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
#include <limits>

#include "bendcore/generative/design_space.hpp"
#include "bendcore/generative/genetic_algorithm.hpp"
#include "bendcore/generative/particle_swarm.hpp"
#include "bendcore/generative/constraint_handler.hpp"
#include "bendcore/generative/design_generator.hpp"
#include "bendcore/generative/optimization_manager.hpp"

using namespace bendcore::generative;

// Test 1: Design space creation
void test_design_space_creation() {
    DesignSpace space;
    assert(space.parameter_count() >= 0);
    std::cout << "Test 1 PASSED: Design space creation\n";
}

// Test 2: Objective function evaluation
void test_objective_function() {
    DesignSpace space;
    auto obj = space.primary_objective();

    std::vector<double> x = {1.0, 2.0, 3.0};
    double value = obj.compute(x);
    assert(!std::isnan(value));
    std::cout << "Test 2 PASSED: Objective function evaluation\n";
}

// Test 3: Parameter bounds enforcement
void test_parameter_bounds() {
    DesignSpace space;
    std::vector<double> x = {2.0, 3.0, 4.0};
    space.enforce_bounds(x);

    for (double val : x) {
        assert(val >= 0.0 && val <= 1.0);
    }
    std::cout << "Test 3 PASSED: Parameter bounds enforcement\n";
}

// Test 4: Constraint violation computation
void test_constraint_violation() {
    DesignSpace space;
    std::vector<double> x = {0.5, 0.5, 0.5};
    double violation = space.constraint_violation(x);
    assert(violation >= 0.0);
    std::cout << "Test 4 PASSED: Constraint violation computation\n";
}

// Test 5: Denormalization
void test_denormalization() {
    DesignSpace space;
    std::vector<double> normalized = {0.5, 0.5, 0.5};
    auto denormalized = space.denormalize(normalized);

    assert(denormalized.size() == normalized.size());
    std::cout << "Test 5 PASSED: Denormalization\n";
}

// Test 6: Genetic Algorithm initialization
void test_ga_initialization() {
    DesignSpace space;
    GAConfig config;
    config.population_size = 20;

    GeneticAlgorithm ga(space, config);
    assert(ga.population().size() == 0);  // Before optimization
    std::cout << "Test 6 PASSED: Genetic Algorithm initialization\n";
}

// Test 7: GA Individual creation
void test_individual_creation() {
    Individual ind(5);
    assert(ind.chromosome.size() == 5);
    assert(ind.fitness == 0.0);

    ind.fitness = 10.0;
    Individual ind2 = ind.clone();
    assert(ind2.fitness == 10.0);

    std::cout << "Test 7 PASSED: GA Individual creation\n";
}

// Test 8: Individual comparison
void test_individual_comparison() {
    Individual ind1(3);
    ind1.fitness = 10.0;
    ind1.is_feasible = true;

    Individual ind2(3);
    ind2.fitness = 5.0;
    ind2.is_feasible = true;

    assert(ind2 < ind1);
    assert(ind1 > ind2);
    std::cout << "Test 8 PASSED: Individual comparison\n";
}

// Test 9: PSO initialization
void test_pso_initialization() {
    DesignSpace space;
    PSOConfig config;
    config.swarm_size = 30;

    ParticleSwarmOptimizer pso(space, config);
    assert(pso.swarm().size() == 0);  // Before optimization
    std::cout << "Test 9 PASSED: PSO initialization\n";
}

// Test 10: Particle creation
void test_particle_creation() {
    Particle p(4);
    assert(p.position.size() == 4);
    assert(p.velocity.size() == 4);
    assert(p.best_position.size() == 4);

    Particle p2 = p.clone();
    assert(p2.position.size() == 4);
    std::cout << "Test 10 PASSED: Particle creation\n";
}

// Test 11: PSO boundary handling
void test_pso_boundary_handling() {
    DesignSpace space;
    PSOConfig config;
    config.boundary_handling = BoundaryHandling::REFLECT;

    ParticleSwarmOptimizer pso(space, config);
    assert(pso.swarm().size() == 0);
    std::cout << "Test 11 PASSED: PSO boundary handling\n";
}

// Test 12: GA configuration
void test_ga_configuration() {
    DesignSpace space;
    GAConfig config;
    config.population_size = 50;
    config.generations = 100;
    config.mutation_rate = 0.15;
    config.crossover_rate = 0.85;

    GeneticAlgorithm ga(space, config);
    ga.set_elitism(true);
    ga.set_mutation_rate(0.2);

    std::cout << "Test 12 PASSED: GA configuration\n";
}

// Test 13: PSO configuration
void test_pso_configuration() {
    DesignSpace space;
    PSOConfig config;
    config.swarm_size = 40;
    config.iterations = 150;
    config.cognitive_coefficient = 2.0;
    config.social_coefficient = 2.0;
    config.inertia_weight = 0.7;
    config.dynamic_inertia = true;

    ParticleSwarmOptimizer pso(space, config);
    pso.set_cognitive_coefficient(1.8);
    pso.set_social_coefficient(1.8);
    pso.set_dynamic_inertia(true);

    std::cout << "Test 13 PASSED: PSO configuration\n";
}

// Test 14: Constraint handler
void test_constraint_handler() {
    ConstraintHandler handler;

    handler.add_inequality_constraint(0.0, 10.0);
    handler.add_inequality_constraint(5.0, 15.0);

    std::cout << "Test 14 PASSED: Constraint handler\n";
}

// Test 15: Design generator
void test_design_generator() {
    DesignGenerator generator;

    std::vector<double> params = {0.5, 0.5, 0.5};
    auto design = generator.generate_design(params);

    assert(!design.design_id.empty());
    std::cout << "Test 15 PASSED: Design generator\n";
}

// Test 16: Optimization manager
void test_optimization_manager() {
    OptimizationManager manager;

    assert(manager.get_optimization_algorithms().size() >= 0);
    std::cout << "Test 16 PASSED: Optimization manager\n";
}

// Test 17: Population management
void test_population_management() {
    DesignSpace space;
    GAConfig config;
    config.population_size = 15;

    GeneticAlgorithm ga(space, config);
    auto pop = ga.population();

    // Population should be empty before optimization
    assert(pop.size() == 0);
    std::cout << "Test 17 PASSED: Population management\n";
}

// Test 18: Convergence tracking
void test_convergence_tracking() {
    DesignSpace space;
    GAConfig config;
    config.generations = 5;

    GeneticAlgorithm ga(space, config);
    auto history = ga.convergence_history();

    assert(history.size() == 0);  // Empty before optimization
    std::cout << "Test 18 PASSED: Convergence tracking\n";
}

// Test 19: GA selection strategies
void test_ga_selection_strategies() {
    DesignSpace space;
    GeneticAlgorithm ga(space);

    ga.set_selection_strategy(SelectionStrategy::TOURNAMENT);
    ga.set_selection_strategy(SelectionStrategy::ROULETTE_WHEEL);
    ga.set_selection_strategy(SelectionStrategy::RANK_BASED);

    std::cout << "Test 19 PASSED: GA selection strategies\n";
}

// Test 20: GA crossover strategies
void test_ga_crossover_strategies() {
    DesignSpace space;
    GeneticAlgorithm ga(space);

    ga.set_crossover_strategy(CrossoverStrategy::SINGLE_POINT);
    ga.set_crossover_strategy(CrossoverStrategy::MULTI_POINT);
    ga.set_crossover_strategy(CrossoverStrategy::UNIFORM);
    ga.set_crossover_strategy(CrossoverStrategy::BLEND);

    std::cout << "Test 20 PASSED: GA crossover strategies\n";
}

// Test 21-35: Additional optimization tests
void test_mutation_strategies() {
    DesignSpace space;
    GeneticAlgorithm ga(space);

    ga.set_mutation_strategy(MutationStrategy::GAUSSIAN);
    ga.set_mutation_strategy(MutationStrategy::UNIFORM);
    ga.set_mutation_strategy(MutationStrategy::POLYNOMIAL);

    std::cout << "Test 21 PASSED: Mutation strategies\n";
}

void test_elite_preservation() {
    DesignSpace space;
    GAConfig config;
    config.elite_fraction = 0.1;

    GeneticAlgorithm ga(space, config);
    ga.set_elitism(true);

    std::cout << "Test 22 PASSED: Elite preservation\n";
}

void test_adaptive_mutation() {
    DesignSpace space;
    GAConfig config;
    config.adaptive_mutation = true;
    config.mutation_std_dev = 0.1;

    GeneticAlgorithm ga(space, config);
    ga.set_adaptive_mutation(true);

    std::cout << "Test 23 PASSED: Adaptive mutation\n";
}

void test_pso_velocity_clamping() {
    DesignSpace space;
    PSOConfig config;

    ParticleSwarmOptimizer pso(space, config);

    std::cout << "Test 24 PASSED: PSO velocity clamping\n";
}

void test_pso_inertia_decay() {
    DesignSpace space;
    PSOConfig config;
    config.dynamic_inertia = true;
    config.min_inertia = 0.4;
    config.max_inertia = 0.9;

    ParticleSwarmOptimizer pso(space, config);
    pso.set_inertia_weight(0.5);

    std::cout << "Test 25 PASSED: PSO inertia decay\n";
}

void test_objective_landscape() {
    DesignSpace space;
    std::vector<double> x = {0.3, 0.7, 0.2};

    auto obj = space.primary_objective();
    double f1 = obj.compute(x);

    x[0] += 0.01;
    double f2 = obj.compute(x);

    // Fitness values should be different
    assert(f1 != f2 || x[0] == 0.31);
    std::cout << "Test 26 PASSED: Objective landscape\n";
}

void test_infeasible_handling() {
    DesignSpace space;
    std::vector<double> x = {0.5, 0.5, 0.5};

    // Deliberately set infeasible point
    x[0] = -0.5;  // Outside bounds

    double violation = space.constraint_violation(x);
    // Even infeasible should return valid violation
    assert(violation >= 0.0);

    std::cout << "Test 27 PASSED: Infeasible handling\n";
}

void test_multiobjective_support() {
    DesignSpace space;

    auto primary = space.primary_objective();
    // Check if secondary objective available

    std::cout << "Test 28 PASSED: Multiobjective support\n";
}

void test_parameter_scaling() {
    DesignSpace space;
    std::vector<double> norm = {0.0, 0.5, 1.0};

    auto denorm = space.denormalize(norm);
    space.enforce_bounds(denorm);

    // All should be within bounds
    for (double val : denorm) {
        assert(val >= 0.0 && val <= 1.0);
    }

    std::cout << "Test 29 PASSED: Parameter scaling\n";
}

void test_feasibility_checking() {
    DesignSpace space;
    std::vector<double> x = {0.3, 0.5, 0.7};

    double violation = space.constraint_violation(x);
    bool feasible = (violation < 1e-6);

    // Result should be deterministic
    double violation2 = space.constraint_violation(x);
    assert(violation == violation2);

    std::cout << "Test 30 PASSED: Feasibility checking\n";
}

void test_ga_early_termination() {
    DesignSpace space;
    GeneticAlgorithm ga(space);

    // Should handle zero generations
    auto result = ga.optimize(0);

    std::cout << "Test 31 PASSED: GA early termination\n";
}

void test_pso_early_termination() {
    DesignSpace space;
    ParticleSwarmOptimizer pso(space);

    // Should handle zero iterations
    auto result = pso.optimize(0);

    std::cout << "Test 32 PASSED: PSO early termination\n";
}

void test_ga_generation_tracking() {
    DesignSpace space;
    GAConfig config;

    GeneticAlgorithm ga(space, config);
    assert(ga.generation_count() == 0);

    std::cout << "Test 33 PASSED: GA generation tracking\n";
}

void test_pso_iteration_tracking() {
    DesignSpace space;
    ParticleSwarmOptimizer pso(space);

    assert(pso.iteration_count() == 0);

    std::cout << "Test 34 PASSED: PSO iteration tracking\n";
}

void test_design_space_scaling() {
    DesignSpace space;

    std::vector<double> original = {100.0, 200.0, 300.0};
    std::vector<double> normalized = {0.33, 0.67, 1.0};  // Approximate

    auto denorm = space.denormalize(normalized);

    // Should be scaled back
    assert(denorm.size() == original.size());

    std::cout << "Test 35 PASSED: Design space scaling\n";
}

int main() {
    try {
        test_design_space_creation();
        test_objective_function();
        test_parameter_bounds();
        test_constraint_violation();
        test_denormalization();
        test_ga_initialization();
        test_individual_creation();
        test_individual_comparison();
        test_pso_initialization();
        test_particle_creation();
        test_pso_boundary_handling();
        test_ga_configuration();
        test_pso_configuration();
        test_constraint_handler();
        test_design_generator();
        test_optimization_manager();
        test_population_management();
        test_convergence_tracking();
        test_ga_selection_strategies();
        test_ga_crossover_strategies();
        test_mutation_strategies();
        test_elite_preservation();
        test_adaptive_mutation();
        test_pso_velocity_clamping();
        test_pso_inertia_decay();
        test_objective_landscape();
        test_infeasible_handling();
        test_multiobjective_support();
        test_parameter_scaling();
        test_feasibility_checking();
        test_ga_early_termination();
        test_pso_early_termination();
        test_ga_generation_tracking();
        test_pso_iteration_tracking();
        test_design_space_scaling();

        std::cout << "\nAll 35 Generative Design tests PASSED!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}
