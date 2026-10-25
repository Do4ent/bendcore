#include "bendcore/document/entity_tree.hpp"
#include <algorithm>
#include <sstream>

namespace BendCore::CadCore {

EntityTree::EntityTree(CadDocument& document) noexcept
    : document_(document)
{}

EntityId EntityTree::createRoot(EntityType type, const std::string& name) {
    Entity entity(type, name);
    EntityId id = entity.id;
    entity.parent_id = EntityId();  // Null parent = root

    document_.addEntity(entity);
    return id;
}

EntityId EntityTree::addChild(const EntityId& parent_id, EntityType type, const std::string& name) {
    // Verify parent exists
    if (!document_.hasEntity(parent_id)) {
        throw std::invalid_argument("Parent entity not found: " + parent_id.toString());
    }

    Entity child(type, name);
    EntityId child_id = child.id;
    child.parent_id = parent_id;

    document_.addEntity(child);
    return child_id;
}

void EntityTree::setParent(const EntityId& entity_id, const EntityId& new_parent_id) {
    // Entity can't be its own parent
    if (entity_id == new_parent_id) {
        throw std::invalid_argument("Entity cannot be its own parent");
    }

    // Check if would create cycle
    if (wouldCreateCycle(entity_id, new_parent_id)) {
        throw std::invalid_argument("Setting parent would create a cycle");
    }

    // Update entity
    Entity& entity = document_.getEntity(entity_id);
    entity.parent_id = new_parent_id;
    document_.markModified();
}

EntityId EntityTree::getParent(const EntityId& entity_id) const {
    const Entity& entity = document_.getEntity(entity_id);
    return entity.parent_id;
}

std::vector<EntityId> EntityTree::getChildren(const EntityId& parent_id) const {
    std::vector<EntityId> children;

    // Verify parent exists
    if (!document_.hasEntity(parent_id)) {
        throw std::invalid_argument("Parent entity not found");
    }

    // Get all entities and filter by parent
    auto all_ids = document_.getEntityIds();
    for (const auto& id : all_ids) {
        const Entity& entity = document_.getEntity(id);
        if (entity.parent_id == parent_id) {
            children.push_back(id);
        }
    }

    return children;
}

std::vector<EntityId> EntityTree::getPath(const EntityId& entity_id) const {
    std::vector<EntityId> path;

    EntityId current = entity_id;
    while (!current.isNull()) {
        path.push_back(current);
        const Entity& entity = document_.getEntity(current);
        current = entity.parent_id;
    }

    // Reverse to get root-to-entity order
    std::reverse(path.begin(), path.end());
    return path;
}

std::string EntityTree::getPathString(const EntityId& entity_id) const {
    auto path = getPath(entity_id);
    std::ostringstream oss;

    for (size_t i = 0; i < path.size(); ++i) {
        if (i > 0) oss << " > ";
        const Entity& entity = document_.getEntity(path[i]);
        oss << entity.name;
    }

    return oss.str();
}

void EntityTree::deleteSubtree(const EntityId& entity_id) {
    // Get all descendants
    auto descendants = getDescendants(entity_id);
    descendants.push_back(entity_id);  // Include entity itself

    // Delete all
    for (const auto& id : descendants) {
        document_.removeEntity(id);
    }
}

size_t EntityTree::getDepth(const EntityId& entity_id) const {
    auto path = getPath(entity_id);
    return path.empty() ? 0 : path.size() - 1;
}

std::vector<EntityId> EntityTree::getDescendants(const EntityId& parent_id) const {
    std::vector<EntityId> descendants;
    collectDescendants(parent_id, descendants);
    return descendants;
}

bool EntityTree::isAncestor(const EntityId& ancestor, const EntityId& descendant) const {
    auto path = getPath(descendant);
    return std::find(path.begin(), path.end(), ancestor) != path.end();
}

bool EntityTree::isDescendant(const EntityId& descendant, const EntityId& ancestor) const {
    return isAncestor(ancestor, descendant);
}

std::vector<EntityId> EntityTree::getRoots() const {
    std::vector<EntityId> roots;
    auto all_ids = document_.getEntityIds();

    for (const auto& id : all_ids) {
        const Entity& entity = document_.getEntity(id);
        if (entity.parent_id.isNull()) {
            roots.push_back(id);
        }
    }

    return roots;
}

std::string EntityTree::validate() const noexcept {
    try {
        // Check for cycles
        if (hasCycles()) {
            return "Hierarchy contains cycles";
        }

        // Check each entity has valid parent
        auto all_ids = document_.getEntityIds();
        for (const auto& id : all_ids) {
            if (!isValidPath(id)) {
                return "Invalid path for entity: " + id.toString();
            }
        }

        return ""; // Valid
    } catch (...) {
        return "Validation error";
    }
}

bool EntityTree::hasCycles() const noexcept {
    auto all_ids = document_.getEntityIds();

    for (const auto& id : all_ids) {
        std::set<EntityId> visited;
        EntityId current = id;

        // Walk up the tree
        while (!current.isNull()) {
            if (visited.find(current) != visited.end()) {
                // Cycle detected
                return true;
            }
            visited.insert(current);

            try {
                const Entity& entity = document_.getEntity(current);
                current = entity.parent_id;
            } catch (...) {
                // Invalid entity
                return true;
            }
        }
    }

    return false;
}

void EntityTree::move(const EntityId& entity_id, const EntityId& new_parent_id) {
    setParent(entity_id, new_parent_id);
}

bool EntityTree::wouldCreateCycle(const EntityId& entity, const EntityId& new_parent) const noexcept {
    // new_parent can't be entity itself
    if (entity == new_parent) {
        return true;
    }

    // Check if new_parent is a descendant of entity
    try {
        auto descendants = getDescendants(entity);
        return std::find(descendants.begin(), descendants.end(), new_parent) != descendants.end();
    } catch (...) {
        return false;
    }
}

void EntityTree::collectDescendants(const EntityId& parent, std::vector<EntityId>& descendants) const {
    try {
        auto children = getChildren(parent);
        for (const auto& child : children) {
            descendants.push_back(child);
            collectDescendants(child, descendants);
        }
    } catch (...) {
        // Silently handle errors
    }
}

bool EntityTree::isValidPath(const EntityId& entity_id) const noexcept {
    try {
        EntityId current = entity_id;
        std::set<EntityId> visited;

        while (!current.isNull()) {
            if (visited.find(current) != visited.end()) {
                // Cycle detected
                return false;
            }
            visited.insert(current);

            const Entity& entity = document_.getEntity(current);
            current = entity.parent_id;
        }

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace BendCore::CadCore
