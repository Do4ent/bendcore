#pragma once

#include "../document/entity_id.hpp"
#include <map>
#include <set>
#include <vector>

namespace BendCore::CadCore {

/**
 * @brief Dependency graph for parameter and object relationships
 */
class DependencyGraph {
public:
    /**
     * @brief Add dependency: A depends on B
     */
    void addDependency(const EntityId& dependent, const EntityId& dependency) noexcept;

    /**
     * @brief Remove dependency
     */
    void removeDependency(const EntityId& dependent, const EntityId& dependency) noexcept;

    /**
     * @brief Get direct dependencies
     */
    std::set<EntityId> getDependencies(const EntityId& node) const noexcept;

    /**
     * @brief Get direct dependents
     */
    std::set<EntityId> getDependents(const EntityId& node) const noexcept;

    /**
     * @brief Detect cycles in graph
     */
    bool hasCycle() const noexcept;

    /**
     * @brief Get topological order using Kahn algorithm
     */
    std::vector<EntityId> topologicalSort() const noexcept;

    /**
     * @brief Check if edge exists
     */
    bool hasEdge(const EntityId& from, const EntityId& to) const noexcept;

    /**
     * @brief Get all nodes
     */
    std::set<EntityId> getNodes() const noexcept;

    /**
     * @brief Clear graph
     */
    void clear() noexcept;

private:
    std::map<EntityId, std::set<EntityId>> graph_;  // dependent -> dependencies
    std::map<EntityId, std::set<EntityId>> reverse_;  // dependency -> dependents

    bool hasCycleDFS(const EntityId& node, std::set<EntityId>& visiting,
                     std::set<EntityId>& visited) const noexcept;
};

} // namespace BendCore::CadCore
