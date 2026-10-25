#pragma once

#include "../document/entity_id.hpp"
#include "vector.hpp"
#include <vector>
#include <optional>
#include <map>
#include <memory>
#include <string>

namespace BendCore::CadCore {

/**
 * @brief Face offset operation for direct editing
 */
class FaceOffset {
public:
    /**
     * @brief Constructor
     */
    FaceOffset(const EntityId& face_id) : face_id_(face_id) {}

    /**
     * @brief Get face ID
     */
    [[nodiscard]] const EntityId& getFaceId() const noexcept { return face_id_; }

    /**
     * @brief Set offset distance
     */
    void setOffsetDistance(double distance) noexcept { offset_distance_ = distance; }

    /**
     * @brief Get offset distance
     */
    [[nodiscard]] double getOffsetDistance() const noexcept { return offset_distance_; }

    /**
     * @brief Set offset mode (outward/inward)
     */
    void setOutward(bool outward) noexcept { is_outward_ = outward; }

    /**
     * @brief Check if offset is outward
     */
    [[nodiscard]] bool isOutward() const noexcept { return is_outward_; }

    /**
     * @brief Apply offset operation
     */
    [[nodiscard]] bool apply() noexcept;

    /**
     * @brief Undo offset operation
     */
    [[nodiscard]] bool undo() noexcept;

private:
    EntityId face_id_;
    double offset_distance_ = 0.0;
    bool is_outward_ = true;
    EntityId original_shape_;
};

/**
 * @brief Edge manipulation for direct editing
 */
class EdgeEditor {
public:
    /**
     * @brief Constructor
     */
    EdgeEditor(const EntityId& edge_id) : edge_id_(edge_id) {}

    /**
     * @brief Get edge ID
     */
    [[nodiscard]] const EntityId& getEdgeId() const noexcept { return edge_id_; }

    /**
     * @brief Chamfer edge
     */
    [[nodiscard]] bool chamfer(double size) noexcept;

    /**
     * @brief Fillet edge
     */
    [[nodiscard]] bool fillet(double radius) noexcept;

    /**
     * @brief Draft edge
     */
    [[nodiscard]] bool draft(double angle) noexcept;

    /**
     * @brief Move edge
     */
    [[nodiscard]] bool moveEdge(const Vector3D& displacement) noexcept;

private:
    EntityId edge_id_;
};

/**
 * @brief Shell hollowing operation
 */
class ShellOp {
public:
    /**
     * @brief Constructor
     */
    ShellOp(const EntityId& solid_id) : solid_id_(solid_id) {}

    /**
     * @brief Get solid ID
     */
    [[nodiscard]] const EntityId& getSolidId() const noexcept { return solid_id_; }

    /**
     * @brief Set shell thickness
     */
    void setThickness(double thickness) noexcept { thickness_ = thickness; }

    /**
     * @brief Get shell thickness
     */
    [[nodiscard]] double getThickness() const noexcept { return thickness_; }

    /**
     * @brief Add removed face
     */
    void addRemovedFace(const EntityId& face_id) noexcept {
        removed_faces_.push_back(face_id);
    }

    /**
     * @brief Get removed faces
     */
    [[nodiscard]] const std::vector<EntityId>& getRemovedFaces() const noexcept {
        return removed_faces_;
    }

    /**
     * @brief Apply shell operation
     */
    [[nodiscard]] bool apply() noexcept;

private:
    EntityId solid_id_;
    double thickness_ = 1.0;
    std::vector<EntityId> removed_faces_;
};

/**
 * @brief Parametric dimension
 */
class ParametricDimension {
public:
    /**
     * @brief Dimension type
     */
    enum class DimensionType {
        LINEAR,      // Distance dimension
        ANGULAR,     // Angle dimension
        RADIAL,      // Radius dimension
        DIAMETER,    // Diameter dimension
    };

    /**
     * @brief Constructor
     */
    ParametricDimension(const EntityId& id, DimensionType type)
        : id_(id), type_(type) {}

    /**
     * @brief Get dimension ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Get dimension type
     */
    [[nodiscard]] DimensionType getType() const noexcept { return type_; }

    /**
     * @brief Set dimension value
     */
    void setValue(double value) noexcept { value_ = value; }

    /**
     * @brief Get dimension value
     */
    [[nodiscard]] double getValue() const noexcept { return value_; }

    /**
     * @brief Set dimension as reference (driving vs reference)
     */
    void setDriving(bool driving) noexcept { is_driving_ = driving; }

    /**
     * @brief Check if dimension is driving
     */
    [[nodiscard]] bool isDriving() const noexcept { return is_driving_; }

