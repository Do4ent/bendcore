#pragma once

#include "assembly.hpp"
#include "assembly_constraints.hpp"
#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <optional>
#include <functional>
#include <chrono>

namespace BendCore::CadCore {

// Forward declarations
class Assembly;
class ComponentInstance;
class AssemblyConstraint;

/**
 * @brief Severity level for validation issues
 */
enum class ValidationSeverity {
    INFO = 0,       ///< Informational message
    WARNING = 1,    ///< Warning (may cause issues)
    ERROR = 2,      ///< Critical error
};

/**
 * @brief Single validation issue
 */
struct ValidationIssue {
    ValidationSeverity severity;           ///< Issue severity level
    std::string message;                   ///< Issue description
    std::string location;                  ///< Where the issue occurred (component/constraint ID)
    std::string recommendation;            ///< Suggested fix
    std::string category;                  ///< Issue category (e.g., "constraint", "geometry")
    int code = 0;                          ///< Issue code for programmatic handling
};

/**
 * @brief Assembly health statistics
 */
struct AssemblyHealthStats {
    size_t total_components = 0;           ///< Total component count
    size_t total_constraints = 0;          ///< Total constraint count
    size_t satisfied_constraints = 0;      ///< Satisfied constraint count
    size_t unsatisfied_constraints = 0;    ///< Unsatisfied constraint count
    size_t redundant_constraints = 0;      ///< Redundant constraint count
    size_t conflicting_constraints = 0;    ///< Conflicting constraint count
    double assembly_complexity = 0.0;      ///< Complexity score (0.0-1.0)
    size_t hierarchy_depth = 0;            ///< Maximum assembly hierarchy depth
    double bounds_volume = 0.0;            ///< Assembly bounding box volume
};

/**
 * @brief Comprehensive assembly validation report
 */
struct AssemblyValidationReport {
    bool is_valid = false;                                      ///< Overall validity
    std::chrono::steady_clock::time_point validation_time;     ///< When validation occurred
    std::vector<ValidationIssue> issues;                        ///< All identified issues
    std::vector<ValidationIssue> errors;                        ///< Error issues only
    std::vector<ValidationIssue> warnings;                      ///< Warning issues only
    std::vector<ValidationIssue> infos;                         ///< Info issues only
    AssemblyHealthStats stats;                                  ///< Health statistics
    std::map<std::string, std::string> metadata;                ///< Additional metadata
    std::string summary;                                        ///< Human-readable summary

    /**
     * @brief Get issue count by severity
     */
    [[nodiscard]] size_t getIssueCount(ValidationSeverity severity) const noexcept;

    /**
     * @brief Get issue count for category
     */
    [[nodiscard]] size_t getIssueCountByCategory(const std::string& category) const noexcept;
};

/**
 * @brief Validation rule interface
 */
class ValidationRule {
public:
    virtual ~ValidationRule() noexcept = default;

    /**
     * @brief Get rule name
     */
    [[nodiscard]] virtual std::string getName() const noexcept = 0;

    /**
     * @brief Get rule description
     */
    [[nodiscard]] virtual std::string getDescription() const noexcept = 0;

    /**
     * @brief Get default severity
     */
    [[nodiscard]] virtual ValidationSeverity getDefaultSeverity() const noexcept = 0;

    /**
     * @brief Execute rule on assembly
     * @return Vector of issues found by this rule
     */
    [[nodiscard]] virtual std::vector<ValidationIssue> validate(const Assembly& assembly) const noexcept = 0;
};

/**
 * @brief Validation profile (collection of rules with settings)
 */
struct ValidationProfile {
    std::string name;                                           ///< Profile name
    std::string description;                                    ///< Profile description
    std::vector<std::string> enabled_rules;                     ///< Enabled rule names
    std::map<std::string, ValidationSeverity> severity_overrides; ///< Severity overrides
    bool check_geometry = true;                                 ///< Check geometry validity
    bool check_constraints = true;                              ///< Check constraint satisfaction
    bool check_references = true;                               ///< Check component references
    bool check_performance = true;                              ///< Check performance metrics
    bool check_dependencies = true;                             ///< Check circular dependencies
};

/**
 * @brief Assembly Validation System
 *
 * Comprehensive validation system for assemblies including:
 * - Integrity checks (references, circular dependencies, duplicates)
 * - Constraint validation (satisfaction, conflicts, redundancy)
 * - Geometry validation (validity, bounds, coordinate systems)
 * - Health reporting with detailed diagnostics
 * - Custom validation rules
 * - Performance analysis
 */
class AssemblyValidator {
public:
    /**
     * @brief Constructor
     */
    AssemblyValidator() noexcept;

    /**
     * @brief Destructor
     */
    ~AssemblyValidator() noexcept = default;

