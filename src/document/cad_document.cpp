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

}  // namespace bendcore::document
