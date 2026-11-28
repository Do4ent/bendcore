#pragma once

#include <vector>
#include <array>
#include <memory>
#include <map>
#include <string>
#include "../geometry/vector.hpp"
#include "../document/entity_id.hpp"

namespace BendCore::CadCore::Simulation {

/**
 * @brief Material properties for structural analysis
 */
struct MaterialProperties {
    std::string name;
    double young_modulus = 0.0;        // Pa (Young's modulus)
    double poisson_ratio = 0.3;        // Poisson's ratio
    double yield_strength = 0.0;       // Pa
    double tensile_strength = 0.0;     // Pa
    double density = 0.0;              // kg/m^3
    double shear_modulus = 0.0;        // Pa (computed from E and nu if 0)

    void computeShearModulus() noexcept;
};

/**
 * @brief Constraint type for structural analysis
 */
enum class ConstraintType {
    FIXED,         // All DOFs fixed
    FIXED_X,       // X displacement fixed
    FIXED_Y,       // Y displacement fixed
    FIXED_Z,       // Z displacement fixed
    PINNED,        // X and Y fixed, Z free
    ROLLER,        // Normal direction fixed: needs a normal, solve() rejects it
};

/**
 * @brief Load type for structural analysis
 */
enum class LoadType {
    FORCE,          // Point force
    DISTRIBUTED,    // Distributed load
    PRESSURE,       // Pressure load
    THERMAL,        // Thermal load
    DISPLACEMENT,   // Displacement boundary condition
};

/**
 * @brief Constraint definition
 */
struct Constraint {
    ConstraintType type;
    size_t node_id;
    Vector3D displacement = Vector3D(0, 0, 0);  // Prescribed value of the fixed components
};

/**
 * @brief Load definition
 */
struct Load {
    // StructuralSolver supports FORCE only; other types make solve() return false.
    LoadType type;
    size_t node_id;
    Vector3D force = Vector3D(0, 0, 0);
    double pressure = 0.0;
    double temperature = 0.0;
    std::vector<size_t> element_indices;  // For distributed loads
};

/**
 * @brief Stress tensor (3x3 symmetric)
 */
class StressTensor {
public:
    StressTensor() noexcept = default;

    // Components: xx, yy, zz, xy, yz, xz
    std::array<double, 6> components{};

    [[nodiscard]] double xx() const noexcept { return components[0]; }
    [[nodiscard]] double yy() const noexcept { return components[1]; }
    [[nodiscard]] double zz() const noexcept { return components[2]; }
    [[nodiscard]] double xy() const noexcept { return components[3]; }
    [[nodiscard]] double yz() const noexcept { return components[4]; }
    [[nodiscard]] double xz() const noexcept { return components[5]; }

    [[nodiscard]] double vonMisesStress() const noexcept;
    [[nodiscard]] double principalStress1() const noexcept;
    [[nodiscard]] double principalStress2() const noexcept;
    [[nodiscard]] double principalStress3() const noexcept;
    // Eigenvalues of the tensor, sigma1 >= sigma2 >= sigma3.
    [[nodiscard]] std::array<double, 3> principalStresses() const noexcept;
};

/**
 * @brief Strain tensor (3x3 symmetric)
 */
class StrainTensor {
public:
    StrainTensor() noexcept = default;

    // Components: xx, yy, zz, xy, yz, xz
    std::array<double, 6> components{};

    [[nodiscard]] double xx() const noexcept { return components[0]; }
    [[nodiscard]] double yy() const noexcept { return components[1]; }
    [[nodiscard]] double zz() const noexcept { return components[2]; }
    [[nodiscard]] double xy() const noexcept { return components[3]; }
    [[nodiscard]] double yz() const noexcept { return components[4]; }
    [[nodiscard]] double xz() const noexcept { return components[5]; }

    [[nodiscard]] double equivalentStrain() const noexcept;
};

/**
 * @brief Structural analysis results at a point
 */
struct StructuralResults {
    size_t element_id;
    Vector3D displacement;
    StressTensor stress;
    StrainTensor strain;
    double safety_factor = 0.0;
};

/**
 * @brief Linear elastic static FEA on 4-node tetrahedra (docs/simulation/SIMULATION.md)
 *
 * Small strains, isotropic linear elasticity (E, nu), constant-strain tetrahedra
 * (K_e = V B^T D B), sparse LU solve of K u = F. Elements must have exactly 4 nodes;
 * loads are nodal forces; constraints prescribe displacement components
 * (Constraint::displacement). solve() returns false for other element types or load
 * types, ROLLER constraints, degenerate elements or an unrestrained mechanism.
 * Results per element: strain, stress (constant), von Mises, safety factor
 * yield_strength / von Mises (infinite for an unstressed element).
 */
class StructuralSolver {
public:
    StructuralSolver();
    ~StructuralSolver() = default;

    // Material
    void setMaterialProperties(const MaterialProperties& props);
    [[nodiscard]] const MaterialProperties& getMaterialProperties() const { return material_; }

    // Geometry
    void setNumNodes(size_t num_nodes);
    void setNumElements(size_t num_elements);
    void addNodeCoordinates(size_t node_id, const Vector3D& coords);
    void addElement(size_t elem_id, const std::vector<size_t>& node_indices);

    // Loads and constraints
    void addLoad(const Load& load);
    void addConstraint(const Constraint& constraint);

    // Solver execution
    bool solve();

    // Results retrieval
    [[nodiscard]] const std::vector<Vector3D>& getDisplacements() const { return displacements_; }
    [[nodiscard]] const std::vector<StructuralResults>& getElementResults() const { return element_results_; }
    [[nodiscard]] double getMaxStress() const { return max_stress_; }
    [[nodiscard]] double getMaxDisplacement() const { return max_displacement_; }
    [[nodiscard]] double getMinSafetyFactor() const { return min_safety_factor_; }
    [[nodiscard]] bool isConverged() const { return converged_; }

private:
    // Material properties
    MaterialProperties material_;

    // Geometry
    std::vector<Vector3D> node_coordinates_;
    std::vector<std::vector<size_t>> elements_;
    size_t num_nodes_ = 0;
    size_t num_elements_ = 0;

    // Loads and constraints
    std::vector<Load> loads_;
    std::vector<Constraint> constraints_;

    // Solution fields
    std::vector<Vector3D> displacements_;
    std::vector<StructuralResults> element_results_;

    // Convergence tracking
    bool converged_ = false;
    double max_stress_ = 0.0;
    double max_displacement_ = 0.0;
    double min_safety_factor_ = 0.0;

};

}  // namespace BendCore::CadCore::Simulation
