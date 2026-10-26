// Iterative Solvers Implementation
// M7 Phase: Real-time Solver (GAP-030)

#include "bendcore/solver/iterative_solvers.hpp"
#include "bendcore/solver/preconditioners.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace bendcore::solver {

// ============================================================================
// Conjugate Gradient Solver
// ============================================================================

double ConjugateGradientSolver::dot_product(
    const std::vector<double>& a,
    const std::vector<double>& b) {
    double result = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        result += a[i] * b[i];
    }
    return result;
}

double ConjugateGradientSolver::l2_norm(const std::vector<double>& v) {
    return std::sqrt(dot_product(v, v));
}

double ConjugateGradientSolver::relative_residual(
    const std::vector<double>& r,
    const std::vector<double>& b) {
    double r_norm = l2_norm(r);
    double b_norm = l2_norm(b);
    return b_norm > 1e-16 ? r_norm / b_norm : r_norm;
}

IterativeSolverResult ConjugateGradientSolver::solve(
    const SparseMatrix& A,
    const std::vector<double>& b,
    const std::vector<double>& x0,
    double tolerance,
    int32_t max_iterations,
    Preconditioner* preconditioner) {

    int32_t n = A.get_size();
    if (static_cast<int32_t>(b.size()) != n || static_cast<int32_t>(x0.size()) != n) {
        throw std::invalid_argument("Vector size mismatch");
    }

    std::vector<double> x = x0;
    std::vector<double> r = b;

    // r = b - A*x
    auto Ax = A.multiply(x);
    for (int32_t i = 0; i < n; ++i) {
        r[i] -= Ax[i];
    }

    double initial_residual = l2_norm(r);
    double final_residual = initial_residual;

    if (initial_residual < tolerance) {
        return IterativeSolverResult{true, 0, initial_residual, x, 1.0};
    }

    // Apply preconditioner
    std::vector<double> y = r;
    if (preconditioner && preconditioner->is_ready()) {
        y = preconditioner->apply(r);
    }

    std::vector<double> p = y;
    double rho = dot_product(r, y);
    double rho_prev = rho;

    int32_t iteration = 0;
    double convergence_rate = 1.0;

    for (iteration = 0; iteration < max_iterations; ++iteration) {
        // Compute step size
        auto Ap = A.multiply(p);
        double alpha = rho / dot_product(p, Ap);

        // Update solution and residual
        for (int32_t i = 0; i < n; ++i) {
            x[i] += alpha * p[i];
            r[i] -= alpha * Ap[i];
        }

        final_residual = l2_norm(r);
        double rel_residual = relative_residual(r, b);

        if (rel_residual < tolerance) {
            convergence_rate = final_residual / std::max(1e-16, initial_residual);
            ++iteration;
            return IterativeSolverResult{true, iteration, final_residual, x, convergence_rate};
        }

        // Apply preconditioner
        y = r;
        if (preconditioner && preconditioner->is_ready()) {
            y = preconditioner->apply(r);
        }

        rho = dot_product(r, y);
        double beta = rho / rho_prev;

        // Update conjugate direction
        for (int32_t i = 0; i < n; ++i) {
            p[i] = y[i] + beta * p[i];
        }

        rho_prev = rho;
    }

    convergence_rate = final_residual / std::max(1e-16, initial_residual);
    return IterativeSolverResult{false, iteration, final_residual, x, convergence_rate};
}

// ============================================================================
// GMRES Solver
// ============================================================================

GMRESSolver::GMRESSolver(int32_t restart_size)
    : restart_size_(restart_size) {}

double GMRESSolver::dot_product(
    const std::vector<double>& a,
    const std::vector<double>& b) {
    double result = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        result += a[i] * b[i];
    }
    return result;
}

double GMRESSolver::l2_norm(const std::vector<double>& v) {
    return std::sqrt(dot_product(v, v));
}

void GMRESSolver::orthogonalize_vector(
    std::vector<double>& v,
    const std::vector<std::vector<double>>& basis) {

    for (const auto& basis_vec : basis) {
        double coeff = dot_product(v, basis_vec);
        for (size_t i = 0; i < v.size(); ++i) {
            v[i] -= coeff * basis_vec[i];
        }
    }
}

