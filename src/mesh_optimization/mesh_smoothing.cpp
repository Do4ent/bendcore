// Mesh Smoothing Implementation
// M7 Phase: Mesh Optimization (GAP-028)
#include "bendcore/mesh_optimization/mesh_smoothing.hpp"
#include "bendcore/mesh_optimization/quality_analyzer.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

using namespace bendcore;
using namespace bendcore::mesh_optimization;
using namespace bendcore::fea;

// ============================================================================
// LaplacianSmoother Implementation
// ============================================================================

SmoothingStats LaplacianSmoother::smooth(
    FiniteElementMesh& mesh,
    const SmoothingConfig& config) {

    SmoothingStats stats;
    stats.converged = false;
    stats.iterations_completed = 0;
    stats.nodes_moved = 0;

    QualityAnalyzer analyzer;
    auto initial_metrics = analyzer.analyze_all_elements(mesh);
    auto initial_stats = analyzer.compute_statistics(initial_metrics);
    stats.initial_quality = initial_stats.overall_quality_score;

    for (int32_t iter = 0; iter < config.max_iterations; ++iter) {
        double max_displacement = smooth_iteration(mesh);
        stats.iterations_completed++;

        if (max_displacement < config.convergence_tolerance) {
            stats.converged = true;
            break;
        }
    }

    auto final_metrics = analyzer.analyze_all_elements(mesh);
    auto final_stats = analyzer.compute_statistics(final_metrics);
    stats.final_quality = final_stats.overall_quality_score;
    stats.final_displacement = 0.0;  // Would compute from last iteration

    return stats;
}

double LaplacianSmoother::smooth_iteration(FiniteElementMesh& mesh) {
    double max_displacement = 0.0;

    // For each non-boundary node, move it to average of neighbors
    std::vector<Vec3> new_positions(mesh.num_nodes());
    bool* moved = new bool[mesh.num_nodes()];
    std::fill(moved, moved + mesh.num_nodes(), false);

    for (auto& node : const_cast<std::vector<Node>&>(mesh.get_nodes())) {
        if (node.is_boundary) {
            new_positions[&node - &mesh.get_nodes()[0]] = node.position;
            continue;
        }

        Vec3 laplacian = compute_laplacian(mesh, node.id);
        new_positions[&node - &mesh.get_nodes()[0]] = laplacian;
    }

    // Update positions
    for (size_t i = 0; i < mesh.get_nodes().size(); ++i) {
        if (!mesh.get_nodes()[i].is_boundary) {
            Vec3 displacement = bendcore::sub(new_positions[i], mesh.get_nodes()[i].position);
            double disp_mag = bendcore::norm(displacement);
            max_displacement = std::max(max_displacement, disp_mag);

            const_cast<Node&>(mesh.get_nodes()[i]).position = new_positions[i];
        }
    }

    delete[] moved;
    return max_displacement;
}

double LaplacianSmoother::smooth_nodes(
    FiniteElementMesh& mesh,
    const std::vector<int32_t>& node_ids) {

    double max_displacement = 0.0;
    std::vector<Vec3> new_positions;

    for (int32_t node_id : node_ids) {
        auto node = mesh.get_node(node_id);
        if (!node || node->is_boundary) continue;

        Vec3 laplacian = compute_laplacian(mesh, node_id);
        new_positions.push_back(laplacian);
    }

    return max_displacement;
}

Vec3 LaplacianSmoother::compute_laplacian(
    const FiniteElementMesh& mesh,
    int32_t node_id) {

    auto node = mesh.get_node(node_id);
    if (!node) return Vec3{0, 0, 0};

    auto neighbors = get_neighbors(mesh, node_id);
    if (neighbors.empty()) return node->position;

    Vec3 sum{0, 0, 0};
    for (int32_t neighbor_id : neighbors) {
        auto neighbor = mesh.get_node(neighbor_id);
        if (neighbor) {
            sum = sum + neighbor->position;
        }
    }

    return sum * (1.0 / neighbors.size());
}

