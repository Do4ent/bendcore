// Adaptive Mesh Refinement Implementation
// M7 Phase: Mesh Optimization (GAP-028)
#include "bendcore/mesh_optimization/adaptive_refinement.hpp"
#include "bendcore/mesh_optimization/quality_analyzer.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

using namespace bendcore::mesh_optimization;
using namespace bendcore::fea;

// ============================================================================
// ZZErrorEstimator Implementation
// ============================================================================

std::vector<ErrorEstimate> ZZErrorEstimator::estimate_errors(
    const FiniteElementMesh& mesh,
    const std::vector<double>& nodal_values,
    const std::vector<double>& element_stresses) {

    std::vector<ErrorEstimate> estimates;

    auto smoothed = compute_smoothed_stresses(mesh, element_stresses);

    for (const auto& element : mesh.get_elements()) {
        ErrorEstimate est;
        est.element_id = element.id;

        // Compute local error indicator
        double error_sq = 0.0;
        for (size_t i = 0; i < element.node_ids.size() && i < element_stresses.size(); ++i) {
            int32_t node_id = element.node_ids[i];
            auto node = mesh.get_node(node_id);
            if (node) {
                double raw = element_stresses[i];
                double smoothed_val = 0.0;
                if (i < smoothed[element.id].size()) {
                    smoothed_val = smoothed[element.id][i];
                }
                double diff = raw - smoothed_val;
                error_sq += diff * diff;
            }
        }

        est.local_error = std::sqrt(error_sq);
        est.error_indicator = est.local_error;
        est.should_refine = est.local_error > 0.1;  // Default threshold
        est.refinement_level = 0;

        estimates.push_back(est);
    }

    return estimates;
}

std::vector<std::vector<double>> ZZErrorEstimator::compute_smoothed_stresses(
    const FiniteElementMesh& mesh,
    const std::vector<double>& element_stresses) {

    std::vector<std::vector<double>> smoothed(mesh.num_elements());

    // Smooth element stresses to nodes
    std::vector<std::vector<double>> nodal_stresses(mesh.num_nodes());
    std::vector<int32_t> node_counts(mesh.num_nodes(), 0);

    for (size_t i = 0; i < mesh.get_elements().size(); ++i) {
        const auto& elem = mesh.get_elements()[i];
        double stress = i < element_stresses.size() ? element_stresses[i] : 0.0;

        for (int32_t node_id : elem.node_ids) {
            auto node = mesh.get_node(node_id);
            if (node) {
                // Find index of this node
                int idx = 0;
                for (const auto& n : mesh.get_nodes()) {
                    if (n.id == node_id) break;
                    idx++;
                }

                if (idx < mesh.num_nodes()) {
                    nodal_stresses[idx].push_back(stress);
                    node_counts[idx]++;
                }
            }
        }
    }

    // Average stresses at nodes
    std::vector<double> averaged_stresses(mesh.num_nodes());
    for (int32_t i = 0; i < mesh.num_nodes(); ++i) {
        if (node_counts[i] > 0) {
            double sum = 0.0;
            for (double s : nodal_stresses[i]) {
                sum += s;
            }
            averaged_stresses[i] = sum / node_counts[i];
        }
    }

    // Interpolate back to elements
    for (size_t i = 0; i < mesh.get_elements().size(); ++i) {
        const auto& elem = mesh.get_elements()[i];
        std::vector<double> elem_smoothed;

        for (int32_t node_id : elem.node_ids) {
            auto node = mesh.get_node(node_id);
            if (node) {
                int idx = 0;
                for (const auto& n : mesh.get_nodes()) {
                    if (n.id == node_id) break;
                    idx++;
                }
                if (idx < mesh.num_nodes()) {
                    elem_smoothed.push_back(averaged_stresses[idx]);
                }
            }
        }

        smoothed[i] = elem_smoothed;
    }

    return smoothed;
}

double ZZErrorEstimator::compute_energy_norm_error(
    const FiniteElementMesh& mesh,
    const std::vector<double>& raw_stresses,
    const std::vector<double>& smoothed_stresses) {

    double error = 0.0;
    size_t min_size = std::min(raw_stresses.size(), smoothed_stresses.size());

    for (size_t i = 0; i < min_size; ++i) {
        double diff = raw_stresses[i] - smoothed_stresses[i];
        error += diff * diff;
    }

    return std::sqrt(error);
}

// ============================================================================
// AdaptiveRefinement Implementation
// ============================================================================

