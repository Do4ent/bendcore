#include "bendcore/dependency/dependency_graph.hpp"
#include <algorithm>

namespace BendCore::CadCore {

void DependencyGraph::addDependency(const EntityId& dependent, const EntityId& dependency) noexcept {
    graph_[dependent].insert(dependency);
    reverse_[dependency].insert(dependent);
}

void DependencyGraph::removeDependency(const EntityId& dependent, const EntityId& dependency) noexcept {
    graph_[dependent].erase(dependency);
    reverse_[dependency].erase(dependent);
}

std::set<EntityId> DependencyGraph::getDependencies(const EntityId& node) const noexcept {
    auto it = graph_.find(node);
    return it != graph_.end() ? it->second : std::set<EntityId>();
}

std::set<EntityId> DependencyGraph::getDependents(const EntityId& node) const noexcept {
    auto it = reverse_.find(node);
    return it != reverse_.end() ? it->second : std::set<EntityId>();
}

bool DependencyGraph::hasCycle() const noexcept {
    std::set<EntityId> visited;
    std::set<EntityId> visiting;

    for (const auto& [node, _] : graph_) {
        if (visited.find(node) == visited.end()) {
            if (hasCycleDFS(node, visiting, visited)) {
                return true;
            }
        }
    }
    return false;
}

std::vector<EntityId> DependencyGraph::topologicalSort() const noexcept {
    std::vector<EntityId> result;
    std::map<EntityId, size_t> inDegree;

    // Initialize all nodes with 0 in-degree
    auto allNodes = getNodes();
    for (const auto& node : allNodes) {
        inDegree[node] = 0;
    }

    // Calculate in-degrees: each dependency increases in-degree of the dependent
    for (const auto& [node, deps] : graph_) {
        inDegree[node] = deps.size();
    }

    std::vector<EntityId> queue;
    for (const auto& [node, degree] : inDegree) {
        if (degree == 0) {
            queue.push_back(node);
        }
    }

    while (!queue.empty()) {
        EntityId node = queue.back();
        queue.pop_back();
        result.push_back(node);

        auto it = reverse_.find(node);
        if (it != reverse_.end()) {
            for (const auto& dependent : it->second) {
                inDegree[dependent]--;
                if (inDegree[dependent] == 0) {
                    queue.push_back(dependent);
                }
            }
        }
    }

    return result;
}

bool DependencyGraph::hasEdge(const EntityId& from, const EntityId& to) const noexcept {
    auto it = graph_.find(from);
    return it != graph_.end() && it->second.find(to) != it->second.end();
}

std::set<EntityId> DependencyGraph::getNodes() const noexcept {
    std::set<EntityId> nodes;
    for (const auto& [node, _] : graph_) {
        nodes.insert(node);
    }
    for (const auto& [node, _] : reverse_) {
        nodes.insert(node);
    }
    return nodes;
}

void DependencyGraph::clear() noexcept {
    graph_.clear();
    reverse_.clear();
}

bool DependencyGraph::hasCycleDFS(const EntityId& node, std::set<EntityId>& visiting,
                                  std::set<EntityId>& visited) const noexcept {
    visiting.insert(node);

    auto it = graph_.find(node);
    if (it != graph_.end()) {
        for (const auto& dep : it->second) {
            if (visiting.find(dep) != visiting.end()) {
                return true;
            }
            if (visited.find(dep) == visited.end()) {
                if (hasCycleDFS(dep, visiting, visited)) {
                    return true;
                }
            }
        }
    }

    visiting.erase(node);
    visited.insert(node);
    return false;
}

} // namespace BendCore::CadCore
