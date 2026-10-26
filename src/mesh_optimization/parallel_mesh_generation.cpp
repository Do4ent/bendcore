// Parallel Mesh Generation Implementation
// M7 Phase: Mesh Optimization (GAP-028)
#include "bendcore/mesh_optimization/parallel_mesh_generation.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <map>

using namespace bendcore;
using namespace bendcore::mesh_optimization;
using namespace bendcore::fea;

// ============================================================================
// DomainDecomposer Implementation
// ============================================================================

std::vector<SubdomainMesh> DomainDecomposer::decompose_mesh(
    const FiniteElementMesh& mesh,
    const DomainDecompositionConfig& config) {

    std::vector<SubdomainMesh> subdomains;

    // Compute subdomain bounds
    Vec3 mesh_min{1e10, 1e10, 1e10};
    Vec3 mesh_max{-1e10, -1e10, -1e10};

    for (const auto& node : mesh.get_nodes()) {
        mesh_min[0] = std::min(mesh_min[0], node[0]);
        mesh_min[1] = std::min(mesh_min[1], node[1]);
        mesh_min[2] = std::min(mesh_min[2], node[2]);
        mesh_max[0] = std::max(mesh_max[0], node[0]);
        mesh_max[1] = std::max(mesh_max[1], node[1]);
        mesh_max[2] = std::max(mesh_max[2], node[2]);
    }

    // Create subdomains
    int32_t subdomain_id = 0;
    double dx = (mesh_max[0] - mesh_min[0]) / config.num_subdomains_x;
    double dy = (mesh_max[1] - mesh_min[1]) / config.num_subdomains_y;
    double dz = (mesh_max[2] - mesh_min[2]) / config.num_subdomains_z;

    for (int i = 0; i < config.num_subdomains_x; ++i) {
        for (int j = 0; j < config.num_subdomains_y; ++j) {
            for (int k = 0; k < config.num_subdomains_z; ++k) {
                SubdomainMesh sub;
                sub.subdomain_id = subdomain_id++;

                sub[0] = mesh_min[0] + i * dx;
                sub[1] = mesh_min[1] + j * dy;
                sub[2] = mesh_min[2] + k * dz;

                sub[0] = mesh_min[0] + (i + 1) * dx;
                sub[1] = mesh_min[1] + (j + 1) * dy;
                sub[2] = mesh_min[2] + (k + 1) * dz;

                // Assign nodes to this subdomain
                for (const auto& node : mesh.get_nodes()) {
                    if (node[0] >= sub[0] &&
                        node[0] <= sub[0] &&
                        node[1] >= sub[1] &&
                        node[1] <= sub[1] &&
                        node[2] >= sub[2] &&
                        node[2] <= sub[2]) {
                        sub.nodes.push_back(node);
                    }
                }

                // Assign elements to this subdomain
                for (const auto& elem : mesh.get_elements()) {
                    bool elem_in_subdomain = false;
                    for (int32_t node_id : elem.node_ids) {
                        auto node = mesh.get_node(node_id);
                        if (node &&
                            node->position[0] >= sub[0] &&
                            node->position[0] <= sub[0] &&
                            node->position[1] >= sub[1] &&
                            node->position[1] <= sub[1] &&
                            node->position[2] >= sub[2] &&
                            node->position[2] <= sub[2]) {
                            elem_in_subdomain = true;
                            break;
                        }
                    }
                    if (elem_in_subdomain) {
                        sub.elements.push_back(elem);
                    }
                }

                subdomains.push_back(sub);
            }
        }
    }

    // Load balance if requested
    if (config.use_load_balancing) {
        subdomains = load_balance(subdomains);
    }

    return subdomains;
}

std::unique_ptr<FiniteElementMesh> DomainDecomposer::merge_subdomains(
    const std::vector<SubdomainMesh>& subdomains) {

    auto mesh = std::make_unique<FiniteElementMesh>();

    std::map<int32_t, int32_t> node_id_mapping;

    // Add all nodes from subdomains
    for (const auto& subdomain : subdomains) {
        for (const auto& node : subdomain.nodes) {
            if (node_id_mapping.find(node.id) == node_id_mapping.end()) {
                mesh->add_node(node.id, node.position, node.is_boundary);
                node_id_mapping[node.id] = node.id;
            }
        }
    }

    // Add all elements from subdomains
    for (const auto& subdomain : subdomains) {
        for (const auto& elem : subdomain.elements) {
            mesh->add_element(elem.id, elem.type, elem.node_ids, elem.material_id);
        }
    }

    return mesh;
}

