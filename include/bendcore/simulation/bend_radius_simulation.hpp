// Bend Radius Simulation - Bend Compensation and Spring-Back Analysis
// M12 Phase: Bend Radius Simulation (GAP-051)
#pragma once

#include <vector>
#include <cmath>
#include <optional>
#include <string>
#include <memory>

namespace bendcore::simulation {

// Material class for bend analysis
enum class MaterialType {
    Steel,
    Aluminum,
    Copper,
    Titanium,
    Stainless
};

// Bend result data structure
struct BendResult {
    double required_bend_radius{0.0};           // mm
    double spring_back_angle{0.0};              // degrees
    double material_stress{0.0};                // MPa
    double strain{0.0};                         // unitless
    bool is_feasible{true};
    std::string failure_reason;
};

// Material bend properties
struct BendMaterialProperties {
    MaterialType material_type;
    double young_modulus{200e9};                // Pa
    double yield_strength{250e6};               // Pa
    double thickness{1.0};                      // mm
    double width{10.0};                         // mm
    double max_bend_radius_ratio{0.1};          // R/t ratio limit
    double k_factor{0.5};                       // Position of neutral axis
    std::string material_name;
};

// Bend parameters
struct BendParameters {
    double bend_angle{90.0};                    // degrees
    double tool_radius{5.0};                    // mm
    double material_thickness{1.0};             // mm
    double bend_direction{0.0};                 // degrees (0=X, 90=Y)
    double forming_speed{50.0};                 // mm/min
    bool is_air_bend{false};
    bool compensate_spring_back{true};
};

// Bend configuration
struct BendConfiguration {
    BendMaterialProperties material;
    BendParameters parameters;
    double target_bend_radius{10.0};            // mm
};

// Main bend radius simulation class
class BendRadiusSimulation {
public:
    BendRadiusSimulation() = default;

    // Configuration
    void setMaterialProperties(const BendMaterialProperties& props) noexcept {
        material_ = props;
    }

    void setBendParameters(const BendParameters& params) noexcept {
        parameters_ = params;
    }

    // Main simulation
    [[nodiscard]] BendResult simulateBend(double bend_angle, double tool_radius) noexcept;

    [[nodiscard]] BendResult simulateBend(const BendConfiguration& config) noexcept;

    // Get minimum required radius
    [[nodiscard]] double getMinimumBendRadius() const noexcept {
        return material_.thickness * material_.max_bend_radius_ratio;
    }

    // Calculate spring-back
    [[nodiscard]] double calculateSpringBack(double bend_angle, double bend_radius) const noexcept;

    // Calculate strain during bending
    [[nodiscard]] double calculateBendStrain(double bend_radius) const noexcept;

    // Calculate stress during bending
    [[nodiscard]] double calculateBendStress(double bend_radius) const noexcept;

    // Determine if material will fracture
    [[nodiscard]] bool checkFracture(double strain) const noexcept {
        // Typical fracture strain for metals
        double fracture_limit = 0.2;  // 20% for steel
        return strain > fracture_limit;
    }

    // Get compensated bend angle (considering spring-back)
    [[nodiscard]] double getCompensatedBendAngle(double target_angle, double bend_radius) const noexcept;

    // Multiple bend simulation
    [[nodiscard]] std::vector<BendResult> simulateSequentialBends(
        const std::vector<BendConfiguration>& configurations) noexcept;

    // Get material properties
    [[nodiscard]] const BendMaterialProperties& getMaterialProperties() const noexcept {
        return material_;
    }

    [[nodiscard]] const BendParameters& getBendParameters() const noexcept {
        return parameters_;
    }

    // Bend factor calculation (for air bending)
    [[nodiscard]] double calculateBendFactor(double tool_radius) const noexcept;

    // K-factor for neutral axis position
    void setKFactor(double k) noexcept {
        material_.k_factor = std::max(0.2, std::min(0.8, k));
    }

    [[nodiscard]] double getKFactor() const noexcept {
        return material_.k_factor;
    }

    // Energy calculation during bending
    [[nodiscard]] double calculateBendingEnergy(double bend_angle, double bend_radius) const noexcept;

private:
    BendMaterialProperties material_;
    BendParameters parameters_;

    // Helper functions
    [[nodiscard]] double radiansFromDegrees(double degrees) const noexcept {
        return degrees * 3.14159265359 / 180.0;
    }

    [[nodiscard]] double degreesFromRadians(double radians) const noexcept {
        return radians * 180.0 / 3.14159265359;
    }
};

}  // namespace bendcore::simulation