    // ==================== Main Validation ====================

    /**
     * @brief Validate assembly with default profile
     * @param assembly Assembly to validate
     * @return Comprehensive validation report
     */
    [[nodiscard]] AssemblyValidationReport validateAssembly(const Assembly& assembly) noexcept;

    /**
     * @brief Validate assembly with custom profile
     * @param assembly Assembly to validate
     * @param profile Validation profile to use
     * @return Comprehensive validation report
     */
    [[nodiscard]] AssemblyValidationReport validateAssembly(
        const Assembly& assembly,
        const ValidationProfile& profile
    ) noexcept;

    // ==================== Integrity Checks ====================

    /**
     * @brief Check component references validity
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkComponentReferences(
        const Assembly& assembly
    ) const noexcept;

    /**
     * @brief Check for circular dependencies
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkCircularDependencies(
        const Assembly& assembly
    ) const noexcept;

    /**
     * @brief Check for duplicate component IDs
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkDuplicateInstances(
        const Assembly& assembly
    ) const noexcept;

    /**
     * @brief Validate component instance geometry
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkComponentGeometry(
        const Assembly& assembly
    ) const noexcept;

    // ==================== Constraint Validation ====================

    /**
     * @brief Validate constraint satisfaction
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkConstraintSatisfaction(
        const Assembly& assembly
    ) const noexcept;

    /**
     * @brief Detect over-constrained assemblies
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkOverConstrained(
        const Assembly& assembly
    ) const noexcept;

    /**
     * @brief Detect under-constrained assemblies
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkUnderConstrained(
        const Assembly& assembly
    ) const noexcept;

    /**
     * @brief Detect conflicting constraints
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkConstraintConflicts(
        const Assembly& assembly
    ) const noexcept;

    /**
     * @brief Detect redundant constraints
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkRedundantConstraints(
        const Assembly& assembly
    ) const noexcept;

    // ==================== Geometry Validation ====================

    /**
     * @brief Check for degenerate geometry
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkDegenerateGeometry(
        const Assembly& assembly
    ) const noexcept;

    /**
     * @brief Check assembly bounds validity
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkAssemblyBounds(
        const Assembly& assembly
    ) const noexcept;

    /**
     * @brief Check coordinate system validity
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkCoordinateSystems(
        const Assembly& assembly
    ) const noexcept;

    /**
     * @brief Check for component intersections (optional)
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkComponentIntersections(
        const Assembly& assembly
    ) const noexcept;

    // ==================== Performance Validation ====================

    /**
     * @brief Check for performance issues
     * @return Vector of validation issues
     */
    [[nodiscard]] std::vector<ValidationIssue> checkPerformanceMetrics(
        const Assembly& assembly
    ) const noexcept;

    /**
     * @brief Analyze assembly complexity
     * @return Complexity score (0.0-1.0)
     */
    [[nodiscard]] double analyzeComplexity(const Assembly& assembly) const noexcept;

    /**
     * @brief Calculate assembly bounding box
     * @return Array of [minX, minY, minZ, maxX, maxY, maxZ]
     */
    [[nodiscard]] std::array<double, 6> calculateAssemblyBounds(
        const Assembly& assembly
    ) const noexcept;

    // ==================== Validation Rules ====================

    /**
     * @brief Register custom validation rule
     * @param rule The validation rule to register
     */
    void registerRule(std::shared_ptr<ValidationRule> rule) noexcept;

    /**
     * @brief Unregister validation rule by name
     * @param rule_name Name of rule to unregister
     * @return true if removed, false if not found
     */
    bool unregisterRule(const std::string& rule_name) noexcept;

    /**
     * @brief Get registered rule by name
     * @return shared_ptr to rule, or nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<ValidationRule> getRule(const std::string& rule_name) const noexcept;

    /**
     * @brief Get all registered rules
     */
    [[nodiscard]] const std::vector<std::shared_ptr<ValidationRule>>& getRules() const noexcept {
        return rules_;
    }

    /**
     * @brief Create validation profile from rule names
     * @param name Profile name
     * @param rule_names Rule names to include
     * @return New validation profile
     */
    [[nodiscard]] ValidationProfile createProfile(
        const std::string& name,
        const std::vector<std::string>& rule_names
    ) const noexcept;

    // ==================== Built-in Profiles ====================

    /**
     * @brief Get strict validation profile (all checks enabled)
     */
    [[nodiscard]] static ValidationProfile getStrictProfile() noexcept;

    /**
     * @brief Get standard validation profile (common checks)
     */
    [[nodiscard]] static ValidationProfile getStandardProfile() noexcept;

