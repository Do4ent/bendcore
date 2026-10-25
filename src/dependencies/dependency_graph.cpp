#include "bendcore/dependencies/dependency_graph.hpp"

#include <algorithm>
#include <queue>
#include <set>

namespace bendcore::dependencies {

// ==================== DependencyNode ====================

bool DependencyNode::operator<(const DependencyNode& other) const {
    if (entity_id == other.entity_id) {
        return param_name < other.param_name;
    }
    return entity_id < other.entity_id;
}

bool DependencyNode::operator==(const DependencyNode& other) const {
    return entity_id == other.entity_id && param_name == other.param_name;
}

// ==================== DependencyGraph ====================

DependencyGraph::DependencyGraph(CadDocument& doc, DocumentEventBus& bus)
    : document_(doc), event_bus_(bus) {}

bool DependencyGraph::add_dependency(
    EntityId dependent_entity,
    const std::string& dependent_param,
    EntityId dependency_entity,
    const std::string& dependency_param
) {
    // Проверить циклические зависимости перед добавлением
    if (would_create_cycle(dependent_entity, dependent_param, dependency_entity, dependency_param)) {
        return false;
    }

    auto dep_key = std::make_pair(dependent_entity, dependent_param);
    auto dependency_key = std::make_pair(dependency_entity, dependency_param);

    // Убедиться, что оба узла существуют
    if (nodes_.count(dep_key) == 0) {
        DependencyNode node;
        node.entity_id = dependent_entity;
        node.param_name = dependent_param;
        nodes_[dep_key] = node;
    }

    if (nodes_.count(dependency_key) == 0) {
        DependencyNode node;
        node.entity_id = dependency_entity;
        node.param_name = dependency_param;
        nodes_[dependency_key] = node;
    }

    // Добавить ребро
    nodes_[dep_key].predecessors.insert(dependency_key);
    nodes_[dependency_key].successors.insert(dep_key);

    // Инвалидировать кэш топологической сортировки
    invalidate_topo_cache();

    // Отметить узлы как грязные
    nodes_[dep_key].is_dirty = true;
    nodes_[dependency_key].is_dirty = true;

    notify_graph_changed();

    return true;
}

bool DependencyGraph::remove_dependency(
    EntityId dependent_entity,
    const std::string& dependent_param,
    EntityId dependency_entity,
    const std::string& dependency_param
) {
    auto dep_key = std::make_pair(dependent_entity, dependent_param);
    auto dependency_key = std::make_pair(dependency_entity, dependency_param);

    auto dep_it = nodes_.find(dep_key);
    auto dependency_it = nodes_.find(dependency_key);

    if (dep_it == nodes_.end() || dependency_it == nodes_.end()) {
        return false;
    }

    // Удалить ребро
    dep_it->second.predecessors.erase(dependency_key);
    dependency_it->second.successors.erase(dep_key);

    invalidate_topo_cache();
    notify_graph_changed();

    return true;
}

std::set<std::pair<EntityId, std::string>> DependencyGraph::get_dependents(
    EntityId entity_id,
    const std::string& param_name
) const {
    auto key = std::make_pair(entity_id, param_name);
    auto it = nodes_.find(key);
    if (it == nodes_.end()) {
        return {};
    }
    return it->second.successors;
}

std::set<std::pair<EntityId, std::string>> DependencyGraph::get_predecessors(
    EntityId entity_id,
    const std::string& param_name
) const {
    auto key = std::make_pair(entity_id, param_name);
    auto it = nodes_.find(key);
    if (it == nodes_.end()) {
        return {};
    }
    return it->second.predecessors;
}

bool DependencyGraph::has_cycle() const {
    std::set<std::pair<EntityId, std::string>> visited;
    std::set<std::pair<EntityId, std::string>> rec_stack;

    for (const auto& [node_key, _] : nodes_) {
        if (visited.count(node_key) == 0) {
            if (dfs_has_cycle(node_key, visited, rec_stack)) {
                return true;
            }
        }
    }

    return false;
}

bool DependencyGraph::would_create_cycle(
    EntityId dependent_entity,
    const std::string& dependent_param,
    EntityId dependency_entity,
    const std::string& dependency_param
) const {
    auto dep_key = std::make_pair(dependent_entity, dependent_param);
    auto dependency_key = std::make_pair(dependency_entity, dependency_param);

    // Проверить, есть ли путь от dependency к dep
    // Если есть, то добавление ребра dep -> dependency создаст цикл
    std::vector<std::pair<EntityId, std::string>> path;
    std::set<std::pair<EntityId, std::string>> visited;

    return dfs_find_path(dependency_key, dep_key, path, visited);
}

std::optional<DependencyPath> DependencyGraph::find_cycle() const {
    std::set<std::pair<EntityId, std::string>> visited;
    std::set<std::pair<EntityId, std::string>> rec_stack;
    std::vector<std::pair<EntityId, std::string>> current_path;

    for (const auto& [node_key, _] : nodes_) {
        if (visited.count(node_key) == 0) {
            // Здесь можно реализовать полный DFS для сохранения пути
            if (dfs_has_cycle(node_key, visited, rec_stack)) {
                // Цикл найден, но путь не сохраняется
                DependencyPath cycle_path;
                return cycle_path;
            }
        }
    }

    return std::nullopt;
}

std::optional<DependencyPath> DependencyGraph::find_path(
    EntityId source_entity,
    const std::string& source_param,
    EntityId target_entity,
    const std::string& target_param
) const {
    std::vector<std::pair<EntityId, std::string>> path;
    std::set<std::pair<EntityId, std::string>> visited;

    auto source_key = std::make_pair(source_entity, source_param);
    auto target_key = std::make_pair(target_entity, target_param);

    if (dfs_find_path(source_key, target_key, path, visited)) {
        DependencyPath result;
        result.nodes = path;
        result.cost = path.size();
        return result;
    }

    return std::nullopt;
}

std::set<std::pair<EntityId, std::string>> DependencyGraph::get_transitive_dependents(
    EntityId entity_id,
    const std::string& param_name
) const {
    std::set<std::pair<EntityId, std::string>> result;
    std::queue<std::pair<EntityId, std::string>> to_process;

    auto key = std::make_pair(entity_id, param_name);
    to_process.push(key);

    std::set<std::pair<EntityId, std::string>> visited;

    while (!to_process.empty()) {
        auto current = to_process.front();
        to_process.pop();

        if (visited.count(current)) {
            continue;
        }

        visited.insert(current);

        // Получить непосредственные зависимые
        auto dependents = get_dependents(current.first, current.second);
        for (const auto& dependent : dependents) {
            if (visited.count(dependent) == 0) {
                result.insert(dependent);
                to_process.push(dependent);
            }
        }
    }

    return result;
}

std::set<std::pair<EntityId, std::string>> DependencyGraph::get_transitive_predecessors(
    EntityId entity_id,
    const std::string& param_name
) const {
    std::set<std::pair<EntityId, std::string>> result;
    std::queue<std::pair<EntityId, std::string>> to_process;

    auto key = std::make_pair(entity_id, param_name);
    to_process.push(key);

    std::set<std::pair<EntityId, std::string>> visited;

    while (!to_process.empty()) {
        auto current = to_process.front();
        to_process.pop();

        if (visited.count(current)) {
            continue;
        }

        visited.insert(current);

        // Получить непосредственные предшественники
        auto predecessors = get_predecessors(current.first, current.second);
        for (const auto& predecessor : predecessors) {
            if (visited.count(predecessor) == 0) {
                result.insert(predecessor);
                to_process.push(predecessor);
            }
        }
    }

    return result;
}

int DependencyGraph::get_level(EntityId entity_id, const std::string& param_name) const {
    auto key = std::make_pair(entity_id, param_name);
    auto it = nodes_.find(key);
    if (it == nodes_.end()) {
        return -1;
    }
    return it->second.depth;
}

std::vector<std::pair<EntityId, std::string>> DependencyGraph::topological_sort() const {
    if (!topo_dirty_ && topo_cache_) {
        return *topo_cache_;
    }

    std::vector<std::pair<EntityId, std::string>> result;
    std::map<std::pair<EntityId, std::string>, int> in_degree;

    // Инициализировать in_degree для всех узлов
    for (const auto& [node_key, node] : nodes_) {
        in_degree[node_key] = node.predecessors.size();
    }

    // Найти все узлы с нулевой входящей степенью
    std::queue<std::pair<EntityId, std::string>> queue;
    for (const auto& [node_key, degree] : in_degree) {
        if (degree == 0) {
            queue.push(node_key);
        }
    }

    // Алгоритм Кахана
    while (!queue.empty()) {
        auto current = queue.front();
        queue.pop();

        result.push_back(current);

        auto it = nodes_.find(current);
        if (it != nodes_.end()) {
            for (const auto& successor : it->second.successors) {
                in_degree[successor]--;
                if (in_degree[successor] == 0) {
                    queue.push(successor);
                }
            }
        }
    }

    topo_cache_ = result;
    topo_dirty_ = false;

    return result;
}

void DependencyGraph::invalidate(EntityId entity_id, const std::string& param_name) {
    auto key = std::make_pair(entity_id, param_name);
    auto it = nodes_.find(key);
    if (it != nodes_.end()) {
        it->second.is_dirty = true;
    }

    mark_successors_dirty(entity_id, param_name);
}

std::vector<std::pair<EntityId, std::string>> DependencyGraph::get_dirty_parameters() const {
    std::vector<std::pair<EntityId, std::string>> result;
    for (const auto& [key, node] : nodes_) {
        if (node.is_dirty) {
            result.push_back(key);
        }
    }
    return result;
}

void DependencyGraph::remove_parameter(EntityId entity_id, const std::string& param_name) {
    auto key = std::make_pair(entity_id, param_name);
    auto it = nodes_.find(key);
    if (it == nodes_.end()) {
        return;
    }

    // Удалить все ребра
    for (const auto& pred : it->second.predecessors) {
        auto pred_it = nodes_.find(pred);
        if (pred_it != nodes_.end()) {
            pred_it->second.successors.erase(key);
        }
    }

    for (const auto& succ : it->second.successors) {
        auto succ_it = nodes_.find(succ);
        if (succ_it != nodes_.end()) {
            succ_it->second.predecessors.erase(key);
        }
    }

    nodes_.erase(it);
    invalidate_topo_cache();
}

void DependencyGraph::remove_entity(EntityId entity_id) {
    std::vector<std::pair<EntityId, std::string>> to_remove;

    for (const auto& [key, _] : nodes_) {
        if (key.first == entity_id) {
            to_remove.push_back(key);
        }
    }

    for (const auto& key : to_remove) {
        remove_parameter(key.first, key.second);
    }
}

std::vector<DependencyNode> DependencyGraph::get_all_nodes() const {
    std::vector<DependencyNode> result;
    for (const auto& [_, node] : nodes_) {
        result.push_back(node);
    }
    return result;
}

size_t DependencyGraph::node_count() const {
    return nodes_.size();
}

size_t DependencyGraph::edge_count() const {
    size_t count = 0;
    for (const auto& [_, node] : nodes_) {
        count += node.successors.size();
    }
    return count;
}

int DependencyGraph::max_depth() const {
    int max = -1;
    for (const auto& [_, node] : nodes_) {
        if (node.depth > max) {
            max = node.depth;
        }
    }
    return max;
}

size_t DependencyGraph::source_count() const {
    size_t count = 0;
    for (const auto& [_, node] : nodes_) {
        if (node.predecessors.empty()) {
            count++;
        }
    }
    return count;
}

std::vector<std::pair<EntityId, std::string>> DependencyGraph::get_source_nodes() const {
    std::vector<std::pair<EntityId, std::string>> result;
    for (const auto& [key, node] : nodes_) {
        if (node.predecessors.empty()) {
            result.push_back(key);
        }
    }
    return result;
}

void DependencyGraph::begin_batch_update() {
    batch_mode_ = true;
    batch_invalidated_.clear();
}

void DependencyGraph::end_batch_update() {
    batch_mode_ = false;
    invalidate_topo_cache();
    notify_graph_changed();
    batch_invalidated_.clear();
}

std::string DependencyGraph::export_to_json() const {
    // Упрощенная реализация
    std::string result = "{\"nodes\":[";
    bool first = true;
    for (const auto& [key, _] : nodes_) {
        if (!first) result += ",";
        result += "{\"entity\":\"" + key.first.uuid() + "\",\"param\":\"" + key.second + "\"}";
        first = false;
    }
    result += "]}";
    return result;
}

bool DependencyGraph::import_from_json(const std::string& json_str) {
    // Упрощенная реализация
    return true;
}

void DependencyGraph::invalidate_topo_cache() {
    topo_dirty_ = true;
    topo_cache_ = std::nullopt;
}

void DependencyGraph::mark_successors_dirty(EntityId entity_id, const std::string& param_name) {
    auto key = std::make_pair(entity_id, param_name);
    auto it = nodes_.find(key);
    if (it != nodes_.end()) {
        for (const auto& successor : it->second.successors) {
            auto succ_it = nodes_.find(successor);
            if (succ_it != nodes_.end()) {
                succ_it->second.is_dirty = true;
                mark_successors_dirty(successor.first, successor.second);
            }
        }
    }
}

bool DependencyGraph::dfs_has_cycle(
    const std::pair<EntityId, std::string>& node,
    std::set<std::pair<EntityId, std::string>>& visited,
    std::set<std::pair<EntityId, std::string>>& rec_stack
) const {
    visited.insert(node);
    rec_stack.insert(node);

    auto it = nodes_.find(node);
    if (it != nodes_.end()) {
        for (const auto& successor : it->second.successors) {
            if (visited.count(successor) == 0) {
                if (dfs_has_cycle(successor, visited, rec_stack)) {
                    return true;
                }
            } else if (rec_stack.count(successor)) {
                return true;
            }
        }
    }

    rec_stack.erase(node);
    return false;
}

bool DependencyGraph::dfs_find_path(
    const std::pair<EntityId, std::string>& current,
    const std::pair<EntityId, std::string>& target,
    std::vector<std::pair<EntityId, std::string>>& path,
    std::set<std::pair<EntityId, std::string>>& visited
) const {
    if (current == target) {
        path.push_back(current);
        return true;
    }

    if (visited.count(current)) {
        return false;
    }

    visited.insert(current);
    path.push_back(current);

    auto it = nodes_.find(current);
    if (it != nodes_.end()) {
        for (const auto& successor : it->second.successors) {
            if (dfs_find_path(successor, target, path, visited)) {
                return true;
            }
        }
    }

    path.pop_back();
    return false;
}

void DependencyGraph::calculate_depth(EntityId entity_id, const std::string& param_name) {
    auto key = std::make_pair(entity_id, param_name);
    auto it = nodes_.find(key);
    if (it == nodes_.end()) {
        return;
    }

    int max_pred_depth = -1;
    for (const auto& pred : it->second.predecessors) {
        int pred_depth = get_level(pred.first, pred.second);
        if (pred_depth > max_pred_depth) {
            max_pred_depth = pred_depth;
        }
    }

    it->second.depth = max_pred_depth + 1;
}

void DependencyGraph::notify_graph_changed() {
    if (!batch_mode_) {
        DocumentEvent event(DocumentEventType::DocumentRevisionChanged, document_.id(),
                          "Dependency graph changed");
        event_bus_.publish(event);
    }
}

}  // namespace bendcore::dependencies