void GMRESSolver::apply_givens_rotation(
    std::vector<double>& h,
    std::vector<double>& cs,
    std::vector<double>& sn,
    int32_t k) {

    for (int32_t i = 0; i < k; ++i) {
        double temp = cs[i] * h[i] + sn[i] * h[i + 1];
        h[i + 1] = -sn[i] * h[i] + cs[i] * h[i + 1];
        h[i] = temp;
    }

    // Compute new rotation
    double alpha = h[k];
    double beta = h[k + 1];
    double gamma = std::sqrt(alpha * alpha + beta * beta);
    cs[k] = alpha / gamma;
    sn[k] = beta / gamma;
    h[k] = gamma;
    h[k + 1] = 0.0;
}

std::vector<double> GMRESSolver::solve_hessenberg(
    const std::vector<std::vector<double>>& H,
    const std::vector<double>& g,
    int32_t m) {

    std::vector<double> y(m);
    for (int32_t i = m - 1; i >= 0; --i) {
        y[i] = g[i];
        for (int32_t j = i + 1; j < m; ++j) {
            y[i] -= H[i][j] * y[j];
        }
        if (std::abs(H[i][i]) > 1e-16) {
            y[i] /= H[i][i];
        }
    }
    return y;
}

IterativeSolverResult GMRESSolver::solve(
    const SparseMatrix& A,
    const std::vector<double>& b,
    const std::vector<double>& x0,
    double tolerance,
    int32_t max_iterations,
    Preconditioner* preconditioner) {

    int32_t n = A.get_size();
    std::vector<double> x = x0;

    int32_t iteration = 0;
    double initial_residual = 0.0;
    double final_residual = 0.0;

    for (int32_t restart = 0; restart * restart_size_ < max_iterations; ++restart) {
        arnoldi_basis_.clear();
        hessenberg_.clear();

        // Initial residual
        auto Ax = A.multiply(x);
        std::vector<double> r(n);
        for (int32_t i = 0; i < n; ++i) {
            r[i] = b[i] - Ax[i];
        }

        if (restart == 0) {
            initial_residual = l2_norm(r);
        }

        double beta = l2_norm(r);
        if (beta < tolerance) {
            return IterativeSolverResult{true, iteration, beta, x, 1.0};
        }

        for (int32_t i = 0; i < n; ++i) {
            r[i] /= beta;
        }
        arnoldi_basis_.push_back(r);

        std::vector<double> g(restart_size_ + 1, 0.0);
        g[0] = beta;

        std::vector<double> cs(restart_size_, 0.0);
        std::vector<double> sn(restart_size_, 0.0);

        int32_t m = 0;
        for (m = 0; m < restart_size_ && iteration < max_iterations; ++m, ++iteration) {
            auto v = A.multiply(arnoldi_basis_[m]);

            if (preconditioner && preconditioner->is_ready()) {
                v = preconditioner->apply(v);
            }

            hessenberg_.push_back(std::vector<double>(m + 2, 0.0));

            orthogonalize_vector(v, arnoldi_basis_);

            double h_norm = l2_norm(v);
            hessenberg_[m][m] = h_norm;

            for (int32_t i = 0; i < n; ++i) {
                v[i] /= h_norm;
            }
            arnoldi_basis_.push_back(v);

            apply_givens_rotation(hessenberg_[m], cs, sn, m);

            g[m + 1] = -sn[m] * g[m];
            g[m] *= cs[m];

            final_residual = std::abs(g[m + 1]);
            if (final_residual < tolerance) {
                break;
            }
        }

        // Back substitution
        auto y = solve_hessenberg(hessenberg_, g, m);

        // Update solution
        for (int32_t i = 0; i < m; ++i) {
            for (int32_t j = 0; j < n; ++j) {
                x[j] += y[i] * arnoldi_basis_[i][j];
            }
        }

        if (final_residual < tolerance) {
            double convergence_rate = final_residual / std::max(1e-16, initial_residual);
            return IterativeSolverResult{true, iteration, final_residual, x, convergence_rate};
        }
    }

    double convergence_rate = final_residual / std::max(1e-16, initial_residual);
    return IterativeSolverResult{false, iteration, final_residual, x, convergence_rate};
}

// ============================================================================
// BiCG-STAB Solver
// ============================================================================

// Define static helper functions for BiCGSTAB
double BiCGSTABSolver::dot_product(
    const std::vector<double>& a,
    const std::vector<double>& b) {
    double result = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        result += a[i] * b[i];
    }
    return result;
}

