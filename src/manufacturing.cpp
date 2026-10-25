#include "bendcore/manufacturing.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace bendcore::manufacturing {

std::string NCCodeGenerator::generateBendingCode(const BendOperation& op) {
    std::ostringstream nc;
    nc << std::fixed << std::setprecision(3);

    // G-code format for bending machine
    nc << "O" << 1000 + op.sequenceNumber << "\n";  // Subroutine
    nc << "(Bend operation " << op.sequenceNumber << ")\n";
    nc << "G00 X" << op.toolPosition << "\n";      // Position tool
    nc << "G01 F" << op.toolSpeed << "\n";         // Set feed rate
    nc << "(Bend angle: " << op.bendAngle << " degrees)\n";
    nc << "(Bend radius: " << op.bendRadius << " mm)\n";
    nc << "M08\n";                                  // Coolant on

    if (op.bendType == "free") {
        nc << "G02 Y" << op.bendRadius << " I0 J" << op.bendRadius << "\n";  // Arc bend
    } else if (op.bendType == "air") {
        nc << "G01 Y" << (op.bendRadius * 0.5) << "\n";  // Reduced distance
    } else if (op.bendType == "bottom") {
        nc << "G01 Y" << (op.bendRadius * 1.2) << "\n";  // Full contact
    }

    nc << "M09\n";                                  // Coolant off
    nc << "M99\n";                                  // End subroutine

    return nc.str();
}

std::string NCCodeGenerator::generateCuttingCode(const CutOperation& op) {
    std::ostringstream nc;
    nc << std::fixed << std::setprecision(3);

    nc << "O" << 2000 + op.sequenceNumber << "\n";
    nc << "(Cut operation " << op.sequenceNumber << ")\n";
    nc << "G00 Z" << op.cutPosition << "\n";
    nc << "G01 F" << op.toolSpeed << "\n";
    nc << "(Cut length: " << op.cutLength << " mm)\n";

    if (op.cutType == "straight") {
        nc << "G01 X" << op.cutLength << "\n";
    } else if (op.cutType == "miter") {
        nc << "G01 X" << (op.cutLength * 0.707) << " Y" << (op.cutLength * 0.707) << "\n";
    } else if (op.cutType == "bevel") {
        nc << "G01 Z" << (op.cutLength * 0.5) << " X" << op.cutLength << "\n";
    }

    nc << "M99\n";
    return nc.str();
}

std::string NCCodeGenerator::generateCompleteProgram(const ManufacturingSheet& sheet) {
    std::ostringstream program;

    program << "(BendCore CAD - Automatic NC Code Generation)\n";
    program << "(Part: " << sheet.partName << " - " << sheet.partNumber << ")\n";
    program << "(Total Machine Time: " << sheet.machineTime << " minutes)\n";
    program << "(Material Cost: $" << std::fixed << std::setprecision(2) << sheet.materialCost << ")\n";
    program << "%\n";
    program << "O9999\n";
    program << "G21\n";  // Metric units
    program << "G40\n";  // Cancel tool offset
    program << "G49\n";  // Cancel height offset
    program << "G80\n";  // Cancel canned cycles

    // Add cutting operations
    for (const auto& cut : sheet.cutOperations) {
        program << generateCuttingCode(cut);
    }

    // Add bending operations
    for (const auto& bend : sheet.bendOperations) {
        program << generateBendingCode(bend);
    }

    program << "M30\n";  // End program
    program << "%\n";

    return program.str();
}

TubeUnroller::UnrollResult TubeUnroller::calculateUnfolding(
    double tubeDiameter,
    double wallThickness,
    const std::vector<double>& bendAngles,
    const std::vector<double>& bendRadii)
{
    UnrollResult result;
    result.unfoldedLength = 0.0;

    // Calculate neutral line diameter (center of wall thickness)
    double neutralDiameter = tubeDiameter - wallThickness;
    double circumference = M_PI * neutralDiameter;

    // For each bend, calculate arc length
    for (size_t i = 0; i < bendAngles.size() && i < bendRadii.size(); i++) {
        double angleRad = bendAngles[i] * M_PI / 180.0;
        double arcLength = bendRadii[i] * angleRad;

        result.unfoldedLength += arcLength;
        result.bendMarks.push_back(result.unfoldedLength);
        result.bendAngles.push_back(bendAngles[i]);
    }

    result.totalLengthNeeded = result.unfoldedLength + circumference;  // Add straight sections

    return result;
}

ManufacturingCostCalculator::CostBreakdown ManufacturingCostCalculator::calculateCost(
    double tubeLength,
    double outerDiameter,
    double wallThickness,
    double materialPricePerKg,
    const std::vector<BendOperation>& bends,
    const std::vector<CutOperation>& cuts)
{
    CostBreakdown cost{};

    // Material cost
    double innerDiameter = outerDiameter - 2.0 * wallThickness;
    double crossSectionArea = (M_PI / 4.0) * (outerDiameter * outerDiameter - innerDiameter * innerDiameter);
    double volume = crossSectionArea * tubeLength;  // mm³
    double steelDensity = 7.85e-3;  // g/mm³
    double weight = volume * steelDensity / 1000.0;  // kg
    cost.materialCost = weight * materialPricePerKg;

    // Cutting cost: ~$2-5 per cut
    cost.cuttingCost = cuts.size() * 3.0;

    // Bending cost: ~$5-15 per bend depending on complexity
    for (const auto& bend : bends) {
        double bendCost = 5.0;
        if (bend.bendAngle > 90.0) bendCost += 5.0;
        if (bend.bendRadius < 20.0) bendCost += 3.0;
        cost.bendingCost += bendCost;
    }

    // Assembly cost
    cost.assemblingCost = 50.0;  // Base assembly cost

    // Quality check cost
    cost.qualityCheckCost = 25.0;

    // Packaging cost
    cost.packagingCost = 10.0;

    cost.totalCost = cost.materialCost + cost.cuttingCost + cost.bendingCost +
                     cost.assemblingCost + cost.qualityCheckCost + cost.packagingCost;

    return cost;
}

CollisionChecker::CollisionReport CollisionChecker::checkMachineCollisions(
    const ManufacturingSheet& sheet,
    double machineWorkspaceX,
    double machineWorkspaceY,
    double /* machineWorkspaceZ */)
{
    CollisionReport report;
    report.hasCollisions = false;

    // Check if any bend operation exceeds workspace
    for (const auto& bend : sheet.bendOperations) {
        if (bend.toolPosition > machineWorkspaceX) {
            report.hasCollisions = true;
            report.collisionLocations.push_back("X-axis");
            report.collisionPositions.push_back(bend.toolPosition);
        }

        if (bend.bendRadius > machineWorkspaceY) {
            report.hasCollisions = true;
            report.collisionLocations.push_back("Y-axis (bend radius)");
            report.collisionPositions.push_back(bend.bendRadius);
        }
    }

    if (report.hasCollisions) {
        report.recommendation = "Increase machine workspace or split bending into multiple operations";
    } else {
        report.recommendation = "No collisions detected - ready for manufacturing";
    }

    return report;
}

} // namespace bendcore::manufacturing
