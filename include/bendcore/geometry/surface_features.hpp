#pragma once

#include "../document/entity_id.hpp"
#include "vector.hpp"
#include <vector>
#include <optional>
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief Profile for surface modeling (Loft/Sweep)
 *
 * Represents a closed or open wire/curve that will be used as a 2D section
 * in surface operations. Can be a sketch, edge loop, or 3D curve.
 */
class SurfaceProfile {
public:
    /**
     * @brief Profile type
     */
    enum class ProfileType {
        CLOSED_LOOP,      // Closed wire (face)
        OPEN_CURVE,       // Open curve
        POINT_CLOUD,      // Discrete points
        SKETCH,           // 2D sketch
        EDGE_SEQUENCE,    // Connected edges
    };

    /**
     * @brief Constructor
     */
    SurfaceProfile(const EntityId& id, ProfileType type)
        : id_(id), type_(type) {}

    /**
     * @brief Get profile ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Get profile type
     */
    [[nodiscard]] ProfileType getType() const noexcept { return type_; }

    /**
     * @brief Add control point
     */
    void addControlPoint(const Vector3D& point) noexcept {
        control_points_.push_back(point);
    }

    /**
     * @brief Get all control points
     */
    [[nodiscard]] const std::vector<Vector3D>& getControlPoints() const noexcept {
        return control_points_;
    }

    /**
     * @brief Get bounding box (min corner)
     */
    [[nodiscard]] Vector3D getBoundingBoxMin() const noexcept;

    /**
     * @brief Get bounding box (max corner)
     */
    [[nodiscard]] Vector3D getBoundingBoxMax() const noexcept;

    /**
     * @brief Get profile length/perimeter
     */
    [[nodiscard]] double getLength() const noexcept;

    /**
     * @brief Check if profile is valid (has enough points)
     */
    [[nodiscard]] bool isValid() const noexcept;

private:
    EntityId id_;
    ProfileType type_;
    std::vector<Vector3D> control_points_;
};

/**
 * @brief Loft surface feature (interpolate between profiles)
 */
class LoftSurface {
public:
    /**
     * @brief Constructor
     */
    LoftSurface(const EntityId& id) : id_(id) {}

    /**
     * @brief Add profile to loft
     */
    void addProfile(const std::shared_ptr<SurfaceProfile>& profile) noexcept {
        profiles_.push_back(profile);
    }

    /**
     * @brief Get all profiles
     */
    [[nodiscard]] const std::vector<std::shared_ptr<SurfaceProfile>>& getProfiles() const noexcept {
        return profiles_;
    }

    /**
     * @brief Set loft parameters
     */
    void setCloseSurface(bool close) noexcept { close_surface_ = close; }
    void setMatchTangency(bool match) noexcept { match_tangency_ = match; }
    void setMaxDegree(int degree) noexcept { max_degree_ = degree; }

    /**
     * @brief Get loft parameters
     */
    [[nodiscard]] bool getCloseSurface() const noexcept { return close_surface_; }
    [[nodiscard]] bool getMatchTangency() const noexcept { return match_tangency_; }
    [[nodiscard]] int getMaxDegree() const noexcept { return max_degree_; }

    /**
     * @brief Validate loft
     */
    [[nodiscard]] bool validate() const noexcept;

    /**
     * @brief Generate loft surface
     */
    [[nodiscard]] EntityId generate() const noexcept;

private:
    EntityId id_;
    std::vector<std::shared_ptr<SurfaceProfile>> profiles_;
    bool close_surface_ = false;
    bool match_tangency_ = false;
    int max_degree_ = 8;
};

/**
 * @brief Sweep surface feature (sweep profile along path)
 */
class SweepSurface {
public:
    /**
     * @brief Constructor
     */
    SweepSurface(const EntityId& id, const std::shared_ptr<SurfaceProfile>& profile)
        : id_(id), profile_(profile) {}

    /**
     * @brief Set sweep path
     */
    void setPath(const std::shared_ptr<SurfaceProfile>& path) noexcept {
        path_ = path;
    }

    /**
     * @brief Get sweep path
     */
    [[nodiscard]] const std::shared_ptr<SurfaceProfile>& getPath() const noexcept {
        return path_;
    }

    /**
     * @brief Get sweep profile
     */
    [[nodiscard]] const std::shared_ptr<SurfaceProfile>& getProfile() const noexcept {
        return profile_;
    }

    /**
     * @brief Set sweep parameters
     */
    void setKeepContact(bool contact) noexcept { keep_contact_ = contact; }
    void setNormalToPath(bool normal) noexcept { normal_to_path_ = normal; }
    void setScaleProfile(double scale) noexcept { scale_profile_ = scale; }
    void setTwistAngle(double angle) noexcept { twist_angle_ = angle; }

    /**
     * @brief Get sweep parameters
     */
    [[nodiscard]] bool getKeepContact() const noexcept { return keep_contact_; }
    [[nodiscard]] bool getNormalToPath() const noexcept { return normal_to_path_; }
    [[nodiscard]] double getScaleProfile() const noexcept { return scale_profile_; }
    [[nodiscard]] double getTwistAngle() const noexcept { return twist_angle_; }

    /**
     * @brief Validate sweep
     */
    [[nodiscard]] bool validate() const noexcept;

    /**
     * @brief Generate sweep surface
     */
    [[nodiscard]] EntityId generate() const noexcept;

private:
    EntityId id_;
    std::shared_ptr<SurfaceProfile> profile_;
    std::shared_ptr<SurfaceProfile> path_;
    bool keep_contact_ = true;
    bool normal_to_path_ = true;
    double scale_profile_ = 1.0;
    double twist_angle_ = 0.0;
};

/**
 * @brief Surface quality metrics
 */
struct SurfaceQualityMetrics {
    double surface_area = 0.0;
    double curvature_continuity = 0.0;  // G0, G1, G2
    size_t triangle_count = 0;
    size_t degenerate_triangles = 0;
    double min_triangle_quality = 0.0;  // 0.0 (bad) to 1.0 (perfect)
    bool is_manifold = false;
    bool is_closed = false;
};

/**
 * @brief Surface analyzer
 */
class SurfaceAnalyzer {
public:
    /**
     * @brief Analyze surface quality
     */
    [[nodiscard]] static SurfaceQualityMetrics analyzeSurface(const EntityId& surface_id) noexcept;

    /**
     * @brief Check surface for defects
     */
    [[nodiscard]] static bool validateSurface(const EntityId& surface_id) noexcept;

    /**
     * @brief Repair surface issues
     */
    [[nodiscard]] static bool repairSurface(const EntityId& surface_id) noexcept;
};

} // namespace BendCore::CadCore
