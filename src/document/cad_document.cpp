#include "bendcore/document/cad_document.hpp"

#include <algorithm>
#include <queue>
#include <set>

namespace bendcore::document {

CadDocument::CadDocument() : document_id_(EntityId::Type::Drawing) {
    // Создаём корневой элемент документа
}

// Управление сущностями
template <>
EntityId CadDocument::add_entity<Part>(const Part& entity) {
    if (!entity.id.is_valid()) return EntityId();

    auto part = std::make_shared<Part>(entity);
    part->created_at = part->updated_at = std::chrono::system_clock::now();

    entities_[entity.id] = part;

    if (entity.parent_id.is_valid()) {
        hierarchy_[entity.parent_id].push_back(entity.id);
    }

    increment_revision();
    rebuild_tree_order();

    return entity.id;
}

template <>
EntityId CadDocument::add_entity<Instance>(const Instance& entity) {
    if (!entity.id.is_valid()) return EntityId();

    auto instance = std::make_shared<Instance>(entity);
    instance->created_at = instance->updated_at = std::chrono::system_clock::now();

    entities_[entity.id] = instance;

    if (entity.parent_id.is_valid()) {
        hierarchy_[entity.parent_id].push_back(entity.id);
    }

    increment_revision();
    rebuild_tree_order();

    return entity.id;
}

template <>
EntityId CadDocument::add_entity<Parameter>(const Parameter& entity) {
    if (!entity.id.is_valid()) return EntityId();

    auto param = std::make_shared<Parameter>(entity);
    param->created_at = param->updated_at = std::chrono::system_clock::now();

    entities_[entity.id] = param;

    if (entity.parent_id.is_valid()) {
        hierarchy_[entity.parent_id].push_back(entity.id);
    }

    increment_revision();
    rebuild_tree_order();

    return entity.id;
}

template <>
EntityId CadDocument::add_entity<Body>(const Body& entity) {
    if (!entity.id.is_valid()) return EntityId();

    auto body = std::make_shared<Body>(entity);
    body->created_at = body->updated_at = std::chrono::system_clock::now();

    entities_[entity.id] = body;

    if (entity.parent_id.is_valid()) {
        hierarchy_[entity.parent_id].push_back(entity.id);
    }

    increment_revision();
    rebuild_tree_order();

    return entity.id;
}

template <>
EntityId CadDocument::add_entity<Sketch>(const Sketch& entity) {
    if (!entity.id.is_valid()) return EntityId();

    auto sketch = std::make_shared<Sketch>(entity);
    sketch->created_at = sketch->updated_at = std::chrono::system_clock::now();

    entities_[entity.id] = sketch;

    if (entity.parent_id.is_valid()) {
        hierarchy_[entity.parent_id].push_back(entity.id);
    }

    increment_revision();
    rebuild_tree_order();

    return entity.id;
}

template <>
Part* CadDocument::get_entity<Part>(EntityId id) {
    auto it = entities_.find(id);
    if (it == entities_.end()) return nullptr;

    auto part = std::dynamic_pointer_cast<Part>(it->second);
    return part ? part.get() : nullptr;
}

template <>
const Part* CadDocument::get_entity<Part>(EntityId id) const {
    auto it = entities_.find(id);
    if (it == entities_.end()) return nullptr;

    auto part = std::dynamic_pointer_cast<Part>(it->second);
    return part ? part.get() : nullptr;
}

// Удаление сущности
bool CadDocument::remove_entity(EntityId id) {
    auto it = entities_.find(id);
    if (it == entities_.end()) return false;

    // Пометить как удалённую вместо удаления (для Undo/Redo)
    it->second->deleted = true;
    increment_revision();

    return true;
}

// Иерархия
bool CadDocument::set_parent(EntityId child_id, EntityId parent_id) {
    if (!contains(child_id) || (parent_id.is_valid() && !contains(parent_id))) {
        return false;
    }

    // Проверка циклов
    EntityId current = parent_id;
    while (current.is_valid()) {
        if (current == child_id) return false;  // Цикл!
        current = parent_of(current);
    }

    // Удалить из старого родителя
    EntityId old_parent = parent_of(child_id);
    if (old_parent.is_valid()) {
        auto& children = hierarchy_[old_parent];
        children.erase(std::remove(children.begin(), children.end(), child_id), children.end());
    }

    // Установить новое родство
    auto entity = entities_[child_id].get();
    entity->parent_id = parent_id;

    if (parent_id.is_valid()) {
        hierarchy_[parent_id].push_back(child_id);
    }

    increment_revision();
    rebuild_tree_order();

    return true;
}

EntityId CadDocument::parent_of(EntityId id) const {
    auto it = entities_.find(id);
    return it != entities_.end() ? it->second->parent_id : EntityId();
}

std::vector<EntityId> CadDocument::children_of(EntityId id) const {
    auto it = hierarchy_.find(id);
    return it != hierarchy_.end() ? it->second : std::vector<EntityId>();
}

std::vector<EntityId> CadDocument::tree_order() const {
    return tree_order_;
}

// Проверка целостности иерархии
bool CadDocument::is_valid_hierarchy() const {
    for (const auto& [id, entity] : entities_) {
        if (entity->deleted) continue;
        if (entity->parent_id.is_valid() && !contains(entity->parent_id)) {
            return false;  // Родитель не существует
        }
    }
    return true;
}

bool CadDocument::has_cycles() const {
    for (const auto& [id, entity] : entities_) {
        if (entity->deleted) continue;

        EntityId current = id;
        std::set<EntityId> visited;

        while (current.is_valid()) {
            if (visited.count(current)) return true;  // Цикл найден
            visited.insert(current);
            current = parent_of(current);
        }
    }
    return false;
}

// Получение всех сущностей типа
std::vector<EntityId> CadDocument::all_parts() const {
    std::vector<EntityId> result;
    for (const auto& [id, entity] : entities_) {
        if (!entity->deleted && id.type() == EntityId::Type::Part) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<EntityId> CadDocument::all_instances() const {
    std::vector<EntityId> result;
    for (const auto& [id, entity] : entities_) {
        if (!entity->deleted && id.type() == EntityId::Type::Instance) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<EntityId> CadDocument::all_parameters() const {
    std::vector<EntityId> result;
    for (const auto& [id, entity] : entities_) {
        if (!entity->deleted && id.type() == EntityId::Type::Parameter) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<EntityId> CadDocument::all_features() const {
    std::vector<EntityId> result;
    for (const auto& [id, entity] : entities_) {
        if (!entity->deleted && id.type() == EntityId::Type::Feature) {
            result.push_back(id);
        }
    }
    return result;
}

// Проверка существования
bool CadDocument::contains(EntityId id) const {
    if (!id.is_valid()) return false;
    auto it = entities_.find(id);
    return it != entities_.end() && !it->second->deleted;
}

EntityId::Type CadDocument::entity_type(EntityId id) const {
    return id.type();
}

// Перестроение порядка дерева
void CadDocument::rebuild_tree_order() {
    tree_order_.clear();

    // BFS обход дерева для порядка
    std::queue<EntityId> q;

    // Найти корневые элементы (без родителя)
    for (const auto& [id, entity] : entities_) {
        if (!entity->deleted && !entity->parent_id.is_valid()) {
            q.push(id);
        }
    }

    while (!q.empty()) {
        EntityId current = q.front();
        q.pop();
        tree_order_.push_back(current);

        // Добавить детей в очередь
        for (EntityId child : children_of(current)) {
            q.push(child);
        }
    }
}

// Path-based access (CAD-CORE-004)
std::optional<EntityId> CadDocument::find_by_path(const std::string& path) const {
    if (path.empty()) return std::nullopt;

    // Проверить кэш
    auto it = path_index_.find(path);
    if (it != path_index_.end() && contains(it->second)) {
        return it->second;
    }

    // Разбить path по "/"
    std::vector<std::string> components;
    size_t start = 0;
    if (path[0] == '/') start = 1;

    for (size_t i = start; i < path.length(); ++i) {
        if (path[i] == '/') {
            if (i > start) {
                components.push_back(path.substr(start, i - start));
            }
            start = i + 1;
        }
    }
    if (start < path.length()) {
        components.push_back(path.substr(start));
    }

    if (components.empty()) return std::nullopt;

    // Начать с root'ов
    EntityId current;
    for (const auto& [id, entity] : entities_) {
        if (!entity->deleted && !entity->parent_id.is_valid() && entity->name == components[0]) {
            current = id;
            break;
        }
    }

    if (!current.is_valid()) return std::nullopt;

    // Пройти по компонентам пути
    for (size_t i = 1; i < components.size(); ++i) {
        auto children = children_of(current);
        EntityId next;
        for (EntityId child : children) {
            auto it = entities_.find(child);
            if (it != entities_.end()) {
                auto* entity = it->second.get();
                if (entity && !entity->deleted && entity->name == components[i]) {
                    next = child;
                    break;
                }
            }
        }
        if (!next.is_valid()) return std::nullopt;
        current = next;
    }

    // Кэшировать результат
    path_index_[path] = current;
    return current;
}

std::string CadDocument::path_of(EntityId id) const {
    if (!id.is_valid()) return "";

    // Проверить кэш
    auto it = path_cache_.find(id);
    if (it != path_cache_.end()) {
        return it->second;
    }

    // Построить путь рекурсивно
    auto entity_it = entities_.find(id);
    if (entity_it == entities_.end()) return "";

    const auto* entity = entity_it->second.get();
    if (!entity) return "";

    std::string path = "/" + entity->name;
    EntityId current = id;

    while (true) {
        EntityId parent = parent_of(current);
        if (!parent.is_valid()) break;
        auto parent_it = entities_.find(parent);
        if (parent_it == entities_.end()) break;
        const auto* parent_entity = parent_it->second.get();
        if (!parent_entity) break;
        path = "/" + parent_entity->name + path;
        current = parent;
    }

    // Кэшировать результат
    path_cache_[id] = path;
    path_index_[path] = id;
    return path;
}

bool CadDocument::move_subtree(EntityId child_id, EntityId new_parent_id) {
    if (!child_id.is_valid() || !new_parent_id.is_valid()) return false;
    if (child_id == new_parent_id) return false;

    // Проверка циклов
    if (is_ancestor_of(child_id, new_parent_id)) return false;

    // Получить старого родителя
    EntityId old_parent = parent_of(child_id);

    // Удалить из старого родителя
    if (old_parent.is_valid()) {
        auto it = hierarchy_.find(old_parent);
        if (it != hierarchy_.end()) {
            auto& children = it->second;
            children.erase(std::remove(children.begin(), children.end(), child_id), children.end());
        }
    }

    // Установить нового родителя
    bool success = set_parent(child_id, new_parent_id);

    if (success) {
        // Инвалидировать кэш путей поддерева
        auto subtree_ids = subtree(child_id);
        for (EntityId id : subtree_ids) {
            path_cache_.erase(id);
        }
        // Также перестроить path_index для поддерева
        for (EntityId id : subtree_ids) {
            auto cached_path_it = path_cache_.find(id);
            if (cached_path_it != path_cache_.end()) {
                path_index_.erase(cached_path_it->second);
            }
        }
    }

    return success;
}

std::vector<EntityId> CadDocument::subtree(EntityId root) const {
    std::vector<EntityId> result;
    if (!root.is_valid() || !contains(root)) return result;

    std::queue<EntityId> q;
    q.push(root);
    result.push_back(root);

    while (!q.empty()) {
        EntityId current = q.front();
        q.pop();

        auto children = children_of(current);
        for (EntityId child : children) {
            q.push(child);
            result.push_back(child);
        }
    }

    return result;
}

std::vector<EntityId> CadDocument::siblings_of(EntityId id) const {
    if (!id.is_valid()) return {};

    EntityId parent = parent_of(id);
    if (!parent.is_valid()) return {};

    return children_of(parent);
}

bool CadDocument::is_ancestor_of(EntityId potential_ancestor, EntityId potential_descendant) const {
    if (!potential_ancestor.is_valid() || !potential_descendant.is_valid()) return false;
    if (potential_ancestor == potential_descendant) return false;

    EntityId current = potential_descendant;
    while (current.is_valid()) {
        current = parent_of(current);
        if (current == potential_ancestor) return true;
    }
    return false;
}

void CadDocument::rebuild_paths() {
    path_cache_.clear();
    path_index_.clear();

    // Перестроить все пути
    for (const auto& [id, entity] : entities_) {
        if (!entity->deleted) {
            [[maybe_unused]] auto path = path_of(id);  // Это заполнит кэш
        }
    }
}

}  // namespace bendcore::document
