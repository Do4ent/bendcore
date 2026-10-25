#include "bendcore/document/cad_document.hpp"
#include <algorithm>
#include <sstream>

namespace BendCore::CadCore {

CadDocument::CadDocument()
    : doc_id_(EntityId::generate())
    , revision_(0)
{}

uint64_t CadDocument::incrementRevision() noexcept {
    return ++revision_;
}

void CadDocument::addEntity(const Entity& entity) {
    // Check for duplicate ID
    if (entities_.find(entity.id) != entities_.end()) {
        throw std::invalid_argument(
            "Entity ID already exists: " + entity.id.toString()
        );
    }

    // Check if parent exists (if not root)
    if (!entity.parent_id.isNull()) {
        auto parent_it = entities_.find(entity.parent_id);
        if (parent_it == entities_.end() ||
            parent_it->second.state == LifecycleState::DELETED) {
            throw std::invalid_argument(
                "Parent entity not found: " + entity.parent_id.toString()
            );
        }
    }

    entities_[entity.id] = entity;
    incrementRevision();
}

Entity& CadDocument::getEntity(const EntityId& id) {
    auto it = entities_.find(id);
    if (it == entities_.end()) {
        throw std::invalid_argument("Entity not found: " + id.toString());
    }

    if (it->second.state == LifecycleState::DELETED) {
        throw std::invalid_argument("Entity is deleted: " + id.toString());
    }

    return it->second;
}

const Entity& CadDocument::getEntity(const EntityId& id) const {
    auto it = entities_.find(id);
    if (it == entities_.end()) {
        throw std::invalid_argument("Entity not found: " + id.toString());
    }

    if (it->second.state == LifecycleState::DELETED) {
        throw std::invalid_argument("Entity is deleted: " + id.toString());
    }

    return it->second;
}

bool CadDocument::hasEntity(const EntityId& id) const noexcept {
    auto it = entities_.find(id);
    if (it == entities_.end()) {
        return false;
    }
    return it->second.state != LifecycleState::DELETED;
}

void CadDocument::removeEntity(const EntityId& id) {
    auto it = entities_.find(id);
    if (it != entities_.end()) {
        it->second.state = LifecycleState::DELETED;
        incrementRevision();
    }
}

std::vector<EntityId> CadDocument::getEntityIds() const {
    std::vector<EntityId> ids;
    for (const auto& [id, entity] : entities_) {
        if (entity.state != LifecycleState::DELETED) {
            ids.push_back(id);
        }
    }
    return ids;
}

std::vector<EntityId> CadDocument::getEntitiesByType(EntityType type) const {
    std::vector<EntityId> ids;
    for (const auto& [id, entity] : entities_) {
        if (entity.state != LifecycleState::DELETED && entity.type == type) {
            ids.push_back(id);
        }
    }
    return ids;
}

void CadDocument::addParameter(const EntityId& owner_id, const Parameter& param) {
    // Verify owner exists
    if (!hasEntity(owner_id)) {
        throw std::invalid_argument("Owner entity not found: " + owner_id.toString());
    }

    parameters_[owner_id].push_back(param);
    incrementRevision();
}

Parameter& CadDocument::getParameter(const EntityId& owner_id, const std::string& name) {
    auto owner_it = parameters_.find(owner_id);
    if (owner_it == parameters_.end()) {
        throw std::invalid_argument("No parameters for owner: " + owner_id.toString());
    }

    auto& params = owner_it->second;
    auto param_it = std::find_if(params.begin(), params.end(),
        [&name](const Parameter& p) { return p.name == name; });

    if (param_it == params.end()) {
        throw std::invalid_argument("Parameter not found: " + name);
    }

    return *param_it;
}

std::vector<std::string> CadDocument::getParameterNames(const EntityId& owner_id) const {
    std::vector<std::string> names;
    auto it = parameters_.find(owner_id);
    if (it != parameters_.end()) {
        for (const auto& param : it->second) {
            names.push_back(param.name);
        }
    }
    return names;
}

void CadDocument::addReference(const Reference& ref) {
    // Verify target exists
    if (!hasEntity(ref.target_id)) {
        throw std::invalid_argument("Target entity not found: " + ref.target_id.toString());
    }

    references_.push_back(ref);
    incrementRevision();
}

std::vector<Reference> CadDocument::getReferences(const EntityId& target_id) const {
    std::vector<Reference> refs;
    for (const auto& ref : references_) {
        if (ref.target_id == target_id) {
            refs.push_back(ref);
        }
    }
    return refs;
}

std::string CadDocument::validate() const noexcept {
    try {
        // Check for duplicate IDs
        if (hasDuplicateIds()) {
            return "Document contains duplicate entity IDs";
        }

        // Check references
        if (!checkReferences()) {
            return "Document contains invalid references";
        }

        return ""; // Valid
    } catch (...) {
        return "Validation error";
    }
}

bool CadDocument::hasDuplicateIds() const noexcept {
    std::map<EntityId, int> id_count;
    for (const auto& [id, entity] : entities_) {
        if (entity.state != LifecycleState::DELETED) {
            id_count[id]++;
        }
    }

    for (const auto& [id, count] : id_count) {
        if (count > 1) {
            return true;
        }
    }

    return false;
}

size_t CadDocument::getActiveEntityCount() const noexcept {
    size_t count = 0;
    for (const auto& [id, entity] : entities_) {
        if (entity.state != LifecycleState::DELETED) {
            count++;
        }
    }
    return count;
}

void CadDocument::clear() noexcept {
    entities_.clear();
    parameters_.clear();
    references_.clear();
    doc_id_ = EntityId::generate();
    revision_ = 0;
}

bool CadDocument::checkReferences() const noexcept {
    for (const auto& ref : references_) {
        if (!hasEntity(ref.target_id)) {
            return false;
        }
    }
    return true;
}

bool CadDocument::checkIdConflicts() const noexcept {
    return !hasDuplicateIds();
}

} // namespace BendCore::CadCore
