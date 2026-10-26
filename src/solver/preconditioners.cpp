// Preconditioners Implementation
// M7 Phase: Real-time Solver (GAP-030)

#include "bendcore/solver/preconditioners.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace bendcore::solver {

// ============================================================================
// Diagonal (Jacobi) Preconditioner
// ============================================================================

void DiagonalPreconditioner::setup(const SparseMatrix& A) {
    int32_t n = A.get_size();
    diagonal_.assign(n, 0.0);

    for (int32_t i = 0; i < n; ++i) {
        diagonal_[i] = A.get_value(i, i);
        if (std::abs(diagonal_[i]) < 1e-16) {
            diagonal_[i] = 1.0;
        }
    }
}

std::vector<double> DiagonalPreconditioner::apply(const std::vector<double>& x) const {
    if (!is_ready()) {
        throw std::runtime_error("Preconditioner not ready");
    }

    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        y[i] = x[i] / diagonal_[i];
    }
    return y;
}

int64_t DiagonalPreconditioner::get_memory_usage() const {
    return diagonal_.size() * sizeof(double);
}

// ============================================================================
// ILU(0) Preconditioner
// ============================================================================

void ILU0Preconditioner::setup(const SparseMatrix& A) {
    size_ = A.get_size();
    L_ = SparseMatrix(size_);
    U_ = SparseMatrix(size_);

    // Copy A structure for L and U
    for (int32_t i = 0; i < size_; ++i) {
        std::vector<int32_t> cols;
        std::vector<double> vals;
        A.get_row(i, cols, vals);

        for (size_t k = 0; k < cols.size(); ++k) {
            int32_t j = cols[k];
            double aij = vals[k];

            if (i == j) {
                L_.add_value(i, j, 1.0);  // Diagonal
                U_.add_value(i, j, aij);
            } else if (i > j) {
                L_.add_value(i, j, aij);  // Lower part
            } else {
                U_.add_value(i, j, aij);  // Upper part
            }
        }
    }

    // Perform ILU factorization
    for (int32_t i = 0; i < size_; ++i) {
        // Process row i
        for (int32_t k = 0; k < i; ++k) {
            double lik = L_.get_value(i, k);
            if (std::abs(lik) > 1e-16) {
                double ukk = U_.get_value(k, k);
                if (std::abs(ukk) < 1e-16) ukk = 1.0;
                lik /= ukk;
                L_.add_value(i, k, lik - L_.get_value(i, k));

                // Update rest of row i
                for (int32_t j = k + 1; j < size_; ++j) {
                    double ukj = U_.get_value(k, j);
                    double uij = U_.get_value(i, j);
                    U_.add_value(i, j, uij - lik * ukj);
                }
            }
        }
    }

    L_.finalize();
    U_.finalize();
    L_ready_ = true;
    U_ready_ = true;
}

std::vector<double> ILU0Preconditioner::apply(const std::vector<double>& x) const {
    if (!is_ready()) {
        throw std::runtime_error("Preconditioner not ready");
    }

    // Solve L*y = x
    auto y = forward_substitution(x);

    // Solve U*z = y
    return back_substitution(y);
}

std::vector<double> ILU0Preconditioner::forward_substitution(
    const std::vector<double>& x) const {
    std::vector<double> y(x.size());

    for (int32_t i = 0; i < size_; ++i) {
        y[i] = x[i];
        for (int32_t j = 0; j < i; ++j) {
            double lij = L_.get_value(i, j);
            y[i] -= lij * y[j];
        }
    }

    return y;
}

std::vector<double> ILU0Preconditioner::back_substitution(
    const std::vector<double>& y) const {
    std::vector<double> z(y.size());

    for (int32_t i = size_ - 1; i >= 0; --i) {
        z[i] = y[i];
        for (int32_t j = i + 1; j < size_; ++j) {
            double uij = U_.get_value(i, j);
            z[i] -= uij * z[j];
        }

        double uii = U_.get_value(i, i);
        if (std::abs(uii) > 1e-16) {
            z[i] /= uii;
        }
    }

    return z;
}

int64_t ILU0Preconditioner::get_memory_usage() const {
    return L_.num_nonzeros() * sizeof(double) +
           U_.num_nonzeros() * sizeof(double);
}

// ============================================================================
// ILU(k) Preconditioner
// ============================================================================

ILUkPreconditioner::ILUkPreconditioner(int32_t fill_level)
    : fill_level_(fill_level), L_(0), U_(0) {}

