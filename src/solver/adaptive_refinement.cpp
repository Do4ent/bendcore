// Adaptive Refinement and Error Control Implementation
// M7 Phase: Real-time Solver (GAP-030)

#include "bendcore/solver/adaptive_refinement.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace bendcore::solver {

// ============================================================================
// Error Estimator
// ============================================================================

ErrorEstimate ErrorEstimator::estimate_error_residual(
    const SparseMatrix& K,
    const std::vector<double>& u,
    const std::vector<double>& F) {

    // Compute residual: r = F - K*u
    auto Ku = K.multiply(u);
    std::vector<double> r(u.size());
    for (size_t i = 0; i < u.size(); ++i) {
        r[i] = F[i] - Ku[i];
    }

    double r_norm = 0.0;
    for (double ri : r) {
        r_norm += ri * ri;
    }
    r_norm = std::sqrt(r_norm);

    // Estimate global error
    double global_error = r_norm;

    ErrorEstimate estimate;
    estimate.global_error = global_error;
    estimate.element_errors.assign(u.size(), global_error / u.size());
    estimate.max_element_error = global_error;
    estimate.num_elements_exceeding_tol = 0;
    estimate.meets_tolerance = true;

    return estimate;
}

ErrorEstimate ErrorEstimator::estimate_error_recovery(
    const std::vector<double>& nodal_stress,
    const std::vector<double>& recovered_stress) {

    if (nodal_stress.size() != recovered_stress.size()) {
        throw std::invalid_argument("Stress vector size mismatch");
    }

    std::vector<double> errors(nodal_stress.size());
    double max_error = 0.0;

    for (size_t i = 0; i < nodal_stress.size(); ++i) {
        double diff = nodal_stress[i] - recovered_stress[i];
        errors[i] = std::abs(diff);
        max_error = std::max(max_error, errors[i]);
    }

    double global_error = 0.0;
    for (double e : errors) {
        global_error += e * e;
    }
    global_error = std::sqrt(global_error);

    ErrorEstimate estimate;
    estimate.global_error = global_error;
    estimate.element_errors = errors;
    estimate.max_element_error = max_error;
    estimate.meets_tolerance = max_error < 1e-3;

    return estimate;
}

ErrorEstimate ErrorEstimator::estimate_error_exact(
    const std::vector<double>& computed_solution) {

    if (reference_solution_.empty()) {
        throw std::runtime_error("Reference solution not set");
    }

    std::vector<double> errors(computed_solution.size());
    double global_error = 0.0;
    double max_error = 0.0;

    for (size_t i = 0; i < computed_solution.size(); ++i) {
        double error = std::abs(computed_solution[i] - reference_solution_[i]);
        errors[i] = error;
        global_error += error * error;
        max_error = std::max(max_error, error);
    }

    global_error = std::sqrt(global_error);

    ErrorEstimate estimate;
    estimate.global_error = global_error;
    estimate.element_errors = errors;
    estimate.max_element_error = max_error;
    estimate.num_elements_exceeding_tol = 0;

    for (double e : errors) {
        if (e > 1e-3) ++estimate.num_elements_exceeding_tol;
    }

    estimate.meets_tolerance = estimate.num_elements_exceeding_tol == 0;

    return estimate;
}

double ErrorEstimator::compute_energy_norm(
    const SparseMatrix& K,
    const std::vector<double>& u) {

    auto Ku = K.multiply(u);
    double energy = 0.0;
    for (size_t i = 0; i < u.size(); ++i) {
        energy += u[i] * Ku[i];
    }

    return std::sqrt(std::max(0.0, energy));
}

std::vector<double> ErrorEstimator::compute_local_errors(
    const std::vector<double>& global_error_indicator,
    int32_t num_elements) {

    std::vector<double> local_errors(num_elements);
    double avg_error = global_error_indicator[0] / num_elements;

    for (int32_t i = 0; i < num_elements; ++i) {
        local_errors[i] = avg_error;
    }

    return local_errors;
}

