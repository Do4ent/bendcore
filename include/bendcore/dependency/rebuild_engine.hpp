#pragma once

#include "dependency_graph.hpp"
#include <set>

namespace BendCore::CadCore {

/**
 * @brief Incremental rebuild engine with dependency resolution
 */
class RebuildEngine {
public:
    RebuildEngine(DependencyGraph& graph) noexcept;

    /**
     * @brief Mark entity as changed
     */
    void markChanged(const EntityId& node) noexcept;

    /**
     * @brief Get entities that need rebuilding
     */
    std::vector<EntityId> getAffected() const noexcept;

    /**
     * @brief Get rebuild order
     */
    std::vector<EntityId> getRebuildOrder() const noexcept;

    /**
     * @brief Clear changed set
     */
    void clearChanged() noexcept { changed_.clear(); }

    /**
     * @brief Get changed count
     */
    size_t getChangedCount() const noexcept { return changed_.size(); }

private:
    DependencyGraph& graph_;
    std::set<EntityId> changed_;
};

} // namespace BendCore::CadCore
