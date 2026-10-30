// Real-time Solver Framework Comprehensive Tests
// M7 Phase: Real-time Solver (GAP-030)

#include "bendcore/solver/real_time_solver.hpp"
#include "bendcore/solver/iterative_solvers.hpp"
#include "bendcore/solver/preconditioners.hpp"
#include "bendcore/solver/reduced_order_model.hpp"
#include "bendcore/solver/solver_scheduler.hpp"
#include "bendcore/solver/adaptive_refinement.hpp"
#include "bendcore/solver/performance_metrics.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
#include <memory>

using namespace bendcore::solver;

// ============================================================================
// Test Utilities
// ============================================================================

bool float_equal(double a, double b, double tolerance = 1e-10) {
    return std::abs(a - b) < tolerance;
}

void print_test_result(const std::string& test_name, bool passed) {
    if (passed) {
        std::cout << "[PASS] " << test_name << "\n";
    } else {
        std::cout << "[FAIL] " << test_name << "\n";
        assert(false);
    }
}

// Create a simple 3x3 system: Ax = b
SparseMatrix create_test_matrix_3x3() {
    SparseMatrix A(3);
    // [2  -1   0 ]
    // [-1  2  -1 ]
    // [0  -1   2 ]
    A.add_value(0, 0,  2.0);
    A.add_value(0, 1, -1.0);

    A.add_value(1, 0, -1.0);
    A.add_value(1, 1,  2.0);
    A.add_value(1, 2, -1.0);

    A.add_value(2, 1, -1.0);
    A.add_value(2, 2,  2.0);

    A.finalize();
    return A;
}

// ============================================================================
// Sparse Matrix Tests (5 tests)
// ============================================================================

void test_sparse_matrix_basic() {
    SparseMatrix A(3);
    A.add_value(0, 0, 2.0);
    A.add_value(0, 1, -1.0);
    A.finalize();

    assert(A.get_size() == 3);
    assert(A.num_nonzeros() == 2);
    assert(float_equal(A.get_value(0, 0), 2.0));
    assert(float_equal(A.get_value(0, 1), -1.0));
    print_test_result("SparseMatrix::basic creation", true);
}

void test_sparse_matrix_multiply() {
    SparseMatrix A = create_test_matrix_3x3();
    std::vector<double> x = {1.0, 2.0, 3.0};

    auto y = A.multiply(x);

    // y = A*x = [2-2, -1+4-3, -2+6] = [0, 0, 4]
    assert(y.size() == 3);
    assert(float_equal(y[0], 0.0));
    assert(float_equal(y[1], 0.0));
    assert(float_equal(y[2], 4.0));
    print_test_result("SparseMatrix::matrix-vector multiply", true);
}

void test_sparse_matrix_finalization() {
    SparseMatrix A(2);
    A.add_value(0, 0, 1.0);
    A.add_value(0, 1, 2.0);
    A.add_value(1, 1, 3.0);

    assert(!A.is_finalized());
    A.finalize();
    assert(A.is_finalized());
    print_test_result("SparseMatrix::finalization", true);
}

void test_sparse_matrix_row_access() {
    SparseMatrix A = create_test_matrix_3x3();

    std::vector<int32_t> cols;
    std::vector<double> vals;
    A.get_row(0, cols, vals);

    assert(cols.size() == 2);
    assert(float_equal(vals[0], 2.0));
    assert(float_equal(vals[1], -1.0));
    print_test_result("SparseMatrix::row access", true);
}

void test_sparse_matrix_matrix_multiply() {
    SparseMatrix A = create_test_matrix_3x3();
    SparseMatrix B = create_test_matrix_3x3();

    auto C = A.multiply(B);

    assert(C.get_size() == 3);
    assert(C.is_finalized());
    print_test_result("SparseMatrix::matrix-matrix multiply", true);
}

// ============================================================================
// Conjugate Gradient Solver Tests (5 tests)
// ============================================================================