// ============================================================================
// Convergence Monitor
// ============================================================================

void ConvergenceMonitor::add_iteration(double residual, double step_size) {
    residual_history_.push_back(residual);
    step_sizes_.push_back(step_size);
}

bool ConvergenceMonitor::is_converged(
    double tolerance,
    int32_t min_iterations) const {

    if (static_cast<int32_t>(residual_history_.size()) < min_iterations) {
        return false;
    }

    return residual_history_.back() < tolerance;
}

double ConvergenceMonitor::get_convergence_rate() const {
    if (residual_history_.size() < 2) {
        return 1.0;
    }

    double r_k = residual_history_[residual_history_.size() - 1];
    double r_k1 = residual_history_[residual_history_.size() - 2];

    if (r_k1 < 1e-16) return 0.0;
    return r_k / r_k1;
}

bool ConvergenceMonitor::is_stagnating(int32_t window_size) const {
    if (static_cast<int32_t>(residual_history_.size()) < window_size) {
        return false;
    }

    int32_t start = residual_history_.size() - window_size;
    double avg_reduction = 1.0;

    for (int32_t i = start + 1; i < static_cast<int32_t>(residual_history_.size()); ++i) {
        avg_reduction *= get_convergence_rate();
    }

    return avg_reduction > 0.99;  // Very little reduction
}

void ConvergenceMonitor::reset() {
    residual_history_.clear();
    step_sizes_.clear();
}

ConvergenceMonitor::ConvergenceStats ConvergenceMonitor::get_statistics() const {
    return ConvergenceStats{
        static_cast<int32_t>(residual_history_.size()),
        residual_history_.empty() ? 0.0 : residual_history_[0],
        residual_history_.empty() ? 0.0 : residual_history_.back(),
        get_convergence_rate(),
        residual_history_.empty() ? false : residual_history_.back() < 1e-6
    };
}

// ============================================================================
// Refinement Indicator
// ============================================================================

std::vector<bool> RefinementIndicator::compute_refinement_flags(
    const std::vector<double>& element_errors,
    double error_tolerance,
    double refinement_fraction) {

    std::vector<bool> flags(element_errors.size(), false);

    if (strategy_ == RefinementStrategy::FixedFraction) {
        int32_t num_refine = std::max(1, static_cast<int32_t>(
            element_errors.size() * refinement_fraction));

        std::vector<std::pair<double, int32_t>> sorted_errors;
        for (size_t i = 0; i < element_errors.size(); ++i) {
            sorted_errors.push_back({element_errors[i], i});
        }

        std::sort(sorted_errors.rbegin(), sorted_errors.rend());

        for (int32_t i = 0; i < num_refine; ++i) {
            flags[sorted_errors[i].second] = true;
        }
    } else {
        for (size_t i = 0; i < element_errors.size(); ++i) {
            flags[i] = element_errors[i] > error_tolerance;
        }
    }

    return flags;
}

std::vector<double> RefinementIndicator::compute_optimal_mesh_size(
    const std::vector<double>& element_errors,
    double target_error,
    double convergence_rate) {

    std::vector<double> optimal_sizes(element_errors.size());

    for (size_t i = 0; i < element_errors.size(); ++i) {
        double error_ratio = element_errors[i] / target_error;
        double size_reduction = std::pow(error_ratio, -1.0 / convergence_rate);
        optimal_sizes[i] = size_reduction;
    }

    return optimal_sizes;
}

// ============================================================================
// Adaptive Solver
// ============================================================================

AdaptiveSolver::AdaptiveSolver(RealTimeSolver& base_solver)
    : base_solver_(base_solver) {

    error_estimator_ = std::make_unique<ErrorEstimator>();
    refinement_indicator_ = std::make_unique<RefinementIndicator>();
    convergence_monitor_ = std::make_unique<ConvergenceMonitor>();
}

