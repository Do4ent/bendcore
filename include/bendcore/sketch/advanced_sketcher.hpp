#pragma once

#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <vector>
#include <string>
#include <map>
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief Construction geometry for sketches (reference only, non-printing)
 */
class ConstructionGeometry {
public:
    /**
     * @brief Geometry type
     */
    enum class GeometryType {
        CONSTRUCTION_POINT,
        CONSTRUCTION_LINE,
        CONSTRUCTION_CIRCLE,
        CONSTRUCTION_ARC,
        CONSTRUCTION_SPLINE,
    };

    /**
     * @brief Constructor
     */
    ConstructionGeometry(const EntityId& id, GeometryType type)
        : id_(id), type_(type), is_construction_(true) {}

    /**
     * @brief Get geometry type
     */
    [[nodiscard]] GeometryType getType() const noexcept { return type_; }

    /**
     * @brief Is construction (non-printing)
     */
    [[nodiscard]] bool isConstruction() const noexcept { return is_construction_; }

    /**
     * @brief Set position
     */
    void setPosition(const Vector3D& pos) noexcept { position_ = pos; }

    /**
     * @brief Get position
     */
    [[nodiscard]] const Vector3D& getPosition() const noexcept { return position_; }

private:
    EntityId id_;
    GeometryType type_;
    bool is_construction_;
    Vector3D position_;
};

/**
 * @brief Advanced geometric relations beyond basic constraints
 */
class GeometricRelation {
public:
    /**
     * @brief Relation type
     */
    enum class RelationType {
        COLLINEAR,          // Points or lines collinear
        EQUAL_LENGTH,       // Equal length lines
        EQUAL_RADIUS,       // Equal radius circles
        TANGENT_CURVES,     // Curves tangent to each other
        PERPENDICULAR_CURVES, // Curves perpendicular
        SYMMETRIC,          // Symmetric about axis
        MIDPOINT,           // Point at midpoint
        OFFSET_DISTANCE,    // Offset by distance
    };

    /**
     * @brief Constructor
     */
    GeometricRelation(const EntityId& id, RelationType type)
        : id_(id), type_(type), is_satisfied_(false) {}

    /**
     * @brief Add entity to relation
     */
    void addEntity(const EntityId& entity_id) noexcept {
        entities_.push_back(entity_id);
    }

    /**
     * @brief Get entities involved
     */
    [[nodiscard]] const std::vector<EntityId>& getEntities() const noexcept {
        return entities_;
    }

    /**
     * @brief Get relation type
     */
    [[nodiscard]] RelationType getType() const noexcept { return type_; }

    /**
     * @brief Check if relation is satisfied
     */
    [[nodiscard]] bool isSatisfied() const noexcept { return is_satisfied_; }

    /**
     * @brief Mark satisfied
     */
    void setSatisfied(bool satisfied) noexcept { is_satisfied_ = satisfied; }

private:
    EntityId id_;
    RelationType type_;
    std::vector<EntityId> entities_;
    bool is_satisfied_;
};

/**
 * @brief Smart dimensioning with automatic placement
 */
class SmartDimensioning {
public:
    /**
     * @brief Constructor
     */
    SmartDimensioning(const EntityId& sketch_id) : sketch_id_(sketch_id) {}

    /**
     * @brief Suggest dimension placement for entity
     */
    [[nodiscard]] Vector3D suggestDimensionPosition(const EntityId& entity_id,
                                                   const Vector3D& entity_center) noexcept;

    /**
     * @brief Auto-dimension sketch (suggest all dimensions)
     */
    [[nodiscard]] std::vector<std::pair<EntityId, Vector3D>> autoDimensionSketch(
        const std::vector<EntityId>& entities) noexcept;

    /**
     * @brief Place dimension with collision avoidance
     */
    [[nodiscard]] bool placeDimensionSmart(const EntityId& dimension_id,
                                          const Vector3D& suggested_pos) noexcept;

    /**
     * @brief Get dimension positions
     */
    [[nodiscard]] const std::map<EntityId, Vector3D>& getDimensionPositions() const noexcept {
        return dimension_positions_;
    }

private:
    EntityId sketch_id_;
    std::map<EntityId, Vector3D> dimension_positions_;
    double min_spacing_ = 5.0;  // mm between dimensions
};

/**
 * @brief Bezier and B-spline curve tools
 */
class CurveTools {
public:
    /**
     * @brief Curve type
     */
    enum class CurveType {
        LINE,
        CIRCLE,
        ARC,
        BEZIER,
        BSPLINE,
        INTERPOLATED,
    };

    /**
     * @brief Create Bezier curve from control points
     */
    [[nodiscard]] bool createBezierCurve(const std::vector<Vector3D>& control_points,
                                        int degree = 3) noexcept;

