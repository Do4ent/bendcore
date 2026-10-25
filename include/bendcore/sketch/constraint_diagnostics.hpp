#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>

namespace bendcore::sketch {

/**
 * @brief Constraint diagnostic status
 */
enum class ConstraintStatus {
    SATISFIED,           // Constraint is satisfied
    VIOLATED,            // Constraint is violated
    REDUNDANT,           // Constraint is redundant (extra, non-conflicting)
    CONFLICTING,         // Constraint conflicts with others
    UNDERCONSTRAINED,    // Part of underconstrained group
    OVERCONSTRAINED      // Part of overconstrained group
};

/**
 * @brief Diagnostic result for a single constraint
 */
struct ConstraintDiagnostic {
    int constraint_id;
    ConstraintStatus status;
    double residual;                    // How much constraint is violated
    std::string description;
    std::vector<int> related_constraints; // Other constraints involved in conflict/redundancy
};

/**
 * @brief Overall sketch diagnostics report
 */
struct SketchDiagnosticReport {
    int degrees_of_freedom;             // DOF count
    bool is_fully_constrained;
    bool is_overconstrained;
    bool is_underconstrained;
    bool is_redundant;
    bool is_consistent;

    std::vector<ConstraintDiagnostic> constraint_diagnostics;
    std::vector<int> free_entities;     // Entities with freedom
    std::vector<std::pair<int, int>> conflicting_pairs; // Pairs of conflicting constraints
    std::vector<int> redundant_constraints;

    std::string status_message;         // Human-readable status
    std::vector<std::string> warnings;
    std::vector<std::string> suggestions; // How to fix issues
};

/**
 * @brief Constraint diagnostics engine
 */
class ConstraintDiagnosticsEngine {
public:
    /**
     * @brief Constructor
     */
    ConstraintDiagnosticsEngine() = default;

    /**
     * @brief Add constraint for tracking
     */
    void addConstraint(int id, const std::string& type);

    /**
     * @brief Remove constraint
     */
    void removeConstraint(int id);

    /**
     * @brief Register geometric entity (point, line, circle, etc)
     */
    void addEntity(int id, const std::string& type);

    /**
     * @brief Check degrees of freedom
     */
    int calculateDegreesOfFreedom(int num_entities, int num_constraints);

    /**
     * @brief Run full diagnostics
     */
    SketchDiagnosticReport diagnose();

    /**
     * @brief Check if constraint is satisfied
     * @param constraint_id ID of constraint to check
     * @param residual Output: how much constraint is violated
     */
    bool isConstraintSatisfied(int constraint_id, double& residual);

    /**
     * @brief Find conflicting constraints
     * @return List of constraint pairs that conflict
     */
    std::vector<std::pair<int, int>> findConflicts();

    /**
     * @brief Find redundant constraints
     * @return List of redundant constraint IDs
     */
    std::vector<int> findRedundantConstraints();

    /**
     * @brief Get entities with degrees of freedom
     * @return List of entity IDs that have freedom
     */
    std::vector<int> getUnderconstrainedEntities();

    /**
     * @brief Get constraint description
     */
    std::string getConstraintDescription(int constraint_id) const;

    /**
     * @brief Suggest constraints to fully constrain sketch
     */
    std::vector<std::string> suggestConstraints();

    /**
     * @brief Generate error message for visualization
     */
    std::string generateErrorMessage(const SketchDiagnosticReport& report);

    /**
     * @brief Clear all diagnostics data
     */
    void clear();

private:
    struct ConstraintInfo {
        int id;
        std::string type;
        std::vector<int> involved_entities;
    };

    struct EntityInfo {
        int id;
        std::string type;
        int dof_per_entity;
    };

    std::map<int, ConstraintInfo> constraints_;
    std::map<int, EntityInfo> entities_;
    SketchDiagnosticReport last_report_;

    // Internal analysis methods
    void analyzeConstraintConsistency();
    void analyzeRedundancy();
    void analyzeUnderConstraint();
    std::vector<std::pair<int, int>> findConflictingPairs();
    std::string generateStatusMessage(const SketchDiagnosticReport& report);
    std::vector<std::string> generateSuggestions(const SketchDiagnosticReport& report);
};

} // namespace bendcore::sketch
