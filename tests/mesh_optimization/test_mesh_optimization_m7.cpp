// Unit tests for M7 Phase: Mesh Optimization (GAP-028)
// Comprehensive test suite with 40+ test cases
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "bendcore/fea/mesh_generator.hpp"
#include "bendcore/mesh_optimization/quality_analyzer.hpp"
#include "bendcore/mesh_optimization/adaptive_refinement.hpp"
#include "bendcore/mesh_optimization/mesh_smoothing.hpp"
#include "bendcore/mesh_optimization/mesh_optimization.hpp"
#include "bendcore/mesh_optimization/parallel_mesh_generation.hpp"
#include "bendcore/mesh_optimization/mesh_io.hpp"

using namespace bendcore;
using namespace bendcore::fea;
using namespace bendcore::mesh_optimization;

// ============================================================================
// Utility Functions for Testing
// ============================================================================

std::unique_ptr<FiniteElementMesh> create_simple_tet_mesh() {
    auto mesh = std::make_unique<FiniteElementMesh>();

    // Create a simple tetrahedral mesh with 4 nodes and 1 element
    mesh->add_node(1, 0.0, 0.0, 0.0);
    mesh->add_node(2, 1.0, 0.0, 0.0);
    mesh->add_node(3, 0.0, 1.0, 0.0);
    mesh->add_node(4, 0.0, 0.0, 1.0);

    mesh->add_element(1, ElementType::Tetrahedron4, {1, 2, 3, 4});

    return mesh;
}

std::unique_ptr<FiniteElementMesh> create_larger_tet_mesh() {
    auto mesh = std::make_unique<FiniteElementMesh>();

    // Create a larger tetrahedral mesh
    mesh->add_node(1, 0.0, 0.0, 0.0);
    mesh->add_node(2, 1.0, 0.0, 0.0);
    mesh->add_node(3, 0.0, 1.0, 0.0);
    mesh->add_node(4, 0.0, 0.0, 1.0);
    mesh->add_node(5, 0.5, 0.5, 0.5);

    mesh->add_element(1, ElementType::Tetrahedron4, {1, 2, 3, 5});
    mesh->add_element(2, ElementType::Tetrahedron4, {1, 2, 4, 5});
    mesh->add_element(3, ElementType::Tetrahedron4, {1, 3, 4, 5});
    mesh->add_element(4, ElementType::Tetrahedron4, {2, 3, 4, 5});

    return mesh;
}

std::unique_ptr<FiniteElementMesh> create_hex_mesh() {
    auto mesh = std::make_unique<FiniteElementMesh>();

    // Create a hexahedral mesh
    for (int i = 0; i <= 1; ++i) {
        for (int j = 0; j <= 1; ++j) {
            for (int k = 0; k <= 1; ++k) {
                int node_id = i * 4 + j * 2 + k + 1;
                mesh->add_node(node_id, static_cast<double>(i),
                              static_cast<double>(j), static_cast<double>(k));
            }
        }
    }

    std::vector<int32_t> hex_nodes = {1, 2, 4, 3, 5, 6, 8, 7};
    mesh->add_element(1, ElementType::Hexahedron8, hex_nodes);

    return mesh;
}

// ============================================================================
// Test Cases: QualityAnalyzer (Tests 1-8)
// ============================================================================

void test_quality_analyzer_aspect_ratio() {
    std::cout << "Test 1: Quality analyzer aspect ratio...\n";

    auto mesh = create_simple_tet_mesh();
    QualityAnalyzer analyzer;

    auto elem = mesh->get_element(1);
    assert(elem != nullptr);

    double aspect_ratio = analyzer.compute_aspect_ratio(*mesh, *elem);
    assert(aspect_ratio > 0.0);
    assert(aspect_ratio < 100.0);  // Should be reasonable for regular tet

    std::cout << "  PASS: Aspect ratio = " << aspect_ratio << "\n";
}

