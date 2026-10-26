#pragma once

#include "assembly_constraints.hpp"
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <stdexcept>

namespace BendCore::CadCore {

/**
 * @brief Constraint validation result
 */
struct ConstraintValidationResult {
    bool is_valid = false;                      ///< Overall validation result
    std::vector<std::string> error_messages;    ///< Error messages
    std::vector<std::string> warning_messages;  ///< Warning messages
};

/**
 * @brief Constraint solver result
 */
struct ConstraintSolverResult {
    bool is_converged = false;                          ///< Did solver converge?
    size_t iterations = 0;                              ///< Number of iterations
    double residual = 0.0;                              ///< Final residual error
    std::vector<EntityId> unsatisfied_constraints;      ///< Constraints that could not be satisfied
    std::vector<EntityId> redundant_constraints;        ///< Redundant constraints detected
    std::vector<EntityId> conflicting_constraints;      ///< Conflicting constraints
    std::map<EntityId, Vector3D> component_positions;   ///< Solved component positions
    std::map<EntityId, Vector3D> component_orientations; ///< Solved component orientations
};

/**
 * @brief Assembly Constraint System
 *
 * Comprehensive system for managing, validating, and solving assembly constraints.
 * This is the main interface for working with constraints in an assembly.
 */
class AssemblyConstraintSystem {
public:
    /**
     * @brief Constructor
     * @param assembly_id The assembly this system manages
     */
    explicit AssemblyConstraintSystem(const EntityId& assembly_id) noexcept
        : assembly_id_(assembly_id), next_constraint_id_(1) {}

    /**
     * @brief Virtual destructor
     */
    virtual ~AssemblyConstraintSystem() noexcept = default;

    // ==================== Constraint Management ====================

    /**
     * @brief Add a Fixed constraint
     * @param component_id Component to fix
     * @param position Fixed position
     * @param orientation Fixed orientation
     * @return Pointer to created constraint
     * @throw std::invalid_argument if parameters are invalid
     */
    std::shared_ptr<AssemblyConstraint> addFixedConstraint(
        const EntityId& component_id, const Vector3D& position,
        const Vector3D& orientation);

    /**
     * @brief Add a Coincident constraint
     * @param entity1_id First entity
     * @param entity2_id Second entity
     * @param geometry1_index Geometry index on entity1
     * @param geometry2_index Geometry index on entity2
     * @return Pointer to created constraint
     * @throw std::invalid_argument if parameters are invalid
     */
    std::shared_ptr<AssemblyConstraint> addCoincidentConstraint(
        const EntityId& entity1_id, const EntityId& entity2_id,
        size_t geometry1_index = 0, size_t geometry2_index = 0);

    /**
     * @brief Add a Distance constraint
     * @param entity1_id First entity
     * @param entity2_id Second entity
     * @param distance Required distance
     * @param geometry1_index Geometry index on entity1
     * @param geometry2_index Geometry index on entity2
     * @return Pointer to created constraint
     * @throw std::invalid_argument if parameters are invalid
     */
    std::shared_ptr<AssemblyConstraint> addDistanceConstraint(
        const EntityId& entity1_id, const EntityId& entity2_id, double distance,
        size_t geometry1_index = 0, size_t geometry2_index = 0);

    /**
     * @brief Add a Parallel constraint
     * @param entity1_id First entity
     * @param entity2_id Second entity
     * @param geometry1_index Geometry index on entity1
     * @param geometry2_index Geometry index on entity2
     * @return Pointer to created constraint
     * @throw std::invalid_argument if parameters are invalid
     */
    std::shared_ptr<AssemblyConstraint> addParallelConstraint(
        const EntityId& entity1_id, const EntityId& entity2_id,
        size_t geometry1_index = 0, size_t geometry2_index = 0);

    /**
     * @brief Add a Perpendicular constraint
     * @param entity1_id First entity
     * @param entity2_id Second entity
     * @param geometry1_index Geometry index on entity1
     * @param geometry2_index Geometry index on entity2
     * @return Pointer to created constraint
     * @throw std::invalid_argument if parameters are invalid
     */
    std::shared_ptr<AssemblyConstraint> addPerpendicularConstraint(
        const EntityId& entity1_id, const EntityId& entity2_id,
        size_t geometry1_index = 0, size_t geometry2_index = 0);

    /**
     * @brief Add a Tangent constraint
     * @param entity1_id First entity (surface)
     * @param entity2_id Second entity (surface)
     * @param geometry1_index Geometry index on entity1
     * @param geometry2_index Geometry index on entity2
     * @return Pointer to created constraint
     * @throw std::invalid_argument if parameters are invalid
     */
    std::shared_ptr<AssemblyConstraint> addTangentConstraint(
        const EntityId& entity1_id, const EntityId& entity2_id,
        size_t geometry1_index = 0, size_t geometry2_index = 0);

