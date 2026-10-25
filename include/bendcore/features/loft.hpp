#pragma once

#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <vector>
#include <string>

namespace BendCore::CadCore {

/**
 * @brief Loft feature: surface between multiple profiles
 *
 * Creates a smooth surface that transitions between two or more profiles.
 * Used for organic shapes, blended transitions, and complex surfaces.
 *
 * Example: Transition between circular and rectangular cross-sections
 */
class LoftFeature {
public:
    /**
     * @brief Loft profile
     */
    struct Profile {
        EntityId sketch_id;      // Sketch defining the profile
        double blend_parameter;  // 0.0 to 1.0 along loft path
        bool is_start = false;
        bool is_end = false;
    };

    /**
     * @brief Loft configuration
     */
    struct Config {
        std::vector<Profile> profiles;  // At least 2 profiles
        bool ruled_surface = false;     // Linear interpolation (fast)
        bool smooth_transition = true;  // Smooth interpolation (accurate)
        double tolerance = 0.01;        // Surface accuracy (mm)
        bool closed = false;            // Wrap back to first profile
    };

    /**
     * @brief Create loft feature
     */
    LoftFeature(const EntityId& id, const std::string& name);

    /**
     * @brief Get feature ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Get feature name
     */
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /**
     * @brief Set loft configuration
     */
    void setConfig(const Config& config) noexcept { config_ = config; }

    /**
     * @brief Get loft configuration
     */
    [[nodiscard]] const Config& getConfig() const noexcept { return config_; }

    /**
     * @brief Add profile to loft
     */
    void addProfile(const Profile& profile) noexcept {
        config_.profiles.push_back(profile);
    }

    /**
     * @brief Remove profile by index
     */
    bool removeProfile(size_t index) noexcept {
        if (index < config_.profiles.size()) {
            config_.profiles.erase(config_.profiles.begin() + index);
            return true;
        }
        return false;
    }

    /**
     * @brief Get number of profiles
     */
    [[nodiscard]] size_t getProfileCount() const noexcept {
        return config_.profiles.size();
    }

    /**
     * @brief Validate loft feasibility
     */
    [[nodiscard]] bool validate() const noexcept;

    /**
     * @brief Get surface generation capability
     */
    [[nodiscard]] bool canGenerateSurface() const noexcept {
        return config_.profiles.size() >= 2;
    }

private:
    EntityId id_;
    std::string name_;
    Config config_;
};

} // namespace BendCore::CadCore