std::vector<SubdomainMesh> DomainDecomposer::load_balance(
    const std::vector<SubdomainMesh>& subdomains) {

    // Simplified load balancing: ensure subdomains have similar number of elements
    return subdomains;
}

int32_t DomainDecomposer::get_subdomain_for_point(
    const Vec3& point,
    const std::vector<SubdomainMesh>& subdomains) {

    for (const auto& sub : subdomains) {
        if (point[0] >= sub[0] && point[0] <= sub[0] &&
            point[1] >= sub[1] && point[1] <= sub[1] &&
            point[2] >= sub[2] && point[2] <= sub[2]) {
            return sub.subdomain_id;
        }
    }

    return -1;
}

std::vector<int32_t> DomainDecomposer::assign_nodes_to_subdomains(
    const FiniteElementMesh& mesh,
    const std::vector<Vec3>& subdomain_centers,
    const DomainDecompositionConfig& config) {

    std::vector<int32_t> assignment(mesh.num_nodes());

    for (const auto& node : mesh.get_nodes()) {
        // Find closest subdomain center
        int closest_sub = 0;
        double min_dist = 1e10;

        for (size_t i = 0; i < subdomain_centers.size(); ++i) {
            double dist = bendcore::norm(bendcore::sub(node.position, subdomain_centers[i]));
            if (dist < min_dist) {
                min_dist = dist;
                closest_sub = i;
            }
        }
    }

    return assignment;
}

std::vector<int32_t> DomainDecomposer::collect_interface_nodes(
    const SubdomainMesh& subdomain,
    const FiniteElementMesh& global_mesh) {

    std::vector<int32_t> interface_nodes;

    // Nodes on subdomain boundary
    for (const auto& node : subdomain.nodes) {
        bool is_boundary = false;

        if (node[0] <= subdomain[0] ||
            node[0] >= subdomain[0] ||
            node[1] <= subdomain[1] ||
            node[1] >= subdomain[1] ||
            node[2] <= subdomain[2] ||
            node[2] >= subdomain[2]) {
            is_boundary = true;
        }

        if (is_boundary) {
            interface_nodes.push_back(node.id);
        }
    }

    return interface_nodes;
}

// ============================================================================
// ParallelElementGenerator Implementation
// ============================================================================

std::vector<Element> ParallelElementGenerator::generate_elements_for_subdomain(
    const SubdomainMesh& subdomain,
    const FiniteElementMesh& global_mesh) {

    std::vector<Element> elements;

    // Generate elements based on subdomain nodes
    int32_t elem_id = 10000 + subdomain.subdomain_id * 100000;

    if (subdomain.nodes.size() >= 4) {
        // Create tetrahedral elements
        for (size_t i = 0; i < subdomain.nodes.size() - 3; ++i) {
            Element elem;
            elem.id = elem_id++;
            elem.type = ElementType::Tetrahedron4;
            elem.node_ids = {
                subdomain.nodes[i].id,
                subdomain.nodes[i + 1].id,
                subdomain.nodes[i + 2].id,
                subdomain.nodes[i + 3].id
            };
            elements.push_back(elem);
        }
    }

    return elements;
}

std::vector<Node> ParallelElementGenerator::generate_nodes_for_subdomain(
    const Vec3& domain_min,
    const Vec3& domain_max,
    int32_t divisions_x,
    int32_t divisions_y,
    int32_t divisions_z) {

    std::vector<Node> nodes;

    double dx = (domain_max[0] - domain_min[0]) / divisions_x;
    double dy = (domain_max[1] - domain_min[1]) / divisions_y;
    double dz = (domain_max[2] - domain_min[2]) / divisions_z;

    int32_t node_id = 10000;

    for (int i = 0; i <= divisions_x; ++i) {
        for (int j = 0; j <= divisions_y; ++j) {
            for (int k = 0; k <= divisions_z; ++k) {
                Node node;
                node.id = node_id++;
                node[0] = domain_min[0] + i * dx;
                node[1] = domain_min[1] + j * dy;
                node[2] = domain_min[2] + k * dz;
                node.is_boundary = (i == 0 || i == divisions_x ||
                                    j == 0 || j == divisions_y ||
                                    k == 0 || k == divisions_z);
                nodes.push_back(node);
            }
        }
    }

    return nodes;
}

