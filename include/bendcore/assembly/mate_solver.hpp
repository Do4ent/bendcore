#pragma once

#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <vector>
#include <string>
#include <map>
#include <optional>

namespace BendCore::CadCore {

/**
 * @brief Assembly Mate Constraint
 *
 * Defines positional/orientational constraints between components in an assembly.
 * Analogous to SOLIDWORKS Mates, Inventor Constraints, NX Assembly Constraints.
 */
class MateConstraint {
public:
    /**
     * @brief Mate constraint types
     */
    enum class MateType {
        FIXED,          // Component position is fixed
        COINCIDENT,     // Planes/surfaces coincide
        CONCENTRIC,     // Axes/circles concentri c
        PARALLEL,       // Surfaces/edges parallel
        PERPENDICULAR,  // Surfaces/edges perpendicular
        DISTANCE,       // Maintain distance between entities
        ANGLE,          // Maintain angle between entities
        TANGENT,        // Surfaces tangent
        SYMMETRIC,      // Symmetric about plane
    };

    /**
     * @brief Create mate constraint
     */
    MateConstraint(const EntityId& id, MateType type);

    /**
     * @brief Get mate ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Get mate type
     */
    [[nodiscard]] MateType getType() const noexcept { return type_; }

    /**
     * @brief Set mate entities (first component)
     */
    void setEntity1(const EntityId& entity_id, size_t geometry_index = 0) noexcept {
        entity1_id_ = entity_id;
        entity1_geometry_ = geometry_index;
    }

    /**
     * @brief Set mate entities (second component)
     */
    void setEntity2(const EntityId& entity_id, size_t geometry_index = 0) noexcept {
        entity2_id_ = entity_id;
        entity2_geometry_ = geometry_index;
    }

    /**
     * @brief Get first entity
     */
    [[nodiscard]] const EntityId& getEntity1() const noexcept { return entity1_id_; }

    /**
     * @brief Get second entity
     */
    [[nodiscard]] const EntityId& getEntity2() const noexcept { return entity2_id_; }

    /**
     * @brief Set distance or angle value
     */
    void setValue(double value) noexcept { value_ = value; }

    /**
     * @brief Get constraint value
     */
    [[nodiscard]] std::optional<double> getValue() const noexcept {
        return value_.has_value() ? value_ : std::optional<double>();
    }

    /**
     * @brief Check if constraint is satisfied
     */
    [[nodiscard]] bool isSatisfied() const noexcept { return is_satisfied_; }

    /**
     * @brief Mark constraint as satisfied
     */
    void setSatisfied(bool satisfied) noexcept { is_satisfied_ = satisfied; }

    /**
     * @brief Validate constraint before solving
     */
    [[nodiscard]] bool validate() const noexcept;

private:
    EntityId id_;
    MateType type_;
    EntityId entity1_id_;
    EntityId entity2_id_;
    size_t entity1_geometry_ = 0;
    size_t entity2_geometry_ = 0;
    std::optional<double> value_;
    bool is_satisfied_ = false;
};

/**
 * @brief Assembly Mate Solver
 *
 * Solves positional constraints for assembly components.
 * Uses constraint satisfaction approach similar to SOLIDWORKS/Inventor.
 */
class MateSolver {
public:
    /**
     * @brief Constructor
     */
    MateSolver() noexcept = default;

    /**
     * @brief Add mate constraint
     */
    void addConstraint(const MateConstraint& constraint) noexcept {
        constraints_.push_back(constraint);
    }

    /**
     * @brief Remove constraint by ID
     */
    bool removeConstraint(const EntityId& constraint_id) noexcept;

    /**
     * @brief Get all constraints
     */
    [[nodiscard]] const std::vector<MateConstraint>& getConstraints() const noexcept {
        return constraints_;
    }

    /**
     * @brief Solve all constraints
     * @return true if all constraints satisfied, false if over/under-constrained
     */
    bool solve() noexcept;

    /**
     * @brief Check if assembly is over-constrained
     */
    [[nodiscard]] bool isOverConstrained() const noexcept {
        return redundant_constraints_.size() > 0;
    }

    /**
     * @brief Check if assembly is under-constrained
     */
    [[nodiscard]] bool isUnderConstrained() const noexcept {
        return unsatisfied_constraints_.size() > 0;
    }

    /**
     * @brief Get unsatisfied constraints
     */
    [[nodiscard]] const std::vector<EntityId>& getUnsatisfiedConstraints() const noexcept {
        return unsatisfied_constraints_;
    }

    /**
     * @brief Get redundant constraints
     */
    [[nodiscard]] const std::vector<EntityId>& getRedundantConstraints() const noexcept {
        return redundant_constraints_;
    }

    /**
     * @brief Get component position after solving
     */
    [[nodiscard]] Vector3D getComponentPosition(const EntityId& component_id) const noexcept;

    /**
     * @brief Get component orientation after solving
     */
    [[nodiscard]] Vector3D getComponentOrientation(const EntityId& component_id) const noexcept;

    /**
     * @brief Clear all constraints
     */
    void clear() noexcept {
        constraints_.clear();
        component_positions_.clear();
        component_orientations_.clear();
        unsatisfied_constraints_.clear();
        redundant_constraints_.clear();
    }

    /**
     * @brief Diagnose assembly constraint issues
     */
    [[nodiscard]] std::string diagnose() const noexcept;

private:
    std::vector<MateConstraint> constraints_;
    std::map<EntityId, Vector3D> component_positions_;
    std::map<EntityId, Vector3D> component_orientations_;
    std::vector<EntityId> unsatisfied_constraints_;
    std::vector<EntityId> redundant_constraints_;

    /**
     * @brief Solve constraint recursively
     */
    bool solveConstraint(MateConstraint& constraint) noexcept;

    /**
     * @brief Check for redundancy
     */
    void checkRedundancy() noexcept;

    /**
     * @brief Initialize positions (identity)
     */
    void initializePositions() noexcept;
};

} // namespace BendCore::CadCore