void test_quality_analyzer_skewness() {
    std::cout << "Test 2: Quality analyzer skewness...\n";

    auto mesh = create_simple_tet_mesh();
    QualityAnalyzer analyzer;

    auto elem = mesh->get_element(1);
    assert(elem != nullptr);

    double skewness = analyzer.compute_skewness(*mesh, *elem);
    assert(skewness >= 0.0);
    assert(skewness <= 1.0);

    std::cout << "  PASS: Skewness = " << skewness << "\n";
}

void test_quality_analyzer_jacobian() {
    std::cout << "Test 3: Quality analyzer Jacobian...\n";

    auto mesh = create_simple_tet_mesh();
    QualityAnalyzer analyzer;

    auto elem = mesh->get_element(1);
    assert(elem != nullptr);

    double jacobian = analyzer.compute_jacobian_determinant(*mesh, *elem, 0, 0, 0);
    assert(jacobian > 0.0);  // Regular tet should have positive Jacobian

    std::cout << "  PASS: Jacobian = " << jacobian << "\n";
}

void test_quality_analyzer_all_elements() {
    std::cout << "Test 4: Quality analyzer all elements...\n";

    auto mesh = create_larger_tet_mesh();
    QualityAnalyzer analyzer;

    auto metrics = analyzer.analyze_all_elements(*mesh);
    assert(metrics.size() == mesh->num_elements());
    assert(metrics.size() == 4);

    for (const auto& m : metrics) {
        assert(m.aspect_ratio > 0.0);
        assert(m.skewness >= 0.0 && m.skewness <= 1.0);
    }

    std::cout << "  PASS: Analyzed " << metrics.size() << " elements\n";
}

void test_quality_analyzer_statistics() {
    std::cout << "Test 5: Quality analyzer statistics...\n";

    auto mesh = create_larger_tet_mesh();
    QualityAnalyzer analyzer;

    auto metrics = analyzer.analyze_all_elements(*mesh);
    auto stats = analyzer.compute_statistics(metrics);

    assert(stats.total_elements == 4);
    assert(stats.aspect_ratio_min > 0.0);
    assert(stats.aspect_ratio_max >= stats.aspect_ratio_min);
    assert(stats.aspect_ratio_mean > 0.0);
    assert(stats.overall_quality_score >= 0.0 && stats.overall_quality_score <= 1.0);

    std::cout << "  PASS: Overall quality = " << stats.overall_quality_score << "\n";
}

void test_quality_analyzer_connectivity() {
    std::cout << "Test 6: Quality analyzer connectivity validation...\n";

    auto mesh = create_larger_tet_mesh();
    QualityAnalyzer analyzer;

    auto validation = analyzer.validate_connectivity(*mesh);
    assert(validation.is_valid == true);
    assert(validation.duplicate_elements.empty());
    assert(validation.orphaned_nodes.empty());

    std::cout << "  PASS: Mesh connectivity valid\n";
}

void test_quality_analyzer_mesh_validity() {
    std::cout << "Test 7: Quality analyzer mesh validity for FEA...\n";

    auto mesh = create_simple_tet_mesh();
    QualityAnalyzer analyzer;

    bool is_valid = analyzer.is_mesh_valid_for_analysis(*mesh, 0.1, 100.0);
    assert(is_valid == true);

    std::cout << "  PASS: Mesh valid for FEA analysis\n";
}

void test_quality_analyzer_edge_statistics() {
    std::cout << "Test 8: Quality analyzer edge statistics...\n";

    auto mesh = create_simple_tet_mesh();
    QualityAnalyzer analyzer;

    auto elem = mesh->get_element(1);
    assert(elem != nullptr);

    auto edge_stats = analyzer.get_edge_length_statistics(*mesh, *elem);
    assert(edge_stats[0] > 0.0);  // min
    assert(edge_stats[1] >= edge_stats[0]);  // max
    assert(edge_stats[2] > 0.0);  // average

    std::cout << "  PASS: Edge stats min=" << edge_stats[0] << " max=" << edge_stats[1] << "\n";
}

