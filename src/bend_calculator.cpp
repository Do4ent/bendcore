#include "bendcore/bend_calculator.h"
#include <algorithm>
#include <cmath>

namespace bendcore::bending {

// Material database implementations
MaterialProperties MaterialDatabase::getSteel() {
    return {
        "Steel",
        210000,     // Young's modulus
        250,        // Yield strength
        400,        // Tensile strength
        7.85,       // Density
        25.0,       // Elongation
        0.15,       // Springback factor
        0.2         // Min bend radius (t/OD ratio)
    };
}

MaterialProperties MaterialDatabase::getAluminum() {
    return {
        "Aluminum",
        70000,
        240,
        290,
        2.70,
        18.0,
        0.25,
        0.15
    };
}

MaterialProperties MaterialDatabase::getCopper() {
    return {
        "Copper",
        130000,
        200,
        220,
        8.96,
        35.0,
        0.10,
        0.10
    };
}

MaterialProperties MaterialDatabase::getTitanium() {
    return {
        "Titanium",
        103000,
        880,
        930,
        4.51,
        18.0,
        0.20,
        0.25
    };
}

std::optional<MaterialProperties> MaterialDatabase::getMaterial(const std::string& name) {
    if (name == "steel" || name == "Steel") return getSteel();
    if (name == "aluminum" || name == "Aluminum") return getAluminum();
    if (name == "copper" || name == "Copper") return getCopper();
    if (name == "titanium" || name == "Titanium") return getTitanium();
    return std::nullopt;
}

BendResult BendCalculator::calculateBend(
    double outerDiameter,
    double wallThickness,
    double bendAngle,
    const std::string& material,
    double bendRadius)
{
    BendResult result{};

    auto matProps = MaterialDatabase::getMaterial(material);
    if (!matProps) {
        result.isFeasible = false;
        result.warningMessage = "Unknown material: " + material;
        return result;
    }

    double innerDiameter = outerDiameter - 2.0 * wallThickness;

    // Calculate min/max bend radii
    result.minAllowedRadius = calculateMinBendRadius(outerDiameter, wallThickness, matProps.value());
    result.maxAllowedRadius = calculateMaxBendRadius(outerDiameter, wallThickness, matProps.value());

    // Use auto-calculated radius if not provided
    if (bendRadius < 0) {
        bendRadius = outerDiameter * 1.5;  // Default: 1.5 × OD
    }

    result.bendRadius = bendRadius;
    result.bendDiameter = 2.0 * bendRadius;

    // Check feasibility
    result.isFeasible = (bendRadius >= result.minAllowedRadius) &&
                       (bendRadius <= result.maxAllowedRadius);

    if (!result.isFeasible) {
        result.warningMessage = "Bend radius outside acceptable range";
        return result;
    }

    // Calculate forces and stresses
    result.bendForce = calculateBendForce(outerDiameter, wallThickness,
                                         bendRadius, matProps.value());
    result.bendStress = calculateBendStress(outerDiameter, innerDiameter,
                                           bendRadius, matProps.value());

    // Calculate springback
    result.springbackAngle = calculateSpringback(bendAngle, bendRadius,
                                               outerDiameter, matProps.value());

    // Calculate strains
    result.strainAtOuter = calculateStrainAtFiber(bendRadius, wallThickness, outerDiameter);
    result.strainAtInner = calculateStrainAtFiber(bendRadius, -wallThickness, outerDiameter);

    // Warnings
    if (result.bendStress > matProps->yieldStrength) {
        result.warningMessage = "Stress exceeds yield strength!";
        result.isFeasible = false;
    }

    return result;
}

double BendCalculator::calculateMinBendRadius(double outerDiameter,
                                             double /* wallThickness */,
                                             const MaterialProperties& mat)
{
    return outerDiameter * mat.minBendRadius;
}

double BendCalculator::calculateMaxBendRadius(double outerDiameter,
                                             double /* wallThickness */,
                                             const MaterialProperties& /* mat */)
{
    // Max radius typically 5-10 times the outer diameter
    return outerDiameter * 8.0;
}

double BendCalculator::calculateBendForce(double outerDiameter,
                                         double wallThickness,
                                         double /* bendRadius */,
                                         const MaterialProperties& mat)
{
    // Simplified bending force calculation using beam theory
    // F = (M * 32) / (π * d^3) where M is bending moment
    // This is a simplified model; production code would use FEA

    double innerDiameter = outerDiameter - 2.0 * wallThickness;
    double sectionModulus = (M_PI / 32.0) *
                           (std::pow(outerDiameter, 4) - std::pow(innerDiameter, 4)) /
                           (outerDiameter / 2.0);

    double bendingMoment = mat.yieldStrength * sectionModulus * 0.5;  // 50% of max
    double force = (bendingMoment * 32.0) / (M_PI * std::pow(outerDiameter, 3));

    return force;
}

double BendCalculator::calculateBendStress(double outerDiameter,
                                          double /* innerDiameter */,
                                          double bendRadius,
                                          const MaterialProperties& mat)
{
    // Stress = E * strain, where strain depends on fiber distance from neutral axis
    double radius = outerDiameter / 2.0;
    double strain = (radius / (bendRadius + radius)) * 100.0;  // At outer fiber
    double stress = (mat.youngModulus * strain) / 100.0;

    return stress;
}

double BendCalculator::calculateSpringback(double bendAngle,
                                          double bendRadius,
                                          double outerDiameter,
                                          const MaterialProperties& mat)
{
    // Springback angle = initial angle * springback factor
    // This is a simplified model
    double springback = bendAngle * mat.springbackFactor;

    // Also depends on radius/diameter ratio
    double radiusFactor = (bendRadius / outerDiameter);
    springback *= std::log(radiusFactor + 1.0) / std::log(2.0);

    return springback;
}

double BendCalculator::calculateStrainAtFiber(double bendRadius,
                                             double fiberDistance,
                                             double /* outerDiameter */)
{
    // Strain = distance / radius of neutral axis
    // Neutral axis is typically at center for tubes
    if (bendRadius <= 0) return 0.0;

    double strain = fiberDistance / (bendRadius + fiberDistance / 2.0);
    return strain * 100.0;  // As percentage
}

} // namespace bendcore::bending
