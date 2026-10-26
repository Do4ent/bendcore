#include "bendcore/tube_bending_sequence.h"
#include "bendcore/bend_calculator.h"
#include <algorithm>
#include <cmath>

namespace bendcore::bending {

// ============= Bend Sequence Planner Implementation =============

SequencePlan BendSequencePlanner::optimizeBendSequence(
    const std::vector<BendOperation>& initialBends,
    double tubeOD,
    double wallThickness,
    const std::string& material)
{
    SequencePlan plan;

    // Create a mutable copy of bends to reorder
    std::vector<BendOperation> optimizedBends = initialBends;

    // Simple optimization: sort by bend angle (ascending for stability)
    std::sort(optimizedBends.begin(), optimizedBends.end(),
        [](const auto& a, const auto& b) { return a.bendAngle < b.bendAngle; });

    // Calculate straightening points
    auto straighteningIndices = calculateStraighteningPoints(optimizedBends, material);

    // Insert straightening operations
    plan.sequence.numberOfIntermediateStraightening = straighteningIndices.size();

    // Calculate heat treatment
    auto heatPlan = calculateHeatTreatment(optimizedBends, tubeOD, wallThickness, material);
    plan.sequence.numberOfHeatTreatmentCycles = heatPlan.numberCycles;

    // Build final sequence
    plan.sequence.operations = optimizedBends;

    // Calculate timing
    int totalTime = 0;
    for (const auto& op : optimizedBends) {
        totalTime += op.estimatedMachineTime;
    }
    // Add straightening time
    totalTime += straighteningIndices.size() * 30;  // 30 seconds per straightening
    // Add heat treatment time
    totalTime += heatPlan.numberCycles * (heatPlan.dwellTimeSeconds + 60);  // 60 sec heating/cooling

    plan.sequence.estimatedTotalTime = totalTime;
    plan.sequence.estimatedSetupTime = 300;  // 5 minutes setup
    plan.sequence.estimatedCost = 150.0 + (totalTime / 3600.0) * 120.0;  // $150 setup + $120/hour

    // Check for conflicts
    auto conflicts = checkSequenceConflicts(optimizedBends);
    for (const auto& conflict : conflicts) {
        plan.warnings.push_back("Conflict between bends: " + conflict.description);
        plan.recommendations.push_back(conflict.resolution);
    }

    // Determine if sequence is optimal
    plan.sequence.isOptimal = (straighteningIndices.size() <= 1) && conflicts.empty();
    plan.isApproved = plan.sequence.isOptimal;

    return plan;
}

std::vector<int> BendSequencePlanner::calculateStraighteningPoints(
    const std::vector<BendOperation>& bends,
    const std::string& material)
{
    std::vector<int> straighteningPoints;

    // Get material properties
    auto matProps = MaterialDatabase::getMaterial(material);
    if (!matProps) return straighteningPoints;

    // Calculate cumulative strain
    double cumulativeStrain = 0;
    const double strainingLimit = 15.0;  // Percentage of elongation limit

    for (size_t i = 0; i < bends.size(); ++i) {
        // Estimate strain from bend angle and radius
        double strain = std::abs(bends[i].bendAngle * 0.1);  // Simplified
        cumulativeStrain += strain;

        // If strain exceeds limit, need straightening
        if (cumulativeStrain > strainingLimit) {
            straighteningPoints.push_back(i);
            cumulativeStrain = 0;
        }
    }

    return straighteningPoints;
}

BendSequencePlanner::HeatTreatmentPlan BendSequencePlanner::calculateHeatTreatment(
    const std::vector<BendOperation>& bends,
    double tubeOD,
    double wallThickness,
    const std::string& material)
{
    HeatTreatmentPlan plan{};

    auto matProps = MaterialDatabase::getMaterial(material);
    if (!matProps) {
        plan.preHeating = false;
        plan.numberCycles = 0;
        return plan;
    }

    // Determine if preheating is needed based on material and deformation
    double maxBendAngle = 0;
    for (const auto& bend : bends) {
        maxBendAngle = std::max(maxBendAngle, bend.bendAngle);
    }

    // Materials that need heating
    bool isSteelOrTitanium = (material == "steel" || material == "Steel" ||
                             material == "titanium" || material == "Titanium");
    bool requiresPreheating = isSteelOrTitanium && maxBendAngle > 60.0;

    plan.preHeating = requiresPreheating;

    if (isSteelOrTitanium) {
        // Steel recrystallization temperature: ~600-700°C
        // Titanium: ~700-800°C
        plan.temperatureCelsius = (material == "steel" || material == "Steel") ? 650.0 : 750.0;
        plan.dwellTimeSeconds = 300;  // 5 minutes
        plan.numberCycles = requiresPreheating ? 1 + (int)(bends.size() / 3) : 0;
    } else {
        plan.numberCycles = 0;
    }

    return plan;
}

std::vector<double> BendSequencePlanner::calculateSpringbackCompensation(
    const std::vector<BendOperation>& bends,
    const std::string& material)
{
    std::vector<double> compensations;

    auto matProps = MaterialDatabase::getMaterial(material);
    if (!matProps) {
        return compensations;
    }

    for (const auto& bend : bends) {
        // Simplified springback compensation
        double springback = bend.bendAngle * matProps->springbackFactor;
        // Compensation angle = original angle + springback
        compensations.push_back(bend.bendAngle + springback);
    }

    return compensations;
}

std::vector<BendSequencePlanner::SetupStep> BendSequencePlanner::generateSetupSequence(
    const std::vector<BendOperation>& bends,
    const std::string& machineType)
{
    std::vector<SetupStep> steps;

    // General setup steps
    int stepNum = 1;

    // Step 1: Machine preparation
    steps.push_back(SetupStep{
        .stepNumber = stepNum++,
        .description = "Prepare machine and check all systems",
        .toolsRequired = {"Gauge", "Lubricant", "Cleaning cloth"},
        .estimatedDurationSeconds = 300
    });

    // Step 2: Tooling installation
    steps.push_back(SetupStep{
        .stepNumber = stepNum++,
        .description = "Install tooling for first bend",
        .toolsRequired = {"Socket set", "Torque wrench"},
        .estimatedDurationSeconds = 600
    });

    // Step 3: Calibration
    steps.push_back(SetupStep{
        .stepNumber = stepNum++,
        .description = "Calibrate machine and test bend",
        .toolsRequired = {"Test tube", "Angle gauge", "Caliper"},
        .estimatedDurationSeconds = 300
    });

    // Add tooling change steps for complex sequences
    if (bends.size() > 3) {
        steps.push_back(SetupStep{
            .stepNumber = stepNum++,
            .description = "Prepare for tooling changes between bends",
            .toolsRequired = {"Socket set", "Torque wrench", "Timing device"},
            .estimatedDurationSeconds = 180
        });
    }

    return steps;
}

std::vector<BendSequencePlanner::SequenceConflict> BendSequencePlanner::checkSequenceConflicts(
    const std::vector<BendOperation>& bends)
{
    std::vector<SequenceConflict> conflicts;

    // Check for potential issues
    for (size_t i = 0; i < bends.size(); ++i) {
        for (size_t j = i + 1; j < bends.size(); ++j) {
            // Check if bends in same plane too close together
            double planeDiff = std::abs(bends[i].bendPlaneRotation - bends[j].bendPlaneRotation);
            if (planeDiff < 15.0 && (i == j - 1)) {  // Adjacent bends in similar planes
                conflicts.push_back(SequenceConflict{
                    .bendId1 = (int)i,
                    .bendId2 = (int)j,
                    .description = "Adjacent bends in similar planes may cause stress concentration",
                    .resolution = "Consider reordering or adding intermediate straightening"
                });
            }

            // Check for excessive angle accumulation
            if (i == j - 1) {
                double angleDiff = std::abs(bends[i].bendAngle + bends[j].bendAngle);
                if (angleDiff > 120.0) {
                    conflicts.push_back(SequenceConflict{
                        .bendId1 = (int)i,
                        .bendId2 = (int)j,
                        .description = "High cumulative angle in short sequence",
                        .resolution = "Add straightening operation between these bends"
                    });
                }
            }
        }
    }

    return conflicts;
}

std::vector<BendSequencePlanner::DimensionalChange> BendSequencePlanner::estimateDimensionalChanges(
    const std::vector<BendOperation>& bends,
    const std::string& material)
{
    std::vector<DimensionalChange> changes;

    auto matProps = MaterialDatabase::getMaterial(material);
    if (!matProps) {
        return changes;
    }

    for (const auto& bend : bends) {
        DimensionalChange change{};

        // Estimate radius change due to springback
        change.radiusChange = bend.bendRadius * matProps->springbackFactor * 0.5;

        // Estimate angle change
        change.angleChange = bend.bendAngle * matProps->springbackFactor;

        // Estimate wall thickness reduction (rough estimate)
        // Higher bend angles and tighter radii cause more thinning
        change.wallThicknessReduction = std::min(0.5, (bend.bendAngle / 100.0) * 0.1);

        changes.push_back(change);
    }

    return changes;
}

// ============= Manufacturing Sequence Generator Implementation =============

std::vector<ManufacturingSequenceGenerator::ManufacturingStep>
ManufacturingSequenceGenerator::generateCompleteSequence(
    const SequencePlan& plan,
    double tubeOD,
    double wallThickness,
    const std::string& material)
{
    std::vector<ManufacturingStep> steps;
    int stepNumber = 1;

    // Preparation step
    steps.push_back(ManufacturingStep{
        .stepNumber = stepNumber++,
        .operationType = "Preparation",
        .description = "Inspect raw tube and prepare for bending",
        .durationSeconds = 300,
        .estimatedCost = 50.0,
        .qualityCheckpoints = {"Tube dimensions", "Surface condition"}
    });

    // Heat treatment if needed
    auto heatPlan = BendSequencePlanner::calculateHeatTreatment(
        plan.sequence.operations, tubeOD, wallThickness, material);
    if (heatPlan.preHeating) {
        steps.push_back(ManufacturingStep{
            .stepNumber = stepNumber++,
            .operationType = "Heating",
            .description = "Preheat tube to " + std::to_string(static_cast<int>(heatPlan.temperatureCelsius)) + " C",
            .durationSeconds = heatPlan.dwellTimeSeconds + 600,
            .estimatedCost = 100.0,
            .qualityCheckpoints = {"Temperature", "Uniformity"}
        });
    }

    // Bending operations
    for (const auto& op : plan.sequence.operations) {
        steps.push_back(ManufacturingStep{
            .stepNumber = stepNumber++,
            .operationType = "Bending",
            .description = "Bend angle " + std::to_string(static_cast<int>(op.bendAngle)) +
                          " degrees with radius " + std::to_string(static_cast<int>(op.bendRadius)) + " mm",
            .durationSeconds = op.estimatedMachineTime,
            .estimatedCost = 80.0,
            .qualityCheckpoints = {"Bend angle", "Bend radius", "Surface quality"}
        });

        // Add intermediate straightening if needed
        if (op.requiresStraightening) {
            steps.push_back(ManufacturingStep{
                .stepNumber = stepNumber++,
                .operationType = "Straightening",
                .description = "Intermediate straightening to relieve stress",
                .durationSeconds = 180,
                .estimatedCost = 30.0,
                .qualityCheckpoints = {"Straightness", "Ovality"}
            });
        }
    }

    // Final inspection
    steps.push_back(ManufacturingStep{
        .stepNumber = stepNumber++,
        .operationType = "Inspection",
        .description = "Final dimensional and visual inspection",
        .durationSeconds = 600,
        .estimatedCost = 75.0,
        .qualityCheckpoints = {"All dimensions", "Surface defects", "Ovality", "Cracking"}
    });

    return steps;
}

std::vector<ManufacturingSequenceGenerator::InspectionCheckpoint>
ManufacturingSequenceGenerator::generateInspectionPlan(
    const std::vector<BendOperation>& bends)
{
    std::vector<InspectionCheckpoint> checkpoints;
    int stepNumber = 1;

    for (size_t i = 0; i < bends.size(); ++i) {
        // Angle checkpoint
        checkpoints.push_back(InspectionCheckpoint{
            .stepNumber = stepNumber++,
            .parameter = "Bend angle " + std::to_string(i + 1),
            .targetValue = bends[i].bendAngle,
            .tolerance = 1.0,  // +/- 1 degree
            .measurementMethod = "Digital angle gauge"
        });

        // Radius checkpoint
        checkpoints.push_back(InspectionCheckpoint{
            .stepNumber = stepNumber++,
            .parameter = "Bend radius " + std::to_string(i + 1),
            .targetValue = bends[i].bendRadius,
            .tolerance = 2.0,  // +/- 2 mm
            .measurementMethod = "Radius gauge or CMM"
        });
    }

    // Final quality checkpoints
    checkpoints.push_back(InspectionCheckpoint{
        .stepNumber = stepNumber++,
        .parameter = "Ovality",
        .targetValue = 0.0,  // Should be minimal
        .tolerance = 1.0,  // mm
        .measurementMethod = "Caliper measurements"
    });

    checkpoints.push_back(InspectionCheckpoint{
        .stepNumber = stepNumber++,
        .parameter = "Surface defects",
        .targetValue = 0.0,
        .tolerance = 0.0,  // Zero defects
        .measurementMethod = "Visual inspection"
    });

    return checkpoints;
}

ManufacturingSequenceGenerator::ProductionMetrics
ManufacturingSequenceGenerator::calculateProductionMetrics(
    const std::vector<ManufacturingStep>& steps)
{
    ProductionMetrics metrics{};

    int totalTime = 0;
    int bendingTime = 0;
    double totalCost = 0;

    for (const auto& step : steps) {
        totalTime += step.durationSeconds;
        totalCost += step.estimatedCost;

        if (step.operationType == "Bending") {
            bendingTime += step.durationSeconds;
        }
    }

    metrics.totalTimeSeconds = totalTime;
    metrics.totalCostUSD = totalCost;

    // Count bends
    int bendCount = 0;
    for (const auto& step : steps) {
        if (step.operationType == "Bending") {
            bendCount++;
        }
    }

    metrics.costPerBend = bendCount > 0 ? totalCost / bendCount : 0;

    // Calculate efficiency (bending time / total time)
    metrics.efficiency = totalTime > 0 ? (bendingTime * 100) / totalTime : 0;

    return metrics;
}

} // namespace bendcore::bending
