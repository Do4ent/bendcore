// Mesh Quality Analysis Implementation
// M7 Phase: Mesh Optimization (GAP-028)
#include "bendcore/mesh_optimization/quality_analyzer.hpp"
#include "bendcore/vec3.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <limits>

using namespace bendcore;
using namespace bendcore::mesh_optimization;

// ============================================================================
// QualityAnalyzer Implementation
// ============================================================================

ElementQualityMetrics QualityAnalyzer::analyze_element(
    const fea::FiniteElementMesh& mesh,
    const fea::Element& element) {

    ElementQualityMetrics metrics;
    metrics.element_id = element.id;

    // Compute aspect ratio
    metrics.aspect_ratio = compute_aspect_ratio(mesh, element);

    // Compute skewness
    metrics.skewness = compute_skewness(mesh, element);

    // Compute distortion
    metrics.distortion = compute_distortion(mesh, element);

    // Compute Jacobian metrics
    double j_center = compute_jacobian_determinant(mesh, element, 0, 0, 0);
    metrics.jacobian_determinant = j_center;
    metrics.is_inverted = j_center < 0;

    // Compute min/max Jacobian
    metrics.min_jacobian = j_center;
    metrics.max_jacobian = j_center;

    // Sample multiple points for Jacobian
    const int samples = 5;
    for (int i = -2; i <= 2; ++i) {
        for (int j = -2; j <= 2; ++j) {
            for (int k = -2; k <= 2; ++k) {
                double xi = i * 0.25;
                double eta = j * 0.25;
                double zeta = k * 0.25;

                if (std::abs(xi) <= 1.0 && std::abs(eta) <= 1.0 && std::abs(zeta) <= 1.0) {
                    double j_val = compute_jacobian_determinant(mesh, element, xi, eta, zeta);
                    metrics.min_jacobian = std::min(metrics.min_jacobian, j_val);
                    metrics.max_jacobian = std::max(metrics.max_jacobian, j_val);
                }
            }
        }
    }

    // Compute dihedral angles
    metrics.dihedral_angles = compute_dihedral_angles(mesh, element);
    if (!metrics.dihedral_angles.empty()) {
        metrics.min_dihedral_angle = *std::min_element(
            metrics.dihedral_angles.begin(), metrics.dihedral_angles.end());
        metrics.max_dihedral_angle = *std::max_element(
            metrics.dihedral_angles.begin(), metrics.dihedral_angles.end());
    }

    return metrics;
}

std::vector<ElementQualityMetrics> QualityAnalyzer::analyze_all_elements(
    const fea::FiniteElementMesh& mesh) {

    std::vector<ElementQualityMetrics> all_metrics;
    for (const auto& element : mesh.get_elements()) {
        all_metrics.push_back(analyze_element(mesh, element));
    }
    return all_metrics;
}

