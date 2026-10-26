#pragma once

#include <string>
#include <vector>
#include <optional>

namespace bendcore::bending {

// Bend operation specification
struct BendOperation {
    int operationId;
    double bendAngle;           // degrees
    double bendRadius;          // mm
    double bendPlaneRotation;   // degrees, for multiplane bends
    bool requiresHeating;       // For large deformations
    bool requiresStraightening; // Intermediate straightening needed
    int estimatedMachineTime;   // seconds
    std::string notes;
};

// Bending sequence specification
struct BendSequence {
    std::vector<BendOperation> operations;
    int estimatedTotalTime;     // seconds
    int estimatedSetupTime;     // seconds
    double estimatedCost;       // USD
    bool isOptimal;
    int numberOfIntermediateStraightening;
    int numberOfHeatTreatmentCycles;
};

// Sequence planning result
struct SequencePlan {
    BendSequence sequence;
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
    bool isApproved;
};

// Bend sequence planner
class BendSequencePlanner {
public:
    // Optimize bend order for a list of bends
    static SequencePlan optimizeBendSequence(
        const std::vector<BendOperation>& initialBends,
        double tubeOD,
        double wallThickness,
        const std::string& material
    );

    // Calculate optimal spacing for intermediate straightening
    static std::vector<int> calculateStraighteningPoints(
        const std::vector<BendOperation>& bends,
        const std::string& material
    );

    // Determine heat treatment requirements
    struct HeatTreatmentPlan {
        bool preHeating;           // Heat before bending
        bool intermediateHeating;  // Between bends
        int numberCycles;
        double temperatureCelsius; // Optimal temperature
        int dwellTimeSeconds;      // Time at temperature
    };

    static HeatTreatmentPlan calculateHeatTreatment(
        const std::vector<BendOperation>& bends,
        double tubeOD,
        double wallThickness,
        const std::string& material
    );

    // Calculate springback compensation
    static std::vector<double> calculateSpringbackCompensation(
        const std::vector<BendOperation>& bends,
        const std::string& material
    );

    // Generate setup sequence
    struct SetupStep {
        int stepNumber;
        std::string description;
        std::vector<std::string> toolsRequired;
        int estimatedDurationSeconds;
    };

    static std::vector<SetupStep> generateSetupSequence(
        const std::vector<BendOperation>& bends,
        const std::string& machineType
    );

    // Check for conflicts in bend sequence
    struct SequenceConflict {
        int bendId1;
        int bendId2;
        std::string description;
        std::string resolution;
    };

    static std::vector<SequenceConflict> checkSequenceConflicts(
        const std::vector<BendOperation>& bends
    );

    // Estimate dimensional changes due to springback
    struct DimensionalChange {
        double radiusChange;    // mm
        double angleChange;     // degrees
        double wallThicknessReduction;  // mm
    };

    static std::vector<DimensionalChange> estimateDimensionalChanges(
        const std::vector<BendOperation>& bends,
        const std::string& material
    );
};

// Manufacturing sequence specification
class ManufacturingSequenceGenerator {
public:
    // Generate complete manufacturing sequence
    struct ManufacturingStep {
        int stepNumber;
        std::string operationType;  // Straightening, Heating, Bending, Inspection
        std::string description;
        int durationSeconds;
        double estimatedCost;
        std::vector<std::string> qualityCheckpoints;
    };

    static std::vector<ManufacturingStep> generateCompleteSequence(
        const SequencePlan& plan,
        double tubeOD,
        double wallThickness,
        const std::string& material
    );

    // Generate inspection plan
    struct InspectionCheckpoint {
        int stepNumber;
        std::string parameter;      // Angle, Radius, WallThickness, Ovality
        double targetValue;
        double tolerance;          // +/- tolerance
        std::string measurementMethod;
    };

    static std::vector<InspectionCheckpoint> generateInspectionPlan(
        const std::vector<BendOperation>& bends
    );

    // Calculate production time and cost
    struct ProductionMetrics {
        int totalTimeSeconds;
        double totalCostUSD;
        double costPerBend;
        int efficiency;  // 0-100, percentage of productive time
    };

    static ProductionMetrics calculateProductionMetrics(
        const std::vector<ManufacturingStep>& steps
    );
};

} // namespace bendcore::bending
