#include "bendcore/dependency/rebuild_engine.hpp"

namespace BendCore::CadCore {

RebuildEngine::RebuildEngine(DependencyGraph& graph) noexcept
    : graph_(graph)
{}

void RebuildEngine::markChanged(const EntityId& node) noexcept {
    changed_.insert(node);
}

std::vector<EntityId> RebuildEngine::getAffected() const noexcept {
    std::set<EntityId> affected = changed_;

    // For each changed node, add its dependents
    for (const auto& node : changed_) {
        auto dependents = graph_.getDependents(node);
        affected.insert(dependents.begin(), dependents.end());
    }

    std::vector<EntityId> result(affected.begin(), affected.end());
    return result;
}

std::vector<EntityId> RebuildEngine::getRebuildOrder() const noexcept {
    auto affected = getAffected();
    auto full_order = graph_.topologicalSort();

    // Filter to affected nodes
    std::vector<EntityId> result;
    for (const auto& node : full_order) {
        if (std::find(affected.begin(), affected.end(), node) != affected.end()) {
            result.push_back(node);
        }
    }

    return result;
}

} // namespace BendCore::CadCore