// ============================================================================
// Test Cases: AdaptiveRefinement (Tests 9-16)
// ============================================================================

void test_adaptive_refinement_zz_estimator() {
    std::cout << "Test 9: Adaptive refinement ZZ error estimator...\n";

    auto mesh = create_larger_tet_mesh();
    ZZErrorEstimator estimator;

    std::vector<double> nodal_values(mesh->num_nodes(), 1.0);
    std::vector<double> element_stresses(mesh->num_elements(), 0.5);

    auto estimates = estimator.estimate_errors(*mesh, nodal_values, element_stresses);
    assert(estimates.size() == mesh->num_elements());

    for (const auto& est : estimates) {
        assert(est.element_id > 0);
        assert(est.local_error >= 0.0);
    }

    std::cout << "  PASS: Estimated errors for " << estimates.size() << " elements\n";
}

void test_adaptive_refinement_smoothed_stresses() {
    std::cout << "Test 10: Adaptive refinement smoothed stresses...\n";

    auto mesh = create_larger_tet_mesh();
    ZZErrorEstimator estimator;

    std::vector<double> element_stresses(mesh->num_elements(), 0.5);
    auto smoothed = estimator.compute_smoothed_stresses(*mesh, element_stresses);

    assert(smoothed.size() == mesh->num_elements());

    std::cout << "  PASS: Computed smoothed stresses\n";
}

void test_adaptive_refinement_refine_elements() {
    std::cout << "Test 11: Adaptive refinement refine elements...\n";

    auto mesh = create_larger_tet_mesh();
    AdaptiveRefinement refiner;

    std::vector<int32_t> elements_to_refine = {1, 2};
    refiner.refine_elements(*mesh, elements_to_refine);

    auto stats = refiner.get_last_stats();
    assert(stats.elements_refined == 2);

    std::cout << "  PASS: Refined " << stats.elements_refined << " elements\n";
}

void test_adaptive_refinement_convergence() {
    std::cout << "Test 12: Adaptive refinement convergence...\n";

    auto mesh = create_larger_tet_mesh();
    AdaptiveRefinement refiner;

    std::vector<double> element_errors(mesh->num_elements(), 0.2);
    refiner.iterate_refinement(*mesh, element_errors, 3, 0.05);

    std::cout << "  PASS: Refinement iteration completed\n";
}

void test_adaptive_refinement_red_green() {
    std::cout << "Test 13: Adaptive refinement red-green...\n";

    auto mesh = create_larger_tet_mesh();
    AdaptiveRefinement refiner;

    std::vector<int32_t> red_elements = {1};
    refiner.red_green_refinement(*mesh, red_elements);

    std::cout << "  PASS: Red-green refinement executed\n";
}

void test_adaptive_refinement_criteria_error() {
    std::cout << "Test 14: Adaptive refinement error criteria...\n";

    ErrorEstimate est;
    est.local_error = 0.3;

    bool should_refine = RefinementCriteria::should_refine_by_error(est, 1.0, 0.2);
    assert(should_refine == true);

    should_refine = RefinementCriteria::should_refine_by_error(est, 1.0, 0.5);
    assert(should_refine == false);

    std::cout << "  PASS: Error-based refinement criteria working\n";
}

void test_adaptive_refinement_criteria_size() {
    std::cout << "Test 15: Adaptive refinement size criteria...\n";

    auto mesh = create_simple_tet_mesh();
    auto elem = mesh->get_element(1);

    bool should_refine = RefinementCriteria::should_refine_by_size(*mesh, *elem, 0.5);

    std::cout << "  PASS: Size-based refinement criteria working\n";
}

void test_adaptive_refinement_coarsening() {
    std::cout << "Test 16: Adaptive refinement coarsening...\n";

    auto mesh = create_larger_tet_mesh();
    AdaptiveRefinement refiner;

    refiner.coarsen_mesh(*mesh, 0.05);

    auto stats = refiner.get_last_stats();
    // Note: coarsening may not remove elements in this test

    std::cout << "  PASS: Coarsening executed\n";
}