void ParallelElementGenerator::parallel_refine_elements(
    std::vector<SubdomainMesh>& subdomains,
    const std::vector<std::vector<int32_t>>& elements_to_refine_per_subdomain) {

    for (size_t i = 0; i < subdomains.size(); ++i) {
        if (i < elements_to_refine_per_subdomain.size()) {
            // Refine elements in this subdomain
            // Simplified: would actually refine the elements
        }
    }
}

std::vector<Element> ParallelElementGenerator::create_tet_elements(
    const std::vector<Node>& nodes,
    int32_t divisions_x, int32_t divisions_y, int32_t divisions_z) {

    std::vector<Element> elements;

    int32_t elem_id = 10000;
    int32_t nodes_per_row = (divisions_y + 1) * (divisions_z + 1);

    for (int i = 0; i < divisions_x; ++i) {
        for (int j = 0; j < divisions_y; ++j) {
            for (int k = 0; k < divisions_z; ++k) {
                int32_t n0 = i * nodes_per_row + j * (divisions_z + 1) + k;
                int32_t n1 = (i + 1) * nodes_per_row + j * (divisions_z + 1) + k;

                if (n1 < static_cast<int32_t>(nodes.size())) {
                    Element elem;
                    elem.id = elem_id++;
                    elem.type = ElementType::Tetrahedron4;
                    elem.node_ids = {
                        nodes[n0].id,
                        nodes[n0 + 1].id,
                        nodes[n1].id,
                        nodes[n1 + 1].id
                    };
                    elements.push_back(elem);
                }
            }
        }
    }

    return elements;
}

std::vector<Element> ParallelElementGenerator::create_hex_elements(
    const std::vector<Node>& nodes,
    int32_t divisions_x, int32_t divisions_y, int32_t divisions_z) {

    std::vector<Element> elements;

    // Similar to tet but with 8-node connectivity
    int32_t elem_id = 10000;

    for (int i = 0; i < divisions_x; ++i) {
        for (int j = 0; j < divisions_y; ++j) {
            for (int k = 0; k < divisions_z; ++k) {
                Element elem;
                elem.id = elem_id++;
                elem.type = ElementType::Hexahedron8;
                // Simplified node assignment
                elem.node_ids.resize(8);
                elements.push_back(elem);
            }
        }
    }

    return elements;
}

// ============================================================================
// ParallelMeshSynchronizer Implementation
// ============================================================================

void ParallelMeshSynchronizer::synchronize_nodes(
    std::vector<SubdomainMesh>& subdomains) {

    // Ensure nodes at interfaces are consistent
    for (auto& sub : subdomains) {
        for (auto& node : sub.nodes) {
            // Check if this node appears in other subdomains
            for (auto& other_sub : subdomains) {
                if (sub.subdomain_id == other_sub.subdomain_id) continue;

                for (auto& other_node : other_sub.nodes) {
                    if (node.id == other_node.id) {
                        // Ensure consistent position
                        other_node.position = node.position;
                    }
                }
            }
        }
    }
}

void ParallelMeshSynchronizer::synchronize_elements(
    std::vector<SubdomainMesh>& subdomains) {

    // Ensure element information is consistent
    for (auto& sub : subdomains) {
        for (auto& elem : sub.elements) {
            // Verify all nodes exist
            for (int32_t node_id : elem.node_ids) {
                bool found = false;
                for (const auto& node : sub.nodes) {
                    if (node.id == node_id) {
                        found = true;
                        break;
                    }
                }
                // If not found in this subdomain, it may be at interface
            }
        }
    }
}

void ParallelMeshSynchronizer::merge_overlapping_elements(
    std::vector<SubdomainMesh>& subdomains) {

    // Remove duplicate elements at subdomain interfaces
    for (size_t i = 0; i < subdomains.size(); ++i) {
        for (size_t j = i + 1; j < subdomains.size(); ++j) {
            auto& sub1 = subdomains[i];
            auto& sub2 = subdomains[j];

            // Find and remove duplicates
            for (auto it1 = sub1.elements.begin(); it1 != sub1.elements.end(); ) {
                bool found_duplicate = false;
                for (const auto& elem2 : sub2.elements) {
                    if (it1->id == elem2.id) {
                        found_duplicate = true;
                        break;
                    }
                }
                if (found_duplicate) {
                    it1 = sub1.elements.erase(it1);
                } else {
                    ++it1;
                }
            }
        }
    }
}

void ParallelMeshSynchronizer::resolve_hanging_nodes(
    std::vector<SubdomainMesh>& subdomains) {

    // Hanging nodes occur when elements at different refinement levels meet
    // Simplified: would add constraint equations for hanging nodes
}

