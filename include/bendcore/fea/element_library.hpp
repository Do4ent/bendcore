// FEA Element Library - Element Definitions and Shape Functions
// M6 Phase: FEA Integration (GAP-025)
#pragma once

#include "bendcore/fea/mesh_generator.hpp"
#include <vector>
#include <array>
#include <cstdint>

namespace bendcore::fea {

// Shape function value and derivatives at a point
struct ShapeFunctionValues {
    std::vector<double> N;      // Shape function values N_i
    std::vector<Vec3> dN_dxi;   // Derivatives with respect to natural coordinates
};

// Integration point (Gauss quadrature)
struct GaussPoint {
    Vec3 natural_coords;  // Natural coordinates in element (-1 to 1)
    double weight;        // Integration weight
};

// Base class for element types
class ElementDefinition {
public:
    virtual ~ElementDefinition() = default;

    // Get number of nodes for this element
    virtual int32_t get_num_nodes() const = 0;

    // Get number of Gauss points for integration
    virtual int32_t get_num_gauss_points() const = 0;

    // Get list of Gauss points and weights
    virtual std::vector<GaussPoint> get_gauss_points() const = 0;

    // Evaluate shape functions at natural coordinates
    virtual ShapeFunctionValues evaluate_shape_functions(const Vec3& natural_coords) const = 0;

    // Compute Jacobian matrix at natural coordinates
    virtual std::array<std::array<double, 3>, 3> compute_jacobian(
        const Vec3& natural_coords,
        const std::vector<Vec3>& node_positions) const = 0;

    // Compute Jacobian determinant
    virtual double compute_jacobian_determinant(
        const Vec3& natural_coords,
        const std::vector<Vec3>& node_positions) const = 0;

    // Get element volume (using Gauss quadrature)
    virtual double compute_volume(const std::vector<Vec3>& node_positions) const = 0;

    // Get node natural coordinates (for reference element)
    virtual Vec3 get_node_natural_coords(int32_t node_index) const = 0;

