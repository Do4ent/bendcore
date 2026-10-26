#pragma once

#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>

namespace BendCore::CadCore {

/**
 * @brief Base class for all assembly constraints
 *
 * This is the foundational class for the Assembly Constraints system.
 * All constraint types (Fixed, Coincident, Distance, Parallel, Perpendicular,
 * Tangent, Symmetric, Angle) inherit from this class.
 */
class AssemblyConstraint {
public:
    /**
     * @brief Enumeration of all supported constraint types
     */
    enum class ConstraintType {
        FIXED,          ///< Component locked to absolute position/orientation
        COINCIDENT,     ///< Two geometry elements (faces, edges, vertices) aligned
        DISTANCE,       ///< Fixed distance between two elements
        PARALLEL,       ///< Two faces/edges parallel to each other
        PERPENDICULAR,  ///< Two faces/edges perpendicular
        TANGENT,        ///< Surface tangency constraint
        SYMMETRIC,      ///< Symmetric positioning about plane
        ANGLE,          ///< Fixed angle between elements
    };

    /**
     * @brief Constraint status enumeration
     */
    enum class ConstraintStatus {
        UNSATISFIED,    ///< Constraint is not satisfied
        SATISFIED,      ///< Constraint is satisfied
        REDUNDANT,      ///< Constraint is redundant with others
        CONFLICTING,    ///< Constraint conflicts with others
        SUPPRESSED,     ///< Constraint is disabled
    };

    /**
     * @brief Virtual destructor
     */
    virtual ~AssemblyConstraint() = default;

    /**
     * @brief Get constraint type
     * @return Constraint type identifier
     */
    [[nodiscard]] virtual ConstraintType getType() const noexcept = 0;

    /**
     * @brief Get constraint ID
     * @return EntityId of this constraint
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Get constraint status
     * @return Current status of the constraint
     */
    [[nodiscard]] ConstraintStatus getStatus() const noexcept { return status_; }

    /**
     * @brief Set constraint status
     * @param status New status
     */
    void setStatus(ConstraintStatus status) noexcept { status_ = status; }

    /**
     * @brief Check if constraint is enabled (not suppressed)
     * @return true if constraint is active
     */
    [[nodiscard]] bool isEnabled() const noexcept {
        return status_ != ConstraintStatus::SUPPRESSED;
    }

    /**
     * @brief Enable/disable (suppress) constraint
     * @param enabled true to enable, false to suppress
     */
    void setEnabled(bool enabled) noexcept {
        if (!enabled) {
            status_ = ConstraintStatus::SUPPRESSED;
        } else if (status_ == ConstraintStatus::SUPPRESSED) {
            status_ = ConstraintStatus::UNSATISFIED;
        }
    }

    /**
     * @brief Validate constraint before solving
     * @return true if constraint is valid and can be solved
     * @throw std::invalid_argument if constraint has invalid parameters
     */
    virtual bool validate() const = 0;

    /**
     * @brief Get constraint description
     * @return Human-readable description of the constraint
     */
    [[nodiscard]] virtual std::string getDescription() const noexcept = 0;

    /**
     * @brief Set constraint priority (higher = more important)
     * @param priority Priority value (default 1.0)
     */
    void setPriority(double priority) noexcept { priority_ = priority; }

    /**
     * @brief Get constraint priority
     * @return Priority value
     */
    [[nodiscard]] double getPriority() const noexcept { return priority_; }

    /**
     * @brief Get involved entity IDs
     * @return Vector of involved entities
     */
    [[nodiscard]] virtual std::vector<EntityId> getInvolvedEntities() const noexcept = 0;

protected:
    /**
     * @brief Protected constructor for derived classes
     * @param id Unique constraint identifier
     */
    explicit AssemblyConstraint(const EntityId& id) noexcept
        : id_(id), status_(ConstraintStatus::UNSATISFIED), priority_(1.0) {}

    EntityId id_;
    ConstraintStatus status_;
    double priority_;
};

/**
 * @brief Fixed constraint - component locked to absolute position
 *
 * This constraint fixes a component at a specific position and orientation.
 * Typically used to anchor the first component in an assembly.
 */
class FixedConstraint : public AssemblyConstraint {
public:
    /**
     * @brief Constructor
     * @param id Constraint identifier
     * @param component_id The component to fix
     * @param position Fixed position in space
     * @param orientation Fixed orientation (as normal vector)
     */
    FixedConstraint(const EntityId& id, const EntityId& component_id,
                    const Vector3D& position, const Vector3D& orientation) noexcept
        : AssemblyConstraint(id), component_id_(component_id), position_(position),
          orientation_(orientation) {}

    [[nodiscard]] ConstraintType getType() const noexcept override {
        return ConstraintType::FIXED;
    }

