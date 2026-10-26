// Reduced Order Modeling Implementation
// M7 Phase: Real-time Solver (GAP-030)

#include "bendcore/solver/reduced_order_model.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <iostream>

namespace bendcore::solver {

// ============================================================================
// POD Basis
// ============================================================================

PODBasis::PODBasis(int32_t basis_size)
    : basis_size_(basis_size), dimension_(0) {}

void PODBasis::build_from_snapshots(
    const std::vector<std::vector<double>>& snapshots,
    double energy_threshold) {

    if (snapshots.empty()) {
        throw std::invalid_argument("Snapshot matrix is empty");
    }

    dimension_ = snapshots[0].size();
    int32_t num_snapshots = snapshots.size();

    // Simplified SVD: compute correlation matrix and eigenvalues
    // In production, use full SVD algorithm

    // Correlation matrix: C = X^T * X / m
    std::vector<std::vector<double>> C(dimension_,
        std::vector<double>(dimension_, 0.0));

    for (int32_t i = 0; i < dimension_; ++i) {
        for (int32_t j = i; j < dimension_; ++j) {
            double sum = 0.0;
            for (int32_t k = 0; k < num_snapshots; ++k) {
                sum += snapshots[k][i] * snapshots[k][j];
            }
            C[i][j] = C[j][i] = sum / num_snapshots;
        }
    }

    // Power iteration to find dominant eigenvector
    basis_.clear();
    singular_values_.clear();

    int32_t num_basis = std::min(basis_size_, dimension_);
    double total_energy = 0.0;

    for (int32_t mode = 0; mode < num_basis; ++mode) {
        // Initialize eigenvector
        std::vector<double> v(dimension_, 1.0 / std::sqrt(dimension_));

        // Power iteration
        for (int32_t iter = 0; iter < 20; ++iter) {
            std::vector<double> Cv(dimension_, 0.0);
            for (int32_t i = 0; i < dimension_; ++i) {
                for (int32_t j = 0; j < dimension_; ++j) {
                    Cv[i] += C[i][j] * v[j];
                }
            }

            double norm = std::sqrt(std::accumulate(Cv.begin(), Cv.end(), 0.0,
                [](double sum, double x) { return sum + x*x; }));

            if (norm < 1e-16) break;

            for (int32_t i = 0; i < dimension_; ++i) {
                v[i] = Cv[i] / norm;
            }
        }

        basis_.push_back(v);

        // Compute singular value
        std::vector<double> Cv(dimension_, 0.0);
        for (int32_t i = 0; i < dimension_; ++i) {
            for (int32_t j = 0; j < dimension_; ++j) {
                Cv[i] += C[i][j] * v[j];
            }
        }

        double sigma = std::sqrt(std::accumulate(Cv.begin(), Cv.end(), 0.0,
            [](double sum, double x) { return sum + x*x; }));
        singular_values_.push_back(sigma);
        total_energy += sigma * sigma;
    }

    // Normalize singular values
    double cumulative_energy = 0.0;
    for (double& sv : singular_values_) {
        cumulative_energy += sv * sv;
    }

    // Reduce basis size if energy threshold is met
    while (basis_.size() > 1) {
        double current_energy = 0.0;
        for (size_t i = 0; i < basis_.size() - 1; ++i) {
            current_energy += singular_values_[i] * singular_values_[i];
        }

        if (current_energy / cumulative_energy >= energy_threshold) {
            basis_.pop_back();
            singular_values_.pop_back();
        } else {
            break;
        }
    }
}

std::vector<double> PODBasis::project(const std::vector<double>& u) const {
    if (static_cast<int32_t>(u.size()) != dimension_) {
        throw std::invalid_argument("Vector dimension mismatch");
    }

    std::vector<double> coeffs(basis_.size());
    for (size_t i = 0; i < basis_.size(); ++i) {
        coeffs[i] = 0.0;
        for (int32_t j = 0; j < dimension_; ++j) {
            coeffs[i] += u[j] * basis_[i][j];
        }
    }

    return coeffs;
}

std::vector<double> PODBasis::reconstruct(const std::vector<double>& coeffs) const {
    std::vector<double> u(dimension_, 0.0);

    for (size_t i = 0; i < basis_.size() && i < coeffs.size(); ++i) {
        for (int32_t j = 0; j < dimension_; ++j) {
            u[j] += coeffs[i] * basis_[i][j];
        }
    }

    return u;
}

std::vector<double> PODBasis::get_cumulative_energy() const {
    std::vector<double> energy(singular_values_.size());

    double total = 0.0;
    for (double sv : singular_values_) {
        total += sv * sv;
    }

    double cumulative = 0.0;
    for (size_t i = 0; i < singular_values_.size(); ++i) {
        cumulative += singular_values_[i] * singular_values_[i];
        energy[i] = cumulative / total;
    }

    return energy;
}

double PODBasis::get_relative_information(int32_t num_modes) const {
    if (num_modes <= 0 || num_modes > static_cast<int32_t>(singular_values_.size())) {
        return 0.0;
    }

    double cumulative = 0.0;
    double total = 0.0;

    for (size_t i = 0; i < singular_values_.size(); ++i) {
        double sv2 = singular_values_[i] * singular_values_[i];
        total += sv2;
        if (static_cast<int32_t>(i) < num_modes) {
            cumulative += sv2;
        }
    }

    return total > 1e-16 ? cumulative / total : 0.0;
}

void PODBasis::print_info() const {
    std::cout << "POD Basis: " << basis_.size() << " modes, dimension=" << dimension_ << "\n";
    auto energy = get_cumulative_energy();
    std::cout << "Cumulative energy: ";
    for (size_t i = 0; i < std::min(size_t(5), energy.size()); ++i) {
        std::cout << energy[i] * 100 << "% ";
    }
    std::cout << "\n";
}

// ============================================================================
// Reduced Order Model
// ============================================================================

ReducedOrderModel::ReducedOrderModel(int32_t basis_size)
    : basis_(basis_size),
      K_reduced_(basis_size),
      reduced_size_(basis_size),
      full_size_(0) {}

void ReducedOrderModel::offline_phase(
    const SparseMatrix& K_full,
    const std::vector<std::vector<double>>& displacement_snapshots,
    const std::vector<std::vector<double>>& load_snapshots) {

    if (displacement_snapshots.empty() || load_snapshots.empty()) {
        throw std::invalid_argument("Snapshot data is empty");
    }

    full_size_ = K_full.get_size();

    // Build POD basis from displacement snapshots
    basis_.build_from_snapshots(displacement_snapshots, 0.99);

    // Project full stiffness matrix to reduced subspace
    K_reduced_ = project_matrix(K_full);

    // Project load vectors
    if (!load_snapshots.empty()) {
        F_reduced_ = project_vector(load_snapshots[0]);
    }

    reduced_size_ = basis_.get_num_basis();
    rom_ready_ = true;
}

std::vector<double> ReducedOrderModel::solve_reduced(
    const std::vector<double>& load_reduced,
    double tolerance,
    int32_t max_iterations) {

    if (!rom_ready_) {
        throw std::runtime_error("ROM not ready");
    }

    // Simple Gaussian elimination for small system
    int32_t n = K_reduced_.get_size();
    std::vector<std::vector<double>> K_dense(n, std::vector<double>(n));
    std::vector<double> b = load_reduced;

    for (int32_t i = 0; i < n; ++i) {
        for (int32_t j = 0; j < n; ++j) {
            K_dense[i][j] = K_reduced_.get_value(i, j);
        }
    }

    // LU decomposition
    for (int32_t i = 0; i < n; ++i) {
        int32_t pivot = i;
        for (int32_t j = i + 1; j < n; ++j) {
            if (std::abs(K_dense[j][i]) > std::abs(K_dense[pivot][i])) {
                pivot = j;
            }
        }

        std::swap(K_dense[i], K_dense[pivot]);
        std::swap(b[i], b[pivot]);

        if (std::abs(K_dense[i][i]) < 1e-16) {
            throw std::runtime_error("Singular matrix");
        }

        for (int32_t j = i + 1; j < n; ++j) {
            double factor = K_dense[j][i] / K_dense[i][i];
            for (int32_t k = i; k < n; ++k) {
                K_dense[j][k] -= factor * K_dense[i][k];
            }
            b[j] -= factor * b[i];
        }
    }

    // Back substitution
    std::vector<double> x(n);
    for (int32_t i = n - 1; i >= 0; --i) {
        x[i] = b[i];
        for (int32_t j = i + 1; j < n; ++j) {
            x[i] -= K_dense[i][j] * x[j];
        }
        x[i] /= K_dense[i][i];
    }

    return x;
}

std::vector<double> ReducedOrderModel::reconstruct_full_solution(
    const std::vector<double>& coeffs_reduced) const {

    return basis_.reconstruct(coeffs_reduced);
}

std::vector<double> ReducedOrderModel::project_to_reduced(
    const std::vector<double>& u_full) const {

    return basis_.project(u_full);
}

double ReducedOrderModel::estimate_error(
    const std::vector<double>& solution_reduced,
    const std::vector<double>& solution_full) const {

    auto reconstructed = basis_.reconstruct(solution_reduced);

    double error = 0.0;
    double norm = 0.0;
    for (size_t i = 0; i < reconstructed.size(); ++i) {
        double diff = solution_full[i] - reconstructed[i];
        error += diff * diff;
        norm += solution_full[i] * solution_full[i];
    }

    return norm > 1e-16 ? std::sqrt(error / norm) : 0.0;
}

ReducedOrderModel::SpeedupInfo ReducedOrderModel::get_speedup_info() const {
    return SpeedupInfo{
        full_size_,
        reduced_size_,
        static_cast<double>(full_size_) / reduced_size_,
        0.01  // Typical loss
    };
}

void ReducedOrderModel::print_statistics() const {
    std::cout << "ROM Statistics:\n";
    std::cout << "  Full size: " << full_size_ << "\n";
    std::cout << "  Reduced size: " << reduced_size_ << "\n";
    std::cout << "  Speedup: " << (full_size_ / static_cast<double>(reduced_size_)) << "x\n";
    basis_.print_info();
}

SparseMatrix ReducedOrderModel::project_matrix(
    const SparseMatrix& K_full) const {

    // K_r = V^T * K * V where V is POD basis
    int32_t r = basis_.get_num_basis();
    SparseMatrix result(r);

    // Simplified projection
    for (int32_t i = 0; i < r; ++i) {
        for (int32_t j = 0; j < r; ++j) {
            double kij = 0.0;
            for (int32_t k = 0; k < full_size_; ++k) {
                for (int32_t l = 0; l < full_size_; ++l) {
                    kij += basis_.get_basis_vectors()[i][k] *
                           K_full.get_value(k, l) *
                           basis_.get_basis_vectors()[j][l];
                }
            }
            if (std::abs(kij) > 1e-16) {
                result.add_value(i, j, kij);
            }
        }
    }

    result.finalize();
    return result;
}

std::vector<double> ReducedOrderModel::project_vector(
    const std::vector<double>& F_full) const {

    return basis_.project(F_full);
}

}  // namespace bendcore::solver
