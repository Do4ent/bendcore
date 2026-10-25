#pragma once

#include <string>
#include <map>
#include <vector>
#include <optional>
#include <cmath>

namespace bendcore::bending {

// Material properties database
struct MaterialProperties {
    std::string name;
    double youngModulus;        // MPa
    double yieldStrength;       // MPa
    double tensileStrength;     // MPa
    double density;             // g/cm³
    double elongationAtBreak;   // %
    double springbackFactor;    // Typical springback (0.0-1.0)
    double minBendRadius;       // Minimum relative radius (t/OD)
};

class MaterialDatabase {
public:
    static MaterialProperties getSteel();
    static MaterialProperties getAluminum();
    static MaterialProperties getCopper();
    static MaterialProperties getTitanium();
    static std::optional<MaterialProperties> getMaterial(const std::string& name);
};

// Bend calculation results
struct BendResult {
    double bendRadius;                  // mm
    double bendDiameter;                // mm
    double minAllowedRadius;            // mm
    double maxAllowedRadius;            // mm
    double bendForce;                   // N
    double bendStress;                  // MPa
    double springbackAngle;             // degrees
    double strainAtOuter;               // %
    double strainAtInner;               // %
    bool isFeasible;                    // Can material be bent with this radius?
    std::string warningMessage;         // Warnings/recommendations
};

class BendCalculator {
public:
    // Main calculation function
    static BendResult calculateBend(
        double outerDiameter,           // mm
        double wallThickness,           // mm
        double bendAngle,               // degrees
        const std::string& material,    // material name
        double bendRadius = -1.0        // target bend radius, auto-calculate if negative
    );

    // Component calculations
    static double calculateMinBendRadius(double outerDiameter,
                                        double wallThickness,
                                        const MaterialProperties& mat);

    static double calculateMaxBendRadius(double outerDiameter,
                                        double wallThickness,
                                        const MaterialProperties& mat);

    static double calculateBendForce(double outerDiameter,
                                    double wallThickness,
                                    double bendRadius,
                                    const MaterialProperties& mat);

    static double calculateBendStress(double outerDiameter,
                                     double innerDiameter,
                                     double bendRadius,
                                     const MaterialProperties& mat);

    static double calculateSpringback(double bendAngle,
                                     double bendRadius,
                                     double outerDiameter,
                                     const MaterialProperties& mat);

    static double calculateStrainAtFiber(double bendRadius,
                                        double fiberDistance,
                                        double outerDiameter);
};

} // namespace bendcore::bending
