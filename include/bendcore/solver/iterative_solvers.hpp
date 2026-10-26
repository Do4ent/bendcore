// Iterative Linear Solvers
// M7 Phase: Real-time Solver (GAP-030)
#pragma once

#include "bendcore/solver/real_time_solver.hpp"
#include <vector>
#include <cstdint>
#include <memory>

namespace bendcore::solver {

// Forward declaration
class Preconditioner;

// Result of iterative solver
struct IterativeSolverResult {
    bool converged;
    int32_t num_iterations;
    double final_residual;
    std::vector<double> solution;
    double convergence_rate;
};

// Base class for iterative solvers
class IterativeSolver {
public:
    virtual ~IterativeSolver() = default;

    virtual IterativeSolverResult solve(
        const SparseMatrix& A,
        const std::vector<double>& b,
        const std::vector<double>& x0,
        double tolerance,
        int32_t max_iterations,
        Preconditioner* preconditioner = nullptr) = 0;
};

// Conjugate Gradient (CG) solver
// For symmetric positive-definite systems
class ConjugateGradientSolver : public IterativeSolver {
public:
    IterativeSolverResult solve(
        const SparseMatrix& A,
        const std::vector<double>& b,
        const std::vector<double>& x0,
        double tolerance,
        int32_t max_iterations,
        Preconditioner* preconditioner = nullptr) override;

private:
    // Utility functions
    static double dot_product(const std::vector<double>& a, const std::vector<double>& b);
    static double l2_norm(const std::vector<double>& v);
    static double relative_residual(
        const std::vector<double>& r,
        const std::vector<double>& b);
};

// GMRES (Generalized Minimal Residual) solver
// For general non-symmetric systems
class GMRESSolver : public IterativeSolver {
public:
    explicit GMRESSolver(int32_t restart_size = 30);

    IterativeSolverResult solve(
        const SparseMatrix& A,
        const std::vector<double>& b,
        const std::vector<double>& x0,
        double tolerance,
        int32_t max_iterations,
        Preconditioner* preconditioner = nullptr) override;

private:
    int32_t restart_size_;

    // Arnoldi iteration
    std::vector<std::vector<double>> arnoldi_basis_;
    std::vector<std::vector<double>> hessenberg_;

    // Utility functions
    static double dot_product(const std::vector<double>& a, const std::vector<double>& b);
    static double l2_norm(const std::vector<double>& v);

    // Gram-Schmidt orthogonalization
    void orthogonalize_vector(
        std::vector<double>& v,
        const std::vector<std::vector<double>>& basis);

    // Givens rotation for QR decomposition
    void apply_givens_rotation(
        std::vector<double>& h,
        std::vector<double>& cs,
        std::vector<double>& sn,
        int32_t k);

    // Back substitution for upper Hessenberg system
    std::vector<double> solve_hessenberg(
        const std::vector<std::vector<double>>& H,
        const std::vector<double>& g,
        int32_t m);
};

// BiCG-STAB (BiConjugate Gradient Stabilized) solver
// For non-symmetric systems (more stable than BiCG)
class BiCGSTABSolver : public IterativeSolver {
public:
    IterativeSolverResult solve(
        const SparseMatrix& A,
        const std::vector<double>& b,
        const std::vector<double>& x0,
        double tolerance,
        int32_t max_iterations,
        Preconditioner* preconditioner = nullptr) override;

private:
    // Utility functions
    static double dot_product(const std::vector<double>& a, const std::vector<double>& b);
    static double l2_norm(const std::vector<double>& v);
};

// Iterative solver factory
class SolverFactory {
public:
    enum class SolverType {
        ConjugateGradient,
        GMRES,
        BiCGSTAB
    };

    static std::unique_ptr<IterativeSolver> create(SolverType type);
    static std::unique_ptr<IterativeSolver> create_for_matrix(const SparseMatrix& A);
};

// Solver selector based on matrix properties
class AdaptiveSolverSelector {
public:
    static SolverFactory::SolverType select_solver(const SparseMatrix& A);
    static bool is_symmetric(const SparseMatrix& A, double tolerance = 1e-10);
};

}  // namespace bendcore::solver
