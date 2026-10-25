#pragma once

#include "entity_id.hpp"
#include "cad_document.hpp"
#include <vector>
#include <string>
#include <stdexcept>
#include <set>

namespace BendCore::CadCore {

/**
 * @brief Entity tree manager for CAD document hierarchy
 *
 * Features:
 * - Create root entities
 * - Add/remove children
 * - Change parent (move in tree)
 * - Get entity path
 * - Delete subtrees
 * - Validate hierarchy (no cycles, no self-parent, no crossing)
 */
class EntityTree {
public:
    /**
     * @brief Constructor
     * @param document The CAD document to manage
     */
    explicit EntityTree(CadDocument& document) noexcept;

    /**
     * @brief Create a root entity (no parent)
     */
    EntityId createRoot(EntityType type, const std::string& name);

    /**
     * @brief Add child to parent
     * @throws std::invalid_argument if parent doesn't exist or cycle would be created
     */
    EntityId addChild(const EntityId& parent_id, EntityType type, const std::string& name);

    /**
     * @brief Change entity's parent (move in tree)
     * @throws std::invalid_argument if invalid move
     */
    void setParent(const EntityId& entity_id, const EntityId& new_parent_id);

    /**
     * @brief Get parent of entity
     * @return parent ID or null if root
     */
    [[nodiscard]] EntityId getParent(const EntityId& entity_id) const;

    /**
     * @brief Get all children of entity
     */
    [[nodiscard]] std::vector<EntityId> getChildren(const EntityId& parent_id) const;

    /**
     * @brief Get path from root to entity
     * @return vector of IDs from root to entity (inclusive)
     */
    [[nodiscard]] std::vector<EntityId> getPath(const EntityId& entity_id) const;

    /**
     * @brief Get path as string
     */
    [[nodiscard]] std::string getPathString(const EntityId& entity_id) const;

    /**
     * @brief Delete entity and all its descendants
     */
    void deleteSubtree(const EntityId& entity_id);

    /**
     * @brief Get depth of entity in hierarchy
     * @return 0 for root, >0 for children
     */
    [[nodiscard]] size_t getDepth(const EntityId& entity_id) const;

    /**
     * @brief Get all descendants of entity
     */
    [[nodiscard]] std::vector<EntityId> getDescendants(const EntityId& parent_id) const;

    /**
     * @brief Check if entity is ancestor of other
     */
    [[nodiscard]] bool isAncestor(const EntityId& ancestor, const EntityId& descendant) const;

    /**
     * @brief Check if entity is descendant of other
     */
    [[nodiscard]] bool isDescendant(const EntityId& descendant, const EntityId& ancestor) const;

    /**
     * @brief Get all root entities
     */
    [[nodiscard]] std::vector<EntityId> getRoots() const;

    /**
     * @brief Validate hierarchy (no cycles, valid references)
     * @return error message or empty string if valid
     */
    [[nodiscard]] std::string validate() const noexcept;

    /**
     * @brief Check for cycles in tree
     */
    [[nodiscard]] bool hasCycles() const noexcept;

    /**
     * @brief Move entity to new parent
     */
    void move(const EntityId& entity_id, const EntityId& new_parent_id);

private:
    CadDocument& document_;

    /**
     * @brief Check if would create a cycle
     */
    bool wouldCreateCycle(const EntityId& entity, const EntityId& new_parent) const noexcept;

    /**
     * @brief Get all descendants recursively
     */
    void collectDescendants(const EntityId& parent, std::vector<EntityId>& descendants) const;

    /**
     * @brief Check path validity
     */
    bool isValidPath(const EntityId& entity_id) const noexcept;
};

} // namespace BendCore::CadCore
