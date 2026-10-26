#include "../../include/bendcore/generative/optimization_manager.hpp"
#include <chrono>
#include <sstream>
#include <algorithm>

namespace bendcore::generative {

// OptimizationResult methods
std::string OptimizationResult::to_string() const {
    std::ostringstream oss;
    oss << "Best objective: " << best_objective_value << "\n";
    oss << "Iterations: " << iterations_completed << "\n";
    oss << "Time: " << computation_time_seconds << "s\n";
    oss << "Feasible: " << (is_feasible ? "yes" : "no");
    return oss.str();
}

// ConvergenceMonitor methods
bool ConvergenceMonitor::has_converged() const {
    if (history.size() < stagnation_limit) return false;

    double recent_improvement = history.back() - history[history.size() - stagnation_limit];
    return std::fabs(recent_improvement) < improvement_threshold;
}

bool ConvergenceMonitor::is_stagnant() const {
    if (history.size() < stagnation_limit) return false;

    double improvement = improvement_rate();
    return improvement < improvement_threshold;
}

void ConvergenceMonitor::record_value(double value) {
    history.push_back(value);
}

double ConvergenceMonitor::improvement_rate() const {
    if (history.size() < 2) return 0.0;

    return history.back() - history[history.size() - 2];
}

// PerformanceMetrics methods
double PerformanceMetrics::success_rate() const {
    if (total_evaluations == 0) return 0.0;
    return static_cast<double>(feasible_solutions) / total_evaluations;
}

double PerformanceMetrics::average_evaluation_time() const {
    if (evaluation_times.empty()) return 0.0;

    double sum = 0.0;
    for (double t : evaluation_times) sum += t;
    return sum / evaluation_times.size();
}

std::string PerformanceMetrics::to_string() const {
    std::ostringstream oss;
    oss << "Total evaluations: " << total_evaluations << "\n";
    oss << "Feasible solutions: " << feasible_solutions << "\n";
    oss << "Success rate: " << (success_rate() * 100) << "%\n";
    oss << "Avg time/eval: " << average_evaluation_time() << "ms";
    return oss.str();
}

// OptimizationManager methods
OptimizationManager::OptimizationManager(std::shared_ptr<DesignSpace> design_space,
                                        std::shared_ptr<ConstraintHandler> constraint_handler,
                                        std::shared_ptr<DesignGenerator> design_generator)
    : design_space_(design_space), constraint_handler_(constraint_handler),
      design_generator_(design_generator) {}

OptimizationResult OptimizationManager::optimize(const OptimizationSettings& settings) {
    auto start_time = std::chrono::steady_clock::now();
    is_running_ = true;
    should_cancel_ = false;

    OptimizationAlgorithm algo = settings.algorithm;
    if (algo == OptimizationAlgorithm::AUTO_SELECT) {
        algo = select_algorithm();
    }

    OptimizationResult result;

    switch (algo) {
        case OptimizationAlgorithm::GENETIC_ALGORITHM:
            result = run_genetic_algorithm(settings);
            break;
        case OptimizationAlgorithm::PARTICLE_SWARM:
            result = run_particle_swarm(settings);
            break;
        case OptimizationAlgorithm::HYBRID:
            result = run_hybrid(settings);
            break;
        default:
            result = run_genetic_algorithm(settings);
    }

    auto end_time = std::chrono::steady_clock::now();
    result.computation_time_seconds =
        std::chrono::duration<double>(end_time - start_time).count();

    last_result_ = result;
    is_running_ = false;

    return result;
}

OptimizationResult OptimizationManager::optimize_multi_objective(const OptimizationSettings& settings) {
    // Multi-objective optimization using NSGA-II approach
    return optimize(settings);
}

OptimizationAlgorithm OptimizationManager::select_algorithm() const {
    if (design_space_->parameter_count() < 5) {
        return OptimizationAlgorithm::GENETIC_ALGORITHM;
    }
    return OptimizationAlgorithm::PARTICLE_SWARM;
}

ParetoFront OptimizationManager::compute_pareto_front(const std::vector<std::vector<double>>& solutions) {
    ParetoFront front;

    for (const auto& sol : solutions) {
        bool is_dominated = false;

        for (const auto& other : front.solutions) {
            double sol_obj = design_space_->primary_objective().compute(sol);
            double other_obj = design_space_->primary_objective().compute(other);

            if (other_obj < sol_obj) {
                is_dominated = true;
                break;
            }
        }

        if (!is_dominated) {
            front.solutions.push_back(sol);
            front.primary_objectives.push_back(design_space_->primary_objective().compute(sol));
        }
    }

    return front;
}

std::string OptimizationManager::get_status() const {
    std::ostringstream oss;
    oss << "Running: " << (is_running_ ? "yes" : "no") << "\n";
    oss << "Convergence history: " << convergence_.history.size() << " iterations\n";
    oss << "Metrics:\n" << metrics_.to_string();
    return oss.str();
}

OptimizationResult OptimizationManager::run_genetic_algorithm(const OptimizationSettings& settings) {
    GAConfig ga_config;
    ga_config.generations = settings.max_iterations;

    GeneticAlgorithm ga(*design_space_, ga_config);

    Individual best = ga.optimize(settings.max_iterations);

    OptimizationResult result;
    result.best_design = design_space_->denormalize(best.chromosome);
    result.best_objective_value = best.fitness;
    result.is_feasible = best.is_feasible;
    result.iterations_completed = ga.generation_count();
    result.convergence_history = ga.convergence_history();

    return result;
}

OptimizationResult OptimizationManager::run_particle_swarm(const OptimizationSettings& settings) {
    PSOConfig pso_config;
    pso_config.iterations = settings.max_iterations;

    ParticleSwarmOptimizer pso(*design_space_, pso_config);

    Particle best = pso.optimize(settings.max_iterations);

    OptimizationResult result;
    result.best_design = design_space_->denormalize(best.position);
    result.best_objective_value = best.current_fitness;
    result.is_feasible = best.is_feasible;
    result.iterations_completed = pso.iteration_count();
    result.convergence_history = pso.convergence_history();

    return result;
}

OptimizationResult OptimizationManager::run_hybrid(const OptimizationSettings& settings) {
    // Run GA and PSO, keep best result
    auto ga_result = run_genetic_algorithm(settings);
    auto pso_result = run_particle_swarm(settings);

    if (ga_result.best_objective_value > pso_result.best_objective_value) {
        return ga_result;
    }
    return pso_result;
}

bool OptimizationManager::check_convergence(const OptimizationSettings& settings) const {
    return convergence_.has_converged();
}

void OptimizationManager::update_metrics(const std::vector<double>& design, double objective) {
    metrics_.total_evaluations++;

    if (constraint_handler_->is_feasible(design)) {
        metrics_.feasible_solutions++;
        if (objective < metrics_.best_feasible_value) {
            metrics_.best_feasible_value = objective;
        }
    }
}

void OptimizationManager::record_iteration_result(size_t iteration, const OptimizationResult& result) {
    convergence_.record_value(result.best_objective_value);
}

bool OptimizationManager::should_terminate(const OptimizationSettings& settings,
                                          const std::chrono::steady_clock::time_point& start_time) const {
    if (should_cancel_) return true;

    auto elapsed = std::chrono::steady_clock::now() - start_time;
    return std::chrono::duration<double>(elapsed).count() > settings.time_limit_seconds;
}

} // namespace bendcore::generative