    /**
     * @brief Create B-spline from control points
     */
    [[nodiscard]] bool createBSpline(const std::vector<Vector3D>& control_points,
                                    const std::vector<double>& knots,
                                    int degree = 3) noexcept;

    /**
     * @brief Interpolate curve through points
     */
    [[nodiscard]] bool createInterpolatedCurve(const std::vector<Vector3D>& points) noexcept;

    /**
     * @brief Evaluate curve at parameter
     */
    [[nodiscard]] Vector3D evaluateCurve(double parameter) const noexcept;

    /**
     * @brief Get curve derivative
     */
    [[nodiscard]] Vector3D getCurveTangent(double parameter) const noexcept;

    /**
     * @brief Get curve curvature
     */
    [[nodiscard]] double getCurveCurvature(double parameter) const noexcept;

private:
    CurveType curve_type_ = CurveType::BEZIER;
    std::vector<Vector3D> control_points_;
    std::vector<double> knots_;
    int degree_ = 3;
};

/**
 * @brief Advanced sketch solver with better diagnostics
 */
class AdvancedSketchSolver {
public:
    /**
     * @brief Solver status
     */
    enum class SolverStatus {
        FULLY_CONSTRAINED,    // 0 DOF
        UNDER_CONSTRAINED,    // > 0 DOF
        OVER_CONSTRAINED,     // Conflicting constraints
        REDUNDANT,            // Extra non-conflicting constraints
        INCONSISTENT,         // No solution exists
    };

    /**
     * @brief Constructor
     */
    AdvancedSketchSolver(const EntityId& sketch_id) : sketch_id_(sketch_id) {}

    /**
     * @brief Solve sketch
     */
    [[nodiscard]] bool solve() noexcept;

    /**
     * @brief Get solver status
     */
    [[nodiscard]] SolverStatus getStatus() const noexcept { return status_; }

    /**
     * @brief Get degrees of freedom
     */
    [[nodiscard]] int getDegreesOfFreedom() const noexcept { return dof_; }

    /**
     * @brief Diagnose under-constrained entities
     */
    [[nodiscard]] std::vector<EntityId> diagnoseFreedom() const noexcept {
        return free_entities_;
    }

    /**
     * @brief Find conflicting constraints
     */
    [[nodiscard]] std::vector<std::pair<EntityId, EntityId>> findConflicts() const noexcept {
        return conflicting_pairs_;
    }

    /**
     * @brief Suggest constraints to fully constrain
     */
    [[nodiscard]] std::vector<std::string> suggestConstraints() const noexcept;

private:
    EntityId sketch_id_;
    SolverStatus status_ = SolverStatus::UNDER_CONSTRAINED;
    int dof_ = 0;
    std::vector<EntityId> free_entities_;
    std::vector<std::pair<EntityId, EntityId>> conflicting_pairs_;
};

/**
 * @brief Advanced sketcher coordinator
 */
class AdvancedSketcher {
public:
    /**
     * @brief Constructor
     */
    AdvancedSketcher(const EntityId& sketch_id)
        : sketch_id_(sketch_id),
          smart_dimensioning_(sketch_id),
          solver_(sketch_id) {}

    /**
     * @brief Add construction geometry
     */
    [[nodiscard]] std::shared_ptr<ConstructionGeometry> addConstructionGeometry(
        ConstructionGeometry::GeometryType type) noexcept;

    /**
     * @brief Add geometric relation
     */
    [[nodiscard]] std::shared_ptr<GeometricRelation> addRelation(
        GeometricRelation::RelationType type) noexcept;

    /**
     * @brief Get smart dimensioning
     */
    [[nodiscard]] SmartDimensioning& getSmartDimensioning() noexcept {
        return smart_dimensioning_;
    }

    /**
     * @brief Get curve tools
     */
    [[nodiscard]] CurveTools& getCurveTools() noexcept {
        return curve_tools_;
    }

    /**
     * @brief Get advanced solver
     */
    [[nodiscard]] AdvancedSketchSolver& getSolver() noexcept {
        return solver_;
    }

    /**
     * @brief Get construction geometry count
     */
    [[nodiscard]] size_t getConstructionGeometryCount() const noexcept {
        return construction_geometry_.size();
    }

    /**
     * @brief Get relation count
     */
    [[nodiscard]] size_t getRelationCount() const noexcept {
        return relations_.size();
    }

private:
    EntityId sketch_id_;
    std::vector<std::shared_ptr<ConstructionGeometry>> construction_geometry_;
    std::vector<std::shared_ptr<GeometricRelation>> relations_;
    SmartDimensioning smart_dimensioning_;
    CurveTools curve_tools_;
    AdvancedSketchSolver solver_;
};

} // namespace BendCore::CadCore