    /**
     * @brief Set display name
     */
    void setName(const std::string& name) noexcept { name_ = name; }

    /**
     * @brief Get display name
     */
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

private:
    EntityId id_;
    DimensionType type_;
    double value_ = 0.0;
    bool is_driving_ = true;
    std::string name_;
};

/**
 * @brief Parametric constraint in 3D (different from assembly mates)
 */
class GeometricConstraint {
public:
    /**
     * @brief Constraint type
     */
    enum class ConstraintType {
        COINCIDENT,   // Points/edges coincident
        TANGENT,      // Surfaces tangent
        PARALLEL,     // Edges/planes parallel
        PERPENDICULAR,// Edges/planes perpendicular
        EQUAL,        // Equal lengths/radii
        SYMMETRIC,    // Symmetric about plane
        FIX,          // Fix point/edge in space
    };

    /**
     * @brief Constructor
     */
    GeometricConstraint(const EntityId& id, ConstraintType type)
        : id_(id), type_(type) {}

    /**
     * @brief Get constraint ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Get constraint type
     */
    [[nodiscard]] ConstraintType getType() const noexcept { return type_; }

    /**
     * @brief Add constrained entity
     */
    void addEntity(const EntityId& entity_id) noexcept {
        entities_.push_back(entity_id);
    }

    /**
     * @brief Get constrained entities
     */
    [[nodiscard]] const std::vector<EntityId>& getEntities() const noexcept {
        return entities_;
    }

    /**
     * @brief Check if constraint is satisfied
     */
    [[nodiscard]] bool isSatisfied() const noexcept { return is_satisfied_; }

    /**
     * @brief Mark constraint as satisfied
     */
    void setSatisfied(bool satisfied) noexcept { is_satisfied_ = satisfied; }

private:
    EntityId id_;
    ConstraintType type_;
    std::vector<EntityId> entities_;
    bool is_satisfied_ = false;
};

/**
 * @brief Parametric sketch in 3D space
 */
class ParametricSketch {
public:
    /**
     * @brief Constructor
     */
    ParametricSketch(const EntityId& id) : id_(id) {}

    /**
     * @brief Get sketch ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Add dimension to sketch
     */
    void addDimension(const std::shared_ptr<ParametricDimension>& dimension) noexcept {
        dimensions_.push_back(dimension);
    }

    /**
     * @brief Get all dimensions
     */
    [[nodiscard]] const std::vector<std::shared_ptr<ParametricDimension>>&
    getDimensions() const noexcept {
        return dimensions_;
    }

    /**
     * @brief Add constraint to sketch
     */
    void addConstraint(const std::shared_ptr<GeometricConstraint>& constraint) noexcept {
        constraints_.push_back(constraint);
    }

    /**
     * @brief Get all constraints
     */
    [[nodiscard]] const std::vector<std::shared_ptr<GeometricConstraint>>&
    getConstraints() const noexcept {
        return constraints_;
    }

    /**
     * @brief Solve sketch constraints
     */
    [[nodiscard]] bool solve() noexcept;

    /**
     * @brief Get sketch health status
     */
    [[nodiscard]] std::string getStatus() const noexcept;

private:
    EntityId id_;
    std::vector<std::shared_ptr<ParametricDimension>> dimensions_;
    std::vector<std::shared_ptr<GeometricConstraint>> constraints_;
    bool is_solved_ = false;
    size_t underconstrained_count_ = 0;
    size_t overconstrained_count_ = 0;
};

/**
 * @brief Direct Editor main control class
 */
class DirectEditor {
public:
    /**
     * @brief Constructor
     */
    DirectEditor(const EntityId& shape_id) : shape_id_(shape_id) {}

    /**
     * @brief Offset face
     */
    [[nodiscard]] std::shared_ptr<FaceOffset> offsetFace(
        const EntityId& face_id, double distance) noexcept;

    /**
     * @brief Edit edge
     */
    [[nodiscard]] std::shared_ptr<EdgeEditor> editEdge(
        const EntityId& edge_id) noexcept;

    /**
     * @brief Create shell from solid
     */
    [[nodiscard]] std::shared_ptr<ShellOp> createShell(
        double thickness) noexcept;

    /**
     * @brief Get all pending operations
     */
    [[nodiscard]] size_t getPendingOperationCount() const noexcept {
        return pending_operations_;
    }

    /**
     * @brief Apply all pending operations
     */
    [[nodiscard]] bool applyAllOperations() noexcept;

    /**
     * @brief Rollback all pending operations
     */
    void rollbackAllOperations() noexcept { pending_operations_ = 0; }

private:
    EntityId shape_id_;
    size_t pending_operations_ = 0;
};

} // namespace BendCore::CadCore