AdaptiveRefinement::AdaptiveRefinement()
    : next_node_id_(10000), next_element_id_(10000) {
    last_stats_ = {0, 0, 0, 0.0};
}

void AdaptiveRefinement::refine_based_on_errors(
    FiniteElementMesh& mesh,
    const std::vector<ErrorEstimate>& estimates,
    double refinement_threshold) {

    std::vector<int32_t> elements_to_refine;

    for (const auto& est : estimates) {
        if (est.should_refine || est.local_error > refinement_threshold) {
            elements_to_refine.push_back(est.element_id);
        }
    }

    refine_elements(mesh, elements_to_refine);
}

void AdaptiveRefinement::refine_elements(
    FiniteElementMesh& mesh,
    const std::vector<int32_t>& element_ids) {

    std::vector<Node> new_nodes;
    std::vector<Element> new_elements;

    for (int32_t elem_id : element_ids) {
        auto elem = mesh.get_element(elem_id);
        if (!elem) continue;

        std::vector<Element> refined;
        if (elem->type == ElementType::Tetrahedron4 ||
            elem->type == ElementType::Tetrahedron10) {
            refined = refine_tetrahedron(*elem, mesh, new_nodes);
        } else if (elem->type == ElementType::Hexahedron8 ||
                   elem->type == ElementType::Hexahedron20) {
            refined = refine_hexahedron(*elem, mesh, new_nodes);
        }

        for (auto& e : refined) {
            new_elements.push_back(e);
        }
    }

    last_stats_.elements_refined = static_cast<int32_t>(element_ids.size());
    last_stats_.new_nodes_added = static_cast<int32_t>(new_nodes.size());
}

void AdaptiveRefinement::red_green_refinement(
    FiniteElementMesh& mesh,
    const std::vector<int32_t>& red_elements) {

    // Red refinement: uniform division
    refine_elements(mesh, red_elements);
}

void AdaptiveRefinement::coarsen_mesh(
    FiniteElementMesh& mesh,
    double coarsening_threshold) {

    // Simplified coarsening: remove elements below size threshold
    // In practice, would use more sophisticated unrefinement

    QualityAnalyzer analyzer;
    auto metrics = analyzer.analyze_all_elements(mesh);

    // Elements with very high aspect ratio (poor quality) candidates for coarsening
    for (const auto& m : metrics) {
        if (m.aspect_ratio > 100.0) {
            // Mark for coarsening (simplified)
            last_stats_.elements_coarsened++;
        }
    }
}

void AdaptiveRefinement::iterate_refinement(
    FiniteElementMesh& mesh,
    const std::vector<double>& element_errors,
    int32_t max_iterations,
    double target_error) {

    for (int32_t iter = 0; iter < max_iterations; ++iter) {
        // Compute global error
        double global_error = 0.0;
        for (double e : element_errors) {
            global_error += e * e;
        }
        global_error = std::sqrt(global_error);

        if (global_error < target_error) {
            break;
        }

        // Estimate refinement
        ZZErrorEstimator estimator;
        std::vector<double> dummy_nodal(mesh.num_nodes(), 0.0);
        auto estimates = estimator.estimate_errors(mesh, dummy_nodal, element_errors);

        // Refine based on estimates
        refine_based_on_errors(mesh, estimates, target_error);
    }
}

std::vector<Element> AdaptiveRefinement::refine_tetrahedron(
    const Element& element,
    const FiniteElementMesh& mesh,
    std::vector<Node>& new_nodes) {

    std::vector<Element> refined_elems;

    // Get vertices
    std::vector<Node> verts;
    for (int32_t node_id : element.node_ids) {
        auto n = mesh.get_node(node_id);
        if (n) verts.push_back(*n);
    }

    if (verts.size() < 4) return refined_elems;

    // Create 4 sub-tetrahedra by connecting edge midpoints
    std::vector<int32_t> edge_node_ids(6);
    for (int i = 0; i < 6; ++i) {
        int n1_idx = (i < 3) ? i : (i - 3 + 1);
        int n2_idx = (i < 3) ? 3 : (i - 3 + 2);

        edge_node_ids[i] = get_or_create_edge_node(
            mesh, new_nodes,
            element.node_ids[n1_idx],
            element.node_ids[n2_idx]);
    }

    // Create 4 sub-elements
    for (int i = 0; i < 4; ++i) {
        Element sub_elem;
        sub_elem.id = next_element_id_++;
        sub_elem.type = element.type;
        sub_elem.material_id = element.material_id;
        sub_elem.node_ids.resize(4);

        // Connectivity for 4 sub-tetrahedra (simplified)
        if (i == 0) {
            sub_elem.node_ids = {
                element.node_ids[0], edge_node_ids[0],
                edge_node_ids[1], edge_node_ids[2]
            };
        } else if (i == 1) {
            sub_elem.node_ids = {
                edge_node_ids[0], element.node_ids[1],
                edge_node_ids[3], edge_node_ids[4]
            };
        } else if (i == 2) {
            sub_elem.node_ids = {
                edge_node_ids[1], edge_node_ids[3],
                element.node_ids[2], edge_node_ids[5]
            };
        } else {
            sub_elem.node_ids = {
                edge_node_ids[2], edge_node_ids[4],
                edge_node_ids[5], element.node_ids[3]
            };
        }

        refined_elems.push_back(sub_elem);
    }

    return refined_elems;
}