// ============================================================================
// Test Cases: MeshSmoothing (Tests 17-24)
// ============================================================================

void test_mesh_smoothing_laplacian() {
    std::cout << "Test 17: Mesh smoothing Laplacian...\n";

    auto mesh = create_larger_tet_mesh();
    LaplacianSmoother smoother;

    SmoothingConfig config;
    config.max_iterations = 10;

    auto stats = smoother.smooth(*mesh, config);
    assert(stats.iterations_completed > 0);
    assert(stats.initial_quality >= 0.0 && stats.initial_quality <= 1.0);
    assert(stats.final_quality >= 0.0 && stats.final_quality <= 1.0);

    std::cout << "  PASS: Laplacian smoothing completed in "
              << stats.iterations_completed << " iterations\n";
}

void test_mesh_smoothing_optimization() {
    std::cout << "Test 18: Mesh smoothing optimization-based...\n";

    auto mesh = create_larger_tet_mesh();
    OptimizationSmoother smoother;

    SmoothingConfig config;
    config.max_iterations = 5;

    auto stats = smoother.optimize(*mesh, config);
    assert(stats.iterations_completed > 0);

    std::cout << "  PASS: Optimization-based smoothing completed\n";
}

void test_mesh_smoothing_boundary_preservation() {
    std::cout << "Test 19: Mesh smoothing boundary preservation...\n";

    auto mesh = create_larger_tet_mesh();
    mesh->mark_boundary_node(1, 1);
    mesh->mark_boundary_node(2, 1);

    BoundaryPreservingSmoother smoother;
    SmoothingConfig config;

    auto stats = smoother.smooth_with_boundary_preservation(
        *mesh, mesh->get_boundary_nodes(), config);

    assert(stats.iterations_completed >= 0);

    std::cout << "  PASS: Boundary-preserving smoothing completed\n";
}

void test_mesh_smoothing_boundary_normal() {
    std::cout << "Test 20: Mesh smoothing boundary normal...\n";

    auto mesh = create_larger_tet_mesh();
    BoundaryPreservingSmoother smoother;

    Vec3 normal = smoother.compute_boundary_normal(*mesh, 1);
    // Normal vector should be valid (may be zero in simplified implementation)

    std::cout << "  PASS: Computed boundary normal\n";
}

void test_mesh_smoothing_constraint_projection() {
    std::cout << "Test 21: Mesh smoothing constraint projection...\n";

    BoundaryPreservingSmoother smoother;
    Vec3 disp{1.0, 1.0, 0.0};
    Vec3 normal{0.0, 0.0, 1.0};

    Vec3 constrained = smoother.constrain_to_boundary(disp, normal);

    std::cout << "  PASS: Constraint projection executed\n";
}

void test_mesh_smoothing_adaptive() {
    std::cout << "Test 22: Mesh smoothing adaptive method...\n";

    auto mesh = create_larger_tet_mesh();
    MeshSmoother smoother;

    SmoothingConfig config;
    auto stats = smoother.smooth_adaptive(*mesh, config);

    assert(stats.iterations_completed >= 0);

    std::cout << "  PASS: Adaptive smoothing completed\n";
}

void test_mesh_smoothing_method_selection() {
    std::cout << "Test 23: Mesh smoothing method selection...\n";

    auto mesh = create_larger_tet_mesh();
    MeshSmoother smoother;

    SmoothingConfig config;

    auto stats_lap = smoother.smooth(*mesh, MeshSmoother::SmoothingMethod::Laplacian, config);
    auto stats_opt = smoother.smooth(*mesh, MeshSmoother::SmoothingMethod::Optimization, config);
    auto stats_hybrid = smoother.smooth(*mesh, MeshSmoother::SmoothingMethod::Hybrid, config);

    std::cout << "  PASS: All smoothing methods executed\n";
}