    [[nodiscard]] bool validate() const override;

    [[nodiscard]] std::string getDescription() const noexcept override;

    [[nodiscard]] std::vector<EntityId> getInvolvedEntities() const noexcept override {
        return {component_id_};
    }

    [[nodiscard]] const EntityId& getComponentId() const noexcept { return component_id_; }

    [[nodiscard]] const Vector3D& getPosition() const noexcept { return position_; }

    [[nodiscard]] const Vector3D& getOrientation() const noexcept {
        return orientation_;
    }

private:
    EntityId component_id_;
    Vector3D position_;
    Vector3D orientation_;
};

/**
 * @brief Coincident constraint - aligns two geometry elements
 *
 * Makes two geometry elements coincident (overlapping, aligned).
 * Can align faces, edges, or vertices.
 */
class CoincidentConstraint : public AssemblyConstraint {
public:
    /**
     * @brief Constructor
     * @param id Constraint identifier
     * @param entity1_id First entity
     * @param entity2_id Second entity
     * @param geometry1_index Geometry index on entity1 (default 0)
     * @param geometry2_index Geometry index on entity2 (default 0)
     */
    CoincidentConstraint(const EntityId& id, const EntityId& entity1_id,
                         const EntityId& entity2_id, size_t geometry1_index = 0,
                         size_t geometry2_index = 0) noexcept
        : AssemblyConstraint(id), entity1_id_(entity1_id), entity2_id_(entity2_id),
          geometry1_index_(geometry1_index), geometry2_index_(geometry2_index) {}

    [[nodiscard]] ConstraintType getType() const noexcept override {
        return ConstraintType::COINCIDENT;
    }

    [[nodiscard]] bool validate() const override;

    [[nodiscard]] std::string getDescription() const noexcept override;

    [[nodiscard]] std::vector<EntityId> getInvolvedEntities() const noexcept override {
        return {entity1_id_, entity2_id_};
    }

    [[nodiscard]] const EntityId& getEntity1() const noexcept { return entity1_id_; }

    [[nodiscard]] const EntityId& getEntity2() const noexcept { return entity2_id_; }

    [[nodiscard]] size_t getGeometry1Index() const noexcept {
        return geometry1_index_;
    }

    [[nodiscard]] size_t getGeometry2Index() const noexcept {
        return geometry2_index_;
    }

private:
    EntityId entity1_id_;
    EntityId entity2_id_;
    size_t geometry1_index_;
    size_t geometry2_index_;
};

/**
 * @brief Distance constraint - maintains fixed distance between elements
 *
 * Maintains a fixed distance between two geometry elements.
 * Commonly used for positioning components at specific distances apart.
 */
class DistanceConstraint : public AssemblyConstraint {
public:
    /**
     * @brief Constructor
     * @param id Constraint identifier
     * @param entity1_id First entity
     * @param entity2_id Second entity
     * @param distance Required distance
     * @param geometry1_index Geometry index on entity1 (default 0)
     * @param geometry2_index Geometry index on entity2 (default 0)
     */
    DistanceConstraint(const EntityId& id, const EntityId& entity1_id,
                       const EntityId& entity2_id, double distance,
                       size_t geometry1_index = 0, size_t geometry2_index = 0) noexcept
        : AssemblyConstraint(id), entity1_id_(entity1_id), entity2_id_(entity2_id),
          distance_(distance), geometry1_index_(geometry1_index),
          geometry2_index_(geometry2_index) {}

    [[nodiscard]] ConstraintType getType() const noexcept override {
        return ConstraintType::DISTANCE;
    }

    [[nodiscard]] bool validate() const override;

    [[nodiscard]] std::string getDescription() const noexcept override;

    [[nodiscard]] std::vector<EntityId> getInvolvedEntities() const noexcept override {
        return {entity1_id_, entity2_id_};
    }

    [[nodiscard]] const EntityId& getEntity1() const noexcept { return entity1_id_; }

    [[nodiscard]] const EntityId& getEntity2() const noexcept { return entity2_id_; }

    [[nodiscard]] double getDistance() const noexcept { return distance_; }

    void setDistance(double distance) noexcept { distance_ = distance; }

    [[nodiscard]] size_t getGeometry1Index() const noexcept {
        return geometry1_index_;
    }

