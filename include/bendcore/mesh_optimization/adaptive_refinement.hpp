// Adaptive Mesh Refinement System
// M7 Phase: Mesh Optimization (GAP-028)
#pragma once

#include "bendcore/fea/mesh_generator.hpp"
#include <vector>
#include <cstdint>
#include <memory>

namespace bendcore::mesh_optimization {

// Error estimation result for refinement decision
struct ErrorEstimate {
    int32_t element_id;
    double local_error;           // Local error estimation
    double relative_error;        // Relative to global error
    double error_indicator;       // Error indicator for refinement
    bool should_refine;           // Refinement decision
    int32_t refinement_level;     // Current refinement level
};

// Zienkiewicz-Zhu error estimator
class ZZErrorEstimator {
public:
    ZZErrorEstimator() = default;
    ~ZZErrorEstimator() = default;

    // Compute error estimates using ZZ method
    std::vector<ErrorEstimate> estimate_errors(
        const fea::FiniteElementMesh& mesh,
        const std::vector<double>& nodal_values,
        const std::vector<double>& element_stresses);

    // Compute smoothed stress field at nodes
    std::vector<std::vector<double>> compute_smoothed_stresses(
        const fea::FiniteElementMesh& mesh,
        const std::vector<double>& element_stresses);

    // Compute energy norm error
    double compute_energy_norm_error(
        const fea::FiniteElementMesh& mesh,
        const std::vector<double>& raw_stresses,
        const std::vector<double>& smoothed_stresses);

private:
    // Smooth nodal values using least-squares fitting
    std::vector<double> smooth_nodal_values(
        const fea::FiniteElementMesh& mesh,
        const std::vector<double>& element_values);
};

// Adaptive refinement controller
class AdaptiveRefinement {
public:
    AdaptiveRefinement();
    ~AdaptiveRefinement() = default;

    // Refine mesh based on error estimates
    void refine_based_on_errors(
        fea::FiniteElementMesh& mesh,
        const std::vector<ErrorEstimate>& estimates,
        double refinement_threshold = 0.3);

    // Uniform refinement of marked elements
    void refine_elements(
        fea::FiniteElementMesh& mesh,
        const std::vector<int32_t>& element_ids);

    // Red-green refinement (preserves element connectivity)
    void red_green_refinement(
        fea::FiniteElementMesh& mesh,
        const std::vector<int32_t>& red_elements);

    // Coarsen mesh by unrefining elements
    void coarsen_mesh(
        fea::FiniteElementMesh& mesh,
        double coarsening_threshold = 0.05);

    // Iterative refinement loop
    void iterate_refinement(
        fea::FiniteElementMesh& mesh,
        const std::vector<double>& element_errors,
        int32_t max_iterations = 5,
        double target_error = 0.1);

    // Get refinement statistics
    struct RefinementStats {
        int32_t elements_refined;
        int32_t elements_coarsened;
        int32_t new_nodes_added;
        double avg_element_size_change;
    };

    RefinementStats get_last_stats() const { return last_stats_; }

private:
    int32_t next_node_id_;
    int32_t next_element_id_;
    RefinementStats last_stats_;

    // Refine a single tetrahedral element
    std::vector<fea::Element> refine_tetrahedron(
        const fea::Element& element,
        const fea::FiniteElementMesh& mesh,
        std::vector<fea::Node>& new_nodes);

    // Refine a single hexahedral element
    std::vector<fea::Element> refine_hexahedron(
        const fea::Element& element,
        const fea::FiniteElementMesh& mesh,
        std::vector<fea::Node>& new_nodes);

    // Find edge midpoint or create new node
    int32_t get_or_create_edge_node(
        const fea::FiniteElementMesh& mesh,
        std::vector<fea::Node>& new_nodes,
        int32_t node1_id, int32_t node2_id);

    // Collect edges sharing a node
    std::vector<std::pair<int32_t, int32_t>> get_edges_at_node(
        const fea::FiniteElementMesh& mesh,
        int32_t node_id);
};

// Refinement criteria selector
class RefinementCriteria {
public:
    // Error-based refinement threshold
    static bool should_refine_by_error(
        const ErrorEstimate& estimate,
        double global_error,
        double threshold_factor = 0.3);

    // Size-based refinement criterion
    static bool should_refine_by_size(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element,
        double max_element_size);

    // Gradient-based criterion
    static bool should_refine_by_gradient(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element,
        const std::vector<double>& nodal_values,
        double gradient_threshold);

    // Aspect ratio criterion
    static bool should_refine_by_aspect_ratio(
        const fea::FiniteElementMesh& mesh,
        const fea::Element& element,
        double max_aspect_ratio = 100.0);
};

}  // namespace bendcore::mesh_optimization