    // Check if point is inside element
    virtual bool is_point_inside(
        const Vec3& point,
        const std::vector<Vec3>& node_positions,
        Vec3& natural_coords) const = 0;
};

// Tetrahedral element with 4 nodes
class Tetrahedron4 : public ElementDefinition {
public:
    int32_t get_num_nodes() const override { return 4; }
    int32_t get_num_gauss_points() const override { return 1; }  // Single point at centroid
    std::vector<GaussPoint> get_gauss_points() const override;
    ShapeFunctionValues evaluate_shape_functions(const Vec3& natural_coords) const override;
    std::array<std::array<double, 3>, 3> compute_jacobian(
        const Vec3& natural_coords,
        const std::vector<Vec3>& node_positions) const override;
    double compute_jacobian_determinant(
        const Vec3& natural_coords,
        const std::vector<Vec3>& node_positions) const override;
    double compute_volume(const std::vector<Vec3>& node_positions) const override;
    Vec3 get_node_natural_coords(int32_t node_index) const override;
    bool is_point_inside(
        const Vec3& point,
        const std::vector<Vec3>& node_positions,
        Vec3& natural_coords) const override;

private:
    // Natural coordinates of nodes: (0,0,0), (1,0,0), (0,1,0), (0,0,1)
};

// Tetrahedral element with 10 nodes (includes midpoints)
class Tetrahedron10 : public ElementDefinition {
public:
    int32_t get_num_nodes() const override { return 10; }
    int32_t get_num_gauss_points() const override { return 4; }  // 4-point integration
    std::vector<GaussPoint> get_gauss_points() const override;
    ShapeFunctionValues evaluate_shape_functions(const Vec3& natural_coords) const override;
    std::array<std::array<double, 3>, 3> compute_jacobian(
        const Vec3& natural_coords,
        const std::vector<Vec3>& node_positions) const override;
    double compute_jacobian_determinant(
        const Vec3& natural_coords,
        const std::vector<Vec3>& node_positions) const override;
    double compute_volume(const std::vector<Vec3>& node_positions) const override;
    Vec3 get_node_natural_coords(int32_t node_index) const override;
    bool is_point_inside(
        const Vec3& point,
        const std::vector<Vec3>& node_positions,
        Vec3& natural_coords) const override;
};

// Hexahedral element with 8 nodes
class Hexahedron8 : public ElementDefinition {
public:
    int32_t get_num_nodes() const override { return 8; }
    int32_t get_num_gauss_points() const override { return 8; }  // 2x2x2 Gauss quadrature
    std::vector<GaussPoint> get_gauss_points() const override;
    ShapeFunctionValues evaluate_shape_functions(const Vec3& natural_coords) const override;
    std::array<std::array<double, 3>, 3> compute_jacobian(
        const Vec3& natural_coords,
        const std::vector<Vec3>& node_positions) const override;
    double compute_jacobian_determinant(
        const Vec3& natural_coords,
        const std::vector<Vec3>& node_positions) const override;
    double compute_volume(const std::vector<Vec3>& node_positions) const override;
    Vec3 get_node_natural_coords(int32_t node_index) const override;
    bool is_point_inside(
        const Vec3& point,
        const std::vector<Vec3>& node_positions,
        Vec3& natural_coords) const override;

private:
    // Natural coordinates at corners: (-1,-1,-1) to (1,1,1)
};

// Hexahedral element with 20 nodes (includes midpoints)
class Hexahedron20 : public ElementDefinition {
public:
    int32_t get_num_nodes() const override { return 20; }
    int32_t get_num_gauss_points() const override { return 27; }  // 3x3x3 Gauss quadrature
    std::vector<GaussPoint> get_gauss_points() const override;
    ShapeFunctionValues evaluate_shape_functions(const Vec3& natural_coords) const override;
    std::array<std::array<double, 3>, 3> compute_jacobian(
        const Vec3& natural_coords,
        const std::vector<Vec3>& node_positions) const override;
    double compute_jacobian_determinant(
        const Vec3& natural_coords,
        const std::vector<Vec3>& node_positions) const override;
    double compute_volume(const std::vector<Vec3>& node_positions) const override;
    Vec3 get_node_natural_coords(int32_t node_index) const override;
    bool is_point_inside(
        const Vec3& point,
        const std::vector<Vec3>& node_positions,
        Vec3& natural_coords) const override;
};

// Factory for creating element definitions
class ElementFactory {
public:
    static std::unique_ptr<ElementDefinition> create_element(ElementType type);
    static std::unique_ptr<ElementDefinition> create_element_4node_tet();
    static std::unique_ptr<ElementDefinition> create_element_10node_tet();
    static std::unique_ptr<ElementDefinition> create_element_8node_hex();
    static std::unique_ptr<ElementDefinition> create_element_20node_hex();
};

// Stiffness matrix computation
class StiffnessMatrixComputer {
public:
    // Compute local stiffness matrix for element
    static std::vector<std::vector<double>> compute_element_stiffness(
        const Element& elem,
        const FiniteElementMesh& mesh,
        double youngs_modulus,
        double poissons_ratio);

    // Compute local mass matrix for element
    static std::vector<std::vector<double>> compute_element_mass(
        const Element& elem,
        const FiniteElementMesh& mesh,
        double density);

    // Expand local matrix to global coordinates
    static std::vector<std::vector<double>> expand_to_global(
        const std::vector<std::vector<double>>& local_matrix,
        int32_t num_dofs_per_node = 3);
};

// Strain and stress computation
class StrainStressComputer {
public:
    // Compute strain tensor at a point
    static std::array<std::array<double, 3>, 3> compute_strain_tensor(
        const std::vector<Vec3>& displacements,
        const ElementDefinition& elem_def,
        const std::vector<Vec3>& node_positions,
        const Vec3& natural_coords);

    // Compute stress tensor from strain
    static std::array<std::array<double, 3>, 3> compute_stress_tensor(
        const std::array<std::array<double, 3>, 3>& strain,
        double youngs_modulus,
        double poissons_ratio);

    // Compute von Mises stress
    static double compute_von_mises_stress(
        const std::array<std::array<double, 3>, 3>& stress);

    // Compute principal stresses
    static std::array<double, 3> compute_principal_stresses(
        const std::array<std::array<double, 3>, 3>& stress);
};

}  // namespace bendcore::fea