std::vector<Element> AdaptiveRefinement::refine_hexahedron(
    const Element& element,
    const FiniteElementMesh& mesh,
    std::vector<Node>& new_nodes) {

    std::vector<Element> refined_elems;

    // Hexahedron is divided into 8 sub-hexahedra
    // Similar process to tetrahedron but more complex connectivity

    // Simplified: create one element per 1/8 of original
    for (int i = 0; i < 8; ++i) {
        Element sub_elem;
        sub_elem.id = next_element_id_++;
        sub_elem.type = element.type;
        sub_elem.material_id = element.material_id;

        // Simplified node assignment (real implementation would compute proper midpoints)
        sub_elem.node_ids = element.node_ids;

        refined_elems.push_back(sub_elem);
    }

    return refined_elems;
}

int32_t AdaptiveRefinement::get_or_create_edge_node(
    const FiniteElementMesh& mesh,
    std::vector<Node>& new_nodes,
    int32_t node1_id, int32_t node2_id) {

    auto n1 = mesh.get_node(node1_id);
    auto n2 = mesh.get_node(node2_id);

    if (!n1 || !n2) return -1;

    // Create midpoint node
    Node midpoint;
    midpoint.id = next_node_id_++;
    // Use vec3 helper functions: add then multiply by scalar
    midpoint.position = bendcore::mul(bendcore::add(n1->position, n2->position), 0.5);
    midpoint.is_boundary = n1->is_boundary && n2->is_boundary;

    new_nodes.push_back(midpoint);
    return midpoint.id;
}

// ============================================================================
// RefinementCriteria Implementation
// ============================================================================

bool RefinementCriteria::should_refine_by_error(
    const ErrorEstimate& estimate,
    double global_error,
    double threshold_factor) {

    return estimate.local_error > (threshold_factor * global_error);
}

bool RefinementCriteria::should_refine_by_size(
    const FiniteElementMesh& mesh,
    const Element& element,
    double max_element_size) {

    QualityAnalyzer analyzer;
    auto stats = analyzer.get_edge_length_statistics(mesh, element);
    return stats[1] > max_element_size;  // max edge length
}

bool RefinementCriteria::should_refine_by_gradient(
    const FiniteElementMesh& mesh,
    const Element& element,
    const std::vector<double>& nodal_values,
    double gradient_threshold) {

    // Compute gradient of nodal values across element
    if (element.node_ids.size() < 2) return false;

    double max_grad = 0.0;
    for (size_t i = 0; i < element.node_ids.size(); ++i) {
        for (size_t j = i + 1; j < element.node_ids.size(); ++j) {
            auto ni = mesh.get_node(element.node_ids[i]);
            auto nj = mesh.get_node(element.node_ids[j]);

            if (ni && nj && i < nodal_values.size() && j < nodal_values.size()) {
                double dist = bendcore::norm(bendcore::sub(ni->position, nj->position));
                if (dist > 0) {
                    double grad = std::abs(nodal_values[i] - nodal_values[j]) / dist;
                    max_grad = std::max(max_grad, grad);
                }
            }
        }
    }

    return max_grad > gradient_threshold;
}

bool RefinementCriteria::should_refine_by_aspect_ratio(
    const FiniteElementMesh& mesh,
    const Element& element,
    double max_aspect_ratio) {

    QualityAnalyzer analyzer;
    double ar = analyzer.compute_aspect_ratio(mesh, element);
    return ar > max_aspect_ratio;
}

}  // namespace bendcore::mesh_optimization