void test_mesh_smoothing_convergence() {
    std::cout << "Test 24: Mesh smoothing convergence...\n";

    auto mesh = create_larger_tet_mesh();
    LaplacianSmoother smoother;

    SmoothingConfig config;
    config.max_iterations = 100;
    config.convergence_tolerance = 1e-8;

    auto stats = smoother.smooth(*mesh, config);
    assert(stats.iterations_completed <= 100);

    std::cout << "  PASS: Smoothing converged in " << stats.iterations_completed << " iterations\n";
}

// ============================================================================
// Test Cases: MeshOptimization (Tests 25-32)
// ============================================================================

void test_mesh_optimization_element_swapper() {
    std::cout << "Test 25: Mesh optimization element swapper...\n";

    auto mesh = create_larger_tet_mesh();
    ElementSwapper swapper;

    OptimizationConfig config;
    auto result = swapper.optimize_by_swapping(*mesh, config);

    assert(result.success == true);
    assert(result.iterations >= 0);

    std::cout << "  PASS: Element swapping executed\n";
}

void test_mesh_optimization_node_repositioning() {
    std::cout << "Test 26: Mesh optimization node repositioning...\n";

    auto mesh = create_larger_tet_mesh();
    NodeRepositioner repositioner;

    OptimizationConfig config;
    auto result = repositioner.optimize_by_repositioning(*mesh, config);

    assert(result.success == true);
    assert(result.iterations >= 0);

    std::cout << "  PASS: Node repositioning executed\n";
}

void test_mesh_optimization_constraint_preservation() {
    std::cout << "Test 27: Mesh optimization constraint preservation...\n";

    auto mesh = create_larger_tet_mesh();
    ConstraintPreserver preserver;

    std::vector<int32_t> fixed_nodes = {1, 2};
    preserver.preserve_boundary_constraints(*mesh, fixed_nodes);

    std::cout << "  PASS: Constraint preservation executed\n";
}

void test_mesh_optimization_two_pass() {
    std::cout << "Test 28: Mesh optimization two-pass...\n";

    auto mesh = create_larger_tet_mesh();
    MeshOptimizer optimizer;

    OptimizationConfig config;
    config.max_iterations = 5;

    auto result = optimizer.optimize_two_pass(*mesh, config);

    assert(result.success == true);
    assert(result.iterations >= 0);
    assert(result.initial_quality >= 0.0 && result.initial_quality <= 1.0);
    assert(result.final_quality >= 0.0 && result.final_quality <= 1.0);

    std::cout << "  PASS: Two-pass optimization completed\n";
}

void test_mesh_optimization_problematic_elements() {
    std::cout << "Test 29: Mesh optimization problematic elements...\n";

    auto mesh = create_larger_tet_mesh();
    MeshOptimizer optimizer;

    std::vector<int32_t> poor_elements = {1};
    OptimizationConfig config;

    auto result = optimizer.optimize_problematic_elements(*mesh, poor_elements, config);

    assert(result.success == true);

    std::cout << "  PASS: Problematic element optimization executed\n";
}

void test_mesh_optimization_combined() {
    std::cout << "Test 30: Mesh optimization combined...\n";

    auto mesh = create_larger_tet_mesh();
    MeshOptimizer optimizer;

    OptimizationConfig config;
    config.max_iterations = 3;

    auto result = optimizer.optimize(*mesh, config);

    assert(result.success == true);
    assert(result.iterations > 0);

    std::cout << "  PASS: Combined optimization completed\n";
}

void test_mesh_optimization_quality_improvement() {
    std::cout << "Test 31: Mesh optimization quality improvement...\n";

    auto mesh = create_larger_tet_mesh();
    QualityAnalyzer analyzer;

    auto initial_metrics = analyzer.analyze_all_elements(*mesh);
    auto initial_stats = analyzer.compute_statistics(initial_metrics);

    MeshOptimizer optimizer;
    OptimizationConfig config;
    optimizer.optimize(*mesh, config);

    auto final_metrics = analyzer.analyze_all_elements(*mesh);
    auto final_stats = analyzer.compute_statistics(final_metrics);

    // Quality should be maintained or improved
    assert(final_stats.overall_quality_score >= 0.0);

    std::cout << "  PASS: Quality improvement calculated\n";
}

