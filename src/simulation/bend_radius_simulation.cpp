#include "bendcore/simulation/bend_radius_simulation.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace bendcore::simulation {

BendResult BendRadiusSimulation::simulateBend(double bend_angle, double tool_radius) noexcept {
    BendResult result;

    // Calculate bend radius
    double bend_radius = tool_radius + (material_.thickness / 2.0);
    result.required_bend_radius = bend_radius;

    // Check if radius is achievable
    double min_radius = getMinimumBendRadius();
    if (bend_radius < min_radius) {
        result.is_feasible = false;
        result.failure_reason = "Tool radius too small; minimum R/t ratio exceeded";
        return result;
    }

    // Calculate strain
    double angle_rad = radiansFromDegrees(bend_angle);
    double inner_radius = tool_radius;
    double outer_radius = tool_radius + material_.thickness;

    // Strain at outer fiber: ε = (R + t) * θ / R
    result.strain = (outer_radius * angle_rad - inner_radius * angle_rad) / inner_radius;

    // Check for fracture
    if (checkFracture(result.strain)) {
        result.is_feasible = false;
        result.failure_reason = "Material fracture limit exceeded";
        return result;
    }

    // Calculate stress
    result.material_stress = calculateBendStress(bend_radius);

    if (result.material_stress > material_.yield_strength / 1e6) {  // Convert to MPa
        result.is_feasible = false;
        result.failure_reason = "Yield strength exceeded";
        return result;
    }

    // Calculate spring-back
    result.spring_back_angle = calculateSpringBack(bend_angle, bend_radius);

    return result;
}

BendResult BendRadiusSimulation::simulateBend(const BendConfiguration& config) noexcept {
    setMaterialProperties(config.material);
    setBendParameters(config.parameters);
    return simulateBend(config.parameters.bend_angle, config.parameters.tool_radius);
}

double BendRadiusSimulation::calculateSpringBack(double bend_angle, double bend_radius) const noexcept {
    // Spring-back angle ≈ (θ * σy) / (E * R/t)
    // Simplified: spring-back ≈ 0.1 to 0.3 * bend_angle

    double stress_factor = material_.yield_strength / material_.young_modulus;
    double radius_ratio = bend_radius / material_.thickness;

    if (radius_ratio < 1e-12) return 0.0;

    double spring_back_ratio = stress_factor / radius_ratio;
    spring_back_ratio = std::max(0.0, std::min(0.4, spring_back_ratio));

    return bend_angle * spring_back_ratio;
}

double BendRadiusSimulation::calculateBendStrain(double bend_radius) const noexcept {
    // Maximum strain at outer surface during bending
    // ε = t / (2*R)
    if (bend_radius < 1e-12) return 0.0;
    return material_.thickness / (2.0 * bend_radius);
}

double BendRadiusSimulation::calculateBendStress(double bend_radius) const noexcept {
    // Bending stress: σ = E * ε = E * t / (2*R)
    double strain = calculateBendStrain(bend_radius);
    double stress_pa = material_.young_modulus * strain;

    // Convert to MPa
    return stress_pa / 1e6;
}

double BendRadiusSimulation::getCompensatedBendAngle(double target_angle, double bend_radius) const noexcept {
    double spring_back = calculateSpringBack(target_angle, bend_radius);
    return target_angle + spring_back;
}

std::vector<BendResult> BendRadiusSimulation::simulateSequentialBends(
    const std::vector<BendConfiguration>& configurations) noexcept {
    std::vector<BendResult> results;
    results.reserve(configurations.size());

    for (const auto& config : configurations) {
        results.push_back(simulateBend(config));
    }

    return results;
}

double BendRadiusSimulation::calculateBendFactor(double tool_radius) const noexcept {
    // Bend factor relates to the position of neutral axis
    // For air bending: BF ≈ 1.3 to 2.0
    double thickness = material_.thickness;
    double ratio = tool_radius / thickness;

    if (ratio < 1.0) return 1.3;
    if (ratio < 2.0) return 1.5;
    if (ratio < 5.0) return 1.8;
    return 2.0;
}

double BendRadiusSimulation::calculateBendingEnergy(double bend_angle, double bend_radius) const noexcept {
    // Elastic bending energy U = (1/2) * M * θ
    // Moment M = E*I / R

    double angle_rad = radiansFromDegrees(bend_angle);
    double e = material_.young_modulus;

    // Simplified moment of inertia for rectangular section
    // I = (b*t³) / 12
    double moment_of_inertia = (material_.width * material_.thickness * material_.thickness * material_.thickness) / 12.0;

    if (bend_radius < 1e-12) return 0.0;

    double moment = (e * moment_of_inertia) / bend_radius;
    double energy = 0.5 * moment * angle_rad;

    return energy;
}

}  // namespace bendcore::simulation