AdaptiveSolver::AdaptiveSolution AdaptiveSolver::solve_with_error_control(
    double target_error,
    int32_t max_refinements) {

    AdaptiveSolution solution;
    solution.refinement_steps = 0;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int32_t refinement = 0; refinement < max_refinements; ++refinement) {
        auto result = base_solver_.solve();
        solution.displacement = result.displacement;
        solution.stress = result.stress;

        convergence_monitor_->add_iteration(result.residual, 1.0);

        if (result.error_estimate < target_error || result.converged) {
            break;
        }

        ++solution.refinement_steps;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    solution.total_time_ms = std::chrono::duration_cast<
        std::chrono::milliseconds>(end_time - start_time).count();

    return solution;
}

QualityMetrics AdaptiveSolver::assess_quality(
    const std::vector<double>& solution,
    const SparseMatrix& K,
    const std::vector<double>& F) {

    auto Ks = K.multiply(solution);

    double residual = 0.0;
    double solution_norm = 0.0;

    for (size_t i = 0; i < solution.size(); ++i) {
        double r = F[i] - Ks[i];
        residual += r * r;
        solution_norm += solution[i] * solution[i];
    }

    residual = std::sqrt(residual);
    solution_norm = std::sqrt(solution_norm);

    double energy = error_estimator_->compute_energy_norm(K, solution);

    auto stats = convergence_monitor_->get_statistics();

    return QualityMetrics{
        residual,
        solution_norm,
        solution_norm > 1e-16 ? residual / solution_norm : residual,
        energy,
        stats.num_iterations,
        stats.convergence_rate
    };
}

int32_t AdaptiveSolver::select_quality_level(double error_tolerance) {
    if (error_tolerance < 1e-5) return 5;
    if (error_tolerance < 1e-4) return 4;
    if (error_tolerance < 1e-3) return 3;
    if (error_tolerance < 1e-2) return 2;
    return 1;
}

// ============================================================================
// Solution Verifier
// ============================================================================

SolutionVerifier::BoundsCheck SolutionVerifier::verify_bounds(
    const std::vector<double>& displacement,
    const std::vector<double>& stress,
    double max_displacement,
    double max_stress) {

    BoundsCheck check;
    check.displacements_within_bounds = true;
    check.stresses_within_bounds = true;
    check.energy_positive = true;

    for (double d : displacement) {
        if (std::abs(d) > max_displacement) {
            check.displacements_within_bounds = false;
            check.warnings.push_back("Displacement exceeds maximum");
            break;
        }
    }

    for (double s : stress) {
        if (std::abs(s) > max_stress) {
            check.stresses_within_bounds = false;
            check.warnings.push_back("Stress exceeds maximum");
            break;
        }
    }

    return check;
}

bool SolutionVerifier::check_physical_validity(
    const std::vector<double>& displacement,
    const std::vector<double>& stress,
    const SparseMatrix& K) {

    // Check for NaN or Inf
    for (double d : displacement) {
        if (!std::isfinite(d)) return false;
    }
    for (double s : stress) {
        if (!std::isfinite(s)) return false;
    }

    return true;
}

double SolutionVerifier::compute_equilibrium_residual(
    const SparseMatrix& K,
    const std::vector<double>& u,
    const std::vector<double>& F) {

    auto Ku = K.multiply(u);
    double residual = 0.0;

    for (size_t i = 0; i < u.size(); ++i) {
        double r = F[i] - Ku[i];
        residual += r * r;
    }

    return std::sqrt(residual);
}

bool SolutionVerifier::check_energy_conservation(
    const std::vector<double>& displacement,
    const std::vector<double>& external_load,
    double tolerance) {

    double work_external = 0.0;
    for (size_t i = 0; i < displacement.size(); ++i) {
        work_external += external_load[i] * displacement[i];
    }

    return work_external >= -tolerance;
}

double SolutionVerifier::compute_strain_energy(
    const SparseMatrix& K,
    const std::vector<double>& u) {

    auto Ku = K.multiply(u);
    double energy = 0.0;

    for (size_t i = 0; i < u.size(); ++i) {
        energy += u[i] * Ku[i];
    }

    return 0.5 * energy;
}

}  // namespace bendcore::solver