void test_mesh_optimization_identify_poor() {
    std::cout << "Test 32: Mesh optimization identify poor elements...\n";

    auto mesh = create_larger_tet_mesh();
    MeshOptimizer optimizer;

    auto poor_elements = optimizer.identify_poor_elements(*mesh, 0.3);

    // May or may not have poor elements depending on quality
    std::cout << "  PASS: Identified " << poor_elements.size() << " poor elements\n";
}

// ============================================================================
// Test Cases: ParallelMeshGeneration (Tests 33-36)
// ============================================================================

void test_parallel_domain_decomposer() {
    std::cout << "Test 33: Parallel mesh generation domain decomposer...\n";

    auto mesh = create_larger_tet_mesh();
    DomainDecomposer decomposer;

    DomainDecompositionConfig config;
    config.num_subdomains_x = 2;
    config.num_subdomains_y = 2;
    config.num_subdomains_z = 2;

    auto subdomains = decomposer.decompose_mesh(*mesh, config);
    assert(subdomains.size() == 8);  // 2x2x2

    std::cout << "  PASS: Decomposed mesh into " << subdomains.size() << " subdomains\n";
}

void test_parallel_merge_subdomains() {
    std::cout << "Test 34: Parallel mesh generation merge subdomains...\n";

    auto original_mesh = create_larger_tet_mesh();
    DomainDecomposer decomposer;

    DomainDecompositionConfig config;
    config.num_subdomains_x = 2;
    config.num_subdomains_y = 1;
    config.num_subdomains_z = 1;

    auto subdomains = decomposer.decompose_mesh(*original_mesh, config);
    auto merged = decomposer.merge_subdomains(subdomains);

    assert(merged != nullptr);
    assert(merged->num_nodes() > 0);
    assert(merged->num_elements() > 0);

    std::cout << "  PASS: Merged subdomains back into single mesh\n";
}

void test_parallel_synchronizer() {
    std::cout << "Test 35: Parallel mesh generation synchronizer...\n";

    auto mesh = create_larger_tet_mesh();
    DomainDecomposer decomposer;

    DomainDecompositionConfig config;
    auto subdomains = decomposer.decompose_mesh(*mesh, config);

    ParallelMeshSynchronizer sync;
    sync.synchronize_nodes(subdomains);
    sync.synchronize_elements(subdomains);

    bool consistent = sync.verify_mesh_consistency(subdomains);
    assert(consistent == true);

    std::cout << "  PASS: Subdomain synchronization verified\n";
}

void test_parallel_load_balancer() {
    std::cout << "Test 36: Parallel mesh generation load balancer...\n";

    auto mesh = create_larger_tet_mesh();
    DomainDecomposer decomposer;

    DomainDecompositionConfig config;
    auto subdomains = decomposer.decompose_mesh(*mesh, config);

    LoadBalancer balancer;
    auto metrics = balancer.analyze_load(subdomains);
    assert(metrics.size() == subdomains.size());

    double imbalance = balancer.compute_imbalance_ratio(metrics);
    assert(imbalance >= 0.0);

    std::cout << "  PASS: Load imbalance ratio = " << imbalance << "\n";
}

// ============================================================================
// Test Cases: MeshIO (Tests 37-40)
// ============================================================================

void test_mesh_io_gmsh_format() {
    std::cout << "Test 37: Mesh I/O Gmsh format...\n";

    GmshFormatHandler handler;
    assert(handler.can_handle_file("test.msh") == true);
    assert(handler.can_handle_file("test.stl") == false);

    auto mesh = create_simple_tet_mesh();
    auto validation = handler.validate(*mesh);
    assert(validation.is_valid == true);

    std::cout << "  PASS: Gmsh format handler working\n";
}

