#include "bendcore/tube_bending_machine.h"
#include <algorithm>
#include <cmath>

namespace bendcore::bending {

// ============= Machine Library Implementation =============

BendingMachine MachineLibrary::getRotaryDrawStandard() {
    BendingMachine machine{
        "Rotary Draw Standard",
        MachineType::ROTARY_DRAW,

        // Capacity
        .maxTubeOD = 150.0,
        .minTubeOD = 10.0,
        .maxWallThickness = 10.0,
        .minWallThickness = 0.5,

        // Performance
        .maxBendRadius = 500.0,
        .minBendRadius = 15.0,
        .maxBendForce = 250000.0,  // 250 kN
        .bendSpeed = 100.0,
        .cycleTime = 15.0,

        // Precision
        .angleTolerance = 1.0,  // degrees
        .radiusTolerance = 2.0, // mm
        .springbackCompensation = 0.8,

        // Capabilities
        .supportsMultiplane = true,
        .supportsInduction = false,
        .supportsPressureFollower = true,
        .maxConsecutiveBends = 5,

        // Cost
        .hourlyRate = 150.0,
        .setupTime = 30.0,

        // Tooling
        .availableTooling = {}
    };

    // Add standard tooling for this machine
    machine.availableTooling.push_back(Tooling{
        "Die R20", 20.0, 30.0, 15.0, 5000.0, 12.0, 50.0, false, false, 2.0, 500.0
    });
    machine.availableTooling.push_back(Tooling{
        "Die R30", 30.0, 40.0, 22.0, 8000.0, 20.0, 80.0, true, true, 3.0, 700.0
    });
    machine.availableTooling.push_back(Tooling{
        "Die R50", 50.0, 60.0, 35.0, 15000.0, 40.0, 120.0, true, true, 5.0, 1000.0
    });

    return machine;
}

BendingMachine MachineLibrary::getPushBendingStandard() {
    BendingMachine machine{
        "Push Bending Standard",
        MachineType::PUSH_BENDING,

        // Capacity
        .maxTubeOD = 200.0,
        .minTubeOD = 20.0,
        .maxWallThickness = 15.0,
        .minWallThickness = 1.0,

        // Performance
        .maxBendRadius = 800.0,
        .minBendRadius = 30.0,
        .maxBendForce = 500000.0,  // 500 kN
        .bendSpeed = 80.0,
        .cycleTime = 20.0,

        // Precision
        .angleTolerance = 2.0,
        .radiusTolerance = 3.0,
        .springbackCompensation = 0.7,

        // Capabilities
        .supportsMultiplane = false,
        .supportsInduction = false,
        .supportsPressureFollower = false,
        .maxConsecutiveBends = 3,

        // Cost
        .hourlyRate = 100.0,
        .setupTime = 20.0,

        // Tooling
        .availableTooling = {}
    };

    machine.availableTooling.push_back(Tooling{
        "Die R25", 25.0, 35.0, 0.0, 10000.0, 20.0, 100.0, false, false, 4.0, 600.0
    });
    machine.availableTooling.push_back(Tooling{
        "Die R50", 50.0, 70.0, 0.0, 20000.0, 50.0, 150.0, false, false, 6.0, 800.0
    });

    return machine;
}

BendingMachine MachineLibrary::getCompressionStandard() {
    BendingMachine machine{
        "Compression Bending Standard",
        MachineType::COMPRESSION,

        // Capacity
        .maxTubeOD = 300.0,
        .minTubeOD = 50.0,
        .maxWallThickness = 20.0,
        .minWallThickness = 2.0,

        // Performance
        .maxBendRadius = 2000.0,  // Larger radii
        .minBendRadius = 50.0,
        .maxBendForce = 1000000.0,  // 1000 kN
        .bendSpeed = 50.0,
        .cycleTime = 30.0,

        // Precision
        .angleTolerance = 3.0,
        .radiusTolerance = 5.0,
        .springbackCompensation = 0.6,

        // Capabilities
        .supportsMultiplane = false,
        .supportsInduction = true,
        .supportsPressureFollower = false,
        .maxConsecutiveBends = 2,

        // Cost
        .hourlyRate = 120.0,
        .setupTime = 45.0,

        // Tooling
        .availableTooling = {}
    };

    machine.availableTooling.push_back(Tooling{
        "Roll R100", 100.0, 150.0, 0.0, 50000.0, 80.0, 200.0, false, false, 15.0, 2000.0
    });
    machine.availableTooling.push_back(Tooling{
        "Roll R200", 200.0, 300.0, 0.0, 100000.0, 150.0, 350.0, false, false, 30.0, 3500.0
    });

    return machine;
}

BendingMachine MachineLibrary::getRollBendingStandard() {
    BendingMachine machine{
        "Roll Bending Standard",
        MachineType::ROLL_BENDING,

        // Capacity
        .maxTubeOD = 500.0,
        .minTubeOD = 100.0,
        .maxWallThickness = 25.0,
        .minWallThickness = 3.0,

        // Performance
        .maxBendRadius = 5000.0,
        .minBendRadius = 100.0,
        .maxBendForce = 2000000.0,  // 2000 kN
        .bendSpeed = 20.0,
        .cycleTime = 60.0,

        // Precision
        .angleTolerance = 5.0,
        .radiusTolerance = 10.0,
        .springbackCompensation = 0.5,

        // Capabilities
        .supportsMultiplane = true,
        .supportsInduction = true,
        .supportsPressureFollower = false,
        .maxConsecutiveBends = 1,

        // Cost
        .hourlyRate = 200.0,
        .setupTime = 60.0,

        // Tooling
        .availableTooling = {}
    };

    machine.availableTooling.push_back(Tooling{
        "Roll Smooth", 300.0, 400.0, 0.0, 200000.0, 200.0, 500.0, false, false, 50.0, 5000.0
    });

    return machine;
}

BendingMachine MachineLibrary::getMandrelBendingStandard() {
    BendingMachine machine{
        "Mandrel Bending Standard",
        MachineType::MANDREL_BENDING,

        // Capacity
        .maxTubeOD = 80.0,
        .minTubeOD = 8.0,
        .maxWallThickness = 3.0,
        .minWallThickness = 0.3,

        // Performance
        .maxBendRadius = 300.0,
        .minBendRadius = 8.0,
        .maxBendForce = 100000.0,  // 100 kN
        .bendSpeed = 150.0,
        .cycleTime = 10.0,

        // Precision
        .angleTolerance = 0.5,  // Very tight tolerance
        .radiusTolerance = 1.0,
        .springbackCompensation = 0.9,

        // Capabilities
        .supportsMultiplane = true,
        .supportsInduction = false,
        .supportsPressureFollower = true,
        .maxConsecutiveBends = 8,

        // Cost
        .hourlyRate = 180.0,
        .setupTime = 25.0,

        // Tooling
        .availableTooling = {}
    };

    machine.availableTooling.push_back(Tooling{
        "Mandrel R8", 8.0, 12.0, 6.0, 2000.0, 6.0, 20.0, true, true, 0.5, 300.0
    });
    machine.availableTooling.push_back(Tooling{
        "Mandrel R15", 15.0, 20.0, 12.0, 4000.0, 12.0, 40.0, true, true, 1.0, 400.0
    });
    machine.availableTooling.push_back(Tooling{
        "Mandrel R25", 25.0, 35.0, 20.0, 8000.0, 25.0, 70.0, true, true, 2.0, 600.0
    });

    return machine;
}

BendingMachine MachineLibrary::getInductionBendingStandard() {
    BendingMachine machine{
        "Induction Bending Standard",
        MachineType::INDUCTION_BENDING,

        // Capacity
        .maxTubeOD = 400.0,
        .minTubeOD = 50.0,
        .maxWallThickness = 20.0,
        .minWallThickness = 2.0,

        // Performance
        .maxBendRadius = 3000.0,
        .minBendRadius = 50.0,
        .maxBendForce = 1500000.0,  // 1500 kN
        .bendSpeed = 30.0,
        .cycleTime = 45.0,

        // Precision
        .angleTolerance = 2.0,
        .radiusTolerance = 3.0,
        .springbackCompensation = 0.85,

        // Capabilities
        .supportsMultiplane = true,
        .supportsInduction = true,
        .supportsPressureFollower = true,
        .maxConsecutiveBends = 4,

        // Cost
        .hourlyRate = 250.0,
        .setupTime = 40.0,

        // Tooling
        .availableTooling = {}
    };

    machine.availableTooling.push_back(Tooling{
        "Induction Die R50", 50.0, 80.0, 35.0, 80000.0, 40.0, 200.0, true, true, 8.0, 1500.0
    });
    machine.availableTooling.push_back(Tooling{
        "Induction Die R100", 100.0, 150.0, 75.0, 150000.0, 100.0, 350.0, true, true, 15.0, 2500.0
    });

    return machine;
}

std::optional<BendingMachine> MachineLibrary::getMachineByName(const std::string& name) {
    if (name.find("Rotary Draw") != std::string::npos) return getRotaryDrawStandard();
    if (name.find("Push") != std::string::npos) return getPushBendingStandard();
    if (name.find("Compression") != std::string::npos) return getCompressionStandard();
    if (name.find("Roll") != std::string::npos) return getRollBendingStandard();
    if (name.find("Mandrel") != std::string::npos) return getMandrelBendingStandard();
    if (name.find("Induction") != std::string::npos) return getInductionBendingStandard();
    return std::nullopt;
}

std::vector<BendingMachine> MachineLibrary::getMachinesByType(MachineType type) {
    std::vector<BendingMachine> machines;
    if (type == MachineType::ROTARY_DRAW) machines.push_back(getRotaryDrawStandard());
    else if (type == MachineType::PUSH_BENDING) machines.push_back(getPushBendingStandard());
    else if (type == MachineType::COMPRESSION) machines.push_back(getCompressionStandard());
    else if (type == MachineType::ROLL_BENDING) machines.push_back(getRollBendingStandard());
    else if (type == MachineType::MANDREL_BENDING) machines.push_back(getMandrelBendingStandard());
    else if (type == MachineType::INDUCTION_BENDING) machines.push_back(getInductionBendingStandard());
    return machines;
}

// ============= Machine Selector Implementation =============

std::optional<BendingMachine> MachineSelector::selectBestMachine(
    double tubeOD,
    double wallThickness,
    double bendRadius,
    const std::string& material)
{
    auto recommendations = recommendMachines(tubeOD, wallThickness, bendRadius, material, 1);
    if (!recommendations.empty()) {
        return recommendations[0].first;
    }
    return std::nullopt;
}

MachineCapacityCheck MachineSelector::checkMachineCapacity(
    const BendingMachine& machine,
    double tubeOD,
    double wallThickness,
    double bendRadius,
    double bendForce,
    int numberOfBends)
{
    MachineCapacityCheck check{};

    // Check physical capacity
    bool sizeOk = (tubeOD >= machine.minTubeOD && tubeOD <= machine.maxTubeOD) &&
                 (wallThickness >= machine.minWallThickness &&
                  wallThickness <= machine.maxWallThickness);

    // Check bend radius
    bool radiusOk = (bendRadius >= machine.minBendRadius && bendRadius <= machine.maxBendRadius);

    // Check force
    bool forceOk = bendForce <= machine.maxBendForce;

    // Check consecutive bends
    bool bendsOk = numberOfBends <= machine.maxConsecutiveBends;

    check.isCapable = sizeOk && radiusOk && forceOk && bendsOk;

    if (!sizeOk) {
        check.reason = "Tube size outside machine capacity";
    } else if (!radiusOk) {
        check.reason = "Bend radius outside machine range";
    } else if (!forceOk) {
        check.reason = "Bend force exceeds machine capacity";
    } else if (!bendsOk) {
        check.reason = "Too many consecutive bends";
    } else {
        check.reason = "Machine is suitable";
    }

    // Estimate time
    check.estimatedCycleTime = machine.cycleTime;
    check.estimatedTotalTime = machine.setupTime * 60 + (machine.cycleTime * numberOfBends);
    check.estimatedCost = machine.setupTime * (machine.hourlyRate / 60.0) +
                         (machine.cycleTime * numberOfBends / 60.0) * machine.hourlyRate;

    // Tooling suitability
    if (check.isCapable && !machine.availableTooling.empty()) {
        auto bestTooling = ToolingSelector::selectBestTooling(machine, tubeOD, wallThickness, bendRadius);
        check.toolingSuitability = bestTooling ? 0.95 : 0.7;
    } else {
        check.toolingSuitability = 0.0;
    }

    return check;
}

std::vector<std::pair<BendingMachine, double>>
MachineSelector::recommendMachines(
    double tubeOD,
    double wallThickness,
    double bendRadius,
    const std::string& material,
    int maxRecommendations)
{
    std::vector<std::pair<BendingMachine, double>> results;

    // Check all machine types
    for (int type = 0; type <= 5; ++type) {
        auto machines = MachineLibrary::getMachinesByType(static_cast<MachineType>(type));
        for (const auto& machine : machines) {
            // Estimate bend force (simplified)
            double bendForce = 50000.0;  // Average force

            auto capacity = checkMachineCapacity(machine, tubeOD, wallThickness, bendRadius, bendForce, 1);
            if (capacity.isCapable) {
                results.push_back({machine, capacity.toolingSuitability});
            }
        }
    }

    // Sort by suitability (descending)
    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // Limit results
    if (results.size() > static_cast<size_t>(maxRecommendations)) {
        results.resize(maxRecommendations);
    }

    return results;
}

// ============= Tooling Selector Implementation =============

std::optional<Tooling> ToolingSelector::selectBestTooling(
    const BendingMachine& machine,
    double tubeOD,
    double wallThickness,
    double bendRadius)
{
    auto suitable = getSuitableTooling(machine, tubeOD, wallThickness, bendRadius);
    if (!suitable.empty()) {
        return suitable[0].first;
    }
    return std::nullopt;
}

std::vector<std::pair<Tooling, double>>
ToolingSelector::getSuitableTooling(
    const BendingMachine& machine,
    double tubeOD,
    double wallThickness,
    double bendRadius)
{
    std::vector<std::pair<Tooling, double>> results;

    for (const auto& tooling : machine.availableTooling) {
        // Check if tooling fits tube
        if (tubeOD >= tooling.minTubeOD && tubeOD <= tooling.maxTubeOD) {
            // Check if die radius is close to bend radius
            double radiusDiff = std::abs(tooling.dieRadius - bendRadius);
            double suitability = 1.0 - (radiusDiff / bendRadius) * 0.5;
            if (suitability < 0) suitability = 0;
            if (suitability > 1) suitability = 1;

            results.push_back({tooling, suitability});
        }
    }

    // Sort by suitability
    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    return results;
}

std::optional<ToolingSelector::ToolingConfig>
ToolingSelector::optimizeToolingConfiguration(
    const BendingMachine& machine,
    double tubeOD,
    double wallThickness,
    double bendRadius,
    const std::string& material)
{
    auto die = selectBestTooling(machine, tubeOD, wallThickness, bendRadius);
    if (!die) return std::nullopt;

    ToolingConfig config{
        .die = die.value(),
        .mandrel = std::nullopt,
        .follower = std::nullopt,
        .totalForce = die->clamperForce,
        .isOptimal = false
    };

    // For mandrel bending machines, add mandrel
    if (machine.type == MachineType::MANDREL_BENDING && die->mandrelDiameter > 0) {
        config.mandrel = die.value();
    }

    // Check if follower is beneficial
    if (machine.supportsPressureFollower && die->hasFollower && wallThickness < 2.0) {
        config.follower = die.value();
        config.totalForce *= 1.2;  // Increase force with follower
        config.isOptimal = true;
    }

    // Check wall thinning risk
    double strainEstimate = (wallThickness / bendRadius) * 100.0;
    if (strainEstimate < 5.0) {
        config.isOptimal = true;
    }

    return config;
}

} // namespace bendcore::bending
