#pragma once

#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <vector>
#include <string>
#include <map>
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief Bend allowance for sheet metal operations
 */
class BendAllowance {
public:
    /**
     * @brief Constructor
     */
    BendAllowance(const EntityId& id, double material_thickness, double bend_radius)
        : id_(id), thickness_(material_thickness), radius_(bend_radius) {}

    /**
     * @brief Get bend allowance value (mm)
     */
    [[nodiscard]] double getBendAllowance() const noexcept;

    /**
     * @brief Get K-factor (determines neutral axis position)
     */
    [[nodiscard]] double getKFactor() const noexcept { return k_factor_; }

    /**
     * @brief Set K-factor (0.0 - 0.5, where 0.5 is center)
     */
    void setKFactor(double k) noexcept {
        if (k >= 0.0 && k <= 0.5) k_factor_ = k;
    }

    /**
     * @brief Get bend angle (degrees)
     */
    [[nodiscard]] double getBendAngle() const noexcept { return bend_angle_; }

    /**
     * @brief Set bend angle
     */
    void setBendAngle(double angle) noexcept { bend_angle_ = angle; }

    /**
     * @brief Set material (for bend allowance lookup)
     */
    void setMaterial(const std::string& material) noexcept { material_ = material; }

private:
    EntityId id_;
    double thickness_;
    double radius_;
    double k_factor_ = 0.33;  // Default K-factor
    double bend_angle_ = 90.0;  // Default 90 degrees
    std::string material_;
};

/**
 * @brief Unfold operation for sheet metal flattening
 */
class UnfoldSheet {
public:
    /**
     * @brief Constructor
     */
    UnfoldSheet(const EntityId& solid_id) : solid_id_(solid_id) {}

    /**
     * @brief Add bend edge with bend data
     */
    void addBendEdge(const EntityId& edge_id, const BendAllowance& allowance) noexcept {
        bend_edges_[edge_id] = std::make_shared<BendAllowance>(allowance);
    }

    /**
     * @brief Get all bend edges
     */
    [[nodiscard]] const std::map<EntityId, std::shared_ptr<BendAllowance>>& getBendEdges() const noexcept {
        return bend_edges_;
    }

    /**
     * @brief Perform unfold operation
     */
    [[nodiscard]] bool unfold() noexcept;

    /**
     * @brief Get unfolded profile
     */
    [[nodiscard]] std::vector<Vector3D> getUnfoldedProfile() const noexcept {
        return unfolded_profile_;
    }

    /**
     * @brief Export flat pattern to DXF
     */
    [[nodiscard]] bool exportToDXF(const std::string& filename) const noexcept;

private:
    EntityId solid_id_;
    std::map<EntityId, std::shared_ptr<BendAllowance>> bend_edges_;
    std::vector<Vector3D> unfolded_profile_;
};

/**
 * @brief Tube notch/intersection operation
 */
class TubeNotch {
public:
    /**
     * @brief Intersection type
     */
    enum class IntersectionType {
        BUTT,           // Butt joint
        OVERLAP,        // Overlapping tubes
        MITERED,        // Mitered connection
        FISHMOUTH,      // Fish mouth cut
        SADDLE,         // Saddle connection
    };

    /**
     * @brief Constructor
     */
    TubeNotch(const EntityId& main_tube_id, const EntityId& intersecting_tube_id)
        : main_tube_id_(main_tube_id), intersecting_tube_id_(intersecting_tube_id) {}

    /**
     * @brief Set intersection type
     */
    void setIntersectionType(IntersectionType type) noexcept { intersection_type_ = type; }

    /**
     * @brief Generate notch profile
     */
    [[nodiscard]] bool generateNotch() noexcept;

    /**
     * @brief Get notch curve points
     */
    [[nodiscard]] const std::vector<Vector3D>& getNotchCurve() const noexcept {
        return notch_curve_;
    }

    /**
     * @brief Export notch to cutting program
     */
    [[nodiscard]] bool exportForCutting(const std::string& format) const noexcept;

private:
    EntityId main_tube_id_;
    EntityId intersecting_tube_id_;
    IntersectionType intersection_type_ = IntersectionType::BUTT;
    std::vector<Vector3D> notch_curve_;
};

/**
 * @brief Stamping pattern for repeated features
 */
class StampingPattern {
public:
    /**
     * @brief Pattern type
     */
    enum class PatternType {
        LINEAR,         // Linear array
        CIRCULAR,       // Circular array
        CUSTOM,         // Custom distribution
    };

