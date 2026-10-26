#pragma once

#include <vector>
#include <random>
#include <algorithm>
#include <memory>
#include "design_space.hpp"

namespace bendcore::generative {

/**
 * Genetic Algorithm Engine
 * Population-based optimization using Darwin's evolution principles
 */

// Individual in the population
struct Individual {
    std::vector<double> chromosome;  // Design variables
    double fitness = 0.0;
    double constraint_violation = 0.0;
    bool is_feasible = false;

    Individual() = default;
    explicit Individual(size_t dimension) : chromosome(dimension, 0.5) {}
    Individual(const std::vector<double>& genes) : chromosome(genes) {}

    [[nodiscard]] Individual clone() const;
    [[nodiscard]] bool operator<(const Individual& other) const;
    [[nodiscard]] bool operator>(const Individual& other) const;
};

// Genetic Algorithm configuration
struct GAConfig {
    size_t population_size = 100;
    size_t generations = 200;
    double mutation_rate = 0.1;
    double crossover_rate = 0.8;
    double elite_fraction = 0.05;
    bool adaptive_mutation = true;
    double mutation_std_dev = 0.1;
};

// Selection strategy
enum class SelectionStrategy {
    ROULETTE_WHEEL,
    TOURNAMENT,
    RANK_BASED
};

// Crossover strategy
enum class CrossoverStrategy {
    SINGLE_POINT,
    MULTI_POINT,
    UNIFORM,
    BLEND
};

// Mutation strategy
enum class MutationStrategy {
    GAUSSIAN,
    UNIFORM,
    POLYNOMIAL
};

// Genetic Algorithm optimizer
class GeneticAlgorithm {
public:
    GeneticAlgorithm(const DesignSpace& space, const GAConfig& config = GAConfig());

    // Run optimization
    [[nodiscard]] Individual optimize(size_t max_iterations = 0);

    // Get results
    [[nodiscard]] const std::vector<Individual>& population() const { return population_; }
    [[nodiscard]] const Individual& best_individual() const { return best_individual_; }
    [[nodiscard]] std::vector<double> convergence_history() const { return convergence_history_; }
    [[nodiscard]] size_t generation_count() const { return generation_count_; }

    // Configuration
    void set_selection_strategy(SelectionStrategy strategy) { selection_strategy_ = strategy; }
    void set_crossover_strategy(CrossoverStrategy strategy) { crossover_strategy_ = strategy; }
    void set_mutation_strategy(MutationStrategy strategy) { mutation_strategy_ = strategy; }

    // Control
    void set_elitism(bool enable) { use_elitism_ = enable; }
    void set_adaptive_mutation(bool enable) { config_.adaptive_mutation = enable; }
    void set_mutation_rate(double rate) { config_.mutation_rate = rate; }

private:
    const DesignSpace& design_space_;
    GAConfig config_;
    std::vector<Individual> population_;
    Individual best_individual_;
    std::vector<double> convergence_history_;
    size_t generation_count_ = 0;

    SelectionStrategy selection_strategy_ = SelectionStrategy::TOURNAMENT;
    CrossoverStrategy crossover_strategy_ = CrossoverStrategy::UNIFORM;
    MutationStrategy mutation_strategy_ = MutationStrategy::GAUSSIAN;
    bool use_elitism_ = true;

    std::mt19937 rng_;

    // Internal operations
    void initialize_population();
    void evaluate_population();
    void evolve_generation();
    void evaluate_individual(Individual& individual);

    // Selection
    size_t select_parent();
    size_t roulette_wheel_select();
    size_t tournament_select();
    size_t rank_based_select();

    // Crossover
    Individual crossover(const Individual& parent1, const Individual& parent2);
    Individual single_point_crossover(const Individual& parent1, const Individual& parent2);
    Individual multi_point_crossover(const Individual& parent1, const Individual& parent2);
    Individual uniform_crossover(const Individual& parent1, const Individual& parent2);
    Individual blend_crossover(const Individual& parent1, const Individual& parent2);

    // Mutation
    void mutate(Individual& individual);
    void gaussian_mutation(Individual& individual);
    void uniform_mutation(Individual& individual);
    void polynomial_mutation(Individual& individual);

    // Utility
    [[nodiscard]] double compute_fitness(const Individual& individual) const;
    [[nodiscard]] double adapt_mutation_rate() const;
};

} // namespace bendcore::generative