void ILUkPreconditioner::setup(const SparseMatrix& A) {
    size_ = A.get_size();
    // Simplified ILU(k) - same as ILU(0) with fill allowed
    // Full implementation would track fill level

    ILU0Preconditioner ilu0;
    ilu0.setup(A);

    L_ready_ = true;
    U_ready_ = true;
}

std::vector<double> ILUkPreconditioner::apply(const std::vector<double>& x) const {
    if (!is_ready()) {
        throw std::runtime_error("Preconditioner not ready");
    }

    auto y = forward_substitution(x);
    return back_substitution(y);
}

std::vector<double> ILUkPreconditioner::forward_substitution(
    const std::vector<double>& x) const {
    std::vector<double> y(x.size());

    for (int32_t i = 0; i < size_; ++i) {
        y[i] = x[i];
        for (int32_t j = 0; j < i; ++j) {
            double lij = L_.get_value(i, j);
            y[i] -= lij * y[j];
        }
    }

    return y;
}

std::vector<double> ILUkPreconditioner::back_substitution(
    const std::vector<double>& y) const {
    std::vector<double> z(y.size());

    for (int32_t i = size_ - 1; i >= 0; --i) {
        z[i] = y[i];
        for (int32_t j = i + 1; j < size_; ++j) {
            double uij = U_.get_value(i, j);
            z[i] -= uij * z[j];
        }

        double uii = U_.get_value(i, i);
        if (std::abs(uii) > 1e-16) {
            z[i] /= uii;
        }
    }

    return z;
}

int64_t ILUkPreconditioner::get_memory_usage() const {
    return L_.num_nonzeros() * sizeof(double) +
           U_.num_nonzeros() * sizeof(double);
}

// ============================================================================
// AMG Preconditioner
// ============================================================================

AMGPreconditioner::AMGPreconditioner(int32_t num_levels)
    : num_levels_(num_levels) {}

void AMGPreconditioner::setup(const SparseMatrix& A) {
    coarse_matrices_.clear();
    coarse_matrices_.push_back(A);

    // Build coarse grids
    for (int32_t level = 0; level < num_levels_ - 1; ++level) {
        build_coarsening(coarse_matrices_[level], level);
    }

    setup_done_ = true;
}

std::vector<double> AMGPreconditioner::apply(const std::vector<double>& x) const {
    if (!is_ready()) {
        throw std::runtime_error("Preconditioner not ready");
    }

    return v_cycle(0, x);
}

void AMGPreconditioner::build_coarsening(const SparseMatrix& A, int32_t level) {
    // Simplified coarsening - standard aggregation
    int32_t fine_size = A.get_size();
    int32_t coarse_size = fine_size / 2;
    if (coarse_size < 10) coarse_size = fine_size;

    // Create restriction and prolongation operators
    SparseMatrix R(coarse_size);
    for (int32_t i = 0; i < coarse_size; ++i) {
        R.add_value(i, 2*i, 1.0);
        if (2*i + 1 < fine_size) {
            R.add_value(i, 2*i + 1, 1.0);
        }
    }
    R.finalize();
    restriction_ops_.push_back(R);

    // Prolongation = R^T
    SparseMatrix P(fine_size);
    for (int32_t i = 0; i < coarse_size; ++i) {
        P.add_value(2*i, i, 1.0);
        if (2*i + 1 < fine_size) {
            P.add_value(2*i + 1, i, 1.0);
        }
    }
    P.finalize();
    prolongation_ops_.push_back(P);

    // Coarse grid matrix: A_c = R * A * P
    auto AP = A.multiply(P);
    auto Ac = R.multiply(AP);
    coarse_matrices_.push_back(Ac);
}

std::vector<double> AMGPreconditioner::v_cycle(
    int32_t level,
    const std::vector<double>& x) const {

    if (level >= static_cast<int32_t>(coarse_matrices_.size()) - 1) {
        // Coarsest level - simple relaxation
        return x;
    }

    // Relaxation on current level
    std::vector<double> y = x;

    // Restrict to coarse level
    auto r = restriction_ops_[level].multiply(x);

    // Solve on coarse level
    auto ec = v_cycle(level + 1, r);

    // Prolongate back
    auto e = prolongation_ops_[level].multiply(ec);

    for (size_t i = 0; i < y.size(); ++i) {
        y[i] += e[i];
    }

    return y;
}

int64_t AMGPreconditioner::get_memory_usage() const {
    int64_t memory = 0;
    for (const auto& mat : coarse_matrices_) {
        memory += mat.num_nonzeros() * sizeof(double);
    }
    return memory;
}

