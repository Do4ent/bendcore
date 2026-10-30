// Real-time Simulation Solver Framework Implementation
// M7 Phase: Real-time Solver (GAP-030)

#include "bendcore/solver/real_time_solver.hpp"
#include "bendcore/solver/iterative_solvers.hpp"
#include "bendcore/solver/preconditioners.hpp"
#include "bendcore/solver/reduced_order_model.hpp"
#include "bendcore/solver/performance_metrics.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace bendcore::solver {

// ============================================================================
// SparseMatrix Implementation
// ============================================================================

SparseMatrix::SparseMatrix(int32_t size)
    : size_(size), finalized_(false) {
    row_ptrs_.resize(size_ + 1, 0);
    temp_rows_.resize(size_);
}

void SparseMatrix::add_value(int32_t i, int32_t j, double value) {
    if (i < 0 || i >= size_ || j < 0 || j >= size_) {
        throw std::out_of_range("Matrix index out of bounds");
    }
    if (finalized_) {
        throw std::runtime_error("Cannot add values to finalized matrix");
    }
    if (std::abs(value) < 1e-16) {
        return;  // Skip near-zero values
    }

    // Find or insert position
    auto& row = temp_rows_[i];
    auto it = std::find_if(row.begin(), row.end(),
        [j](const auto& p) { return p.first == j; });

    if (it != row.end()) {
        it->second += value;
    } else {
        row.emplace_back(j, value);
    }
}

double SparseMatrix::get_value(int32_t i, int32_t j) const {
    if (i < 0 || i >= size_ || j < 0 || j >= size_) {
        throw std::out_of_range("Matrix index out of bounds");
    }

    if (!finalized_) {
        // До finalize() значения лежат в построчном буфере сборки.
        for (const auto& [col, val] : temp_rows_[i]) {
            if (col == j) return val;
        }
        return 0.0;
    }

    for (int32_t idx = row_ptrs_[i]; idx < row_ptrs_[i + 1]; ++idx) {
        if (columns_[idx] == j) {
            return values_[idx];
        }
    }
    return 0.0;
}