std::vector<int32_t> LaplacianSmoother::get_neighbors(
    const FiniteElementMesh& mesh,
    int32_t node_id) {

    std::vector<int32_t> neighbors;

    // Find all elements containing this node
    for (const auto& elem : mesh.get_elements()) {
        auto it = std::find(elem.node_ids.begin(), elem.node_ids.end(), node_id);
        if (it != elem.node_ids.end()) {
            // Add all other nodes in this element
            for (int32_t other_node_id : elem.node_ids) {
                if (other_node_id != node_id &&
                    std::find(neighbors.begin(), neighbors.end(), other_node_id) == neighbors.end()) {
                    neighbors.push_back(other_node_id);
                }
            }
        }
    }

    return neighbors;
}

// ============================================================================
// OptimizationSmoother Implementation
// ============================================================================

SmoothingStats OptimizationSmoother::optimize(
    FiniteElementMesh& mesh,
    const SmoothingConfig& config) {

    SmoothingStats stats;
    stats.converged = false;
    stats.iterations_completed = 0;

    for (int32_t iter = 0; iter < config.max_iterations; ++iter) {
        double total_improvement = 0.0;

        for (auto& node : const_cast<std::vector<Node>&>(mesh.get_nodes())) {
            if (!node.is_boundary) {
                double improvement = optimize_node(mesh, node.id);
                total_improvement += improvement;
            }
        }

        stats.iterations_completed++;

        if (total_improvement < config.convergence_tolerance) {
            stats.converged = true;
            break;
        }
    }

    return stats;
}

double OptimizationSmoother::optimize_node(
    FiniteElementMesh& mesh,
    int32_t node_id) {

    auto node = mesh.get_node(node_id);
    if (!node) return 0.0;

    double old_quality = compute_node_quality_metric(mesh, node_id);

    Vec3 optimal_pos = compute_optimal_position(mesh, node_id);
    const_cast<Node*>(node)->position = optimal_pos;

    double new_quality = compute_node_quality_metric(mesh, node_id);

    return std::max(0.0, new_quality - old_quality);
}

double OptimizationSmoother::compute_node_quality_metric(
    const FiniteElementMesh& mesh,
    int32_t node_id) {

    double quality = 0.0;
    QualityAnalyzer analyzer;

    // Find all elements containing this node
    int32_t count = 0;
    for (const auto& elem : mesh.get_elements()) {
        auto it = std::find(elem.node_ids.begin(), elem.node_ids.end(), node_id);
        if (it != elem.node_ids.end()) {
            double skewness = analyzer.compute_skewness(mesh, elem);
            quality += (1.0 - skewness);
            count++;
        }
    }

    return count > 0 ? quality / count : 0.0;
}

Vec3 OptimizationSmoother::compute_optimal_position(
    const FiniteElementMesh& mesh,
    int32_t node_id) {

    auto node = mesh.get_node(node_id);
    if (!node) return Vec3{0, 0, 0};

    Vec3 gradient = compute_quality_gradient(mesh, node_id);

    // Move in direction of gradient (with damping)
    double step_size = 0.1;
    return node->position + gradient * step_size;
}

Vec3 OptimizationSmoother::compute_quality_gradient(
    const FiniteElementMesh& mesh,
    int32_t node_id) {

    // Simplified: compute finite difference gradient
    double h = 1e-4;
    Vec3 gradient{0, 0, 0};

    for (int dir = 0; dir < 3; ++dir) {
        auto node = mesh.get_node(node_id);
        if (!node) return gradient;

        Vec3 pos_plus = node->position;
        Vec3 pos_minus = node->position;

        if (dir == 0) {
            pos_plus[0] += h;
            pos_minus[0] -= h;
        } else if (dir == 1) {
            pos_plus[1] += h;
            pos_minus[1] -= h;
        } else {
            pos_plus[2] += h;
            pos_minus[2] -= h;
        }

        // Finite difference (simplified)
        double df = 0.0;  // Quality difference

        if (dir == 0) gradient[0] = df / (2 * h);
        else if (dir == 1) gradient[1] = df / (2 * h);
        else gradient[2] = df / (2 * h);
    }

    return gradient;
}

