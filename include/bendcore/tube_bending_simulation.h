#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cmath>

namespace bendcore::bending {

// Stress analysis result
struct StressAnalysis {
    double maxVonMisesStress;   // MPa
    double maxPrincipalStress;  // MPa
    double minPrincipalStress;  // MPa
    double averageStress;       // MPa
    double stressConcentrationFactor;  // Kt
    bool yielding;              // Whether material yields
    std::string stressDistribution;  // Description
};

// Strain analysis result
struct StrainAnalysis {
    double maxTotalStrain;      // %
    double maxPlasticStrain;    // %
    double maxElasticStrain;    // %
    double strainAtOuter;       // % at outer fiber
    double strainAtInner;       // % at inner fiber
    double maxShearStrain;      // %
    bool localization;          // Whether strain localizes
};

// Wall thickness analysis
struct WallThicknessAnalysis {
    double initialThickness;    // mm
    double finalThickness;      // mm
    double thinningPercentage;  // % reduction
    double maxThinningLocation; // mm from bend start
    bool crackingRisk;          // High thinning risk
    bool ovality;               // Cross-section becomes oval
    double maxOvality;          // %
};

// Material flow simulation
struct MaterialFlow {
    double circumferentialFlow;  // Amount of material movement
    double axialFlow;            // Along tube axis
    double radialFlow;           // Radial direction
    double totalDisplacement;    // Total material movement
    std::vector<double> flowProfile;  // Distribution along tube length
};

// Springback and recovery
struct SpringbackAnalysis {
    double springbackAngle;     // degrees
    double springbackRadius;    // mm change in radius
    double elasticRecoveryTime; // seconds to stabilize
    double totalRecovery;       // % recovery
    bool stabilized;            // Whether springback completed
    std::string compensationStrategy;  // How to compensate
};

// Complete bending simulation result
struct BendingSimulation {
    StressAnalysis stresses;
    StrainAnalysis strains;
    WallThicknessAnalysis wallThickness;
    MaterialFlow materialFlow;
    SpringbackAnalysis springback;

    bool isSuccessful;          // Bending can be done
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
    double qualityScore;        // 0-100
};

// Bending simulation engine
class BendingSimulator {
public:
    // Run complete bending simulation
    static BendingSimulation simulateBend(
        double tubeOD,              // mm
        double wallThickness,       // mm
        double bendAngle,           // degrees
        double bendRadius,          // mm
        const std::string& material // material type
    );

    // Analyze stress distribution
    static StressAnalysis analyzeStress(
        double tubeOD,
        double wallThickness,
        double bendRadius,
        double bendAngle,
        const std::string& material
    );

    // Analyze strain distribution
    static StrainAnalysis analyzeStrain(
        double tubeOD,
        double wallThickness,
        double bendRadius,
        double bendAngle,
        const std::string& material
    );

    // Predict wall thickness changes
    static WallThicknessAnalysis predictWallThicknessThinning(
        double tubeOD,
        double wallThickness,
        double bendRadius,
        double bendAngle
    );

    // Simulate material flow
    static MaterialFlow simulateMaterialFlow(
        double tubeOD,
        double wallThickness,
        double bendRadius,
        double bendAngle
    );

    // Predict springback
    static SpringbackAnalysis predictSpringback(
        double tubeOD,
        double wallThickness,
        double bendRadius,
        double bendAngle,
        const std::string& material
    );

    // Defect risk assessment
    struct DefectRisk {
        bool crackingRisk;
        double crackingProbability;  // 0-1
        bool wrinkling;
        double wrinklingProbability;
        bool ovality;
        double ovalityProbability;
        bool springback;
        double springbackProbability;
    };

    static DefectRisk assessDefectRisk(
        double tubeOD,
        double wallThickness,
        double bendRadius,
        double bendAngle,
        const std::string& material
    );
};

// Quality prediction
class QualityPredictor {
public:
    // Predict final part quality
    struct QualityPrediction {
        double expectedDimensionalAccuracy;  // 0-100
        double expectedSurfaceQuality;       // 0-100
        double expectedMechanicalProperties; // 0-100
        int estimatedYieldRate;              // % of parts meeting spec
        std::vector<std::string> expectedDefects;
    };

    static QualityPrediction predictQuality(
        const BendingSimulation& simulation,
        double toleranceLevel  // 0-tight, 0.5-normal, 1-loose
    );

    // Calculate cumulative quality degradation
    static double calculateQualityDegradation(
        const std::vector<BendingSimulation>& bendSequence
    );
};

} // namespace bendcore::bending