void SparseMatrix::finalize() {
    if (finalized_) return;

    values_.clear();
    columns_.clear();
    row_ptrs_.assign(size_ + 1, 0);

    int32_t nnz = 0;
    for (int32_t i = 0; i < size_; ++i) {
        row_ptrs_[i] = nnz;

        // Sort by column index
        std::sort(temp_rows_[i].begin(), temp_rows_[i].end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        for (const auto& [col, val] : temp_rows_[i]) {
            columns_.push_back(col);
            values_.push_back(val);
            ++nnz;
        }
    }
    row_ptrs_[size_] = nnz;
    temp_rows_.clear();
    finalized_ = true;
}

std::vector<double> SparseMatrix::multiply(const std::vector<double>& x) const {
    if (static_cast<int32_t>(x.size()) != size_) {
        throw std::invalid_argument("Vector size mismatch");
    }
    if (!finalized_) {
        throw std::runtime_error("Matrix must be finalized before multiplication");
    }

    std::vector<double> y(size_, 0.0);
    for (int32_t i = 0; i < size_; ++i) {
        for (int32_t idx = row_ptrs_[i]; idx < row_ptrs_[i + 1]; ++idx) {
            y[i] += values_[idx] * x[columns_[idx]];
        }
    }
    return y;
}

SparseMatrix SparseMatrix::multiply(const SparseMatrix& B) const {
    if (size_ != B.size_) {
        throw std::invalid_argument("Matrix dimensions incompatible for multiplication");
    }

    SparseMatrix C(size_);
    for (int32_t i = 0; i < size_; ++i) {
        for (int32_t k_idx = row_ptrs_[i]; k_idx < row_ptrs_[i + 1]; ++k_idx) {
            int32_t k = columns_[k_idx];
            double aik = values_[k_idx];

            for (int32_t j_idx = B.row_ptrs_[k]; j_idx < B.row_ptrs_[k + 1]; ++j_idx) {
                int32_t j = B.columns_[j_idx];
                double bkj = B.values_[j_idx];
                C.add_value(i, j, aik * bkj);
            }
        }
    }
    C.finalize();
    return C;
}

void SparseMatrix::get_row(int32_t row, std::vector<int32_t>& cols, std::vector<double>& vals) const {
    if (row < 0 || row >= size_) {
        throw std::out_of_range("Row index out of bounds");
    }

    cols.clear();
    vals.clear();

    for (int32_t idx = row_ptrs_[row]; idx < row_ptrs_[row + 1]; ++idx) {
        cols.push_back(columns_[idx]);
        vals.push_back(values_[idx]);
    }
}

void SparseMatrix::print_info() const {
    std::cout << "SparseMatrix: " << size_ << "x" << size_
              << ", NNZ=" << num_nonzeros()
              << ", Density=" << (100.0 * num_nonzeros() / (size_ * size_)) << "%\n";
}

// ============================================================================
// RealTimeSolver Implementation
// ============================================================================

RealTimeSolver::RealTimeSolver(int32_t system_size)
    : system_size_(system_size),
      stiffness_matrix_(system_size),
      config_(RealtimeSolverConfig()),
      current_displacement_(system_size, 0.0),
      current_stress_(system_size, 0.0),
      previous_displacement_(system_size, 0.0) {

    force_vector_.resize(system_size, 0.0);

    // Initialize solvers
    iterative_solver_ = std::make_unique<ConjugateGradientSolver>();
    preconditioner_ = PreconditionerFactory::create(PreconditionerFactory::PreconditionerType::ILU0);
    metrics_ = std::make_unique<SolverMetrics>();
}

RealTimeSolver::~RealTimeSolver() = default;

void RealTimeSolver::set_stiffness_matrix(const SparseMatrix& K) {
    stiffness_matrix_ = K;
}

void RealTimeSolver::set_force_vector(const std::vector<double>& F) {
    if (static_cast<int32_t>(F.size()) != system_size_) {
        throw std::invalid_argument("Force vector size mismatch");
    }
    force_vector_ = F;
}

void RealTimeSolver::set_boundary_conditions(const std::vector<int32_t>& fixed_dofs) {
    fixed_dofs_ = fixed_dofs;
}

RealtimeSolution RealTimeSolver::solve_increment(
    const std::vector<double>& delta_load,
    const std::vector<double>* initial_guess) {

    // Add incremental load
    std::vector<double> current_load = force_vector_;
    for (int32_t i = 0; i < system_size_; ++i) {
        current_load[i] += config_.load_step * delta_load[i];
    }
    force_vector_ = current_load;

    // Use current solution as initial guess for next increment
    const std::vector<double>* guess = initial_guess ? initial_guess : &current_displacement_;
    return solve_with_solver(guess);
}

RealtimeSolution RealTimeSolver::solve(const std::vector<double>* initial_guess) {
    return solve_with_solver(initial_guess);
}

RealtimeSolution RealTimeSolver::solve_adaptive(
    const std::vector<double>& target_load,
    int32_t quality_level) {

    // Adjust solver config based on quality level
    auto old_config = config_;

    switch (quality_level) {
        case 1:  // Draft
            config_.max_iterations = 10;
            config_.residual_tolerance = 1e-2;
            break;
        case 2:  // Fast
            config_.max_iterations = 25;
            config_.residual_tolerance = 1e-3;
            break;
        case 3:  // Normal
            config_.max_iterations = 100;
            config_.residual_tolerance = 1e-4;
            break;
        case 4:  // Fine
            config_.max_iterations = 250;
            config_.residual_tolerance = 1e-5;
            break;
        case 5:  // High Precision
            config_.max_iterations = 1000;
            config_.residual_tolerance = 1e-6;
            break;
    }

    force_vector_ = target_load;
    auto result = solve_with_solver(&current_displacement_);

    config_ = old_config;
    return result;
}

std::vector<double> RealTimeSolver::compute_sensitivity(
    int32_t parameter_id,
    double delta) {

    if (parameter_id < 0 || parameter_id >= system_size_) {
        throw std::out_of_range("Parameter ID out of bounds");
    }

    // Compute finite difference: (u(p+delta) - u(p)) / delta
    std::vector<double> perturbed_load = force_vector_;
    perturbed_load[parameter_id] += delta;

    std::vector<double> original_displacement = current_displacement_;

    // Solve with perturbed load
    std::vector<double> old_force = force_vector_;
    force_vector_ = perturbed_load;
    auto result = solve_with_solver(&original_displacement);
    force_vector_ = old_force;

    // Compute sensitivity
    std::vector<double> sensitivity(system_size_);
    for (int32_t i = 0; i < system_size_; ++i) {
        sensitivity[i] = (result.displacement[i] - original_displacement[i]) / delta;
    }

    return sensitivity;
}

void RealTimeSolver::print_metrics() const {
    if (!metrics_) return;

    std::cout << "\n=== Solver Metrics ===\n";
    std::cout << "Iterations: " << metrics_->num_iterations << "\n";
    std::cout << "Initial Residual: " << metrics_->initial_residual << "\n";
    std::cout << "Final Residual: " << metrics_->final_residual << "\n";
    std::cout << "Convergence Rate: " << metrics_->convergence_rate << "\n";
    std::cout << "Assembly Time: " << metrics_->assembly_time.count() << " ms\n";
    std::cout << "Preconditioner Setup: " << metrics_->preconditioner_setup_time.count() << " ms\n";
    std::cout << "Solver Time: " << metrics_->solver_time.count() << " ms\n";
    std::cout << "Total Time: " << metrics_->total_time.count() << " ms\n";
    std::cout << "Matrix NNZ: " << metrics_->num_nonzeros << "\n";
}

int64_t RealTimeSolver::get_memory_usage() const {
    int64_t memory = 0;
    memory += sizeof(*this);
    memory += stiffness_matrix_.num_nonzeros() * sizeof(double);
    memory += stiffness_matrix_.get_size() * sizeof(int32_t);
    memory += force_vector_.size() * sizeof(double);
    memory += current_displacement_.size() * sizeof(double);
    memory += current_stress_.size() * sizeof(double);
    return memory;
}

void RealTimeSolver::clear_cache() {
    // For future optimization
}

void RealTimeSolver::apply_boundary_conditions(
    SparseMatrix& K,
    std::vector<double>& F,
    const std::vector<int32_t>& fixed_dofs) {

    // Apply Dirichlet boundary conditions by zeroing rows/columns
    for (int32_t dof : fixed_dofs) {
        if (dof >= 0 && dof < system_size_) {
            // Set K[dof, :] to zero and K[dof, dof] = 1
            // This is simplified; full implementation would modify K directly
            F[dof] = 0.0;
        }
    }
}

RealtimeSolution RealTimeSolver::solve_with_solver(
    const std::vector<double>* initial_guess) {

    auto start_time = std::chrono::high_resolution_clock::now();

    // Create working copies
    SparseMatrix K = stiffness_matrix_;
    std::vector<double> F = force_vector_;

    // Apply boundary conditions
    apply_boundary_conditions(K, F, fixed_dofs_);

    // Setup preconditioner
    if (preconditioner_ && preconditioner_->is_ready() == false) {
        preconditioner_->setup(K);
    }

    // Initial guess
    std::vector<double> x0 = initial_guess ? *initial_guess : std::vector<double>(system_size_, 0.0);

    // Solve using iterative solver
    auto result = iterative_solver_->solve(
        K, F, x0,
        config_.residual_tolerance,
        config_.max_iterations,
        preconditioner_.get());

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Store solution
    current_displacement_ = result.solution;
    compute_stress_field(current_displacement_);

    // Compute max values
    double max_displacement = 0.0;
    for (double d : current_displacement_) {
        max_displacement = std::max(max_displacement, std::abs(d));
    }

    double max_stress = 0.0;
    for (double s : current_stress_) {
        max_stress = std::max(max_stress, std::abs(s));
    }

    RealtimeSolution solution;
    solution.converged = result.converged;
    solution.displacement = current_displacement_;
    solution.stress = current_stress_;
    solution.max_displacement = max_displacement;
    solution.max_stress = max_stress;
    solution.iterations = result.num_iterations;
    solution.residual = result.final_residual;
    solution.solve_time = elapsed;
    solution.error_estimate = 0.0;  // TODO: compute actual error estimate

    update_metrics(result.converged, result.num_iterations, result.final_residual,
                   elapsed, solution.error_estimate);

    return solution;
}

void RealTimeSolver::compute_stress_field(const std::vector<double>& displacement) {
    // Simple stress computation: stress = K * displacement
    current_stress_ = stiffness_matrix_.multiply(displacement);
}

void RealTimeSolver::update_metrics(
    bool converged,
    int32_t iterations,
    double residual,
    std::chrono::milliseconds solve_time,
    double error_estimate) {

    metrics_->num_iterations = iterations;
    metrics_->final_residual = residual;
    metrics_->solver_time = solve_time;
    metrics_->num_nonzeros = stiffness_matrix_.num_nonzeros();
    metrics_->matrix_size = system_size_;
}

}  // namespace bendcore::solver
