#include "bendcore/features/feature_system.hpp"
#include <algorithm>

namespace BendCore::CadCore {

EntityId FeatureSystem::createFeature(FeatureType type, const std::string& name) noexcept {
    EntityId newId;
    Feature feature;
    feature.id = newId;
    feature.type = type;
    feature.name = name;
    feature.isSuppressed = false;
    feature.revision = 0;

    features_[newId] = feature;
    return newId;
}

bool FeatureSystem::addConstraint(const EntityId& featureId, const Constraint& constraint) noexcept {
    auto it = features_.find(featureId);
    if (it == features_.end()) {
        return false;
    }

    it->second.constraints.push_back(constraint);
    it->second.revision++;
    return true;
}

const Feature* FeatureSystem::getFeature(const EntityId& id) const noexcept {
    auto it = features_.find(id);
    if (it == features_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<const Feature*> FeatureSystem::getAllFeatures() const noexcept {
    std::vector<const Feature*> result;
    for (const auto& pair : features_) {
        result.push_back(&pair.second);
    }
    return result;
}

std::vector<EntityId> FeatureSystem::getChildren(const EntityId& featureId) const noexcept {
    auto it = features_.find(featureId);
    if (it == features_.end()) {
        return {};
    }
    return it->second.childIds;
}

bool FeatureSystem::setParent(const EntityId& featureId, const EntityId& parentId) noexcept {
    auto it = features_.find(featureId);
    if (it == features_.end()) {
        return false;
    }

    // Find and remove from old parent
    if (it->second.parentId != EntityId()) {
        auto oldParentIt = features_.find(it->second.parentId);
        if (oldParentIt != features_.end()) {
            auto& childIds = oldParentIt->second.childIds;
            childIds.erase(
                std::remove(childIds.begin(), childIds.end(), featureId),
                childIds.end()
            );
        }
    }

    // Add to new parent
    if (parentId != EntityId()) {
        auto newParentIt = features_.find(parentId);
        if (newParentIt == features_.end()) {
            return false;
        }
        newParentIt->second.childIds.push_back(featureId);
    }

    it->second.parentId = parentId;
    it->second.revision++;
    return true;
}

bool FeatureSystem::suppressFeature(const EntityId& featureId) noexcept {
    auto it = features_.find(featureId);
    if (it == features_.end()) {
        return false;
    }

    it->second.isSuppressed = true;
    it->second.revision++;

    // Suppress all children
    for (const auto& childId : it->second.childIds) {
        suppressFeature(childId);
    }

    return true;
}

bool FeatureSystem::unsuppressFeature(const EntityId& featureId) noexcept {
    auto it = features_.find(featureId);
    if (it == features_.end()) {
        return false;
    }

    it->second.isSuppressed = false;
    it->second.revision++;
    return true;
}

bool FeatureSystem::validateFeatureTree() const noexcept {
    // Check for cycles
    for (const auto& pair : features_) {
        const Feature& feature = pair.second;

        // Check parent exists
        if (feature.parentId != EntityId()) {
            if (features_.find(feature.parentId) == features_.end()) {
                return false;
            }
        }

        // Check all children exist
        for (const auto& childId : feature.childIds) {
            if (features_.find(childId) == features_.end()) {
                return false;
            }
        }
    }

    return true;
}

std::vector<EntityId> FeatureSystem::getBuildOrder() const noexcept {
    std::vector<EntityId> order;
    std::map<EntityId, int> inDegree;

    // Initialize in-degree
    for (const auto& pair : features_) {
        if (inDegree.find(pair.first) == inDegree.end()) {
            inDegree[pair.first] = 0;
        }
        for (const auto& childId : pair.second.childIds) {
            inDegree[childId]++;
        }
    }

    // Find all features with no parents (in-degree 0)
    std::vector<EntityId> queue;
    for (const auto& pair : inDegree) {
        if (pair.second == 0) {
            queue.push_back(pair.first);
        }
    }

    // Topological sort
    while (!queue.empty()) {
        EntityId current = queue.front();
        queue.erase(queue.begin());
        order.push_back(current);

        auto it = features_.find(current);
        if (it != features_.end()) {
            for (const auto& childId : it->second.childIds) {
                inDegree[childId]--;
                if (inDegree[childId] == 0) {
                    queue.push_back(childId);
                }
            }
        }
    }

    return order;
}

} // namespace BendCore::CadCore