    /**
     * @brief Get fast validation profile (quick checks only)
     */
    [[nodiscard]] static ValidationProfile getFastProfile() noexcept;

    /**
     * @brief Get minimal validation profile (essential checks only)
     */
    [[nodiscard]] static ValidationProfile getMinimalProfile() noexcept;

private:
    std::vector<std::shared_ptr<ValidationRule>> rules_;
    std::map<std::string, std::shared_ptr<ValidationRule>> rules_by_name_;

    /**
     * @brief Initialize built-in validation rules
     */
    void initializeBuiltInRules() noexcept;

    /**
     * @brief Apply custom rules to validation report
     */
    void applyCustomRules(
        const Assembly& assembly,
        const ValidationProfile& profile,
        AssemblyValidationReport& report
    ) const noexcept;

    /**
     * @brief Generate summary text from validation issues
     */
    [[nodiscard]] std::string generateSummary(
        const AssemblyValidationReport& report
    ) const noexcept;

    /**
     * @brief Check if assembly is empty or has missing components
     */
    [[nodiscard]] std::vector<ValidationIssue> checkEmptyAssembly(
        const Assembly& assembly
    ) const noexcept;
};

/**
 * @brief Built-in validation rule implementations
 */

/**
 * @brief Rule: Check for empty components
 */
class EmptyComponentRule : public ValidationRule {
public:
    [[nodiscard]] std::string getName() const noexcept override { return "empty_components"; }
    [[nodiscard]] std::string getDescription() const noexcept override {
        return "Detects assemblies with no components";
    }
    [[nodiscard]] ValidationSeverity getDefaultSeverity() const noexcept override {
        return ValidationSeverity::WARNING;
    }
    [[nodiscard]] std::vector<ValidationIssue> validate(const Assembly& assembly) const noexcept override;
};

/**
 * @brief Rule: Check constraint satisfaction
 */
class ConstraintSatisfactionRule : public ValidationRule {
public:
    [[nodiscard]] std::string getName() const noexcept override { return "constraint_satisfaction"; }
    [[nodiscard]] std::string getDescription() const noexcept override {
        return "Verifies all constraints are satisfied";
    }
    [[nodiscard]] ValidationSeverity getDefaultSeverity() const noexcept override {
        return ValidationSeverity::ERROR;
    }
    [[nodiscard]] std::vector<ValidationIssue> validate(const Assembly& assembly) const noexcept override;
};

/**
 * @brief Rule: Check for redundant constraints
 */
class RedundantConstraintRule : public ValidationRule {
public:
    [[nodiscard]] std::string getName() const noexcept override { return "redundant_constraints"; }
    [[nodiscard]] std::string getDescription() const noexcept override {
        return "Detects redundant constraint definitions";
    }
    [[nodiscard]] ValidationSeverity getDefaultSeverity() const noexcept override {
        return ValidationSeverity::WARNING;
    }
    [[nodiscard]] std::vector<ValidationIssue> validate(const Assembly& assembly) const noexcept override;
};

/**
 * @brief Rule: Check for conflicting constraints
 */
class ConflictingConstraintRule : public ValidationRule {
public:
    [[nodiscard]] std::string getName() const noexcept override { return "conflicting_constraints"; }
    [[nodiscard]] std::string getDescription() const noexcept override {
        return "Detects constraints that conflict with each other";
    }
    [[nodiscard]] ValidationSeverity getDefaultSeverity() const noexcept override {
        return ValidationSeverity::ERROR;
    }
    [[nodiscard]] std::vector<ValidationIssue> validate(const Assembly& assembly) const noexcept override;
};

/**
 * @brief Rule: Check for assembly complexity
 */
class ComplexityRule : public ValidationRule {
public:
    [[nodiscard]] std::string getName() const noexcept override { return "assembly_complexity"; }
    [[nodiscard]] std::string getDescription() const noexcept override {
        return "Warns about overly complex assemblies";
    }
    [[nodiscard]] ValidationSeverity getDefaultSeverity() const noexcept override {
        return ValidationSeverity::INFO;
    }
    [[nodiscard]] std::vector<ValidationIssue> validate(const Assembly& assembly) const noexcept override;
};

/**
 * @brief Rule: Check for isolated components
 */
class IsolatedComponentRule : public ValidationRule {
public:
    [[nodiscard]] std::string getName() const noexcept override { return "isolated_components"; }
    [[nodiscard]] std::string getDescription() const noexcept override {
        return "Detects components not constrained to other components";
    }
    [[nodiscard]] ValidationSeverity getDefaultSeverity() const noexcept override {
        return ValidationSeverity::WARNING;
    }
    [[nodiscard]] std::vector<ValidationIssue> validate(const Assembly& assembly) const noexcept override;
};

} // namespace BendCore::CadCore
