// Preconditioners for Iterative Solvers
// M7 Phase: Real-time Solver (GAP-030)
#pragma once

#include "bendcore/solver/real_time_solver.hpp"
#include <vector>
#include <cstdint>
#include <memory>

namespace bendcore::solver {

// Base class for preconditioners
class Preconditioner {
public:
    virtual ~Preconditioner() = default;

    // Setup preconditioner from matrix A
    virtual void setup(const SparseMatrix& A) = 0;

    // Apply preconditioner: y = M^{-1} * x
    virtual std::vector<double> apply(const std::vector<double>& x) const = 0;

    // Check if preconditioner is ready
    virtual bool is_ready() const = 0;

    // Get memory usage
    virtual int64_t get_memory_usage() const = 0;
};

// Diagonal (Jacobi) preconditioner
// M = diag(A)
class DiagonalPreconditioner : public Preconditioner {
public:
    void setup(const SparseMatrix& A) override;
    std::vector<double> apply(const std::vector<double>& x) const override;
    bool is_ready() const override { return diagonal_.size() > 0; }
    int64_t get_memory_usage() const override;

private:
    std::vector<double> diagonal_;
};

// ILU(0) preconditioner (Incomplete LU factorization)
// Performs LU decomposition but keeps only nonzero positions of A
class ILU0Preconditioner : public Preconditioner {
public:
    void setup(const SparseMatrix& A) override;
    std::vector<double> apply(const std::vector<double>& x) const override;
    bool is_ready() const override { return L_ready_ && U_ready_; }
    int64_t get_memory_usage() const override;

private:
    // Store L and U in sparse format
    SparseMatrix L_{0};
    SparseMatrix U_{0};
    bool L_ready_ = false;
    bool U_ready_ = false;
    int32_t size_;

    // Forward substitution: solve L*y = x
    std::vector<double> forward_substitution(const std::vector<double>& x) const;

    // Back substitution: solve U*z = y
    std::vector<double> back_substitution(const std::vector<double>& y) const;
};

// ILU(k) preconditioner
// Allows fill-in up to k levels
class ILUkPreconditioner : public Preconditioner {
public:
    explicit ILUkPreconditioner(int32_t fill_level = 1);

    void setup(const SparseMatrix& A) override;
    std::vector<double> apply(const std::vector<double>& x) const override;
    bool is_ready() const override { return L_ready_ && U_ready_; }
    int64_t get_memory_usage() const override;

private:
    int32_t fill_level_;
    SparseMatrix L_{0};
    SparseMatrix U_{0};
    bool L_ready_ = false;
    bool U_ready_ = false;
    int32_t size_;

    // Forward and back substitution
    std::vector<double> forward_substitution(const std::vector<double>& x) const;
    std::vector<double> back_substitution(const std::vector<double>& y) const;
};

// Algebraic Multigrid (AMG) preconditioner (simplified)
// Uses coarsening and interpolation for acceleration
class AMGPreconditioner : public Preconditioner {
public:
    explicit AMGPreconditioner(int32_t num_levels = 2);

    void setup(const SparseMatrix& A) override;
    std::vector<double> apply(const std::vector<double>& x) const override;
    bool is_ready() const override { return setup_done_; }
    int64_t get_memory_usage() const override;

private:
    int32_t num_levels_;
    bool setup_done_ = false;

    // Coarse grid matrices
    std::vector<SparseMatrix> coarse_matrices_;

    // Restriction (fine to coarse) and prolongation (coarse to fine) operators
    std::vector<SparseMatrix> restriction_ops_;
    std::vector<SparseMatrix> prolongation_ops_;

    // Diagonal scalers for smoothing
    std::vector<std::vector<double>> smoothers_;

    // Coarse grid aggregation
    void build_coarsening(const SparseMatrix& A, int32_t level);

    // V-cycle iteration
    std::vector<double> v_cycle(
        int32_t level,
        const std::vector<double>& x) const;
};

// SSOR (Symmetric Successive Over-Relaxation) preconditioner
class SSORPreconditioner : public Preconditioner {
public:
    explicit SSORPreconditioner(double omega = 1.0);

    void setup(const SparseMatrix& A) override;
    std::vector<double> apply(const std::vector<double>& x) const override;
    bool is_ready() const override { return A_ready_; }
    int64_t get_memory_usage() const override;

private:
    double omega_;
    std::vector<double> diagonal_;
    const SparseMatrix* A_ = nullptr;
    bool A_ready_ = false;
    int32_t size_;
};

// Composite preconditioner (combination of two preconditioners)
class CompositePreconditioner : public Preconditioner {
public:
    void set_left_preconditioner(std::unique_ptr<Preconditioner> P1);
    void set_right_preconditioner(std::unique_ptr<Preconditioner> P2);

    void setup(const SparseMatrix& A) override;
    std::vector<double> apply(const std::vector<double>& x) const override;
    bool is_ready() const override;
    int64_t get_memory_usage() const override;

private:
    std::unique_ptr<Preconditioner> P1_;
    std::unique_ptr<Preconditioner> P2_;
};

// Preconditioner factory
class PreconditionerFactory {
public:
    enum class PreconditionerType {
        Diagonal,
        ILU0,
        ILU1,
        ILU2,
        AMG,
        SSOR
    };

    static std::unique_ptr<Preconditioner> create(PreconditionerType type);
    static std::unique_ptr<Preconditioner> create_for_matrix(
        const SparseMatrix& A,
        bool symmetric = true);
};

}  // namespace bendcore::solver
