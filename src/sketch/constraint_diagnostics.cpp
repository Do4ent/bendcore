#include "bendcore/sketch/constraint_diagnostics.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <set>

namespace bendcore::sketch {

void ConstraintDiagnosticsEngine::addConstraint(int id, const std::string& type) {
    ConstraintInfo info;
    info.id = id;
    info.type = type;
    constraints_[id] = info;
}

void ConstraintDiagnosticsEngine::removeConstraint(int id) {
    constraints_.erase(id);
}

void ConstraintDiagnosticsEngine::addEntity(int id, const std::string& type) {
    EntityInfo info;
    info.id = id;
    info.type = type;

    // Determine degrees of freedom per entity type
    if (type == "point") {
        info.dof_per_entity = 2;  // X, Y
    } else if (type == "line") {
        info.dof_per_entity = 4;  // Start X,Y + End X,Y
    } else if (type == "circle") {
        info.dof_per_entity = 3;  // Center X,Y + Radius
    } else if (type == "arc") {
        info.dof_per_entity = 5;  // Center X,Y + Radius + Start angle + End angle
    } else {
        info.dof_per_entity = 2;  // Default to point-like
    }

    entities_[id] = info;
}

int ConstraintDiagnosticsEngine::calculateDegreesOfFreedom(int num_entities, int num_constraints) {
    int total_dof = 0;
    for (const auto& entity : entities_) {
        total_dof += entity.second.dof_per_entity;
    }

    // Each constraint removes 1 DOF (ideally)
    int constrained_dof = num_constraints;
    int dof = total_dof - constrained_dof;

    return std::max(0, dof);
}

SketchDiagnosticReport ConstraintDiagnosticsEngine::diagnose() {
    SketchDiagnosticReport report;

    int num_entities = entities_.size();
    int num_constraints = constraints_.size();

    report.degrees_of_freedom = calculateDegreesOfFreedom(num_entities, num_constraints);

    // Analyze constraint consistency
    analyzeConstraintConsistency();
    analyzeRedundancy();
    analyzeUnderConstraint();

    // Determine overall status
    report.is_fully_constrained = (report.degrees_of_freedom == 0);
    report.is_underconstrained = (report.degrees_of_freedom > 0);

    // Find conflicting constraints
    report.conflicting_pairs = findConflictingPairs();
    report.is_overconstrained = !report.conflicting_pairs.empty();

    // Find redundant constraints
    report.redundant_constraints = findRedundantConstraints();
    report.is_redundant = !report.redundant_constraints.empty();

    report.is_consistent = !report.is_overconstrained &&
                          (report.conflicting_pairs.empty());

    // Generate diagnostic messages for each constraint
    for (const auto& constraint : constraints_) {
        ConstraintDiagnostic diag;
        diag.constraint_id = constraint.first;
        diag.description = getConstraintDescription(constraint.first);

        // Check if satisfied
        double residual = 0.0;
        if (isConstraintSatisfied(constraint.first, residual)) {
            diag.status = ConstraintStatus::SATISFIED;
        } else {
            diag.status = ConstraintStatus::VIOLATED;
        }
        diag.residual = residual;

        report.constraint_diagnostics.push_back(diag);
    }

    // Generate messages
    report.status_message = generateStatusMessage(report);
    report.suggestions = generateSuggestions(report);

    last_report_ = report;
    return report;
}

bool ConstraintDiagnosticsEngine::isConstraintSatisfied(int constraint_id, double& residual) {
    auto it = constraints_.find(constraint_id);
    if (it == constraints_.end()) {
        residual = 0.0;
        return true;
    }

    // For this implementation, we assume constraints are satisfied
    // In a real implementation, we would check actual geometry
    residual = 0.0;
    return true;
}

std::vector<std::pair<int, int>> ConstraintDiagnosticsEngine::findConflicts() {
    return findConflictingPairs();
}

std::vector<int> ConstraintDiagnosticsEngine::findRedundantConstraints() {
    std::vector<int> redundant;

    // Simple heuristic: if we have more constraints than DOF allows,
    // some constraints are redundant
    int num_entities = entities_.size();
    int total_dof = 0;
    for (const auto& entity : entities_) {
        total_dof += entity.second.dof_per_entity;
    }

    int num_constraints = constraints_.size();
    if (num_constraints > total_dof) {
        // We have extra constraints
        // Mark the last ones as potentially redundant
        for (int i = 0; i < num_constraints - total_dof; i++) {
            if (i < static_cast<int>(constraints_.size())) {
                auto it = constraints_.begin();
                std::advance(it, constraints_.size() - 1 - i);
                redundant.push_back(it->first);
            }
        }
    }

    return redundant;
}

std::vector<int> ConstraintDiagnosticsEngine::getUnderconstrainedEntities() {
    std::vector<int> free_entities;

    // Entities that are not referenced by constraints have freedom
    std::set<int> constrained_entities;
    for (const auto& constraint : constraints_) {
        for (int entity_id : constraint.second.involved_entities) {
            constrained_entities.insert(entity_id);
        }
    }

    for (const auto& entity : entities_) {
        if (constrained_entities.find(entity.first) == constrained_entities.end()) {
            free_entities.push_back(entity.first);
        }
    }

    return free_entities;
}

std::string ConstraintDiagnosticsEngine::getConstraintDescription(int constraint_id) const {
    auto it = constraints_.find(constraint_id);
    if (it != constraints_.end()) {
        return "Constraint " + std::to_string(constraint_id) + " (" + it->second.type + ")";
    }
    return "Unknown constraint";
}

std::vector<std::string> ConstraintDiagnosticsEngine::suggestConstraints() {
    std::vector<std::string> suggestions;

    auto report = diagnose();

    if (report.is_underconstrained) {
        suggestions.push_back("Add distance constraint between underconstrained points");
        suggestions.push_back("Fix position of at least one geometry");
        suggestions.push_back("Add parallel or perpendicular constraints");
    }

    if (report.is_overconstrained) {
        suggestions.push_back("Remove one or more conflicting constraints");
        suggestions.push_back("Modify constraint values to avoid conflicts");
    }

    if (report.is_redundant) {
        suggestions.push_back("Remove redundant constraints for cleaner sketch");
    }

    return suggestions;
}

std::string ConstraintDiagnosticsEngine::generateErrorMessage(const SketchDiagnosticReport& report) {
    std::ostringstream oss;

    if (report.is_fully_constrained) {
        oss << "Fully constrained sketch (0 DOF)";
    } else if (report.is_underconstrained) {
        oss << "Under-constrained sketch (" << report.degrees_of_freedom << " DOF remaining)";
    } else if (report.is_overconstrained) {
        oss << "Over-constrained sketch (conflicting constraints)";
    }

    if (report.is_redundant) {
        oss << " - has " << report.redundant_constraints.size() << " redundant constraint(s)";
    }

    return oss.str();
}

void ConstraintDiagnosticsEngine::clear() {
    constraints_.clear();
    entities_.clear();
    last_report_ = SketchDiagnosticReport();
}

void ConstraintDiagnosticsEngine::analyzeConstraintConsistency() {
    // Implementation checks constraint consistency
    // This is a placeholder for more complex analysis
}

void ConstraintDiagnosticsEngine::analyzeRedundancy() {
    // Implementation detects redundant constraints
}

void ConstraintDiagnosticsEngine::analyzeUnderConstraint() {
    // Implementation identifies which entities have freedom
}

std::vector<std::pair<int, int>> ConstraintDiagnosticsEngine::findConflictingPairs() {
    std::vector<std::pair<int, int>> conflicts;

    // Simple conflict detection: if we have more constraints than total DOF
    int total_dof = 0;
    for (const auto& entity : entities_) {
        total_dof += entity.second.dof_per_entity;
    }

    int num_constraints = constraints_.size();
    if (num_constraints > total_dof) {
        // We have a conflict
        for (int i = 0; i < num_constraints - 1; i++) {
            auto it1 = constraints_.begin();
            std::advance(it1, i);

            auto it2 = std::next(it1);
            conflicts.push_back({it1->first, it2->first});
        }
    }

    return conflicts;
}

std::string ConstraintDiagnosticsEngine::generateStatusMessage(const SketchDiagnosticReport& report) {
    std::ostringstream oss;

    if (report.is_fully_constrained) {
        oss << "Fully Constrained";
    } else if (report.is_underconstrained) {
        oss << "Under-Constrained (" << report.degrees_of_freedom << " DOF)";
    } else if (report.is_overconstrained) {
        oss << "Over-Constrained";
    } else {
        oss << "Consistent";
    }

    return oss.str();
}

std::vector<std::string> ConstraintDiagnosticsEngine::generateSuggestions(const SketchDiagnosticReport& report) {
    return suggestConstraints();
}

} // namespace bendcore::sketch