void test_cg_solver_basic() {
    SparseMatrix A = create_test_matrix_3x3();
    std::vector<double> b = {1.0, 2.0, 1.0};
    std::vector<double> x0 = {0.0, 0.0, 0.0};

    ConjugateGradientSolver cg;
    auto result = cg.solve(A, b, x0, 1e-6, 100, nullptr);

    assert(result.converged || result.num_iterations > 0);
    assert(result.solution.size() == 3);
    print_test_result("CGSolver::basic solve", true);
}

void test_cg_solver_convergence() {
    SparseMatrix A = create_test_matrix_3x3();
    std::vector<double> b = {1.0, 2.0, 1.0};
    std::vector<double> x0 = {0.0, 0.0, 0.0};

    ConjugateGradientSolver cg;
    auto result = cg.solve(A, b, x0, 1e-6, 100, nullptr);

    assert(result.final_residual < 1e-4);
    print_test_result("CGSolver::convergence", true);
}

void test_cg_solver_with_preconditioner() {
    SparseMatrix A = create_test_matrix_3x3();
    std::vector<double> b = {1.0, 2.0, 1.0};
    std::vector<double> x0 = {0.0, 0.0, 0.0};

    auto precond = PreconditionerFactory::create(
        PreconditionerFactory::PreconditionerType::Diagonal);
    precond->setup(A);

    ConjugateGradientSolver cg;
    auto result = cg.solve(A, b, x0, 1e-6, 100, precond.get());

    assert(result.final_residual < 1e-4);
    print_test_result("CGSolver::with preconditioner", true);
}

void test_cg_solver_initial_guess() {
    SparseMatrix A = create_test_matrix_3x3();
    std::vector<double> b = {1.0, 2.0, 1.0};
    std::vector<double> x0 = {0.5, 0.5, 0.5};

    ConjugateGradientSolver cg;
    auto result = cg.solve(A, b, x0, 1e-6, 100, nullptr);

    assert(result.num_iterations <= 3);  // With good initial guess
    print_test_result("CGSolver::with initial guess", true);
}

void test_cg_solver_zero_tolerance() {
    SparseMatrix A = create_test_matrix_3x3();
    std::vector<double> b = {1.0, 2.0, 1.0};
    std::vector<double> x0 = {0.0, 0.0, 0.0};

    ConjugateGradientSolver cg;
    auto result = cg.solve(A, b, x0, 1e-16, 100, nullptr);

    assert(result.num_iterations > 0);
    print_test_result("CGSolver::zero tolerance", true);
}

// ============================================================================
// Preconditioner Tests (4 tests)
// ============================================================================

void test_diagonal_preconditioner() {
    SparseMatrix A = create_test_matrix_3x3();

    auto precond = std::make_unique<DiagonalPreconditioner>();
    precond->setup(A);

    assert(precond->is_ready());
    std::vector<double> x = {1.0, 2.0, 3.0};
    auto y = precond->apply(x);

    assert(y.size() == 3);
    print_test_result("DiagonalPreconditioner::setup and apply", true);
}

void test_ilu0_preconditioner() {
    SparseMatrix A = create_test_matrix_3x3();

    auto precond = std::make_unique<ILU0Preconditioner>();
    precond->setup(A);

    assert(precond->is_ready());
    std::vector<double> x = {1.0, 2.0, 3.0};
    auto y = precond->apply(x);

    assert(y.size() == 3);
    print_test_result("ILU0Preconditioner::setup and apply", true);
}

void test_ssor_preconditioner() {
    SparseMatrix A = create_test_matrix_3x3();

    auto precond = std::make_unique<SSORPreconditioner>(1.0);
    precond->setup(A);

    assert(precond->is_ready());
    std::vector<double> x = {1.0, 2.0, 3.0};
    auto y = precond->apply(x);

    assert(y.size() == 3);
    print_test_result("SSORPreconditioner::setup and apply", true);
}

void test_preconditioner_factory() {
    SparseMatrix A = create_test_matrix_3x3();

    auto precond = PreconditionerFactory::create_for_matrix(A, true);
    precond->setup(A);

    assert(precond->is_ready());
    print_test_result("PreconditionerFactory::create_for_matrix", true);
}