double BiCGSTABSolver::l2_norm(const std::vector<double>& v) {
    return std::sqrt(dot_product(v, v));
}

IterativeSolverResult BiCGSTABSolver::solve(
    const SparseMatrix& A,
    const std::vector<double>& b,
    const std::vector<double>& x0,
    double tolerance,
    int32_t max_iterations,
    Preconditioner* preconditioner) {

    int32_t n = A.get_size();
    std::vector<double> x = x0;

    auto Ax = A.multiply(x);
    std::vector<double> r(n);
    for (int32_t i = 0; i < n; ++i) {
        r[i] = b[i] - Ax[i];
    }

    std::vector<double> r0 = r;

    double initial_residual = l2_norm(r);
    if (initial_residual < tolerance) {
        return IterativeSolverResult{true, 0, initial_residual, x, 1.0};
    }

    std::vector<double> p = r;
    double rho = 1.0;
    double alpha = 1.0;
    double omega = 1.0;

    int32_t iteration = 0;
    double final_residual = initial_residual;

    for (iteration = 0; iteration < max_iterations; ++iteration) {
        double rho_new = dot_product(r0, r);

        if (std::abs(rho_new) < 1e-16) {
            break;
        }

        double beta = (rho_new / rho) * (alpha / omega);

        for (int32_t i = 0; i < n; ++i) {
            p[i] = r[i] + beta * (p[i] - omega * A.multiply(p)[i]);
        }

        auto Ap = A.multiply(p);
        if (preconditioner && preconditioner->is_ready()) {
            Ap = preconditioner->apply(Ap);
        }

        alpha = rho_new / dot_product(r0, Ap);

        std::vector<double> s(n);
        for (int32_t i = 0; i < n; ++i) {
            s[i] = r[i] - alpha * Ap[i];
            x[i] += alpha * p[i];
        }

        auto As = A.multiply(s);
        if (preconditioner && preconditioner->is_ready()) {
            As = preconditioner->apply(As);
        }

        omega = dot_product(As, s) / dot_product(As, As);

        for (int32_t i = 0; i < n; ++i) {
            x[i] += omega * s[i];
            r[i] = s[i] - omega * As[i];
        }

        final_residual = l2_norm(r);
        if (final_residual < tolerance) {
            ++iteration;
            double convergence_rate = final_residual / std::max(1e-16, initial_residual);
            return IterativeSolverResult{true, iteration, final_residual, x, convergence_rate};
        }

        rho = rho_new;
    }

    double convergence_rate = final_residual / std::max(1e-16, initial_residual);
    return IterativeSolverResult{false, iteration, final_residual, x, convergence_rate};
}

// ============================================================================
// Solver Factory
// ============================================================================

std::unique_ptr<IterativeSolver> SolverFactory::create(SolverType type) {
    switch (type) {
        case SolverType::ConjugateGradient:
            return std::make_unique<ConjugateGradientSolver>();
        case SolverType::GMRES:
            return std::make_unique<GMRESSolver>(30);
        case SolverType::BiCGSTAB:
            return std::make_unique<BiCGSTABSolver>();
        default:
            throw std::invalid_argument("Unknown solver type");
    }
}

std::unique_ptr<IterativeSolver> SolverFactory::create_for_matrix(const SparseMatrix& A) {
    // Use adaptive selector
    auto type = AdaptiveSolverSelector::select_solver(A);
    return create(type);
}

// ============================================================================
// Adaptive Solver Selector
// ============================================================================

SolverFactory::SolverType AdaptiveSolverSelector::select_solver(const SparseMatrix& A) {
    if (is_symmetric(A)) {
        return SolverFactory::SolverType::ConjugateGradient;
    } else {
        return SolverFactory::SolverType::BiCGSTAB;
    }
}

bool AdaptiveSolverSelector::is_symmetric(const SparseMatrix& A, double tolerance) {
    int32_t n = A.get_size();
    int32_t samples = std::min(n, 100);

    for (int32_t i = 0; i < samples; ++i) {
        for (int32_t j = i; j < samples; ++j) {
            double aij = A.get_value(i, j);
            double aji = A.get_value(j, i);
            if (std::abs(aij - aji) > tolerance * (1.0 + std::abs(aij))) {
                return false;
            }
        }
    }

    return true;
}

}  // namespace bendcore::solver
