#include "bendcore/topology_optimization/topology_optimizer.hpp"
#include "bendcore/topology_optimization/simp_method.hpp"
#include "bendcore/topology_optimization/sensitivity_analysis.hpp"
#include "bendcore/topology_optimization/density_filtering.hpp"
#include "bendcore/topology_optimization/optimization_algorithms.hpp"
#include "bendcore/topology_optimization/constraint_handling.hpp"
#include "bendcore/topology_optimization/post_processing.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace bendcore {
namespace topology_optimization {

TopologyOptimizer::TopologyOptimizer(const ProblemDefinition& problem,
                                     const OptimizationParameters& params)
    : problem_(problem), parameters_(params) {

    initialize();
}

void TopologyOptimizer::initialize() {
    // Initialize components
    SIMPMethod::Parameters simp_params;
    simp_params.E_0 = problem_.E_0;
    simp_params.E_min = problem_.E_0 * 1e-3;
    simp_params.penalty_exponent = parameters_.simp_penalty;
    simp_params.volume_fraction = 1.0 - parameters_.max_iterations;  // Will be overwritten
    simp_params.move_limit = parameters_.simp_move_limit;
    simp_params.max_iterations = parameters_.max_iterations;
    simp_params.convergence_tolerance = parameters_.convergence_tolerance;

    simp_method_ = std::make_unique<SIMPMethod>(simp_params);
    simp_method_->initialize_densities(problem_.n_elements,
                                      problem_.element_volumes.empty() ? 0.5 :
                                      std::accumulate(problem_.element_volumes.begin(),
                                                     problem_.element_volumes.end(), 0.0) /
                                      (problem_.n_elements * 2.0));

    // Initialize sensitivity analyzer
    SensitivityAnalysis::ConvergenceCriteria conv_criteria;
    conv_criteria.density_change_tolerance = parameters_.convergence_tolerance;
    conv_criteria.objective_change_tolerance = parameters_.convergence_tolerance * 0.1;
    sensitivity_analyzer_ = std::make_unique<SensitivityAnalysis>(conv_criteria);

    // Initialize density filter
    DensityFilter::Parameters filter_params;
    filter_params.filter_radius = parameters_.filter_radius;
    filter_params.apply_to_sensitivities = true;
    filter_params.iterations = parameters_.use_density_filtering ? 1 : 0;
    density_filter_ = std::make_unique<DensityFilter>(filter_params);

    // Initialize optimizer
    OptimizationAlgorithm::Parameters opt_params;
    opt_params.algorithm = OptimizationAlgorithm::AlgorithmType::OPTIMALITY_CRITERIA;
    opt_params.move_limit = parameters_.simp_move_limit;
    opt_params.max_iterations = parameters_.max_iterations;
    opt_params.convergence_tolerance = parameters_.convergence_tolerance;
    optimizer_ = std::make_unique<OptimizationAlgorithm>(opt_params);

    // Initialize constraint handler
    ConstraintHandler::Parameters constraint_params;
    constraint_params.volume_fraction = 1.0 - parameters_.max_iterations;  // Placeholder
    constraint_params.stress_limit = 1e6;
    constraint_params.min_feature_size = parameters_.min_feature_size;
    constraint_params.constraint_tolerance = parameters_.volume_constraint_tolerance;
    constraint_handler_ = std::make_unique<ConstraintHandler>(constraint_params);

    // Add volume constraint
    constraint_handler_->add_constraint(
        ConstraintHandler::Constraint(ConstraintHandler::ConstraintType::VOLUME, 0.0, 1.0));

    if (parameters_.apply_manufacturing_constraints) {
        constraint_handler_->add_constraint(
            ConstraintHandler::Constraint(ConstraintHandler::ConstraintType::MANUFACTURING, 0.0, 1.0));
    }

    // Initialize post processor
    PostProcessor::Parameters post_params;
    post_params.threshold_value = 0.5;
    post_params.min_feature_size = parameters_.min_feature_size;
    post_processor_ = std::make_unique<PostProcessor>(post_params);

    // Initialize state
    current_densities_ = simp_method_->get_densities();
}

TopologyOptimizer::OptimizationResult TopologyOptimizer::optimize() {
    OptimizationResult result;

    for (int iter = 0; iter < parameters_.max_iterations; ++iter) {
        if (optimize_iteration(iter)) {
            result.converged = true;
            break;
        }
    }

    result.iterations = iteration_count_;
    result.final_densities = current_densities_;
    result.objective_history = objective_history_;
    result.constraint_history = constraint_history_;

    // Post-processing
    result.design_manufacturable =
        post_processor_->check_manufacturability(current_densities_,
                                               problem_.element_positions,
                                               parameters_.min_feature_size) > 0.8;

    result.manufacturability_score =
        post_processor_->check_manufacturability(current_densities_,
                                               problem_.element_positions,
                                               parameters_.min_feature_size);

    return result;
}

bool TopologyOptimizer::optimize_iteration(int iteration) {
    iteration_count_++;

    // Run FEA to get displacements and stresses
    auto [displacements, stresses] = compute_fea(current_densities_, problem_.loads);

    // Compute compliance (objective)
    double compliance = compute_compliance(current_densities_, displacements);
    objective_history_.push_back(compliance);

    // Compute sensitivities
    sensitivities_ = compute_sensitivities(current_densities_, displacements);

    // Apply density filtering to sensitivities
    if (parameters_.use_density_filtering) {
        sensitivities_ = density_filter_->filter_sensitivities(sensitivities_,
                                                              current_densities_,
                                                              problem_.element_positions);
    }

    // Evaluate constraints
    auto constraint_values = constraint_handler_->evaluate_constraints(
        current_densities_,
        problem_.element_volumes,
        stresses,
        problem_.element_positions);

    constraint_history_.push_back(
        constraint_values.empty() ? 0.0 : constraint_values[0]);

    // Perform optimization step
    std::vector<double> lower_bounds(current_densities_.size(), 0.0);
    std::vector<double> upper_bounds(current_densities_.size(), 1.0);

    double target_volume = problem_.element_volumes.empty() ?
                          current_densities_.size() * 0.5 :
                          std::accumulate(problem_.element_volumes.begin(),
                                        problem_.element_volumes.end(), 0.0) * 0.5;

    current_densities_ = optimizer_->step_with_volume_constraint(
        current_densities_, sensitivities_, lower_bounds, upper_bounds, target_volume);

    // Apply density filter to densities
    if (parameters_.use_density_filtering) {
        current_densities_ = density_filter_->filter(current_densities_,
                                                     problem_.element_positions);
    }

    // Apply symmetry constraint if requested
    if (parameters_.apply_symmetry) {
        // Would need symmetry pairs to be defined
    }

    // Update SIMP densities
    simp_method_->set_densities(current_densities_);

    // Check convergence
    return sensitivity_analyzer_->check_convergence_density(
        current_densities_, simp_method_->get_densities());
}

std::pair<std::vector<double>, std::vector<double>>
TopologyOptimizer::compute_fea(const std::vector<double>& densities,
                                const std::vector<std::array<double, 3>>& loads) {

    // Placeholder FEA implementation
    // In a real implementation, this would call a full FEA solver

    std::vector<double> displacements(problem_.n_elements, 0.0);
    std::vector<double> stresses(problem_.n_elements, 0.0);

    // Simple dummy implementation for testing
    for (size_t i = 0; i < problem_.n_elements; ++i) {
        // Displacement inversely proportional to density
        double E = SIMPMethod::material_interpolation(densities[i],
                                                     problem_.E_0,
                                                     problem_.E_0 * 1e-3,
                                                     parameters_.simp_penalty);
        displacements[i] = 1.0 / (E + 1e-6);

        // Stress proportional to strain
        stresses[i] = std::abs(displacements[i]) * E;
    }

    return {displacements, stresses};
}

double TopologyOptimizer::compute_compliance(const std::vector<double>& densities,
                                             const std::vector<double>& displacements) {

    double compliance = 0.0;

    for (size_t i = 0; i < densities.size(); ++i) {
        if (i < displacements.size()) {
            double E = SIMPMethod::material_interpolation(densities[i],
                                                         problem_.E_0,
                                                         problem_.E_0 * 1e-3,
                                                         parameters_.simp_penalty);
            compliance += displacements[i] * displacements[i] * E;
        }
    }

    return compliance;
}

std::vector<double> TopologyOptimizer::compute_sensitivities(
    const std::vector<double>& densities,
    const std::vector<double>& displacements) {

    return SensitivityAnalysis::compute_compliance_sensitivity(
        densities, displacements, problem_.element_volumes,
        problem_.n_elements, parameters_.simp_penalty,
        problem_.E_0, problem_.E_0 * 1e-3);
}

} // namespace topology_optimization
} // namespace bendcore
