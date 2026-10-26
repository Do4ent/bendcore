#pragma once

#include <string>
#include <map>
#include <vector>
#include <optional>
#include <cmath>

namespace bendcore::bending {

// Machine types for tube bending
enum class MachineType {
    ROTARY_DRAW,        // Rotary draw bending (most common, best quality)
    PUSH_BENDING,       // Push bending (horizontal, simple)
    COMPRESSION,        // Compression bending (for large radii)
    ROLL_BENDING,       // Roll bending (continuous, for complex shapes)
    MANDREL_BENDING,    // Mandrel bending (precision, thin walls)
    INDUCTION_BENDING   // Induction bending (with heating)
};

// Tooling specification
struct Tooling {
    std::string name;
    double dieRadius;           // mm, radius of the die
    double dieOpeningWidth;     // mm
    double mandrelDiameter;     // mm, 0 if not used
    double clamperForce;        // N, clamping force
    double minTubeOD;          // mm
    double maxTubeOD;          // mm
    bool hasExpander;          // Whether tooling has expander
    bool hasFollower;          // Whether tooling has pressure follower
    double toolWeight;         // kg
    double cost;               // USD
};

// Bending machine specification
struct BendingMachine {
    std::string name;
    MachineType type;

    // Capacity
    double maxTubeOD;           // mm
    double minTubeOD;           // mm
    double maxWallThickness;    // mm
    double minWallThickness;    // mm

    // Performance
    double maxBendRadius;       // mm
    double minBendRadius;       // mm
    double maxBendForce;        // N
    double bendSpeed;           // mm/min
    double cycleTime;           // seconds

    // Precision
    double angleTolerance;      // degrees
    double radiusTolerance;     // mm
    double springbackCompensation;  // 0-1, auto-compensation factor

    // Capabilities
    bool supportsMultiplane;    // Can do bends in different planes
    bool supportsInduction;     // Has induction heating
    bool supportsPressureFollower;  // Can use pressure follower
    double maxConsecutiveBends; // Maximum bends without intermediate straightening

    // Cost metrics
    double hourlyRate;          // USD/hour
    double setupTime;           // minutes

    // Available tooling
    std::vector<Tooling> availableTooling;
};

// Machine capacity check result
struct MachineCapacityCheck {
    bool isCapable;
    std::string reason;
    double estimatedCycleTime;  // seconds
    double estimatedTotalTime;  // seconds (with setup)
    double estimatedCost;       // USD
    double toolingSuitability;  // 0-1, how well tooling matches
};

// Machine library for standard tube bending machines
class MachineLibrary {
public:
    // Get standard machines
    static BendingMachine getRotaryDrawStandard();
    static BendingMachine getPushBendingStandard();
    static BendingMachine getCompressionStandard();
    static BendingMachine getRollBendingStandard();
    static BendingMachine getMandrelBendingStandard();
    static BendingMachine getInductionBendingStandard();

    // Get machine by name
    static std::optional<BendingMachine> getMachineByName(const std::string& name);

    // Get all machines of a specific type
    static std::vector<BendingMachine> getMachinesByType(MachineType type);
};

// Machine selector
class MachineSelector {
public:
    // Select best machine for given tube parameters
    static std::optional<BendingMachine> selectBestMachine(
        double tubeOD,              // mm
        double wallThickness,       // mm
        double bendRadius,          // mm
        const std::string& material // material type
    );

    // Check if machine can handle given parameters
    static MachineCapacityCheck checkMachineCapacity(
        const BendingMachine& machine,
        double tubeOD,
        double wallThickness,
        double bendRadius,
        double bendForce,
        int numberOfBends
    );

    // Recommend machines sorted by suitability
    static std::vector<std::pair<BendingMachine, double>>
    recommendMachines(
        double tubeOD,
        double wallThickness,
        double bendRadius,
        const std::string& material,
        int maxRecommendations = 3
    );
};

// Tooling selector
class ToolingSelector {
public:
    // Select best tooling for given parameters
    static std::optional<Tooling> selectBestTooling(
        const BendingMachine& machine,
        double tubeOD,
        double wallThickness,
        double bendRadius
    );

    // Get all suitable tooling options
    static std::vector<std::pair<Tooling, double>>
    getSuitableTooling(
        const BendingMachine& machine,
        double tubeOD,
        double wallThickness,
        double bendRadius
    );

    // Calculate optimal tooling configuration
    struct ToolingConfig {
        Tooling die;
        std::optional<Tooling> mandrel;
        std::optional<Tooling> follower;
        double totalForce;  // N
        bool isOptimal;
    };

    static std::optional<ToolingConfig> optimizeToolingConfiguration(
        const BendingMachine& machine,
        double tubeOD,
        double wallThickness,
        double bendRadius,
        const std::string& material
    );
};

} // namespace bendcore::bending