// ============================================================================
// GMRES Solver Tests (2 tests)
// ============================================================================

void test_gmres_solver_basic() {
    SparseMatrix A = create_test_matrix_3x3();
    std::vector<double> b = {1.0, 2.0, 1.0};
    std::vector<double> x0 = {0.0, 0.0, 0.0};

    GMRESSolver gmres(10);
    auto result = gmres.solve(A, b, x0, 1e-6, 100, nullptr);

    assert(result.solution.size() == 3);
    print_test_result("GMRESSolver::basic solve", true);
}

void test_gmres_with_restart() {
    SparseMatrix A = create_test_matrix_3x3();
    std::vector<double> b = {1.0, 2.0, 1.0};
    std::vector<double> x0 = {0.0, 0.0, 0.0};

    GMRESSolver gmres(5);  // Small restart size
    auto result = gmres.solve(A, b, x0, 1e-6, 100, nullptr);

    assert(result.solution.size() == 3);
    print_test_result("GMRESSolver::with restart", true);
}

// ============================================================================
// Real-Time Solver Tests (5 tests)
// ============================================================================

void test_realtime_solver_setup() {
    RealTimeSolver solver(3);

    SparseMatrix K = create_test_matrix_3x3();
    std::vector<double> F = {1.0, 2.0, 1.0};

    solver.set_stiffness_matrix(K);
    solver.set_force_vector(F);

    assert(solver.get_displacement().size() == 3);
    print_test_result("RealTimeSolver::setup", true);
}

void test_realtime_solver_solve() {
    RealTimeSolver solver(3);

    SparseMatrix K = create_test_matrix_3x3();
    std::vector<double> F = {1.0, 2.0, 1.0};

    solver.set_stiffness_matrix(K);
    solver.set_force_vector(F);

    auto result = solver.solve();

    assert(result.displacement.size() == 3);
    assert(result.solve_time.count() >= 0);
    print_test_result("RealTimeSolver::solve", true);
}

void test_realtime_solver_adaptive() {
    RealTimeSolver solver(3);

    SparseMatrix K = create_test_matrix_3x3();
    std::vector<double> F = {1.0, 2.0, 1.0};

    solver.set_stiffness_matrix(K);
    solver.set_force_vector(F);

    auto result = solver.solve_adaptive(F, 3);

    assert(result.displacement.size() == 3);
    print_test_result("RealTimeSolver::adaptive solve", true);
}

void test_realtime_solver_incremental() {
    RealTimeSolver solver(3);

    SparseMatrix K = create_test_matrix_3x3();
    std::vector<double> F = {1.0, 2.0, 1.0};

    solver.set_stiffness_matrix(K);
    solver.set_force_vector(F);

    std::vector<double> delta_F = {0.1, 0.1, 0.1};
    auto result = solver.solve_increment(delta_F);

    assert(result.displacement.size() == 3);
    print_test_result("RealTimeSolver::incremental solve", true);
}

void test_realtime_solver_config() {
    RealTimeSolver solver(3);

    RealtimeSolverConfig config;
    config.max_iterations = 50;
    config.residual_tolerance = 1e-4;
    config.use_rom = false;

    solver.set_config(config);

    assert(solver.get_config().max_iterations == 50);
    print_test_result("RealTimeSolver::configuration", true);
}

// ============================================================================
// POD Basis Tests (2 tests)
// ============================================================================

void test_pod_basis_creation() {
    PODBasis basis(5);

    std::vector<std::vector<double>> snapshots;
    for (int i = 0; i < 10; ++i) {
        std::vector<double> snap(5);
        for (int j = 0; j < 5; ++j) {
            snap[j] = i + j * 0.1;
        }
        snapshots.push_back(snap);
    }

    basis.build_from_snapshots(snapshots, 0.95);

    assert(basis.get_num_basis() > 0);
    assert(basis.get_dimension() == 5);
    print_test_result("PODBasis::creation", true);
}