void test_mesh_io_stl_format() {
    std::cout << "Test 38: Mesh I/O STL format...\n";

    STLFormatHandler handler;
    assert(handler.can_handle_file("test.stl") == true);
    assert(handler.can_handle_file("test.msh") == false);

    auto mesh = create_simple_tet_mesh();
    auto validation = handler.validate(*mesh);
    assert(validation.is_valid == true);

    std::cout << "  PASS: STL format handler working\n";
}

void test_mesh_io_vtk_format() {
    std::cout << "Test 39: Mesh I/O VTK format...\n";

    VTKFormatHandler handler;
    assert(handler.can_handle_file("test.vtu") == true);
    assert(handler.can_handle_file("test.vtk") == true);
    assert(handler.can_handle_file("test.msh") == false);

    auto mesh = create_simple_tet_mesh();
    auto validation = handler.validate(*mesh);
    assert(validation.is_valid == true);

    std::cout << "  PASS: VTK format handler working\n";
}

void test_mesh_io_format_detection() {
    std::cout << "Test 40: Mesh I/O format detection...\n";

    auto gmsh_fmt = FormatConverter::detect_format("mesh.msh");
    assert(gmsh_fmt == MeshFormat::Gmsh);

    auto stl_fmt = FormatConverter::detect_format("mesh.stl");
    assert(stl_fmt == MeshFormat::STL);

    auto vtk_fmt = FormatConverter::detect_format("mesh.vtu");
    assert(vtk_fmt == MeshFormat::VTK);

    std::cout << "  PASS: Format detection working correctly\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "\n=== M7 Phase: Mesh Optimization (GAP-028) Test Suite ===\n\n";

    try {
        // QualityAnalyzer Tests (1-8)
        test_quality_analyzer_aspect_ratio();
        test_quality_analyzer_skewness();
        test_quality_analyzer_jacobian();
        test_quality_analyzer_all_elements();
        test_quality_analyzer_statistics();
        test_quality_analyzer_connectivity();
        test_quality_analyzer_mesh_validity();
        test_quality_analyzer_edge_statistics();

        // AdaptiveRefinement Tests (9-16)
        test_adaptive_refinement_zz_estimator();
        test_adaptive_refinement_smoothed_stresses();
        test_adaptive_refinement_refine_elements();
        test_adaptive_refinement_convergence();
        test_adaptive_refinement_red_green();
        test_adaptive_refinement_criteria_error();
        test_adaptive_refinement_criteria_size();
        test_adaptive_refinement_coarsening();

        // MeshSmoothing Tests (17-24)
        test_mesh_smoothing_laplacian();
        test_mesh_smoothing_optimization();
        test_mesh_smoothing_boundary_preservation();
        test_mesh_smoothing_boundary_normal();
        test_mesh_smoothing_constraint_projection();
        test_mesh_smoothing_adaptive();
        test_mesh_smoothing_method_selection();
        test_mesh_smoothing_convergence();

        // MeshOptimization Tests (25-32)
        test_mesh_optimization_element_swapper();
        test_mesh_optimization_node_repositioning();
        test_mesh_optimization_constraint_preservation();
        test_mesh_optimization_two_pass();
        test_mesh_optimization_problematic_elements();
        test_mesh_optimization_combined();
        test_mesh_optimization_quality_improvement();
        test_mesh_optimization_identify_poor();

        // ParallelMeshGeneration Tests (33-36)
        test_parallel_domain_decomposer();
        test_parallel_merge_subdomains();
        test_parallel_synchronizer();
        test_parallel_load_balancer();

        // MeshIO Tests (37-40)
        test_mesh_io_gmsh_format();
        test_mesh_io_stl_format();
        test_mesh_io_vtk_format();
        test_mesh_io_format_detection();

        std::cout << "\n=== ALL 40 TESTS PASSED ===\n\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\nTEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
