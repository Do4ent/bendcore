#pragma once

#include <string>
#include <vector>
#include <optional>
#include <map>
#include <cmath>

namespace bendcore::manufacturing {

struct BendOperation {
    unsigned int sequenceNumber;
    double bendAngle;           // degrees
    double bendRadius;          // mm
    double toolPosition;        // mm along tube
    std::string bendType;       // "free", "air", "bottom", "wiping"
    double toolSpeed;           // mm/s
    unsigned int clampForce;    // N
    std::string notes;
};

struct CutOperation {
    unsigned int sequenceNumber;
    double cutPosition;         // mm
    double cutLength;           // mm
    std::string cutType;        // "straight", "miter", "bevel"
    double toolSpeed;           // mm/s
};

struct ManufacturingSheet {
    std::string partNumber;
    std::string partName;
    double materialCost;        // currency
    double laborCost;           // currency
    double machineTime;         // minutes
    std::vector<BendOperation> bendOperations;
    std::vector<CutOperation> cutOperations;
    std::map<std::string, unsigned int> toolsRequired;
    std::string qualityNotes;
};

class NCCodeGenerator {
public:
    static std::string generateBendingCode(const BendOperation& op);
    static std::string generateCuttingCode(const CutOperation& op);
    static std::string generateCompleteProgram(const ManufacturingSheet& sheet);
};

class TubeUnroller {
public:
    struct UnrollResult {
        double unfoldedLength;
        std::vector<double> bendMarks;  // positions of bends in unfolded state
        std::vector<double> bendAngles;
        double totalLengthNeeded;
    };

    static UnrollResult calculateUnfolding(
        double tubeDiameter,
        double wallThickness,
        const std::vector<double>& bendAngles,
        const std::vector<double>& bendRadii
    );
};

class ManufacturingCostCalculator {
public:
    struct CostBreakdown {
        double materialCost;
        double cuttingCost;
        double bendingCost;
        double assemblingCost;
        double qualityCheckCost;
        double packagingCost;
        double totalCost;
    };

    static CostBreakdown calculateCost(
        double tubeLength,
        double outerDiameter,
        double wallThickness,
        double materialPricePerKg,
        const std::vector<BendOperation>& bends,
        const std::vector<CutOperation>& cuts
    );
};

class CollisionChecker {
public:
    struct CollisionReport {
        bool hasCollisions;
        std::vector<std::string> collisionLocations;
        std::vector<double> collisionPositions;  // mm along tube
        std::string recommendation;
    };

    static CollisionReport checkMachineCollisions(
        const ManufacturingSheet& sheet,
        double machineWorkspaceX,
        double machineWorkspaceY,
        double machineWorkspaceZ
    );
};

} // namespace bendcore::manufacturing
