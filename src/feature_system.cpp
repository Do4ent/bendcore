#include "bendcore/features/feature_system.hpp"
#include <functional>
#include <set>

namespace BendCore::CadCore {

EntityId FeatureSystem::createFeature(FeatureType type, const std::string& name) noexcept {
    Feature f;
    f.id = EntityId::generate();
    f.type = type;
    f.name = name;
    f.isSuppressed = false;
    f.revision = 0;
    features_[f.id] = f;
    return f.id;
}

bool FeatureSystem::addConstraint(const EntityId& featureId, const Constraint& constraint) noexcept {
    auto it = features_.find(featureId);
    if (it == features_.end()) return false;

    it->second.constraints.push_back(constraint);
    it->second.revision++;
    return true;
}

const Feature* FeatureSystem::getFeature(const EntityId& id) const noexcept {
    auto it = features_.find(id);
    return it != features_.end() ? &it->second : nullptr;
}

std::vector<const Feature*> FeatureSystem::getAllFeatures() const noexcept {
    std::vector<const Feature*> result;
    for (const auto& [id, feature] : features_) {
        result.push_back(&feature);
    }
    return result;
}

std::vector<EntityId> FeatureSystem::getChildren(const EntityId& featureId) const noexcept {
    std::vector<EntityId> result;
    for (const auto& [id, feature] : features_) {
        if (feature.parentId == featureId) {
            result.push_back(id);
        }
    }
    return result;
}

bool FeatureSystem::setParent(const EntityId& featureId, const EntityId& parentId) noexcept {
    auto fit = features_.find(featureId);
    auto pit = features_.find(parentId);

    if (fit == features_.end() || pit == features_.end()) return false;

    fit->second.parentId = parentId;
    pit->second.childIds.push_back(featureId);
    fit->second.revision++;
    pit->second.revision++;

    return true;
}

bool FeatureSystem::suppressFeature(const EntityId& featureId) noexcept {
    auto it = features_.find(featureId);
    if (it == features_.end()) return false;

    it->second.isSuppressed = true;
    it->second.revision++;
    return true;
}

bool FeatureSystem::unsuppressFeature(const EntityId& featureId) noexcept {
    auto it = features_.find(featureId);
    if (it == features_.end()) return false;

    it->second.isSuppressed = false;
    it->second.revision++;
    return true;
}

bool FeatureSystem::validateFeatureTree() const noexcept {
    for (const auto& [id, feature] : features_) {
        if (feature.parentId == EntityId()) continue;

        if (features_.find(feature.parentId) == features_.end()) {
            return false;
        }
    }

    return true;
}

std::vector<EntityId> FeatureSystem::getBuildOrder() const noexcept {
    std::vector<EntityId> order;
    std::set<EntityId> visited;

    // Find root features (no parent)
    for (const auto& [id, feature] : features_) {
        if ((feature.parentId == EntityId()) && visited.find(id) == visited.end()) {
            // DFS to build topological order
            std::function<void(const EntityId&)> dfs = [&](const EntityId& fid) {
                visited.insert(fid);
                order.push_back(fid);

                for (const auto& childId : getChildren(fid)) {
                    if (visited.find(childId) == visited.end()) {
                        dfs(childId);
                    }
                }
            };

            dfs(id);
        }
    }

    return order;
}

} // namespace BendCore::CadCore