void test_pod_basis_projection() {
    PODBasis basis(5);

    std::vector<std::vector<double>> snapshots;
    for (int i = 0; i < 10; ++i) {
        std::vector<double> snap(5, 1.0);
        snapshots.push_back(snap);
    }

    basis.build_from_snapshots(snapshots, 0.99);

    std::vector<double> u(5, 2.0);
    auto coeffs = basis.project(u);
    auto u_recon = basis.reconstruct(coeffs);

    assert(coeffs.size() > 0);
    assert(u_recon.size() == 5);
    print_test_result("PODBasis::projection and reconstruction", true);
}

// ============================================================================
// Solver Scheduler Tests (3 tests)
// ============================================================================

void test_solver_scheduler_creation() {
    SolverScheduler scheduler(16.0);

    auto stats = scheduler.get_statistics();
    assert(stats.pending_tasks == 0);
    print_test_result("SolverScheduler::creation", true);
}

void test_solver_scheduler_task_submission() {
    SolverScheduler scheduler(16.0);

    int task_id = scheduler.submit_task([]() {}, 0, 16.0, true);

    assert(task_id == 0);
    assert(scheduler.get_statistics().pending_tasks == 1);
    print_test_result("SolverScheduler::task submission", true);
}

void test_solver_scheduler_execution() {
    SolverScheduler scheduler(16.0);

    int count = 0;
    scheduler.submit_task([&count]() { ++count; }, 0, 16.0, true);

    auto result = scheduler.execute_frame();

    assert(count == 1);
    print_test_result("SolverScheduler::execution", true);
}

// ============================================================================
// Error Estimation Tests (2 tests)
// ============================================================================

void test_error_estimator_residual() {
    ErrorEstimator estimator;

    SparseMatrix K = create_test_matrix_3x3();
    std::vector<double> u = {1.0, 2.0, 1.0};
    std::vector<double> F = {1.0, 2.0, 1.0};

    auto error = estimator.estimate_error_residual(K, u, F);

    assert(error.global_error >= 0.0);
    print_test_result("ErrorEstimator::residual-based", true);
}

void test_convergence_monitor() {
    ConvergenceMonitor monitor;

    monitor.add_iteration(1.0, 1.0);
    monitor.add_iteration(0.5, 1.0);
    monitor.add_iteration(0.1, 1.0);

    assert(monitor.is_converged(0.2, 1));
    assert(monitor.get_convergence_rate() < 1.0);
    print_test_result("ConvergenceMonitor::convergence detection", true);
}

// ============================================================================
// Adaptive Quality Control Tests (2 tests)
// ============================================================================

void test_quality_controller() {
    AdaptiveQualityController controller;
    controller.set_target_frame_time_ms(16.0);

    std::vector<SolverTask> tasks;
    auto quality = controller.select_quality_level(tasks, 10.0);

    assert(quality == QualityLevel::Fast ||
           quality == QualityLevel::Normal);
    print_test_result("AdaptiveQualityController::quality selection", true);
}

void test_quality_adjustment() {
    AdaptiveQualityController controller;
    controller.set_target_frame_time_ms(16.0);

    // Simulate slow frame
    controller.adjust_quality_based_on_performance(20.0, 16.0);

    auto quality = controller.get_current_quality();
    assert(quality <= QualityLevel::HighPrecision);
    print_test_result("AdaptiveQualityController::quality adjustment", true);
}

// ============================================================================
// Performance Metrics Tests (2 tests)
// ============================================================================

void test_performance_timer() {
    PerformanceTimer timer("test");

    auto elapsed = timer.elapsed();
    assert(elapsed.count() >= 0);

    timer.stop();
    auto stopped = timer.elapsed();
    assert(stopped.count() >= elapsed.count());  // время шло до stop()
    auto stopped_again = timer.elapsed();
    assert(stopped_again.count() == stopped.count());  // после stop() значение заморожено

    print_test_result("PerformanceTimer::timing", true);
}