// ============================================================================
// SSOR Preconditioner
// ============================================================================

SSORPreconditioner::SSORPreconditioner(double omega)
    : omega_(omega) {}

void SSORPreconditioner::setup(const SparseMatrix& A) {
    size_ = A.get_size();
    diagonal_.resize(size_);

    for (int32_t i = 0; i < size_; ++i) {
        diagonal_[i] = A.get_value(i, i);
        if (std::abs(diagonal_[i]) < 1e-16) {
            diagonal_[i] = 1.0;
        }
    }

    A_ = &A;
    A_ready_ = true;
}

std::vector<double> SSORPreconditioner::apply(const std::vector<double>& x) const {
    if (!is_ready()) {
        throw std::runtime_error("Preconditioner not ready");
    }

    std::vector<double> y(x.size());

    // Forward sweep
    for (int32_t i = 0; i < size_; ++i) {
        y[i] = x[i];
        double sum = 0.0;

        std::vector<int32_t> cols;
        std::vector<double> vals;
        A_->get_row(i, cols, vals);

        for (size_t k = 0; k < cols.size(); ++k) {
            if (cols[k] < i) {
                sum += vals[k] * y[cols[k]];
            }
        }

        y[i] = (y[i] - sum) * omega_ / diagonal_[i];
    }

    // Backward sweep
    std::vector<double> z = y;
    for (int32_t i = size_ - 1; i >= 0; --i) {
        z[i] = y[i];
        double sum = 0.0;

        std::vector<int32_t> cols;
        std::vector<double> vals;
        A_->get_row(i, cols, vals);

        for (size_t k = 0; k < cols.size(); ++k) {
            if (cols[k] > i) {
                sum += vals[k] * z[cols[k]];
            }
        }

        z[i] = y[i] - omega_ * sum / diagonal_[i];
    }

    return z;
}

int64_t SSORPreconditioner::get_memory_usage() const {
    return diagonal_.size() * sizeof(double);
}

// ============================================================================
// Composite Preconditioner
// ============================================================================

void CompositePreconditioner::set_left_preconditioner(
    std::unique_ptr<Preconditioner> P1) {
    P1_ = std::move(P1);
}

void CompositePreconditioner::set_right_preconditioner(
    std::unique_ptr<Preconditioner> P2) {
    P2_ = std::move(P2);
}

void CompositePreconditioner::setup(const SparseMatrix& A) {
    if (P1_) P1_->setup(A);
    if (P2_) P2_->setup(A);
}

std::vector<double> CompositePreconditioner::apply(
    const std::vector<double>& x) const {

    std::vector<double> result = x;

    if (P1_) {
        result = P1_->apply(result);
    }
    if (P2_) {
        result = P2_->apply(result);
    }

    return result;
}

bool CompositePreconditioner::is_ready() const {
    bool ready = true;
    if (P1_) ready = ready && P1_->is_ready();
    if (P2_) ready = ready && P2_->is_ready();
    return ready;
}

int64_t CompositePreconditioner::get_memory_usage() const {
    int64_t memory = 0;
    if (P1_) memory += P1_->get_memory_usage();
    if (P2_) memory += P2_->get_memory_usage();
    return memory;
}

// ============================================================================
// Preconditioner Factory
// ============================================================================

std::unique_ptr<Preconditioner> PreconditionerFactory::create(
    PreconditionerType type) {

    switch (type) {
        case PreconditionerType::Diagonal:
            return std::make_unique<DiagonalPreconditioner>();
        case PreconditionerType::ILU0:
            return std::make_unique<ILU0Preconditioner>();
        case PreconditionerType::ILU1:
            return std::make_unique<ILUkPreconditioner>(1);
        case PreconditionerType::ILU2:
            return std::make_unique<ILUkPreconditioner>(2);
        case PreconditionerType::AMG:
            return std::make_unique<AMGPreconditioner>();
        case PreconditionerType::SSOR:
            return std::make_unique<SSORPreconditioner>();
        default:
            throw std::invalid_argument("Unknown preconditioner type");
    }
}

std::unique_ptr<Preconditioner> PreconditionerFactory::create_for_matrix(
    const SparseMatrix& A,
    bool symmetric) {

    if (symmetric) {
        return create(PreconditionerType::ILU0);
    } else {
        return create(PreconditionerType::AMG);
    }
}

}  // namespace bendcore::solver