    /**
     * @brief Add a Symmetric constraint
     * @param entity1_id First entity
     * @param entity2_id Second entity
     * @param symmetry_plane_id Symmetry plane
     * @param geometry1_index Geometry index on entity1
     * @param geometry2_index Geometry index on entity2
     * @return Pointer to created constraint
     * @throw std::invalid_argument if parameters are invalid
     */
    std::shared_ptr<AssemblyConstraint> addSymmetricConstraint(
        const EntityId& entity1_id, const EntityId& entity2_id,
        const EntityId& symmetry_plane_id, size_t geometry1_index = 0,
        size_t geometry2_index = 0);

    /**
     * @brief Add an Angle constraint
     * @param entity1_id First entity
     * @param entity2_id Second entity
     * @param angle Angle in degrees (0-180)
     * @param geometry1_index Geometry index on entity1
     * @param geometry2_index Geometry index on entity2
     * @return Pointer to created constraint
     * @throw std::invalid_argument if parameters are invalid
     */
    std::shared_ptr<AssemblyConstraint> addAngleConstraint(
        const EntityId& entity1_id, const EntityId& entity2_id, double angle,
        size_t geometry1_index = 0, size_t geometry2_index = 0);

    /**
     * @brief Add a pre-created constraint
     * @param constraint Constraint to add
     * @throw std::invalid_argument if constraint is invalid
     */
    void addConstraint(std::shared_ptr<AssemblyConstraint> constraint);

    /**
     * @brief Remove constraint by ID
     * @param constraint_id Constraint to remove
     * @return true if constraint was found and removed
     */
    bool removeConstraint(const EntityId& constraint_id) noexcept;

    /**
     * @brief Get constraint by ID
     * @param constraint_id Constraint ID
     * @return Pointer to constraint or nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<AssemblyConstraint> getConstraint(
        const EntityId& constraint_id) const noexcept;

    /**
     * @brief Get all constraints
     * @return Vector of all constraints
     */
    [[nodiscard]] const std::vector<std::shared_ptr<AssemblyConstraint>>&
    getConstraints() const noexcept {
        return constraints_;
    }

    /**
     * @brief Get constraint count
     * @return Number of constraints in system
     */
    [[nodiscard]] size_t getConstraintCount() const noexcept {
        return constraints_.size();
    }

    /**
     * @brief Get all constraints for a specific entity
     * @param entity_id Entity to query
     * @return Vector of constraints involving this entity
     */
    [[nodiscard]] std::vector<std::shared_ptr<AssemblyConstraint>>
    getConstraintsForEntity(const EntityId& entity_id) const noexcept;

    /**
     * @brief Get all constraints of a specific type
     * @param type Constraint type to query
     * @return Vector of constraints of this type
     */
    [[nodiscard]] std::vector<std::shared_ptr<AssemblyConstraint>>
    getConstraintsByType(AssemblyConstraint::ConstraintType type) const noexcept;

    /**
     * @brief Clear all constraints
     */
    void clearConstraints() noexcept {
        constraints_.clear();
        constraint_map_.clear();
    }

    // ==================== Constraint Validation ====================

    /**
     * @brief Validate all constraints
     * @return Validation result with details
     */
    [[nodiscard]] ConstraintValidationResult validateAll() const noexcept;

    /**
     * @brief Validate a single constraint
     * @param constraint_id Constraint to validate
     * @return Validation result
     */
    [[nodiscard]] ConstraintValidationResult validateConstraint(
        const EntityId& constraint_id) const noexcept;

    /**
     * @brief Check if assembly is over-constrained
     * @return true if redundant constraints are detected
     */
    [[nodiscard]] bool isOverConstrained() const noexcept {
        return redundant_constraints_.size() > 0;
    }

    /**
     * @brief Check if assembly is under-constrained
     * @return true if unsatisfied constraints are detected
     */
    [[nodiscard]] bool isUnderConstrained() const noexcept {
        return unsatisfied_constraints_.size() > 0;
    }

    /**
     * @brief Get redundant constraints
     * @return Vector of redundant constraint IDs
     */
    [[nodiscard]] const std::vector<EntityId>& getRedundantConstraints()
        const noexcept {
        return redundant_constraints_;
    }

    /**
     * @brief Get unsatisfied constraints
     * @return Vector of unsatisfied constraint IDs
     */
    [[nodiscard]] const std::vector<EntityId>& getUnsatisfiedConstraints()
        const noexcept {
        return unsatisfied_constraints_;
    }

