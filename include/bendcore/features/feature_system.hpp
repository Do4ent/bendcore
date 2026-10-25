#pragma once

#include "../document/entity_id.hpp"
#include <string>
#include <vector>
#include <map>

namespace BendCore::CadCore {

/**
 * @brief Feature type enumeration
 */
enum class FeatureType {
    SKETCH,
    PAD,
    POCKET,
    HOLE,
    FILLET,
    CHAMFER,
    PATTERN,
    MIRROR,
    CUSTOM
};

/**
 * @brief Constraint for parametric features
 */
struct Constraint {
    EntityId id;
    std::string name;
    std::string expression;
    double value = 0.0;
    bool isActive = true;
    uint64_t revision = 0;
};

/**
 * @brief Feature in the feature tree
 */
struct Feature {
    EntityId id;
    FeatureType type;
    std::string name;
    EntityId parentId;
    std::vector<EntityId> childIds;
    std::vector<Constraint> constraints;
    bool isSuppressed = false;
    uint64_t revision = 0;
};

/**
 * @brief Feature system manager
 */
class FeatureSystem {
public:
    FeatureSystem() noexcept = default;

    /**
     * @brief Create feature
     */
    EntityId createFeature(FeatureType type, const std::string& name) noexcept;

    /**
     * @brief Add constraint to feature
     */
    bool addConstraint(const EntityId& featureId, const Constraint& constraint) noexcept;

    /**
     * @brief Get feature
     */
    const Feature* getFeature(const EntityId& id) const noexcept;

    /**
     * @brief Get all features
     */
    std::vector<const Feature*> getAllFeatures() const noexcept;

    /**
     * @brief Get feature children
     */
    std::vector<EntityId> getChildren(const EntityId& featureId) const noexcept;

    /**
     * @brief Set parent feature
     */
    bool setParent(const EntityId& featureId, const EntityId& parentId) noexcept;

    /**
     * @brief Suppress feature
     */
    bool suppressFeature(const EntityId& featureId) noexcept;

    /**
     * @brief Unsuppress feature
     */
    bool unsuppressFeature(const EntityId& featureId) noexcept;

    /**
     * @brief Validate feature tree
     */
    bool validateFeatureTree() const noexcept;

    /**
     * @brief Get build order (topological sort of features)
     */
    std::vector<EntityId> getBuildOrder() const noexcept;

private:
    std::map<EntityId, Feature> features_;
};

} // namespace BendCore::CadCore
