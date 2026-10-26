#pragma once

#include "../document/entity_id.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <cstdint>

namespace BendCore::CadCore {

/**
 * @brief Locating feature (datum/reference point for fixture)
 */
struct LocatingFeature {
    EntityId feature_id;
    std::string feature_type;  // Hole, surface, edge, etc.
    std::string description;
    double x = 0.0, y = 0.0, z = 0.0;  // Coordinates
    bool is_primary = false;             // Primary locating feature
    uint32_t redundancy_count = 0;       // Number of redundant features
};

/**
 * @brief Clamping element in fixture
 */
struct ClampingElement {
    EntityId clamp_id;
    std::string clamp_type;        // Vise, clamp, jack, etc.
    std::string description;
    double clamping_force_n = 0.0; // Clamping force in Newtons
    double travel_mm = 0.0;         // Travel distance in mm
    std::vector<EntityId> contact_points;  // Entity IDs of contact points
    bool is_adjustable = false;
};

/**
 * @brief Fixture design and analysis
 */
struct FixtureDesign {
    EntityId fixture_id;
    std::string fixture_name;
    std::string description;
    std::vector<LocatingFeature> locating_features;
    std::vector<ClampingElement> clamping_elements;
    double total_cost = 0.0;
    double lead_time_days = 0.0;
    std::string material;
    uint32_t setup_time_minutes = 0;
};

/**
 * @brief Accessibility analysis result
 */
struct AccessibilityAnalysis {
    bool is_accessible = true;
    std::vector<std::string> blocked_areas;
    std::vector<std::string> obstructed_paths;
    double clearance_mm = 0.0;  // Minimum clearance
    std::vector<std::string> recommendations;
};

/**
 * @brief Fixture Designer - Generates fixture designs
 *
 * Provides:
 * - Fixture generation
 * - Locating feature design
 * - Clamping analysis
 * - Accessibility checking
 * - Cost estimation
 */
class FixtureDesigner {
public:
    /**
     * @brief Constructor
     * @param assembly_id ID of assembly to design fixture for
     */
    explicit FixtureDesigner(const EntityId& assembly_id) noexcept
        : assembly_id_(assembly_id) {}

    /**
     * @brief Create a new fixture design
     * @param fixture_name Fixture name
     * @return Fixture ID
     */
    EntityId createFixture(const std::string& fixture_name);

    /**
     * @brief Get fixture design
     * @param fixture_id Fixture ID
     * @return Fixture design, or empty optional if not found
     */
    [[nodiscard]] std::optional<FixtureDesign> getFixture(const EntityId& fixture_id) const noexcept;

    /**
     * @brief Get all fixtures
     */
    [[nodiscard]] const std::vector<FixtureDesign>& getAllFixtures() const noexcept {
        return fixtures_;
    }

    /**
     * @brief Add locating feature to fixture
     * @param fixture_id Fixture ID
     * @param feature Locating feature
     * @return true if added, false if fixture not found
     */
    bool addLocatingFeature(const EntityId& fixture_id, const LocatingFeature& feature) noexcept;

    /**
     * @brief Add clamping element to fixture
     * @param fixture_id Fixture ID
     * @param clamp Clamping element
     * @return true if added, false if fixture not found
     */
    bool addClampingElement(const EntityId& fixture_id, const ClampingElement& clamp) noexcept;

    /**
     * @brief Analyze fixture accessibility
     * @param fixture_id Fixture ID
     * @return Accessibility analysis results
     */
    [[nodiscard]] AccessibilityAnalysis analyzeAccessibility(const EntityId& fixture_id) const;

    /**
     * @brief Calculate fixture cost
     * @param fixture_id Fixture ID
     * @return Estimated cost, or -1 if fixture not found
     */
    [[nodiscard]] double calculateFixtureCost(const EntityId& fixture_id) const noexcept;

    /**
     * @brief Calculate locating feature redundancy
     * @param fixture_id Fixture ID
     * @return Redundancy score (0.0 = no redundancy, 1.0 = fully redundant)
     */
    [[nodiscard]] double analyzeLocatingRedundancy(const EntityId& fixture_id) const noexcept;

    /**
     * @brief Calculate clamping force distribution
     * @param fixture_id Fixture ID
     * @return Vector of clamping forces at contact points
     */
    [[nodiscard]] std::vector<double> calculateClampingDistribution(
        const EntityId& fixture_id) const;

    /**
     * @brief Verify fixture adequacy (6-point locating, proper clamping)
     * @param fixture_id Fixture ID
     * @return Verification message, empty if all OK
     */
    [[nodiscard]] std::string verifyFixture(const EntityId& fixture_id) const;

    /**
     * @brief Generate fixture assembly time estimate
     * @param fixture_id Fixture ID
     * @return Setup time in minutes
     */
    [[nodiscard]] uint32_t estimateSetupTime(const EntityId& fixture_id) const noexcept;

    /**
     * @brief Remove fixture
     * @param fixture_id Fixture ID to remove
     * @return true if removed, false if not found
     */
    bool removeFixture(const EntityId& fixture_id) noexcept;

    /**
     * @brief Get total cost of all fixtures
     */
    [[nodiscard]] double getTotalFixtureCost() const noexcept;

    /**
     * @brief Get number of fixtures
     */
    [[nodiscard]] size_t getFixtureCount() const noexcept { return fixtures_.size(); }

    /**
     * @brief Export fixture design as text
     * @param fixture_id Fixture ID
     * @return Text representation of fixture
     */
    [[nodiscard]] std::string exportFixtureAsText(const EntityId& fixture_id) const;

private:
    EntityId assembly_id_;
    std::vector<FixtureDesign> fixtures_;

    // Helper functions
    bool findFixture(const EntityId& fixture_id) noexcept;
};

} // namespace BendCore::CadCore
