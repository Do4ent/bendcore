#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include "design_space.hpp"
#include "genetic_algorithm.hpp"
#include "particle_swarm.hpp"
#include "constraint_handler.hpp"
#include "design_generator.hpp"

namespace bendcore::generative {

/**
 * Optimization Manager
 * Orchestrates algorithm selection and execution
 */

// Optimization result
struct OptimizationResult {
    std::vector<double> best_design;
    double best_objective_value = 0.0;
    std::vector<double> secondary_objectives;
    std::vector<double> constraint_violations;
    bool is_feasible = false;
    size_t iterations_completed = 0;
    double computation_time_seconds = 0.0;
    std::vector<double> convergence_history;

    [[nodiscard]] std::string to_string() const;
};

// Algorithm type
enum class OptimizationAlgorithm {
    GENETIC_ALGORITHM,
    PARTICLE_SWARM,
    HYBRID,
    AUTO_SELECT
};

// Optimization settings
struct OptimizationSettings {
    OptimizationAlgorithm algorithm = OptimizationAlgorithm::AUTO_SELECT;
    size_t max_iterations = 500;
    size_t max_evaluations = 10000;
    double time_limit_seconds = 300.0;  // 5 minutes
    bool verbose = false;
    bool parallel = false;
    size_t num_threads = 4;
    double convergence_tolerance = 1e-6;

    // Warm start
    std::optional<std::vector<double>> initial_design;
    std::vector<OptimizationResult> previous_results;
};

// Convergence checker
struct ConvergenceMonitor {
    std::vector<double> history;
    size_t stagnation_limit = 50;
    double improvement_threshold = 1e-8;

    [[nodiscard]] bool has_converged() const;
    [[nodiscard]] bool is_stagnant() const;
    void record_value(double value);
    [[nodiscard]] double improvement_rate() const;
};

// Performance metrics
struct PerformanceMetrics {
    size_t total_evaluations = 0;
    size_t feasible_solutions = 0;
    double best_feasible_value = std::numeric_limits<double>::max();
    double average_constraint_violation = 0.0;
    std::vector<double> evaluation_times;

    [[nodiscard]] double success_rate() const;
    [[nodiscard]] double average_evaluation_time() const;
    [[nodiscard]] std::string to_string() const;
};

// Multi-objective result
struct ParetoFront {
    std::vector<std::vector<double>> solutions;  // Pareto optimal designs
    std::vector<double> primary_objectives;      // Primary objective values
    std::vector<std::vector<double>> secondary_objectives;  // Secondary objective values

    [[nodiscard]] bool is_dominated(const std::vector<double>& objectives) const;
    [[nodiscard]] size_t size() const { return solutions.size(); }
    [[nodiscard]] std::vector<std::vector<double>> compute_distances() const;
};

// Optimization manager
class OptimizationManager {
public:
    OptimizationManager(std::shared_ptr<DesignSpace> design_space,
                       std::shared_ptr<ConstraintHandler> constraint_handler,
                       std::shared_ptr<DesignGenerator> design_generator);

    // Optimization execution
    [[nodiscard]] OptimizationResult optimize(const OptimizationSettings& settings = OptimizationSettings());
    [[nodiscard]] OptimizationResult optimize_multi_objective(const OptimizationSettings& settings = OptimizationSettings());

    // Algorithm selection
    [[nodiscard]] OptimizationAlgorithm select_algorithm() const;
    void set_preferred_algorithm(OptimizationAlgorithm algorithm) { preferred_algorithm_ = algorithm; }

    // Results
    [[nodiscard]] const OptimizationResult& last_result() const { return last_result_; }
    [[nodiscard]] const PerformanceMetrics& metrics() const { return metrics_; }
    [[nodiscard]] const ConvergenceMonitor& convergence() const { return convergence_; }

    // Pareto front computation
    [[nodiscard]] ParetoFront compute_pareto_front(const std::vector<std::vector<double>>& solutions);

    // Warm start
    void set_initial_design(const std::vector<double>& design) { initial_design_ = design; }
    void add_previous_result(const OptimizationResult& result) { previous_results_.push_back(result); }

    // Status and monitoring
    [[nodiscard]] std::string get_status() const;
    [[nodiscard]] bool is_running() const { return is_running_; }
    void cancel_optimization() { should_cancel_ = true; }

    // Callbacks
    using IterationCallback = std::function<void(size_t iteration, const OptimizationResult& current_best)>;
    void set_iteration_callback(IterationCallback callback) { iteration_callback_ = callback; }

private:
    std::shared_ptr<DesignSpace> design_space_;
    std::shared_ptr<ConstraintHandler> constraint_handler_;
    std::shared_ptr<DesignGenerator> design_generator_;

    OptimizationAlgorithm preferred_algorithm_ = OptimizationAlgorithm::AUTO_SELECT;
    OptimizationResult last_result_;
    PerformanceMetrics metrics_;
    ConvergenceMonitor convergence_;
    bool is_running_ = false;
    bool should_cancel_ = false;

    std::optional<std::vector<double>> initial_design_;
    std::vector<OptimizationResult> previous_results_;
    IterationCallback iteration_callback_;

    // Algorithm runners
    [[nodiscard]] OptimizationResult run_genetic_algorithm(const OptimizationSettings& settings);
    [[nodiscard]] OptimizationResult run_particle_swarm(const OptimizationSettings& settings);
    [[nodiscard]] OptimizationResult run_hybrid(const OptimizationSettings& settings);

    // Utility
    [[nodiscard]] bool check_convergence(const OptimizationSettings& settings) const;
    void update_metrics(const std::vector<double>& design, double objective);
    void record_iteration_result(size_t iteration, const OptimizationResult& result);
    [[nodiscard]] bool should_terminate(const OptimizationSettings& settings,
                                       const std::chrono::steady_clock::time_point& start_time) const;
};

} // namespace bendcore::generative