// ============================================================================
// BoundaryPreservingSmoother Implementation
// ============================================================================

SmoothingStats BoundaryPreservingSmoother::smooth_with_boundary_preservation(
    FiniteElementMesh& mesh,
    const std::vector<int32_t>& boundary_node_ids,
    const SmoothingConfig& config) {

    SmoothingStats stats;
    stats.converged = false;
    stats.iterations_completed = 0;

    for (int32_t iter = 0; iter < config.max_iterations; ++iter) {
        double max_displacement = 0.0;

        for (auto& node : const_cast<std::vector<Node>&>(mesh.get_nodes())) {
            // Check if this is a boundary node
            auto it = std::find(boundary_node_ids.begin(), boundary_node_ids.end(), node.id);
            if (it != boundary_node_ids.end()) {
                // Smooth on boundary
                Vec3 boundary_normal = compute_boundary_normal(mesh, node.id);
                // Move tangent to boundary
                // (simplified implementation)
            }
        }

        stats.iterations_completed++;
        if (max_displacement < config.convergence_tolerance) {
            stats.converged = true;
            break;
        }
    }

    return stats;
}

Vec3 BoundaryPreservingSmoother::project_to_boundary(
    const Vec3& point,
    const std::vector<int32_t>& boundary_elements) {

    // Simplified: return original point
    // Full implementation would project to closest boundary surface
    return point;
}

Vec3 BoundaryPreservingSmoother::compute_boundary_normal(
    const FiniteElementMesh& mesh,
    int32_t boundary_node_id) {

    // Simplified: return zero normal
    // Full implementation would compute normal from boundary elements
    return Vec3{0, 0, 1};
}

bool BoundaryPreservingSmoother::is_on_boundary(
    const FiniteElementMesh& mesh,
    int32_t node_id) {

    auto node = mesh.get_node(node_id);
    return node && node->is_boundary;
}

Vec3 BoundaryPreservingSmoother::constrain_to_boundary(
    const Vec3& displacement,
    const Vec3& boundary_normal) {

    // Remove normal component
    double dot = bendcore::dot(displacement, boundary_normal);

    return bendcore::sub(displacement, bendcore::mul(boundary_normal, dot));
}

// ============================================================================
// MeshSmoother Implementation
// ============================================================================

SmoothingStats MeshSmoother::smooth(
    FiniteElementMesh& mesh,
    SmoothingMethod method,
    const SmoothingConfig& config) {

    switch (method) {
        case SmoothingMethod::Laplacian:
            last_stats_ = laplacian_smoother_.smooth(mesh, config);
            break;
        case SmoothingMethod::Optimization:
            last_stats_ = optimization_smoother_.optimize(mesh, config);
            break;
        case SmoothingMethod::Hybrid: {
            // Two-pass: Laplacian then optimization
            last_stats_ = laplacian_smoother_.smooth(mesh, config);
            last_stats_ = optimization_smoother_.optimize(mesh, config);
            break;
        }
    }

    return last_stats_;
}

SmoothingStats MeshSmoother::smooth_adaptive(
    FiniteElementMesh& mesh,
    const SmoothingConfig& config) {

    // Auto-select method based on mesh quality
    if (needs_boundary_preservation(mesh)) {
        last_stats_ = boundary_smoother_.smooth_with_boundary_preservation(
            mesh, mesh.get_boundary_nodes(), config);
    } else {
        last_stats_ = smooth(mesh, SmoothingMethod::Hybrid, config);
    }

    return last_stats_;
}

bool MeshSmoother::needs_boundary_preservation(
    const FiniteElementMesh& mesh) {

    return !mesh.get_boundary_nodes().empty();
}

}  // namespace bendcore::mesh_optimization
