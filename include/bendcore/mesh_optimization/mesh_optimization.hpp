// Mesh Optimization Algorithms
// M7 Phase: Mesh Optimization (GAP-028)
#pragma once

#include "bendcore/fea/mesh_generator.hpp"
#include "bendcore/vec3.hpp"
#include <vector>
#include <cstdint>
#include <array>

namespace bendcore::mesh_optimization {

// Optimization configuration
struct OptimizationConfig {
    int32_t max_iterations = 1000;
    double convergence_tolerance = 1e-8;
    double quality_improvement_threshold = 0.01;
    bool preserve_volume = true;
    bool preserve_topology = true;
};

// Optimization result statistics
struct OptimizationResult {
    bool success;
    int32_t iterations;
    double initial_quality;
    double final_quality;
    double quality_improvement;
    int32_t elements_improved;
    int32_t elements_degraded;
    std::vector<int32_t> problematic_elements;
};

// Element swapping and flipping optimizer
class ElementSwapper {
public:
    ElementSwapper() = default;
    ~ElementSwapper() = default;

    // Perform edge/face swapping to improve quality
    OptimizationResult optimize_by_swapping(
        fea::FiniteElementMesh& mesh,
        const OptimizationConfig& config = OptimizationConfig());

    // Try swapping edge to improve elements
    bool try_edge_swap(
        fea::FiniteElementMesh& mesh,
        int32_t node1_id, int32_t node2_id);

    // Try flipping face orientation if needed
    bool try_face_flip(
        fea::FiniteElementMesh& mesh,
        int32_t element_id);

    // Check if swap improves element quality
    bool is_swap_beneficial(
        const fea::FiniteElementMesh& mesh,
        const std::vector<fea::Element>& original_elements,
        const std::vector<fea::Element>& swapped_elements);

private:
    // Find elements sharing an edge
    std::vector<int32_t> find_edge_neighbors(
        const fea::FiniteElementMesh& mesh,
        int32_t node1_id, int32_t node2_id);

    // Find elements sharing a face
    std::vector<int32_t> find_face_neighbors(
        const fea::FiniteElementMesh& mesh,
        int32_t element_id);
};

// Node repositioning optimizer
class NodeRepositioner {
public:
    NodeRepositioner() = default;
    ~NodeRepositioner() = default;

    // Reposition nodes for quality improvement
    OptimizationResult optimize_by_repositioning(
        fea::FiniteElementMesh& mesh,
        const OptimizationConfig& config = OptimizationConfig());

    // Reposition single node
    bool reposition_node(
        fea::FiniteElementMesh& mesh,
        int32_t node_id);

    // Compute optimal position for node (unconstrained)
    Vec3 compute_optimal_unconstrained_position(
        const fea::FiniteElementMesh& mesh,
        int32_t node_id);

    // Compute optimal position with constraints
    Vec3 compute_optimal_constrained_position(
        const fea::FiniteElementMesh& mesh,
        int32_t node_id,
        const std::vector<int32_t>& boundary_nodes);

private:
    // Compute quality metric for node
    double compute_node_metric(
        const fea::FiniteElementMesh& mesh,
        int32_t node_id);

    // Gradient descent step for node optimization
    Vec3 compute_gradient_step(
        const fea::FiniteElementMesh& mesh,
        int32_t node_id);
};

// Constraint preservation system
class ConstraintPreserver {
public:
    ConstraintPreserver() = default;
    ~ConstraintPreserver() = default;

    // Preserve geometric constraints during optimization
    void preserve_volume_constraints(
        fea::FiniteElementMesh& mesh,
        const std::vector<double>& reference_element_volumes);

    // Preserve boundary constraints
    void preserve_boundary_constraints(
        fea::FiniteElementMesh& mesh,
        const std::vector<int32_t>& fixed_nodes);

    // Preserve material interface constraints
    void preserve_material_interfaces(
        fea::FiniteElementMesh& mesh,
        const std::vector<std::pair<int32_t, int32_t>>& material_boundaries);

    // Project position to constraint surface
    Vec3 project_to_constraints(
        const Vec3& position,
        const Vec3& original_position,
        const std::vector<int32_t>& constraint_flags);

private:
    // Check if constraint is satisfied
    bool is_constraint_satisfied(
        const fea::FiniteElementMesh& mesh,
        const Vec3& position,
        int32_t constraint_type);
};

// Global mesh optimizer
class MeshOptimizer {
public:
    MeshOptimizer() = default;
    ~MeshOptimizer() = default;

    // Optimize mesh quality (combined approach)
    OptimizationResult optimize(
        fea::FiniteElementMesh& mesh,
        const OptimizationConfig& config = OptimizationConfig());

    // Targeted optimization for poor elements
    OptimizationResult optimize_problematic_elements(
        fea::FiniteElementMesh& mesh,
        const std::vector<int32_t>& poor_element_ids,
        const OptimizationConfig& config = OptimizationConfig());

    // Two-pass optimization (swapping then repositioning)
    OptimizationResult optimize_two_pass(
        fea::FiniteElementMesh& mesh,
        const OptimizationConfig& config = OptimizationConfig());

    // Get last optimization result
    OptimizationResult get_last_result() const { return last_result_; }

    // Identify problematic elements
    std::vector<int32_t> identify_poor_elements(
        const fea::FiniteElementMesh& mesh,
        double quality_threshold = 0.3);

private:
    ElementSwapper swapper_;
    NodeRepositioner repositioner_;
    ConstraintPreserver constraint_preserver_;
    OptimizationResult last_result_;
};

}  // namespace bendcore::mesh_optimization