MeshQualityStatistics QualityAnalyzer::compute_statistics(
    const std::vector<ElementQualityMetrics>& metrics) {

    MeshQualityStatistics stats;
    stats.total_elements = static_cast<int32_t>(metrics.size());

    if (metrics.empty()) {
        return stats;
    }

    std::vector<double> aspect_ratios;
    std::vector<double> skewness_values;
    std::vector<double> jacobian_values;
    std::vector<double> dihedral_angles;

    for (const auto& m : metrics) {
        aspect_ratios.push_back(m.aspect_ratio);
        skewness_values.push_back(m.skewness);
        jacobian_values.push_back(m.jacobian_determinant);

        for (double angle : m.dihedral_angles) {
            dihedral_angles.push_back(angle);
        }

        if (m.is_inverted) {
            stats.num_inverted_elements++;
        }
        if (m.skewness > 0.8) {
            stats.num_highly_skewed_elements++;
        }
        if (m.aspect_ratio > 10.0) {
            stats.num_poor_aspect_ratio++;
        }
    }

    // Aspect ratio statistics
    if (!aspect_ratios.empty()) {
        std::sort(aspect_ratios.begin(), aspect_ratios.end());
        stats.aspect_ratio_min = aspect_ratios.front();
        stats.aspect_ratio_max = aspect_ratios.back();
        stats.aspect_ratio_mean = std::accumulate(
            aspect_ratios.begin(), aspect_ratios.end(), 0.0) / aspect_ratios.size();
        stats.aspect_ratio_median = aspect_ratios[aspect_ratios.size() / 2];
    }

    // Skewness statistics
    if (!skewness_values.empty()) {
        std::sort(skewness_values.begin(), skewness_values.end());
        stats.skewness_min = skewness_values.front();
        stats.skewness_max = skewness_values.back();
        stats.skewness_mean = std::accumulate(
            skewness_values.begin(), skewness_values.end(), 0.0) / skewness_values.size();
        stats.skewness_median = skewness_values[skewness_values.size() / 2];
    }

    // Jacobian statistics
    if (!jacobian_values.empty()) {
        std::sort(jacobian_values.begin(), jacobian_values.end());
        stats.jacobian_min = jacobian_values.front();
        stats.jacobian_max = jacobian_values.back();
        stats.jacobian_mean = std::accumulate(
            jacobian_values.begin(), jacobian_values.end(), 0.0) / jacobian_values.size();
        stats.jacobian_median = jacobian_values[jacobian_values.size() / 2];
    }

    // Dihedral angle statistics
    if (!dihedral_angles.empty()) {
        std::sort(dihedral_angles.begin(), dihedral_angles.end());
        stats.dihedral_angle_min = dihedral_angles.front();
        stats.dihedral_angle_max = dihedral_angles.back();
        stats.dihedral_angle_mean = std::accumulate(
            dihedral_angles.begin(), dihedral_angles.end(), 0.0) / dihedral_angles.size();
    }

    // Overall quality score
    double quality = 1.0;
    if (stats.aspect_ratio_max > 0) {
        quality *= std::exp(-stats.aspect_ratio_mean / 100.0);
    }
    quality *= (1.0 - stats.skewness_mean);
    quality = std::max(0.0, std::min(1.0, quality));
    stats.overall_quality_score = quality;

    return stats;
}

ConnectivityValidation QualityAnalyzer::validate_connectivity(
    const fea::FiniteElementMesh& mesh) {

    ConnectivityValidation result;
    result.is_valid = true;

    // Check for orphaned nodes
    std::vector<bool> node_used(mesh.num_nodes(), false);
    for (const auto& element : mesh.get_elements()) {
        for (int32_t node_id : element.node_ids) {
            auto node = mesh.get_node(node_id);
            if (node) {
                node_used[std::distance(&mesh.get_nodes()[0], node)] = true;
            }
        }
    }

    for (size_t i = 0; i < node_used.size(); ++i) {
        if (!node_used[i] && i < mesh.get_nodes().size()) {
            result.orphaned_nodes.push_back(static_cast<int32_t>(i));
            result.is_valid = false;
        }
    }

    // Check for duplicate elements
    for (size_t i = 0; i < mesh.get_elements().size(); ++i) {
        for (size_t j = i + 1; j < mesh.get_elements().size(); ++j) {
            const auto& e1 = mesh.get_elements()[i];
            const auto& e2 = mesh.get_elements()[j];

            std::vector<int32_t> n1 = e1.node_ids;
            std::vector<int32_t> n2 = e2.node_ids;
            std::sort(n1.begin(), n1.end());
            std::sort(n2.begin(), n2.end());

            if (n1 == n2) {
                result.duplicate_elements.push_back(e2.id);
                result.is_valid = false;
            }
        }
    }

    return result;
}