    [[nodiscard]] size_t getGeometry2Index() const noexcept {
        return geometry2_index_;
    }

private:
    EntityId entity1_id_;
    EntityId entity2_id_;
    double distance_;
    size_t geometry1_index_;
    size_t geometry2_index_;
};

/**
 * @brief Parallel constraint - aligns two planes or edges parallel
 *
 * Makes two planes or edges parallel to each other.
 */
class ParallelConstraint : public AssemblyConstraint {
public:
    /**
     * @brief Constructor
     * @param id Constraint identifier
     * @param entity1_id First entity
     * @param entity2_id Second entity
     * @param geometry1_index Geometry index on entity1 (default 0)
     * @param geometry2_index Geometry index on entity2 (default 0)
     */
    ParallelConstraint(const EntityId& id, const EntityId& entity1_id,
                       const EntityId& entity2_id, size_t geometry1_index = 0,
                       size_t geometry2_index = 0) noexcept
        : AssemblyConstraint(id), entity1_id_(entity1_id), entity2_id_(entity2_id),
          geometry1_index_(geometry1_index), geometry2_index_(geometry2_index) {}

    [[nodiscard]] ConstraintType getType() const noexcept override {
        return ConstraintType::PARALLEL;
    }

    [[nodiscard]] bool validate() const override;

    [[nodiscard]] std::string getDescription() const noexcept override;

    [[nodiscard]] std::vector<EntityId> getInvolvedEntities() const noexcept override {
        return {entity1_id_, entity2_id_};
    }

    [[nodiscard]] const EntityId& getEntity1() const noexcept { return entity1_id_; }

    [[nodiscard]] const EntityId& getEntity2() const noexcept { return entity2_id_; }

    [[nodiscard]] size_t getGeometry1Index() const noexcept {
        return geometry1_index_;
    }

    [[nodiscard]] size_t getGeometry2Index() const noexcept {
        return geometry2_index_;
    }

private:
    EntityId entity1_id_;
    EntityId entity2_id_;
    size_t geometry1_index_;
    size_t geometry2_index_;
};

/**
 * @brief Perpendicular constraint - aligns two planes or edges at right angles
 *
 * Makes two planes or edges perpendicular to each other (90-degree angle).
 */
class PerpendicularConstraint : public AssemblyConstraint {
public:
    /**
     * @brief Constructor
     * @param id Constraint identifier
     * @param entity1_id First entity
     * @param entity2_id Second entity
     * @param geometry1_index Geometry index on entity1 (default 0)
     * @param geometry2_index Geometry index on entity2 (default 0)
     */
    PerpendicularConstraint(const EntityId& id, const EntityId& entity1_id,
                            const EntityId& entity2_id, size_t geometry1_index = 0,
                            size_t geometry2_index = 0) noexcept
        : AssemblyConstraint(id), entity1_id_(entity1_id), entity2_id_(entity2_id),
          geometry1_index_(geometry1_index), geometry2_index_(geometry2_index) {}

    [[nodiscard]] ConstraintType getType() const noexcept override {
        return ConstraintType::PERPENDICULAR;
    }

    [[nodiscard]] bool validate() const override;

    [[nodiscard]] std::string getDescription() const noexcept override;

    [[nodiscard]] std::vector<EntityId> getInvolvedEntities() const noexcept override {
        return {entity1_id_, entity2_id_};
    }

    [[nodiscard]] const EntityId& getEntity1() const noexcept { return entity1_id_; }

    [[nodiscard]] const EntityId& getEntity2() const noexcept { return entity2_id_; }

    [[nodiscard]] size_t getGeometry1Index() const noexcept {
        return geometry1_index_;
    }

    [[nodiscard]] size_t getGeometry2Index() const noexcept {
        return geometry2_index_;
    }

private:
    EntityId entity1_id_;
    EntityId entity2_id_;
    size_t geometry1_index_;
    size_t geometry2_index_;
};

/**
 * @brief Tangent constraint - makes two surfaces tangent to each other
 *
 * Creates tangency between two surfaces at a point or along a curve.
 */
class TangentConstraint : public AssemblyConstraint {
public:
    /**
     * @brief Constructor
     * @param id Constraint identifier
     * @param entity1_id First entity (surface)
     * @param entity2_id Second entity (surface)
     * @param geometry1_index Geometry index on entity1 (default 0)
     * @param geometry2_index Geometry index on entity2 (default 0)
     */
    TangentConstraint(const EntityId& id, const EntityId& entity1_id,
                      const EntityId& entity2_id, size_t geometry1_index = 0,
                      size_t geometry2_index = 0) noexcept
        : AssemblyConstraint(id), entity1_id_(entity1_id), entity2_id_(entity2_id),
          geometry1_index_(geometry1_index), geometry2_index_(geometry2_index) {}

    [[nodiscard]] ConstraintType getType() const noexcept override {
        return ConstraintType::TANGENT;
    }

    [[nodiscard]] bool validate() const override;

    [[nodiscard]] std::string getDescription() const noexcept override;

    [[nodiscard]] std::vector<EntityId> getInvolvedEntities() const noexcept override {
        return {entity1_id_, entity2_id_};
    }

    [[nodiscard]] const EntityId& getEntity1() const noexcept { return entity1_id_; }

