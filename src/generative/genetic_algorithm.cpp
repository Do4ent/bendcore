#include "../../include/bendcore/generative/genetic_algorithm.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace bendcore::generative {

// Individual methods
Individual Individual::clone() const {
    Individual cloned;
    cloned.chromosome = chromosome;
    cloned.fitness = fitness;
    cloned.constraint_violation = constraint_violation;
    cloned.is_feasible = is_feasible;
    return cloned;
}

bool Individual::operator<(const Individual& other) const {
    if (is_feasible != other.is_feasible) return is_feasible;
    if (is_feasible) return fitness < other.fitness;
    return constraint_violation < other.constraint_violation;
}

bool Individual::operator>(const Individual& other) const {
    return other < *this;
}

// GeneticAlgorithm methods
GeneticAlgorithm::GeneticAlgorithm(const DesignSpace& space, const GAConfig& config)
    : design_space_(space), config_(config), rng_(std::random_device{}()) {
    population_.resize(config_.population_size);
    best_individual_ = Individual(space.parameter_count());
}

Individual GeneticAlgorithm::optimize(size_t max_iterations) {
    size_t iterations = max_iterations > 0 ? max_iterations : config_.generations;
    generation_count_ = 0;
    convergence_history_.clear();

    initialize_population();
    evaluate_population();
    best_individual_ = *std::max_element(population_.begin(), population_.end(),
                                         [](const Individual& a, const Individual& b) { return a > b; });

    for (generation_count_ = 0; generation_count_ < iterations; ++generation_count_) {
        evolve_generation();
        evaluate_population();

        auto current_best = *std::max_element(population_.begin(), population_.end(),
                                             [](const Individual& a, const Individual& b) { return a > b; });

        if (current_best.fitness > best_individual_.fitness ||
            (!current_best.is_feasible && best_individual_.is_feasible == false &&
             current_best.constraint_violation < best_individual_.constraint_violation)) {
            best_individual_ = current_best;
        }

        convergence_history_.push_back(best_individual_.fitness);
    }

    return best_individual_;
}

void GeneticAlgorithm::initialize_population() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    for (auto& individual : population_) {
        individual.chromosome.resize(design_space_.parameter_count());
        for (auto& gene : individual.chromosome) {
            gene = dist(rng_);
        }
    }
}

void GeneticAlgorithm::evaluate_population() {
    for (auto& individual : population_) {
        evaluate_individual(individual);
    }
}

void GeneticAlgorithm::evaluate_individual(Individual& individual) {
    auto x = design_space_.denormalize(individual.chromosome);
    design_space_.enforce_bounds(x);

    individual.constraint_violation = design_space_.constraint_violation(x);
    individual.is_feasible = (individual.constraint_violation < 1e-6);

    if (individual.is_feasible) {
        individual.fitness = design_space_.primary_objective().compute(x);
    } else {
        individual.fitness = -individual.constraint_violation;
    }
}

void GeneticAlgorithm::evolve_generation() {
    std::vector<Individual> new_population;

    if (use_elitism_) {
        auto num_elite = static_cast<size_t>(config_.population_size * config_.elite_fraction);
        std::vector<Individual> sorted_pop = population_;
        std::sort(sorted_pop.begin(), sorted_pop.end(),
                 [](const Individual& a, const Individual& b) { return a > b; });

        for (size_t i = 0; i < num_elite && i < sorted_pop.size(); ++i) {
            new_population.push_back(sorted_pop[i].clone());
        }
    }

    while (new_population.size() < population_.size()) {
        size_t parent1_idx = select_parent();
        size_t parent2_idx = select_parent();

        Individual offspring = crossover(population_[parent1_idx], population_[parent2_idx]);

        std::uniform_real_distribution<double> dist(0.0, 1.0);
        if (dist(rng_) < config_.mutation_rate) {
            mutate(offspring);
        }

        design_space_.enforce_bounds(offspring.chromosome);
        new_population.push_back(offspring);
    }

    population_ = new_population;
}

size_t GeneticAlgorithm::select_parent() {
    switch (selection_strategy_) {
        case SelectionStrategy::ROULETTE_WHEEL:
            return roulette_wheel_select();
        case SelectionStrategy::TOURNAMENT:
            return tournament_select();
        case SelectionStrategy::RANK_BASED:
            return rank_based_select();
        default:
            return tournament_select();
    }
}

size_t GeneticAlgorithm::roulette_wheel_select() {
    double total_fitness = 0.0;
    double min_fitness = population_[0].fitness;

    for (const auto& ind : population_) {
        min_fitness = std::min(min_fitness, ind.fitness);
    }

    for (const auto& ind : population_) {
        total_fitness += (ind.fitness - min_fitness + 1e-6);
    }

    std::uniform_real_distribution<double> dist(0.0, total_fitness);
    double rand = dist(rng_);
    double cumulative = 0.0;

    for (size_t i = 0; i < population_.size(); ++i) {
        cumulative += (population_[i].fitness - min_fitness + 1e-6);
        if (rand <= cumulative) return i;
    }

    return population_.size() - 1;
}

size_t GeneticAlgorithm::tournament_select() {
    const size_t tournament_size = 3;
    std::uniform_int_distribution<size_t> dist(0, population_.size() - 1);

    size_t best_idx = dist(rng_);
    for (size_t i = 1; i < tournament_size; ++i) {
        size_t idx = dist(rng_);
        if (population_[idx] > population_[best_idx]) {
            best_idx = idx;
        }
    }
    return best_idx;
}