    /**
     * @brief Get conflicting constraints
     * @return Vector of conflicting constraint IDs
     */
    [[nodiscard]] const std::vector<EntityId>& getConflictingConstraints()
        const noexcept {
        return conflicting_constraints_;
    }

    /**
     * @brief Detect conflicting constraints
     * @return Vector of constraint IDs that conflict
     */
    [[nodiscard]] std::vector<EntityId> detectConflicts() const noexcept;

    // ==================== Constraint Solver ====================

    /**
     * @brief Solve all constraints
     * @param max_iterations Maximum iterations (default 100)
     * @param convergence_tolerance Tolerance for convergence (default 1e-6)
     * @return Solver result with positions and orientations
     */
    [[nodiscard]] ConstraintSolverResult solve(size_t max_iterations = 100,
                                               double convergence_tolerance = 1e-6) noexcept;

    /**
     * @brief Get last solver result
     * @return Last computed solver result
     */
    [[nodiscard]] const ConstraintSolverResult& getLastSolverResult() const noexcept {
        return last_solver_result_;
    }

    /**
     * @brief Get component position after solving
     * @param component_id Component to query
     * @return Position vector, or zero if not solved
     */
    [[nodiscard]] Vector3D getComponentPosition(const EntityId& component_id) const noexcept;

    /**
     * @brief Get component orientation after solving
     * @param component_id Component to query
     * @return Orientation vector, or zero if not solved
     */
    [[nodiscard]] Vector3D getComponentOrientation(const EntityId& component_id) const noexcept;

    // ==================== Constraint Enable/Disable ====================

    /**
     * @brief Enable/disable (suppress) a constraint
     * @param constraint_id Constraint to control
     * @param enabled true to enable, false to disable
     * @return true if constraint was found
     */
    bool setConstraintEnabled(const EntityId& constraint_id, bool enabled) noexcept;

    /**
     * @brief Check if constraint is enabled
     * @param constraint_id Constraint to query
     * @return true if enabled
     */
    [[nodiscard]] bool isConstraintEnabled(const EntityId& constraint_id) const noexcept;

    /**
     * @brief Set constraint priority
     * @param constraint_id Constraint to modify
     * @param priority Priority value (higher = more important)
     * @return true if constraint was found
     */
    bool setConstraintPriority(const EntityId& constraint_id, double priority) noexcept;

    // ==================== Serialization ====================

    /**
     * @brief Serialize constraints to JSON string
     * @return JSON representation of all constraints
     * @throw std::runtime_error if serialization fails
     */
    [[nodiscard]] std::string serializeToJSON() const;

    /**
     * @brief Deserialize constraints from JSON string
     * @param json JSON string to load
     * @throw std::runtime_error if deserialization fails or JSON is invalid
     */
    void deserializeFromJSON(const std::string& json);

    /**
     * @brief Serialize constraints to binary format
     * @return Binary representation of all constraints
     * @throw std::runtime_error if serialization fails
     */
    [[nodiscard]] std::vector<uint8_t> serializeToBinary() const;

    /**
     * @brief Deserialize constraints from binary format
     * @param data Binary data to load
     * @throw std::runtime_error if deserialization fails or data is invalid
     */
    void deserializeFromBinary(const std::vector<uint8_t>& data);

    // ==================== Diagnostics ====================

    /**
     * @brief Get diagnostic information
     * @return Human-readable diagnostic string
     */
    [[nodiscard]] std::string getDiagnostics() const noexcept;

    /**
     * @brief Get assembly health status
     * @return Status string with current assembly state
     */
    [[nodiscard]] std::string getHealthStatus() const noexcept;

protected:
    EntityId assembly_id_;
    std::vector<std::shared_ptr<AssemblyConstraint>> constraints_;
    std::map<EntityId, std::shared_ptr<AssemblyConstraint>> constraint_map_;
    int64_t next_constraint_id_;

    // Solver state
    ConstraintSolverResult last_solver_result_;
    std::vector<EntityId> redundant_constraints_;
    std::vector<EntityId> unsatisfied_constraints_;
    std::vector<EntityId> conflicting_constraints_;

    /**
     * @brief Generate unique constraint ID
     * @return New EntityId for constraint
     */
    [[nodiscard]] EntityId generateConstraintId() noexcept;

    /**
     * @brief Iterative constraint solver
     * @param max_iterations Maximum iterations
     * @param tolerance Convergence tolerance
     * @return true if converged
     */
    bool solveConstraints(size_t max_iterations, double tolerance) noexcept;

    /**
     * @brief Check for constraint redundancy
     */
    void checkRedundancy() noexcept;

    /**
     * @brief Initialize component positions
     */
    void initializePositions() noexcept;
};

} // namespace BendCore::CadCore
