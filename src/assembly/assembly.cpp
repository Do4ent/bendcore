#include "bendcore/assembly/assembly.hpp"
#include "bendcore/assembly/component_instance.hpp"
#include "bendcore/assembly/mate_solver.hpp"
#include <algorithm>
#include <set>
#include <sstream>

namespace BendCore::CadCore {

Assembly::Assembly(const EntityId& id, const std::string& name, const std::string& description)
    : id_(id)
    , name_(name)
    , description_(description)
    , revision_(0)
    , constraint_solver_(std::make_shared<MateSolver>())
{
}

uint64_t Assembly::incrementRevision() noexcept {
    return ++revision_;
}

std::optional<std::string> Assembly::getMetadata(const std::string& key) const noexcept {
    auto it = metadata_.find(key);
    if (it != metadata_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Assembly::setMetadata(const std::string& key, const std::string& value) noexcept {
    metadata_[key] = value;
    incrementRevision();
}

EntityId Assembly::addComponentInstance(std::shared_ptr<ComponentInstance> instance) {
    if (!instance) {
        return EntityId();  // Return null ID
    }

    // Validate instance
    std::string error = instance->validate();
    if (!error.empty()) {
        return EntityId();
    }

    // Check for duplicate instance ID
    auto it = std::find_if(
        component_instances_.begin(),
        component_instances_.end(),
        [&instance](const std::shared_ptr<ComponentInstance>& comp) {
            return comp->getId() == instance->getId();
        }
    );

    if (it != component_instances_.end()) {
        return EntityId();  // Instance with this ID already exists
    }

    component_instances_.push_back(instance);
    incrementRevision();

    return instance->getId();
}

bool Assembly::removeComponentInstance(const EntityId& instance_id) noexcept {
    auto it = std::find_if(
        component_instances_.begin(),
        component_instances_.end(),
        [&instance_id](const std::shared_ptr<ComponentInstance>& comp) {
            return comp->getId() == instance_id;
        }
    );

    if (it != component_instances_.end()) {
        component_instances_.erase(it);
        incrementRevision();
        return true;
    }

    return false;
}

std::shared_ptr<ComponentInstance> Assembly::getComponentInstance(
    const EntityId& instance_id) const noexcept {
    auto it = std::find_if(
        component_instances_.begin(),
        component_instances_.end(),
        [&instance_id](const std::shared_ptr<ComponentInstance>& comp) {
            return comp->getId() == instance_id;
        }
    );

    if (it != component_instances_.end()) {
        return *it;
    }

    return nullptr;
}

std::shared_ptr<ComponentInstance> Assembly::getComponentInstanceAt(size_t index) const noexcept {
    if (index >= component_instances_.size()) {
        return nullptr;
    }
    return component_instances_[index];
}

void Assembly::clear() noexcept {
    component_instances_.clear();
    constraint_solver_->clear();
    incrementRevision();
}

std::string Assembly::validate() const noexcept {
    // Check for empty name
    if (name_.empty()) {
        return "Assembly name cannot be empty";
    }

    // Check component instances
    for (const auto& instance : component_instances_) {
        if (!instance) {
            return "Invalid component instance (null pointer)";
        }

        std::string error = instance->validate();
        if (!error.empty()) {
            return "Component instance error: " + error;
        }
    }

    // Check for duplicate instance IDs
    std::set<EntityId> seen_ids;
    for (const auto& instance : component_instances_) {
        if (seen_ids.count(instance->getId()) > 0) {
            return "Duplicate component instance ID: " + instance->getId().toString();
        }
        seen_ids.insert(instance->getId());
    }

    return "";  // Valid
}

bool Assembly::hasCircularReferences(std::set<EntityId>& checked_ids) const noexcept {
    // Check if we've already checked this assembly (would indicate a cycle)
    if (checked_ids.count(id_) > 0) {
        return true;
    }

    checked_ids.insert(id_);

    // Check each component instance for circular references
    // In a real implementation, we would check if any instance references
    // a Part that contains an Assembly that references this Assembly
    // For now, this is a placeholder for the logic

    return false;
}

std::shared_ptr<Assembly> createAssembly(
    const std::string& name,
    const std::string& description)
{
    return std::make_shared<Assembly>(
        EntityId::generate(),
        name,
        description
    );
}

} // namespace BendCore::CadCore
