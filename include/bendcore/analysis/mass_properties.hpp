#pragma once

#include "../geometry/vector.hpp"
#include "../document/entity_id.hpp"
#include <map>
#include <vector>
#include <memory>
#include <string>
#include <optional>
#include <array>

namespace BendCore::CadCore {

/**
 * @brief 3x3 Matrix for inertia tensor calculations
 */
class Matrix3x3 {
public:
    // Elements stored as [row][col]
    std::array<std::array<double, 3>, 3> data;

    Matrix3x3() noexcept {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                data[i][j] = 0.0;
            }
        }
    }

    [[nodiscard]] double get(int row, int col) const noexcept {
        return data[row][col];
    }

    void set(int row, int col, double value) noexcept {
        data[row][col] = value;
    }

    Matrix3x3 operator+(const Matrix3x3& other) const noexcept {
        Matrix3x3 result;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                result.data[i][j] = data[i][j] + other.data[i][j];
            }
        }
        return result;
    }

    Vector3D operator*(const Vector3D& v) const noexcept {
        return Vector3D(
            data[0][0] * v.x + data[0][1] * v.y + data[0][2] * v.z,
            data[1][0] * v.x + data[1][1] * v.y + data[1][2] * v.z,
            data[2][0] * v.x + data[2][1] * v.y + data[2][2] * v.z
        );
    }
};

/**
 * @brief Material properties
 */
struct Material {
    std::string name;
    std::string id;
    double density = 0.0;  // kg/m^3
    double cost_per_kg = 0.0;
    std::string color;

    Material() = default;
    Material(const std::string& name, double density)
        : name(name), density(density) {
        id = name;
    }
};

/**
 * @brief Mass properties of a single component
 */
struct ComponentMassData {
    EntityId component_id;
    std::string component_name;
    double mass = 0.0;  // kg
    double volume = 0.0;  // cubic units
    double surface_area = 0.0;  // square units
    Vector3D center_of_gravity;
    Material material;
    bool is_custom_mass = false;
};

/**
 * @brief Complete mass properties of an assembly
 */
struct MassProperties {
    double total_mass = 0.0;  // kg
    Vector3D center_of_gravity;
    Matrix3x3 inertia_tensor;
    double volume = 0.0;
    double surface_area = 0.0;

    // Principal axes and moments
    std::array<double, 3> principal_moments;  // Sorted: Ix, Iy, Iz
    std::array<Vector3D, 3> principal_axes;   // Normalized direction vectors
    std::array<double, 3> radius_of_gyration;

    // Bounding box
    Vector3D bbox_min;
    Vector3D bbox_max;

    // Material composition
    std::map<std::string, double> material_composition;  // material_name -> mass
};

/**
 * @brief Material library manager
 */
class MaterialLibrary {
public:
    /**
     * @brief Add material to library
     */
    void addMaterial(const Material& material);

    /**
     * @brief Get material by name
     */
    const Material* getMaterial(const std::string& name) const;

    /**
     * @brief Get all materials
     */
    [[nodiscard]] const std::vector<Material>& getMaterials() const;

    /**
     * @brief Remove material
     */
    bool removeMaterial(const std::string& name);

    /**
     * @brief Update density for material
     */
    bool updateDensity(const std::string& name, double density);

    /**
     * @brief Check if material exists
     */
    [[nodiscard]] bool hasMaterial(const std::string& name) const;

    /**
     * @brief Get default material library (Common metals)
     */
    static MaterialLibrary getDefaultLibrary();

private:
    std::map<std::string, Material> materials_;
};

/**
 * @brief Mass properties calculator for assemblies
 */
class MassPropertiesCalculator {
public:
    /**
     * @brief Constructor with material library
     */
    explicit MassPropertiesCalculator(const MaterialLibrary& library)
        : material_library_(library) {}

