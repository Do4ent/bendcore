// FEA Mesh Generation System
// M6 Phase: FEA Integration (GAP-025)
#pragma once

#include "bendcore/vec3.hpp"
#include <vector>
#include <array>
#include <cstdint>
#include <memory>

namespace bendcore::fea {

// Element types supported by the FEA system
enum class ElementType {
    Tetrahedron4,    // 4-node tetrahedral element
    Tetrahedron10,   // 10-node tetrahedral element (with midpoints)
    Hexahedron8,     // 8-node hexahedral element
    Hexahedron20     // 20-node hexahedral element (with midpoints)
};

// Node structure for FEA mesh
struct Node {
    int32_t id;
    Vec3 position;
    bool is_boundary;
    int32_t boundary_marker;  // For material assignment
};

// Element structure mapping nodes
struct Element {
    int32_t id;
    ElementType type;
    std::vector<int32_t> node_ids;  // IDs of nodes forming this element
    int32_t material_id;
};

// Quality metrics for mesh
struct MeshQuality {
    double aspect_ratio;       // Max aspect ratio
    double min_dihedral_angle; // Minimum dihedral angle (degrees)
    double max_dihedral_angle; // Maximum dihedral angle (degrees)
    double jacobian_min;       // Minimum Jacobian determinant
    double skewness;           // Element skewness (0-1, 0=perfect)
};

// Complete finite element mesh
class FiniteElementMesh {
public:
    FiniteElementMesh();
    ~FiniteElementMesh();

    // Node management
    void add_node(int32_t id, const Vec3& position, bool is_boundary = false);
    void add_node(int32_t id, double x, double y, double z, bool is_boundary = false);
    Node* get_node(int32_t id);
    const Node* get_node(int32_t id) const;
    const std::vector<Node>& get_nodes() const { return nodes_; }
    int32_t num_nodes() const { return static_cast<int32_t>(nodes_.size()); }

    // Element management
    void add_element(int32_t id, ElementType type, const std::vector<int32_t>& node_ids, int32_t material_id = 0);
    Element* get_element(int32_t id);
    const Element* get_element(int32_t id) const;
    const std::vector<Element>& get_elements() const { return elements_; }
    int32_t num_elements() const { return static_cast<int32_t>(elements_.size()); }

    // Mesh statistics
    int32_t get_num_nodes_for_element(ElementType type) const;
    int32_t get_num_gauss_points(ElementType type) const;

    // Boundary conditions
    void mark_boundary_node(int32_t node_id, int32_t marker);
    const std::vector<int32_t>& get_boundary_nodes() const { return boundary_nodes_; }

    // Mesh validation
    bool validate() const;
    MeshQuality compute_quality() const;
    double compute_element_volume(const Element& elem) const;
    double compute_element_jacobian(const Element& elem) const;

    // Clear mesh
    void clear();

private:
    std::vector<Node> nodes_;
    std::vector<Element> elements_;
    std::vector<int32_t> boundary_nodes_;
};

// Tetrahedral mesh generator
class TetrahedralMeshGenerator {
public:
    TetrahedralMeshGenerator();

    // Generate mesh from bounding box with uniform refinement
    std::unique_ptr<FiniteElementMesh> generate_uniform_mesh(
        const Vec3& min_bounds, const Vec3& max_bounds,
        int32_t divisions_x, int32_t divisions_y, int32_t divisions_z);

    // Generate mesh with adaptive refinement
    std::unique_ptr<FiniteElementMesh> generate_adaptive_mesh(
        const Vec3& min_bounds, const Vec3& max_bounds,
        double target_element_size, int32_t max_refinement_levels);

    // Generate mesh from point cloud (Delaunay-inspired)
    std::unique_ptr<FiniteElementMesh> generate_from_point_cloud(
        const std::vector<Vec3>& points, double characteristic_length);

    // Mesh refinement
    static void refine_mesh(FiniteElementMesh& mesh, const std::vector<int32_t>& element_ids_to_refine);

private:
    int32_t next_node_id_;
    int32_t next_element_id_;
};

// Hexahedral mesh generator
class HexahedralMeshGenerator {
public:
    HexahedralMeshGenerator();

    // Generate structured hex mesh from bounding box
    std::unique_ptr<FiniteElementMesh> generate_structured_mesh(
        const Vec3& min_bounds, const Vec3& max_bounds,
        int32_t divisions_x, int32_t divisions_y, int32_t divisions_z);

    // Generate unstructured hex mesh with refinement
    std::unique_ptr<FiniteElementMesh> generate_unstructured_mesh(
        const Vec3& min_bounds, const Vec3& max_bounds,
        double target_element_size);

private:
    int32_t next_node_id_;
    int32_t next_element_id_;
};

// Mesh refinement strategies
class MeshRefiner {
public:
    // Uniform refinement of all elements
    static void refine_uniform(FiniteElementMesh& mesh);

    // Adaptive refinement based on error estimate
    static void refine_adaptive(
        FiniteElementMesh& mesh,
        const std::vector<double>& element_errors,
        double refinement_threshold);

    // Mark elements for refinement using red-green refinement
    static void mark_elements_for_refinement(
        FiniteElementMesh& mesh,
        const std::vector<int32_t>& element_ids);

    // Coarsen mesh by removing small elements
    static void coarsen_mesh(FiniteElementMesh& mesh, double size_threshold);
};

// Mesh quality checker
class MeshQualityChecker {
public:
    static bool check_element_validity(const FiniteElementMesh& mesh, const Element& elem);
    static double compute_aspect_ratio(const FiniteElementMesh& mesh, const Element& elem);
    static double compute_skewness(const FiniteElementMesh& mesh, const Element& elem);
    static double compute_jacobian(const FiniteElementMesh& mesh, const Element& elem);
    static std::vector<double> compute_dihedral_angles(const FiniteElementMesh& mesh, const Element& elem);
};

}  // namespace bendcore::fea
