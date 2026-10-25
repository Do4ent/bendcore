#pragma once

#include "entity_id.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <stdexcept>

namespace BendCore::CadCore {

/**
 * @brief Entity types in CAD document
 */
enum class EntityType {
    DOCUMENT,
    PART,
    BODY,
    SKETCH,
    FEATURE,
    ASSEMBLY,
    COMPONENT_INSTANCE,
    PARAMETER,
    DRAWING,
    REFERENCE,
    GEOMETRY_OBJECT
};

/**
 * @brief Entity lifecycle states
 */
enum class LifecycleState {
    ACTIVE,
    SUPPRESSED,
    INVALID,
    DELETED
};

/**
 * @brief Base entity in CAD document
 */
struct Entity {
    EntityId id;
    EntityType type;
    std::string name;
    EntityId parent_id;  // Parent in hierarchy
    uint64_t revision;   // Incremented on changes
    LifecycleState state;
    std::map<std::string, std::string> metadata;

    Entity()
        : id(EntityId::generate())
        , type(EntityType::PART)
        , name("")
        , revision(0)
        , state(LifecycleState::ACTIVE)
    {}

    explicit Entity(EntityType t, const std::string& n)
        : id(EntityId::generate())
        , type(t)
        , name(n)
        , revision(0)
        , state(LifecycleState::ACTIVE)
    {}
};

/**
 * @brief Parameter in CAD document
 */
struct Parameter {
    std::string name;
    double value;
    std::string units;
    std::string formula;
    EntityId owner_id;  // Entity that owns this parameter
};

/**
 * @brief Reference to geometry or other entities
 */
struct Reference {
    EntityId target_id;
    std::string description;
    uint64_t target_revision;  // Track what revision was referenced
};

/**
 * @brief Central CAD document model
 *
 * Single source of truth for:
 * - Entities and hierarchy
 * - Parameters and formulas
 * - Geometry references
 * - Document state and revisions
 */
class CadDocument {
public:
    /**
     * @brief Create a new empty document
     */
    CadDocument();

    /**
     * @brief Get document ID
     */
    [[nodiscard]] const EntityId& getDocumentId() const noexcept { return doc_id_; }

    /**
     * @brief Get document revision number
     */
    [[nodiscard]] uint64_t getRevision() const noexcept { return revision_; }

    /**
     * @brief Increment and return new revision
     */
    uint64_t incrementRevision() noexcept;

    /**
     * @brief Add entity to document
     * @throws std::invalid_argument if ID already exists
     */
    void addEntity(const Entity& entity);

    /**
     * @brief Get entity by ID
     * @throws std::invalid_argument if not found or deleted
     */
    Entity& getEntity(const EntityId& id);

    /**
     * @brief Get entity by ID (const version)
     * @throws std::invalid_argument if not found or deleted
     */
    const Entity& getEntity(const EntityId& id) const;

    /**
     * @brief Check if entity exists and is active
     */
    [[nodiscard]] bool hasEntity(const EntityId& id) const noexcept;

    /**
     * @brief Remove entity (mark as deleted)
     */
    void removeEntity(const EntityId& id);

    /**
     * @brief Get all active entities
     */
    [[nodiscard]] std::vector<EntityId> getEntityIds() const;

    /**
     * @brief Get entities of specific type
     */
    [[nodiscard]] std::vector<EntityId> getEntitiesByType(EntityType type) const;

    /**
     * @brief Add parameter to document
     */
    void addParameter(const EntityId& owner_id, const Parameter& param);

    /**
     * @brief Get parameter by name and owner
     * @throws std::invalid_argument if not found
     */
    Parameter& getParameter(const EntityId& owner_id, const std::string& name);

    /**
     * @brief Get parameters for owner entity
     */
    [[nodiscard]] std::vector<std::string> getParameterNames(const EntityId& owner_id) const;

    /**
     * @brief Add reference
     */
    void addReference(const Reference& ref);

    /**
     * @brief Get references to entity
     */
    [[nodiscard]] std::vector<Reference> getReferences(const EntityId& target_id) const;

    /**
     * @brief Validate document integrity
     * @return error message or empty string if valid
     */
    [[nodiscard]] std::string validate() const noexcept;

    /**
     * @brief Check for duplicate Entity IDs
     */
    [[nodiscard]] bool hasDuplicateIds() const noexcept;

    /**
     * @brief Get entity count
     */
    [[nodiscard]] size_t getEntityCount() const noexcept { return entities_.size(); }

    /**
     * @brief Get active entity count (excluding deleted)
     */
    [[nodiscard]] size_t getActiveEntityCount() const noexcept;

    /**
     * @brief Clear all entities
     */
    void clear() noexcept;

    /**
     * @brief Mark document as modified
     */
    void markModified() noexcept { incrementRevision(); }

private:
    EntityId doc_id_;
    uint64_t revision_;
    std::map<EntityId, Entity> entities_;          // All entities including deleted
    std::map<EntityId, std::vector<Parameter>> parameters_;  // Parameters by owner
    std::vector<Reference> references_;

    /**
     * @brief Check if ID conflicts exist
     */
    bool checkIdConflicts() const noexcept;

    /**
     * @brief Check for invalid references
     */
    bool checkReferences() const noexcept;
};

} // namespace BendCore::CadCore
