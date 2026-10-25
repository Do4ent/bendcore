#include "bendcore/constraint_system.h"
#include <algorithm>
#include <cmath>

namespace bendcore::assembly {

std::string Constraint::getDescription() const {
    std::string desc;
    switch (type_) {
        case ConstraintType::COINCIDENT:
            desc = "Coincident: " + target_.entityId1 + " with " + target_.entityId2;
            break;
        case ConstraintType::PARALLEL:
            desc = "Parallel: " + target_.entityId1 + " || " + target_.entityId2;
            break;
        case ConstraintType::PERPENDICULAR:
            desc = "Perpendicular: " + target_.entityId1 + " ⊥ " + target_.entityId2;
            break;
        case ConstraintType::DISTANCE:
            if (target_.value) {
                desc = "Distance: " + std::to_string(target_.value.value()) + "mm";
            }
            break;
        case ConstraintType::ANGLE:
            if (target_.value) {
                desc = "Angle: " + std::to_string(target_.value.value()) + "°";
            }
            break;
        case ConstraintType::CONCENTRIC:
            desc = "Concentric: " + target_.entityId1 + " with " + target_.entityId2;
            break;
        default:
            desc = "Unknown constraint";
    }
    return desc;
}

bool Constraint::satisfies(double /* tolerance */) const {
    // Simplified validation - in real implementation would check geometry
    return isActive_;
}

void ConstraintSystem::addConstraint(const Constraint& constraint) {
    constraints_.push_back(constraint);
}

void ConstraintSystem::removeConstraint(const std::string& /* constraintId */) {
    // Simplified - would need constraint IDs in real implementation
}

void ConstraintSystem::clearAllConstraints() {
    constraints_.clear();
}

std::vector<Constraint> ConstraintSystem::getConstraintsByType(ConstraintType type) const {
    std::vector<Constraint> result;
    for (const auto& c : constraints_) {
        if (c.getType() == type) {
            result.push_back(c);
        }
    }
    return result;
}

std::vector<Constraint> ConstraintSystem::getConstraintsByEntity(const std::string& entityId) const {
    std::vector<Constraint> result;
    for (const auto& c : constraints_) {
        const auto& target = c.getTarget();
        if (target.entityId1 == entityId || target.entityId2 == entityId) {
            result.push_back(c);
        }
    }
    return result;
}

bool ConstraintSystem::resolveConstraints(const std::vector<Vector3>& initialPositions,
                                         std::vector<Vector3>& resolvedPositions) {
    resolvedPositions = initialPositions;
    return solveIterative(resolvedPositions, 100);
}

bool ConstraintSystem::validateAllConstraints(double tolerance) const {
    for (const auto& constraint : constraints_) {
        if (!constraint.satisfies(tolerance)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> ConstraintSystem::getConflictingConstraints() const {
    std::vector<std::string> conflicts;
    // Simplified - would detect conflicts between constraints
    return conflicts;
}

bool ConstraintSystem::solveIterative(std::vector<Vector3>& /* positions */,
                                     unsigned int iterations) {
    for (unsigned int iter = 0; iter < iterations; ++iter) {
        for (const auto& constraint : constraints_) {
            if (!constraint.isActive()) continue;

            // Apply constraint corrections iteratively
            // This is a simplified solver - production code would use proper constraint solver
            // const auto& target = constraint.getTarget();

            // Would apply corrections to positions array based on constraint type
        }
    }
    return validateAllConstraints();
}

} // namespace bendcore::assembly
