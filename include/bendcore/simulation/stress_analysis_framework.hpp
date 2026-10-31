// Stress Analysis Framework - Comprehensive Stress and Deformation Calculations
// M12 Phase: Stress Analysis (GAP-050)
#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <optional>
#include <memory>

namespace bendcore::simulation {

// 3x3 Stress tensor representation
class StressTensor {
public:
    StressTensor() : components_({0.0, 0.0, 0.0, 0.0, 0.0, 0.0}) {}

    explicit StressTensor(double xx, double yy, double zz, double xy = 0.0, double yz = 0.0, double xz = 0.0)
        : components_({xx, yy, zz, xy, yz, xz}) {}

    [[nodiscard]] double xx() const noexcept { return components_[0]; }
    [[nodiscard]] double yy() const noexcept { return components_[1]; }
    [[nodiscard]] double zz() const noexcept { return components_[2]; }
    [[nodiscard]] double xy() const noexcept { return components_[3]; }
    [[nodiscard]] double yz() const noexcept { return components_[4]; }
    [[nodiscard]] double xz() const noexcept { return components_[5]; }

    void set_xx(double val) noexcept { components_[0] = val; }
    void set_yy(double val) noexcept { components_[1] = val; }
    void set_zz(double val) noexcept { components_[2] = val; }
    void set_xy(double val) noexcept { components_[3] = val; }
    void set_yz(double val) noexcept { components_[4] = val; }
    void set_xz(double val) noexcept { components_[5] = val; }

    [[nodiscard]] double vonMisesStress() const noexcept {
        double diff_xy = xx() - yy();
        double diff_yz = yy() - zz();
        double diff_zx = zz() - xx();
        double tau_sq = xy() * xy() + yz() * yz() + xz() * xz();
        return std::sqrt((diff_xy * diff_xy + diff_yz * diff_yz + diff_zx * diff_zx + 6.0 * tau_sq) / 2.0);
    }

    [[nodiscard]] double maxPrincipalStress() const noexcept {
        return std::max({std::abs(xx()), std::abs(yy()), std::abs(zz())});
    }

    [[nodiscard]] double meanStress() const noexcept {
        return (xx() + yy() + zz()) / 3.0;
    }

    [[nodiscard]] double deviatoricStress() const noexcept {
        double mean = meanStress();
        double s_xx = xx() - mean;
        double s_yy = yy() - mean;
        double s_zz = zz() - mean;
        double s_sq = s_xx * s_xx + s_yy * s_yy + s_zz * s_zz;
        double tau_sq = xy() * xy() + yz() * yz() + xz() * xz();
        return std::sqrt(s_sq + 2.0 * tau_sq);
    }

private:
    std::array<double, 6> components_;
};

// Material properties for stress analysis
struct MaterialProperties {
    double young_modulus{200e9};        // Pa (steel default)
    double poisson_ratio{0.3};
    double yield_strength{250e6};       // Pa
    double density{7850.0};             // kg/m³
    double thermal_expansion{12e-6};    // 1/K
    double thermal_conductivity{50.0};  // W/(m·K)
};

// Geometric properties for analysis
struct GeometricProperties {
    double length{1.0};                 // meters
    double cross_section_area{0.01};    // m²
    double moment_of_inertia{1e-5};     // m⁴
    double torsional_constant{1e-5};    // m⁴
};

// Load case definition
struct LoadCase {
    std::string name;
    double axial_force{0.0};            // N
    double shear_force_y{0.0};          // N
    double shear_force_z{0.0};          // N
    double bending_moment_y{0.0};       // N·m
    double bending_moment_z{0.0};       // N·m
    double torsion{0.0};                // N·m
    double temperature_change{0.0};     // K
};

// Stress analysis result
struct StressAnalysisResult {
    StressTensor stress_tensor;
    double von_mises_equivalent{0.0};
    double max_principal_stress{0.0};
    double safety_factor{0.0};
    double deformation{0.0};            // meters
    double strain_energy{0.0};          // Joules
    bool is_safe{true};
};

// Main stress analysis framework
class StressAnalysisFramework {
public:
    StressAnalysisFramework() = default;

    // Material and geometry setters
    void setMaterialProperties(const MaterialProperties& props) noexcept {
        material_ = props;
    }

    void setGeometricProperties(const GeometricProperties& props) noexcept {
        geometry_ = props;
    }

    // Analysis methods
    [[nodiscard]] StressAnalysisResult analyzeLoadCase(const LoadCase& load) noexcept;

    [[nodiscard]] std::vector<StressAnalysisResult> analyzeMultipleLoadCases(
        const std::vector<LoadCase>& loads) noexcept;

    // Get getters
    [[nodiscard]] const MaterialProperties& getMaterialProperties() const noexcept { return material_; }
    [[nodiscard]] const GeometricProperties& getGeometricProperties() const noexcept { return geometry_; }

    // Stress calculations
    [[nodiscard]] double calculateAxialStress(double force) const noexcept {
        if (geometry_.cross_section_area < 1e-12) return 0.0;
        return force / geometry_.cross_section_area;
    }

    [[nodiscard]] double calculateBendingStress(double moment) const noexcept {
        if (geometry_.moment_of_inertia < 1e-12) return 0.0;
        double y = std::sqrt(geometry_.moment_of_inertia);  // approximate distance from neutral axis
        return moment * y / geometry_.moment_of_inertia;
    }

    [[nodiscard]] double calculateShearStress(double force) const noexcept {
        if (geometry_.cross_section_area < 1e-12) return 0.0;
        return force / geometry_.cross_section_area;
    }

    [[nodiscard]] double calculateDeformation(const LoadCase& load) const noexcept;

    [[nodiscard]] double calculateStrainEnergy(const StressTensor& stress) const noexcept;

    [[nodiscard]] double calculateSafetyFactor(const StressTensor& stress) const noexcept {
        double vm = stress.vonMisesStress();
        if (vm < 1e-12) return 1000.0;
        return material_.yield_strength / vm;
    }

private:
    MaterialProperties material_;
    GeometricProperties geometry_;
};

}  // namespace bendcore::simulation