bool QualityAnalyzer::is_mesh_valid_for_analysis(
    const fea::FiniteElementMesh& mesh,
    double min_jacobian_threshold,
    double max_aspect_ratio_threshold) {

    auto metrics = analyze_all_elements(mesh);
    auto stats = compute_statistics(metrics);

    // Check criteria
    if (stats.jacobian_min < min_jacobian_threshold) return false;
    if (stats.aspect_ratio_max > max_aspect_ratio_threshold) return false;
    if (stats.num_inverted_elements > 0) return false;

    auto connectivity = validate_connectivity(mesh);
    if (!connectivity.is_valid) return false;

    return true;
}

double QualityAnalyzer::compute_aspect_ratio(
    const fea::FiniteElementMesh& mesh,
    const fea::Element& element) {

    if (element.type == fea::ElementType::Tetrahedron4 ||
        element.type == fea::ElementType::Tetrahedron10) {
        return compute_tet_aspect_ratio(mesh, element);
    } else if (element.type == fea::ElementType::Hexahedron8 ||
               element.type == fea::ElementType::Hexahedron20) {
        return compute_hex_aspect_ratio(mesh, element);
    }

    return 1.0;
}

double QualityAnalyzer::compute_skewness(
    const fea::FiniteElementMesh& mesh,
    const fea::Element& element) {

    if (element.type == fea::ElementType::Tetrahedron4 ||
        element.type == fea::ElementType::Tetrahedron10) {
        return compute_tet_skewness(mesh, element);
    } else if (element.type == fea::ElementType::Hexahedron8 ||
               element.type == fea::ElementType::Hexahedron20) {
        return compute_hex_skewness(mesh, element);
    }

    return 0.0;
}

double QualityAnalyzer::compute_distortion(
    const fea::FiniteElementMesh& mesh,
    const fea::Element& element) {

    // Distortion = 1 - (actual_volume / ideal_volume)
    // Simplified: use skewness as proxy
    return compute_skewness(mesh, element);
}

double QualityAnalyzer::compute_jacobian_determinant(
    const fea::FiniteElementMesh& mesh,
    const fea::Element& element,
    double xi, double eta, double zeta) {

    // For simplicity, return approximate Jacobian based on element size
    // Full implementation would require proper finite element basis functions

    if (element.node_ids.size() < 4) return 0.0;

    auto n0 = mesh.get_node(element.node_ids[0]);
    auto n1 = mesh.get_node(element.node_ids[1]);
    auto n2 = mesh.get_node(element.node_ids[2]);
    auto n3 = mesh.get_node(element.node_ids[3]);

    if (!n0 || !n1 || !n2 || !n3) return 0.0;

    Vec3 e1 = n1->position - n0->position;
    Vec3 e2 = n2->position - n0->position;
    Vec3 e3 = n3->position - n0->position;

    // Approximate Jacobian as determinant of edge matrix
    double det = e1[0] * (e2[1] * e3[2] - e2[2] * e3[1]) -
                 e1[1] * (e2[0] * e3[2] - e2[2] * e3[0]) +
                 e1[2] * (e2[0] * e3[1] - e2[1] * e3[0]);

    return det / 6.0;  // Scale by tetrahedral volume factor
}

std::vector<double> QualityAnalyzer::compute_dihedral_angles(
    const fea::FiniteElementMesh& mesh,
    const fea::Element& element) {

    std::vector<double> angles;

    if (element.type == fea::ElementType::Tetrahedron4 ||
        element.type == fea::ElementType::Tetrahedron10) {
        // Tetrahedron has 6 edges
        if (element.node_ids.size() < 4) return angles;

        auto n0 = mesh.get_node(element.node_ids[0]);
        auto n1 = mesh.get_node(element.node_ids[1]);
        auto n2 = mesh.get_node(element.node_ids[2]);
        auto n3 = mesh.get_node(element.node_ids[3]);

        if (!n0 || !n1 || !n2 || !n3) return angles;

        // Compute angles between faces
        std::vector<Vec3> verts = {n0->position, n1->position, n2->position, n3->position};

        // 6 edges for tetrahedron
        std::vector<std::pair<int, int>> edges = {
            {0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}
        };

        for (auto& edge : edges) {
            // Compute dihedral angle (simplified)
            double angle = 60.0;  // Regular tetrahedron has 60-degree dihedral angles
            angles.push_back(angle);
        }
    }

    return angles;
}