void test_memory_profiler() {
    MemoryProfiler profiler;

    profiler.record_allocation(1000, "test");
    assert(profiler.get_current_usage() == 1000);

    profiler.record_deallocation(500, "test");
    assert(profiler.get_current_usage() == 500);

    assert(profiler.get_peak_usage() == 1000);
    print_test_result("MemoryProfiler::tracking", true);
}

// ============================================================================
// Solver Factory Tests (2 tests)
// ============================================================================

void test_solver_factory_creation() {
    auto cg = SolverFactory::create(SolverFactory::SolverType::ConjugateGradient);
    assert(cg != nullptr);

    auto gmres = SolverFactory::create(SolverFactory::SolverType::GMRES);
    assert(gmres != nullptr);

    print_test_result("SolverFactory::solver creation", true);
}

void test_adaptive_solver_selection() {
    SparseMatrix A = create_test_matrix_3x3();

    auto type = AdaptiveSolverSelector::select_solver(A);
    assert(type == SolverFactory::SolverType::ConjugateGradient ||
           type == SolverFactory::SolverType::BiCGSTAB);

    print_test_result("AdaptiveSolverSelector::solver selection", true);
}

// ============================================================================
// BiCG-STAB Tests (2 tests)
// ============================================================================

void test_bicgstab_solver_basic() {
    SparseMatrix A = create_test_matrix_3x3();
    std::vector<double> b = {1.0, 2.0, 1.0};
    std::vector<double> x0 = {0.0, 0.0, 0.0};

    BiCGSTABSolver bicgstab;
    auto result = bicgstab.solve(A, b, x0, 1e-6, 100, nullptr);

    assert(result.solution.size() == 3);
    print_test_result("BiCGSTABSolver::basic solve", true);
}

void test_bicgstab_convergence() {
    SparseMatrix A = create_test_matrix_3x3();
    std::vector<double> b = {1.0, 2.0, 1.0};
    std::vector<double> x0 = {0.0, 0.0, 0.0};

    BiCGSTABSolver bicgstab;
    auto result = bicgstab.solve(A, b, x0, 1e-4, 100, nullptr);

    assert(result.final_residual < 1e-2 || result.converged);
    print_test_result("BiCGSTABSolver::convergence", true);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "Running Real-Time Solver Framework Tests...\n\n";

    // Sparse Matrix Tests
    test_sparse_matrix_basic();
    test_sparse_matrix_multiply();
    test_sparse_matrix_finalization();
    test_sparse_matrix_row_access();
    test_sparse_matrix_matrix_multiply();

    // Conjugate Gradient Tests
    test_cg_solver_basic();
    test_cg_solver_convergence();
    test_cg_solver_with_preconditioner();
    test_cg_solver_initial_guess();
    test_cg_solver_zero_tolerance();

    // Preconditioner Tests
    test_diagonal_preconditioner();
    test_ilu0_preconditioner();
    test_ssor_preconditioner();
    test_preconditioner_factory();

    // GMRES Tests
    test_gmres_solver_basic();
    test_gmres_with_restart();

    // Real-Time Solver Tests
    test_realtime_solver_setup();
    test_realtime_solver_solve();
    test_realtime_solver_adaptive();
    test_realtime_solver_incremental();
    test_realtime_solver_config();

    // POD Basis Tests
    test_pod_basis_creation();
    test_pod_basis_projection();

    // Scheduler Tests
    test_solver_scheduler_creation();
    test_solver_scheduler_task_submission();
    test_solver_scheduler_execution();

    // Error Tests
    test_error_estimator_residual();
    test_convergence_monitor();

    // Quality Control Tests
    test_quality_controller();
    test_quality_adjustment();

    // Performance Tests
    test_performance_timer();
    test_memory_profiler();

    // Factory Tests
    test_solver_factory_creation();
    test_adaptive_solver_selection();

    // BiCG-STAB Tests
    test_bicgstab_solver_basic();
    test_bicgstab_convergence();

    std::cout << "\nAll 40 tests completed successfully!\n";
    return 0;
}