size_t GeneticAlgorithm::rank_based_select() {
    std::vector<std::pair<size_t, Individual>> ranked;
    for (size_t i = 0; i < population_.size(); ++i) {
        ranked.emplace_back(i, population_[i]);
    }

    std::sort(ranked.begin(), ranked.end(),
             [](const auto& a, const auto& b) { return a.second > b.second; });

    std::uniform_real_distribution<double> dist(0.0, 1.0);
    double p = 2.0 / (population_.size() + 1);

    for (size_t i = 0; i < ranked.size(); ++i) {
        if (dist(rng_) < p * (1.0 - p) / population_.size()) {
            return ranked[i].first;
        }
    }

    return ranked[0].first;
}

Individual GeneticAlgorithm::crossover(const Individual& parent1, const Individual& parent2) {
    switch (crossover_strategy_) {
        case CrossoverStrategy::SINGLE_POINT:
            return single_point_crossover(parent1, parent2);
        case CrossoverStrategy::MULTI_POINT:
            return multi_point_crossover(parent1, parent2);
        case CrossoverStrategy::UNIFORM:
            return uniform_crossover(parent1, parent2);
        case CrossoverStrategy::BLEND:
            return blend_crossover(parent1, parent2);
        default:
            return uniform_crossover(parent1, parent2);
    }
}

Individual GeneticAlgorithm::single_point_crossover(const Individual& parent1, const Individual& parent2) {
    Individual offspring(parent1);
    if (parent1.chromosome.empty()) return offspring;

    std::uniform_int_distribution<size_t> dist(0, parent1.chromosome.size() - 1);
    size_t crossover_point = dist(rng_);

    for (size_t i = crossover_point; i < parent1.chromosome.size(); ++i) {
        offspring.chromosome[i] = parent2.chromosome[i];
    }

    return offspring;
}

Individual GeneticAlgorithm::multi_point_crossover(const Individual& parent1, const Individual& parent2) {
    Individual offspring(parent1);
    const size_t num_points = 2;

    std::uniform_int_distribution<size_t> dist(0, parent1.chromosome.size() - 1);
    std::vector<size_t> points;
    for (size_t i = 0; i < num_points; ++i) {
        points.push_back(dist(rng_));
    }
    std::sort(points.begin(), points.end());

    bool swap = true;
    size_t last_point = 0;
    for (size_t point : points) {
        if (swap) {
            for (size_t i = last_point; i < point; ++i) {
                offspring.chromosome[i] = parent2.chromosome[i];
            }
        }
        swap = !swap;
        last_point = point;
    }

    return offspring;
}

Individual GeneticAlgorithm::uniform_crossover(const Individual& parent1, const Individual& parent2) {
    Individual offspring(parent1);
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    for (size_t i = 0; i < offspring.chromosome.size(); ++i) {
        if (dist(rng_) < 0.5) {
            offspring.chromosome[i] = parent2.chromosome[i];
        }
    }

    return offspring;
}

Individual GeneticAlgorithm::blend_crossover(const Individual& parent1, const Individual& parent2) {
    Individual offspring(parent1);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    const double alpha = 0.1;

    for (size_t i = 0; i < offspring.chromosome.size(); ++i) {
        double min_val = std::min(parent1.chromosome[i], parent2.chromosome[i]);
        double max_val = std::max(parent1.chromosome[i], parent2.chromosome[i]);
        double range = max_val - min_val;

        min_val -= alpha * range;
        max_val += alpha * range;

        offspring.chromosome[i] = min_val + dist(rng_) * (max_val - min_val);
        offspring.chromosome[i] = std::max(0.0, std::min(1.0, offspring.chromosome[i]));
    }

    return offspring;
}

void GeneticAlgorithm::mutate(Individual& individual) {
    switch (mutation_strategy_) {
        case MutationStrategy::GAUSSIAN:
            gaussian_mutation(individual);
            break;
        case MutationStrategy::UNIFORM:
            uniform_mutation(individual);
            break;
        case MutationStrategy::POLYNOMIAL:
            polynomial_mutation(individual);
            break;
    }
}

void GeneticAlgorithm::gaussian_mutation(Individual& individual) {
    std::normal_distribution<double> dist(0.0, config_.mutation_std_dev);

    for (auto& gene : individual.chromosome) {
        gene += dist(rng_);
        gene = std::max(0.0, std::min(1.0, gene));
    }
}

void GeneticAlgorithm::uniform_mutation(Individual& individual) {
    std::uniform_real_distribution<double> dist(-config_.mutation_std_dev, config_.mutation_std_dev);

    for (auto& gene : individual.chromosome) {
        gene += dist(rng_);
        gene = std::max(0.0, std::min(1.0, gene));
    }
}

void GeneticAlgorithm::polynomial_mutation(Individual& individual) {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    const double eta = 20.0;  // Distribution index

    for (auto& gene : individual.chromosome) {
        if (dist(rng_) < 0.5) {
            double delta = std::pow(2.0 * dist(rng_), 1.0 / (eta + 1.0)) - 1.0;
            gene += delta * 0.1;
        } else {
            double delta = 1.0 - std::pow(2.0 * (1.0 - dist(rng_)), 1.0 / (eta + 1.0));
            gene += delta * 0.1;
        }
        gene = std::max(0.0, std::min(1.0, gene));
    }
}

double GeneticAlgorithm::compute_fitness(const Individual& individual) const {
    return individual.fitness;
}

double GeneticAlgorithm::adapt_mutation_rate() const {
    if (!config_.adaptive_mutation) return config_.mutation_rate;

    double progress = static_cast<double>(generation_count_) / config_.generations;
    return config_.mutation_rate * (1.0 - progress);
}

} // namespace bendcore::generative