    /**
     * @brief Constructor
     */
    StampingPattern(const EntityId& feature_id) : feature_id_(feature_id) {}

    /**
     * @brief Create linear pattern
     */
    void createLinearPattern(const Vector3D& direction, double spacing, size_t count) noexcept;

    /**
     * @brief Create circular pattern
     */
    void createCircularPattern(const Vector3D& axis, double radius, size_t count) noexcept;

    /**
     * @brief Get pattern instances
     */
    [[nodiscard]] const std::vector<Vector3D>& getInstances() const noexcept {
        return instances_;
    }

    /**
     * @brief Apply pattern
     */
    [[nodiscard]] bool apply() noexcept;

private:
    EntityId feature_id_;
    PatternType pattern_type_ = PatternType::LINEAR;
    std::vector<Vector3D> instances_;
};

/**
 * @brief Rolled feature for cylindrical shapes
 */
class RolledFeature {
public:
    /**
     * @brief Constructor
     */
    RolledFeature(const EntityId& profile_id, double radius)
        : profile_id_(profile_id), roll_radius_(radius) {}

    /**
     * @brief Set number of rotations
     */
    void setRotations(double rotations) noexcept { rotations_ = rotations; }

    /**
     * @brief Get roll radius
     */
    [[nodiscard]] double getRollRadius() const noexcept { return roll_radius_; }

    /**
     * @brief Execute roll operation
     */
    [[nodiscard]] bool executeRoll() noexcept;

    /**
     * @brief Get rolled geometry
     */
    [[nodiscard]] bool isValid() const noexcept { return is_valid_; }

private:
    EntityId profile_id_;
    double roll_radius_;
    double rotations_ = 1.0;
    bool is_valid_ = false;
};

/**
 * @brief Bend table for material-specific bend allowances
 */
class BendTable {
public:
    /**
     * @brief Bend allowance entry
     */
    struct BendEntry {
        double thickness;    // Material thickness (mm)
        double min_radius;   // Minimum bend radius
        double max_radius;   // Maximum bend radius
        double allowance_90; // Bend allowance for 90 degrees
        std::string material;
    };

    /**
     * @brief Add material bend data
     */
    void addMaterialData(const BendEntry& entry) noexcept {
        materials_.push_back(entry);
    }

    /**
     * @brief Look up bend allowance
     */
    [[nodiscard]] double lookupBendAllowance(const std::string& material,
                                            double thickness,
                                            double angle) const noexcept;

    /**
     * @brief Get recommended bend radius
     */
    [[nodiscard]] double getRecommendedRadius(const std::string& material,
                                             double thickness) const noexcept;

    /**
     * @brief Export table to CSV
     */
    [[nodiscard]] std::string exportToCSV() const noexcept;

private:
    std::vector<BendEntry> materials_;
};

/**
 * @brief Sheet metal operations coordinator
 */
class SheetMetalOperations {
public:
    /**
     * @brief Constructor
     */
    SheetMetalOperations(const EntityId& assembly_id) : assembly_id_(assembly_id) {}

    /**
     * @brief Create unfold operation
     */
    [[nodiscard]] std::shared_ptr<UnfoldSheet> createUnfold(const EntityId& solid_id) noexcept;

    /**
     * @brief Create tube notch operation
     */
    [[nodiscard]] std::shared_ptr<TubeNotch> createNotch(const EntityId& main_tube,
                                                        const EntityId& intersecting) noexcept;

    /**
     * @brief Create stamping pattern
     */
    [[nodiscard]] std::shared_ptr<StampingPattern> createPattern(const EntityId& feature) noexcept;

    /**
     * @brief Create rolled feature
     */
    [[nodiscard]] std::shared_ptr<RolledFeature> createRolled(const EntityId& profile,
                                                             double radius) noexcept;

    /**
     * @brief Get bend table
     */
    [[nodiscard]] BendTable& getBendTable() noexcept { return bend_table_; }

    /**
     * @brief Get operation count
     */
    [[nodiscard]] size_t getOperationCount() const noexcept {
        return unfolds_.size() + notches_.size() + patterns_.size() + rolled_.size();
    }

private:
    EntityId assembly_id_;
    std::vector<std::shared_ptr<UnfoldSheet>> unfolds_;
    std::vector<std::shared_ptr<TubeNotch>> notches_;
    std::vector<std::shared_ptr<StampingPattern>> patterns_;
    std::vector<std::shared_ptr<RolledFeature>> rolled_;
    BendTable bend_table_;
};

} // namespace BendCore::CadCore
