#pragma once

#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <string>

namespace BendCore::CadCore {

/**
 * @brief Sweep feature: profile along a path
 *
 * Extrudes a profile (sketch) along a path (edge or curve).
 * Creates complex shapes by sweeping a cross-section along a trajectory.
 *
 * Example: Tube following a curved path, blade on a turbine
 */
class SweepFeature {
public:
    /**
     * @brief Sweep configuration
     */
    struct Config {
        EntityId profile_sketch;     // 2D sketch to sweep
        EntityId path_entity;        // Edge or curve to follow
        double scale_start = 1.0;    // Profile scale at start
        double scale_end = 1.0;      // Profile scale at end
        double twist_angle = 0.0;    // Rotation along path (degrees)
        Vector3D normal_direction;   // Initial normal direction
        bool keep_profile_normal = true;  // Maintain orientation to path
        bool closed_profile = false; // Close the swept surface
        double tolerance = 0.01;     // Surface accuracy (mm)
    };

    /**
     * @brief Create sweep feature
     */
    SweepFeature(const EntityId& id, const std::string& name);

    /**
     * @brief Get feature ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Get feature name
     */
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /**
     * @brief Set sweep configuration
     */
    void setConfig(const Config& config) noexcept { config_ = config; }

    /**
     * @brief Get sweep configuration
     */
    [[nodiscard]] const Config& getConfig() const noexcept { return config_; }

    /**
     * @brief Set scale along path
     */
    void setScale(double start, double end) noexcept {
        config_.scale_start = start;
        config_.scale_end = end;
    }

    /**
     * @brief Set twist rotation
     */
    void setTwist(double angle_degrees) noexcept {
        config_.twist_angle = angle_degrees;
    }

    /**
     * @brief Validate sweep feasibility
     */
    [[nodiscard]] bool validate() const noexcept;

    /**
     * @brief Check if sweep path is valid
     */
    [[nodiscard]] bool hasValidPath() const noexcept {
        return config_.path_entity != EntityId();
    }

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