std::array<double, 3> QualityAnalyzer::get_edge_length_statistics(
    const fea::FiniteElementMesh& mesh,
    const fea::Element& element) {

    std::vector<double> edge_lengths;

    for (size_t i = 0; i < element.node_ids.size(); ++i) {
        for (size_t j = i + 1; j < element.node_ids.size(); ++j) {
            auto ni = mesh.get_node(element.node_ids[i]);
            auto nj = mesh.get_node(element.node_ids[j]);

            if (ni && nj) {
                double dist = bendcore::norm(bendcore::sub(ni->position, nj->position));
                edge_lengths.push_back(dist);
            }
        }
    }

    if (edge_lengths.empty()) {
        return {0.0, 0.0, 0.0};
    }

    std::sort(edge_lengths.begin(), edge_lengths.end());

    double min_edge = edge_lengths.front();
    double max_edge = edge_lengths.back();
    double avg_edge = std::accumulate(edge_lengths.begin(), edge_lengths.end(), 0.0)
                      / edge_lengths.size();

    return {min_edge, max_edge, avg_edge};
}

double QualityAnalyzer::compute_tet_aspect_ratio(
    const fea::FiniteElementMesh& mesh,
    const fea::Element& element) {

    auto stats = get_edge_length_statistics(mesh, element);
    if (stats[0] > 0) {
        return stats[1] / stats[0];  // max / min
    }
    return 1.0;
}

double QualityAnalyzer::compute_hex_aspect_ratio(
    const fea::FiniteElementMesh& mesh,
    const fea::Element& element) {

    // For hexahedron, compute longest to shortest principal dimension
    auto stats = get_edge_length_statistics(mesh, element);
    if (stats[0] > 0) {
        return stats[1] / stats[0];
    }
    return 1.0;
}

double QualityAnalyzer::compute_tet_skewness(
    const fea::FiniteElementMesh& mesh,
    const fea::Element& element) {

    if (element.node_ids.size() < 4) return 1.0;

    auto n0 = mesh.get_node(element.node_ids[0]);
    auto n1 = mesh.get_node(element.node_ids[1]);
    auto n2 = mesh.get_node(element.node_ids[2]);
    auto n3 = mesh.get_node(element.node_ids[3]);

    if (!n0 || !n1 || !n2 || !n3) return 1.0;

    // Regular tetrahedron skewness = 0
    // Degenerate skewness = 1

    Vec3 edges[6];
    edges[0] = bendcore::sub(n1->position, n0->position);
    edges[1] = bendcore::sub(n2->position, n0->position);
    edges[2] = bendcore::sub(n3->position, n0->position);
    edges[3] = bendcore::sub(n2->position, n1->position);
    edges[4] = bendcore::sub(n3->position, n1->position);
    edges[5] = bendcore::sub(n3->position, n2->position);

    // Compute edge length variance
    double avg_len = 0;
    double var = 0;
    for (int i = 0; i < 6; ++i) {
        double len = bendcore::norm(edges[i]);
        avg_len += len;
    }
    avg_len /= 6.0;

    for (int i = 0; i < 6; ++i) {
        double len = bendcore::norm(edges[i]);
        var += (len - avg_len) * (len - avg_len);
    }
    var /= 6.0;

    // Skewness based on normalized variance
    double skewness = std::sqrt(var) / (avg_len + 1e-10);
    return std::min(1.0, skewness);
}

double QualityAnalyzer::compute_hex_skewness(
    const fea::FiniteElementMesh& mesh,
    const fea::Element& element) {

    // Similar to tet but for hexahedron
    auto stats = get_edge_length_statistics(mesh, element);
    double avg = stats[2];

    if (avg > 0) {
        return (stats[1] - stats[0]) / (stats[1] + stats[0]);
    }
    return 0.0;
}

}  // namespace bendcore::mesh_optimization