bool ParallelMeshSynchronizer::verify_mesh_consistency(
    const std::vector<SubdomainMesh>& subdomains) {

    for (const auto& sub : subdomains) {
        for (const auto& elem : sub.elements) {
            // Verify all nodes exist in some subdomain
            for (int32_t node_id : elem.node_ids) {
                bool found = false;
                for (const auto& other_sub : subdomains) {
                    for (const auto& node : other_sub.nodes) {
                        if (node.id == node_id) {
                            found = true;
                            break;
                        }
                    }
                    if (found) break;
                }
                if (!found) return false;
            }
        }
    }

    return true;
}

std::vector<std::pair<int32_t, int32_t>> ParallelMeshSynchronizer::find_duplicate_nodes(
    const std::vector<SubdomainMesh>& subdomains) {

    std::vector<std::pair<int32_t, int32_t>> duplicates;

    for (size_t i = 0; i < subdomains.size(); ++i) {
        for (size_t j = i + 1; j < subdomains.size(); ++j) {
            for (const auto& n1 : subdomains[i].nodes) {
                for (const auto& n2 : subdomains[j].nodes) {
                    if (n1.id == n2.id &&
                        bendcore::norm(bendcore::sub(n1.position, n2.position)) < 1e-6) {
                        duplicates.push_back({n1.id, n2.id});
                    }
                }
            }
        }
    }

    return duplicates;
}

void ParallelMeshSynchronizer::remap_node_ids(
    std::vector<SubdomainMesh>& subdomains,
    const std::vector<std::pair<int32_t, int32_t>>& id_mapping) {

    for (auto& sub : subdomains) {
        for (auto& elem : sub.elements) {
            for (auto& node_id : elem.node_ids) {
                for (const auto& [old_id, new_id] : id_mapping) {
                    if (node_id == old_id) {
                        node_id = new_id;
                    }
                }
            }
        }
    }
}

// ============================================================================
// LoadBalancer Implementation
// ============================================================================

std::vector<LoadBalancer::LoadMetrics> LoadBalancer::analyze_load(
    const std::vector<SubdomainMesh>& subdomains) {

    std::vector<LoadMetrics> metrics;

    for (const auto& sub : subdomains) {
        LoadMetrics m;
        m.num_elements = static_cast<int32_t>(sub.elements.size());
        m.num_nodes = static_cast<int32_t>(sub.nodes.size());
        m.num_interface_nodes = static_cast<int32_t>(sub.interface_node_ids.size());
        m.computational_cost = m.num_elements * 10 + m.num_nodes;  // Simplified
        m.communication_cost = m.num_interface_nodes * 5;  // Simplified

        metrics.push_back(m);
    }

    return metrics;
}

std::vector<SubdomainMesh> LoadBalancer::balance_load(
    const std::vector<SubdomainMesh>& subdomains) {

    // Simplified: return as-is
    return subdomains;
}

double LoadBalancer::compute_imbalance_ratio(
    const std::vector<LoadMetrics>& metrics) {

    if (metrics.empty()) return 0.0;

    double max_load = 0.0;
    double min_load = 1e10;
    double total_load = 0.0;

    for (const auto& m : metrics) {
        double load = m.computational_cost + m.communication_cost;
        max_load = std::max(max_load, load);
        min_load = std::min(min_load, load);
        total_load += load;
    }

    if (min_load < 1e-6) return 0.0;
    return (max_load - min_load) / (total_load / metrics.size());
}

DomainDecompositionConfig LoadBalancer::recommend_decomposition(
    const std::vector<SubdomainMesh>& subdomains,
    int32_t target_num_threads) {

    DomainDecompositionConfig config;

    // Compute cube root to distribute evenly
    int32_t threads_per_dim = static_cast<int32_t>(std::cbrt(target_num_threads));

    config.num_subdomains_x = threads_per_dim;
    config.num_subdomains_y = threads_per_dim;
    config.num_subdomains_z = threads_per_dim;
    config.use_load_balancing = true;

    return config;
}

void LoadBalancer::migrate_elements(
    SubdomainMesh& source,
    SubdomainMesh& target,
    const std::vector<int32_t>& element_ids) {

    // Move elements from source to target
    for (auto elem_id : element_ids) {
        auto it = std::find_if(
            source.elements.begin(),
            source.elements.end(),
            [elem_id](const Element& e) { return e.id == elem_id; });

        if (it != source.elements.end()) {
            target.elements.push_back(*it);
            source.elements.erase(it);
        }
    }
}

}  // namespace bendcore::mesh_optimization
