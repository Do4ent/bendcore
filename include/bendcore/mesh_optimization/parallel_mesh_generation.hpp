// Parallel Mesh Generation System
// M7 Phase: Mesh Optimization (GAP-028)
#pragma once

#include "bendcore/fea/mesh_generator.hpp"
#include "bendcore/vec3.hpp"
#include <vector>
#include <cstdint>
#include <memory>

namespace bendcore::mesh_optimization {

// Domain decomposition configuration
struct DomainDecompositionConfig {
    int32_t num_subdomains_x = 2;
    int32_t num_subdomains_y = 2;
    int32_t num_subdomains_z = 2;
    double overlap_ratio = 0.1;  // Overlap between subdomains
    bool use_load_balancing = true;
};

// Subdomain mesh information
struct SubdomainMesh {
    int32_t subdomain_id;
    std::vector<fea::Node> nodes;
    std::vector<fea::Element> elements;
    std::vector<int32_t> interface_node_ids;  // Nodes on subdomain boundary
    Vec3 domain_min;
    Vec3 domain_max;
};

// Domain decomposition for parallel processing
class DomainDecomposer {
public:
    DomainDecomposer() = default;
    ~DomainDecomposer() = default;

    // Decompose mesh into subdomains
    std::vector<SubdomainMesh> decompose_mesh(
        const fea::FiniteElementMesh& mesh,
        const DomainDecompositionConfig& config);

    // Merge subdomains back into single mesh
    std::unique_ptr<fea::FiniteElementMesh> merge_subdomains(
        const std::vector<SubdomainMesh>& subdomains);

    // Load balance subdomains
    std::vector<SubdomainMesh> load_balance(
        const std::vector<SubdomainMesh>& subdomains);

    // Get subdomain for point
    int32_t get_subdomain_for_point(
        const Vec3& point,
        const std::vector<SubdomainMesh>& subdomains);

private:
    // Assign nodes to subdomains
    std::vector<int32_t> assign_nodes_to_subdomains(
        const fea::FiniteElementMesh& mesh,
        const std::vector<Vec3>& subdomain_centers,
        const DomainDecompositionConfig& config);

    // Collect interface nodes
    std::vector<int32_t> collect_interface_nodes(
        const SubdomainMesh& subdomain,
        const fea::FiniteElementMesh& global_mesh);
};

// Parallel element generation
class ParallelElementGenerator {
public:
    ParallelElementGenerator() = default;
    ~ParallelElementGenerator() = default;

    // Generate elements for subdomain in parallel
    std::vector<fea::Element> generate_elements_for_subdomain(
        const SubdomainMesh& subdomain,
        const fea::FiniteElementMesh& global_mesh);

    // Generate nodes for subdomain
    std::vector<fea::Node> generate_nodes_for_subdomain(
        const Vec3& domain_min,
        const Vec3& domain_max,
        int32_t divisions_x,
        int32_t divisions_y,
        int32_t divisions_z);

    // Parallel mesh refinement
    void parallel_refine_elements(
        std::vector<SubdomainMesh>& subdomains,
        const std::vector<std::vector<int32_t>>& elements_to_refine_per_subdomain);

private:
    // Create tetrahedral elements for subdomain
    std::vector<fea::Element> create_tet_elements(
        const std::vector<fea::Node>& nodes,
        int32_t divisions_x, int32_t divisions_y, int32_t divisions_z);

    // Create hexahedral elements for subdomain
    std::vector<fea::Element> create_hex_elements(
        const std::vector<fea::Node>& nodes,
        int32_t divisions_x, int32_t divisions_y, int32_t divisions_z);
};

// Synchronization and merging for parallel processing
class ParallelMeshSynchronizer {
public:
    ParallelMeshSynchronizer() = default;
    ~ParallelMeshSynchronizer() = default;

    // Synchronize node information across subdomains
    void synchronize_nodes(
        std::vector<SubdomainMesh>& subdomains);

    // Synchronize element information across subdomains
    void synchronize_elements(
        std::vector<SubdomainMesh>& subdomains);

    // Merge overlapping elements
    void merge_overlapping_elements(
        std::vector<SubdomainMesh>& subdomains);

    // Resolve hanging nodes at interfaces
    void resolve_hanging_nodes(
        std::vector<SubdomainMesh>& subdomains);

    // Verify mesh consistency across subdomains
    bool verify_mesh_consistency(
        const std::vector<SubdomainMesh>& subdomains);

private:
    // Find duplicate nodes across subdomains
    std::vector<std::pair<int32_t, int32_t>> find_duplicate_nodes(
        const std::vector<SubdomainMesh>& subdomains);

    // Remap node IDs after merging
    void remap_node_ids(
        std::vector<SubdomainMesh>& subdomains,
        const std::vector<std::pair<int32_t, int32_t>>& id_mapping);
};

// Load balancing for subdomains
class LoadBalancer {
public:
    LoadBalancer() = default;
    ~LoadBalancer() = default;

    // Compute load for subdomain
    struct LoadMetrics {
        int32_t num_elements;
        int32_t num_nodes;
        int32_t num_interface_nodes;
        double computational_cost;
        double communication_cost;
    };

    // Analyze load distribution
    std::vector<LoadMetrics> analyze_load(
        const std::vector<SubdomainMesh>& subdomains);

    // Balance load across subdomains
    std::vector<SubdomainMesh> balance_load(
        const std::vector<SubdomainMesh>& subdomains);

    // Compute imbalance ratio
    double compute_imbalance_ratio(
        const std::vector<LoadMetrics>& metrics);

    // Recommend new decomposition
    DomainDecompositionConfig recommend_decomposition(
        const std::vector<SubdomainMesh>& subdomains,
        int32_t target_num_threads);

private:
    // Migrate elements between subdomains
    void migrate_elements(
        SubdomainMesh& source,
        SubdomainMesh& target,
        const std::vector<int32_t>& element_ids);
};

}  // namespace bendcore::mesh_optimization