    /**
     * @brief Calculate mass properties for a component
     */
    ComponentMassData calculateComponentMass(
        const EntityId& component_id,
        const std::string& component_name,
        double volume,
        const std::string& material_name,
        const Vector3D& centroid = Vector3D(0, 0, 0)
    );

    /**
     * @brief Set custom mass for a component
     */
    void setComponentCustomMass(
        const EntityId& component_id,
        double mass,
        const Vector3D& cog = Vector3D(0, 0, 0)
    );

    /**
     * @brief Calculate total mass properties from component list
     */
    MassProperties calculateAssemblyMassProperties(
        const std::vector<ComponentMassData>& components,
        const Vector3D& reference_point = Vector3D(0, 0, 0)
    );

    /**
     * @brief Calculate inertia tensor about a point
     */
    Matrix3x3 calculateInertiaTensor(
        const std::vector<ComponentMassData>& components,
        const Vector3D& about_point
    );

    /**
     * @brief Calculate center of gravity
     */
    [[nodiscard]] Vector3D calculateCenterOfGravity(
        const std::vector<ComponentMassData>& components
    ) const;

    /**
     * @brief Calculate principal moments of inertia
     */
    std::array<double, 3> calculatePrincipalMoments(
        const Matrix3x3& inertia_tensor
    );

    /**
     * @brief Calculate radius of gyration
     */
    std::array<double, 3> calculateRadiusOfGyration(
        double total_mass,
        const std::array<double, 3>& principal_moments
    );

    /**
     * @brief Calculate bounding box from components
     */
    std::pair<Vector3D, Vector3D> calculateBoundingBox(
        const std::vector<ComponentMassData>& components
    ) const;

    /**
     * @brief Get material composition from components
     */
    std::map<std::string, double> getMaterialComposition(
        const std::vector<ComponentMassData>& components
    ) const;

private:
    const MaterialLibrary& material_library_;
    std::map<EntityId, ComponentMassData> component_cache_;

    /**
     * @brief Calculate moment of inertia for a point mass
     */
    Matrix3x3 calculatePointMassInertia(
        double mass,
        const Vector3D& relative_position
    ) const;

    /**
     * @brief Diagonalize inertia tensor to find principal axes
     */
    void diagonalizeInertia(
        const Matrix3x3& I,
        std::array<double, 3>& eigenvalues,
        std::array<Vector3D, 3>& eigenvectors
    );
};

/**
 * @brief Mass properties report generator
 */
class MassPropertiesReporter {
public:
    /**
     * @brief Generate text report
     */
    [[nodiscard]] std::string generateReport(
        const MassProperties& props,
        const std::string& title = "Mass Properties Report"
    ) const;

    /**
     * @brief Generate CSV export
     */
    [[nodiscard]] std::string generateCSV(
        const MassProperties& props,
        const std::vector<ComponentMassData>& components
    ) const;

    /**
     * @brief Generate JSON export
     */
    [[nodiscard]] std::string generateJSON(
        const MassProperties& props,
        const std::vector<ComponentMassData>& components
    ) const;

    /**
     * @brief Export report to file
     */
    bool exportToFile(
        const MassProperties& props,
        const std::string& filepath,
        const std::string& format = "json"
    ) const;

    /**
     * @brief Generate weight breakdown by component
     */
    [[nodiscard]] std::string generateWeightBreakdown(
        const std::vector<ComponentMassData>& components
    ) const;

    /**
     * @brief Generate weight breakdown by material
     */
    [[nodiscard]] std::string generateMaterialBreakdown(
        const MassProperties& props
    ) const;

    /**
     * @brief Generate balance analysis report
     */
    [[nodiscard]] std::string generateBalanceAnalysis(
        const MassProperties& props,
        const Vector3D& ideal_cog
    ) const;

private:
    [[nodiscard]] std::string formatDouble(double value, int precision = 6) const;
    [[nodiscard]] std::string formatVector(const Vector3D& v) const;
};

}  // namespace BendCore::CadCore