    [[nodiscard]] const EntityId& getEntity2() const noexcept { return entity2_id_; }

    [[nodiscard]] size_t getGeometry1Index() const noexcept {
        return geometry1_index_;
    }

    [[nodiscard]] size_t getGeometry2Index() const noexcept {
        return geometry2_index_;
    }

private:
    EntityId entity1_id_;
    EntityId entity2_id_;
    size_t geometry1_index_;
    size_t geometry2_index_;
};

/**
 * @brief Symmetric constraint - positions entities symmetrically about a plane
 *
 * Creates symmetric positioning of two entities about a symmetry plane.
 */
class SymmetricConstraint : public AssemblyConstraint {
public:
    /**
     * @brief Constructor
     * @param id Constraint identifier
     * @param entity1_id First entity
     * @param entity2_id Second entity
     * @param symmetry_plane_id Symmetry plane entity
     * @param geometry1_index Geometry index on entity1 (default 0)
     * @param geometry2_index Geometry index on entity2 (default 0)
     */
    SymmetricConstraint(const EntityId& id, const EntityId& entity1_id,
                        const EntityId& entity2_id, const EntityId& symmetry_plane_id,
                        size_t geometry1_index = 0, size_t geometry2_index = 0) noexcept
        : AssemblyConstraint(id), entity1_id_(entity1_id), entity2_id_(entity2_id),
          symmetry_plane_id_(symmetry_plane_id), geometry1_index_(geometry1_index),
          geometry2_index_(geometry2_index) {}

    [[nodiscard]] ConstraintType getType() const noexcept override {
        return ConstraintType::SYMMETRIC;
    }

    [[nodiscard]] bool validate() const override;

    [[nodiscard]] std::string getDescription() const noexcept override;

    [[nodiscard]] std::vector<EntityId> getInvolvedEntities() const noexcept override {
        return {entity1_id_, entity2_id_, symmetry_plane_id_};
    }

    [[nodiscard]] const EntityId& getEntity1() const noexcept { return entity1_id_; }

    [[nodiscard]] const EntityId& getEntity2() const noexcept { return entity2_id_; }

    [[nodiscard]] const EntityId& getSymmetryPlane() const noexcept {
        return symmetry_plane_id_;
    }

    [[nodiscard]] size_t getGeometry1Index() const noexcept {
        return geometry1_index_;
    }

    [[nodiscard]] size_t getGeometry2Index() const noexcept {
        return geometry2_index_;
    }

private:
    EntityId entity1_id_;
    EntityId entity2_id_;
    EntityId symmetry_plane_id_;
    size_t geometry1_index_;
    size_t geometry2_index_;
};

/**
 * @brief Angle constraint - maintains fixed angle between two elements
 *
 * Maintains a fixed angle between two planes, edges, or axes.
 */
class AngleConstraint : public AssemblyConstraint {
public:
    /**
     * @brief Constructor
     * @param id Constraint identifier
     * @param entity1_id First entity
     * @param entity2_id Second entity
     * @param angle Angle in degrees (0-180)
     * @param geometry1_index Geometry index on entity1 (default 0)
     * @param geometry2_index Geometry index on entity2 (default 0)
     */
    AngleConstraint(const EntityId& id, const EntityId& entity1_id,
                    const EntityId& entity2_id, double angle, size_t geometry1_index = 0,
                    size_t geometry2_index = 0) noexcept
        : AssemblyConstraint(id), entity1_id_(entity1_id), entity2_id_(entity2_id),
          angle_(angle), geometry1_index_(geometry1_index),
          geometry2_index_(geometry2_index) {}

    [[nodiscard]] ConstraintType getType() const noexcept override {
        return ConstraintType::ANGLE;
    }

    [[nodiscard]] bool validate() const override;

    [[nodiscard]] std::string getDescription() const noexcept override;

    [[nodiscard]] std::vector<EntityId> getInvolvedEntities() const noexcept override {
        return {entity1_id_, entity2_id_};
    }

    [[nodiscard]] const EntityId& getEntity1() const noexcept { return entity1_id_; }

    [[nodiscard]] const EntityId& getEntity2() const noexcept { return entity2_id_; }

    [[nodiscard]] double getAngle() const noexcept { return angle_; }

    void setAngle(double angle) noexcept { angle_ = angle; }

    [[nodiscard]] size_t getGeometry1Index() const noexcept {
        return geometry1_index_;
    }

    [[nodiscard]] size_t getGeometry2Index() const noexcept {
        return geometry2_index_;
    }

private:
    EntityId entity1_id_;
    EntityId entity2_id_;
    double angle_;
    size_t geometry1_index_;
    size_t geometry2_index_;
};

} // namespace BendCore::CadCore
