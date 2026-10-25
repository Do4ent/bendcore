#pragma once

#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <string>
#include <cmath>

namespace BendCore::CadCore {

/**
 * @brief Revolve feature: rotate profile around axis
 *
 * Rotates a 2D profile (sketch) around an axis to create a 3D solid.
 * Essential for creating rotational symmetric parts (shafts, wheels, cones).
 *
 * Example: Rotating a circle profile 360° creates a sphere,
 *          rotating a semicircle creates a full cylinder
 */
class RevolveFeature {
public:
    /**
     * @brief Axis of revolution
     */
    enum class AxisType {
        X,          // X axis
        Y,          // Y axis
        Z,          // Z axis
        CUSTOM      // Custom vector axis
    };

    /**
     * @brief Revolve configuration
     */
    struct Config {
        EntityId profile_sketch;     // 2D sketch to revolve
        AxisType axis_type = AxisType::Z;
        Vector3D axis_origin = {0.0, 0.0, 0.0};  // Axis start point
        Vector3D axis_direction = {0.0, 0.0, 1.0};  // Axis direction (normalized)
        double angle = 360.0;        // Revolution angle (degrees)
        bool bidirectional = false;  // Revolve both sides (only if angle < 360)
        bool thin_wall = false;      // Create thin shell instead of solid
        double thickness = 0.0;      // Shell thickness (if thin_wall=true)
        double tolerance = 0.01;     // Surface accuracy (mm)
    };

    /**
     * @brief Create revolve feature
     */
    RevolveFeature(const EntityId& id, const std::string& name);

    /**
     * @brief Get feature ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Get feature name
     */
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /**
     * @brief Set revolve configuration
     */
    void setConfig(const Config& config) noexcept { config_ = config; }

    /**
     * @brief Get revolve configuration
     */
    [[nodiscard]] const Config& getConfig() const noexcept { return config_; }

    /**
     * @brief Set revolution axis (preset)
     */
    void setAxis(AxisType axis) noexcept {
        config_.axis_type = axis;
        switch (axis) {
            case AxisType::X:
                config_.axis_direction = {1.0, 0.0, 0.0};
                break;
            case AxisType::Y:
                config_.axis_direction = {0.0, 1.0, 0.0};
                break;
            case AxisType::Z:
                config_.axis_direction = {0.0, 0.0, 1.0};
                break;
            case AxisType::CUSTOM:
                // Keep existing direction
                break;
        }
    }

    /**
     * @brief Set custom axis
     */
    void setCustomAxis(const Vector3D& origin, const Vector3D& direction) noexcept {
        config_.axis_type = AxisType::CUSTOM;
        config_.axis_origin = origin;
        config_.axis_direction = direction.normalized();
    }

    /**
     * @brief Set revolution angle
     */
    void setAngle(double degrees) noexcept {
        config_.angle = degrees;
        // Ensure angle is positive and <= 360
        while (config_.angle < 0) config_.angle += 360.0;
        while (config_.angle > 360.0) config_.angle -= 360.0;
    }

    /**
     * @brief Set thin wall mode
     */
    void setThinWall(double thickness_mm) noexcept {
        config_.thin_wall = true;
        config_.thickness = thickness_mm;
    }

    /**
     * @brief Check if full revolution (360°)
     */
    [[nodiscard]] bool isFullRevolution() const noexcept {
        return std::abs(config_.angle - 360.0) < 0.001;
    }

    /**
     * @brief Validate revolve feasibility
     */
    [[nodiscard]] bool validate() const noexcept;

    /**
     * @brief Check if profile is defined
     */
    [[nodiscard]] bool hasValidProfile() const noexcept {
        return config_.profile_sketch != EntityId();
    }

private:
    EntityId id_;
    std::string name_;
    Config config_;
};

} // namespace BendCore::CadCore
