#pragma once

#include "../document/entity_id.hpp"
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <set>
#include <optional>
#include <cstdint>

namespace BendCore::CadCore {

// Forward declarations
class ComponentInstance;
class MateSolver;

/**
 * @brief Assembly - Container for component instances and constraints
 *
 * An Assembly represents a collection of Part instances positioned and
 * constrained in 3D space. Similar to SOLIDWORKS assemblies, Inventor
 * assemblies, or NX assemblies.
 *
 * Features:
 * - Component instance management
 * - Part reference tracking
 * - Circular reference prevention
 * - Assembly metadata
 * - Tree navigation
 */
class Assembly {
public:
    /**
     * @brief Constructor
     * @param id Assembly entity ID
     * @param name Assembly name
     * @param description Assembly description
     */
    Assembly(const EntityId& id, const std::string& name = "", const std::string& description = "");

    /**
     * @brief Get assembly ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return id_; }

    /**
     * @brief Get assembly name
     */
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /**
     * @brief Set assembly name
     */
    void setName(const std::string& name) noexcept {
        name_ = name;
        incrementRevision();
    }

    /**
     * @brief Get assembly description
     */
    [[nodiscard]] const std::string& getDescription() const noexcept { return description_; }

    /**
     * @brief Set assembly description
     */
    void setDescription(const std::string& description) noexcept {
        description_ = description;
        incrementRevision();
    }

    /**
     * @brief Get assembly revision number
     */
    [[nodiscard]] uint64_t getRevision() const noexcept { return revision_; }

    /**
     * @brief Increment assembly revision
     */
    uint64_t incrementRevision() noexcept;

    /**
     * @brief Get metadata value
     */
    [[nodiscard]] std::optional<std::string> getMetadata(const std::string& key) const noexcept;

    /**
     * @brief Set metadata value
     */
    void setMetadata(const std::string& key, const std::string& value) noexcept;

    /**
     * @brief Get all metadata
     */
    [[nodiscard]] const std::map<std::string, std::string>& getMetadataMap() const noexcept {
        return metadata_;
    }

    /**
     * @brief Clear all metadata
     */
    void clearMetadata() noexcept { metadata_.clear(); }

    /**
     * @brief Add component instance to assembly
     * @param instance Component instance to add
     * @return ID of added instance, or null ID if failed
     */
    EntityId addComponentInstance(std::shared_ptr<ComponentInstance> instance);

    /**
     * @brief Remove component instance by ID
     * @param instance_id ID of instance to remove
     * @return true if removed, false if not found
     */
    bool removeComponentInstance(const EntityId& instance_id) noexcept;

    /**
     * @brief Get component instance by ID
     * @param instance_id Instance ID
     * @return shared_ptr to instance, or nullptr if not found
     */
    [[nodiscard]] std::shared_ptr<ComponentInstance> getComponentInstance(
        const EntityId& instance_id) const noexcept;

    /**
     * @brief Get all component instances
     */
    [[nodiscard]] const std::vector<std::shared_ptr<ComponentInstance>>&
    getComponentInstances() const noexcept {
        return component_instances_;
    }

    /**
     * @brief Get component instance count
     */
    [[nodiscard]] size_t getComponentInstanceCount() const noexcept {
        return component_instances_.size();
    }

    /**
     * @brief Check if assembly is empty (no components)
     */
    [[nodiscard]] bool isEmpty() const noexcept { return component_instances_.empty(); }

    /**
     * @brief Get component instance by index
     */
    [[nodiscard]] std::shared_ptr<ComponentInstance> getComponentInstanceAt(size_t index) const noexcept;

    /**
     * @brief Get assembly constraint solver
     */
    [[nodiscard]] MateSolver& getConstraintSolver() noexcept { return *constraint_solver_; }

    /**
     * @brief Get assembly constraint solver (const)
     */
    [[nodiscard]] const MateSolver& getConstraintSolver() const noexcept { return *constraint_solver_; }

    /**
     * @brief Check if assembly has constraint solver
     */
    [[nodiscard]] bool hasConstraintSolver() const noexcept { return constraint_solver_ != nullptr; }

    /**
     * @brief Clear all component instances
     */
    void clear() noexcept;

    /**
     * @brief Validate assembly structure
     * @return error message or empty string if valid
     */
    [[nodiscard]] std::string validate() const noexcept;

    /**
     * @brief Check for circular references (assembly references itself)
     * @param checked_ids Already checked assembly IDs to prevent infinite recursion
     * @return true if circular reference found
     */
    [[nodiscard]] bool hasCircularReferences(std::set<EntityId>& checked_ids) const noexcept;

    /**
     * @brief Get parent assembly ID (if this assembly is part of another assembly)
     */
    [[nodiscard]] EntityId getParentAssemblyId() const noexcept { return parent_assembly_id_; }

    /**
     * @brief Set parent assembly ID
     */
    void setParentAssemblyId(const EntityId& parent_id) noexcept { parent_assembly_id_ = parent_id; }

    /**
     * @brief Check if assembly is a sub-assembly
     */
    [[nodiscard]] bool isSubAssembly() const noexcept { return !parent_assembly_id_.isNull(); }

private:
    EntityId id_;
    std::string name_;
    std::string description_;
    uint64_t revision_ = 0;
    std::map<std::string, std::string> metadata_;
    std::vector<std::shared_ptr<ComponentInstance>> component_instances_;
    std::shared_ptr<MateSolver> constraint_solver_;
    EntityId parent_assembly_id_;  // For sub-assemblies

    // Prevent copying
    Assembly(const Assembly&) = delete;
    Assembly& operator=(const Assembly&) = delete;
};

/**
 * @brief Factory function to create an assembly
 * @param name Assembly name
 * @param description Assembly description
 * @return shared_ptr to new Assembly
 */
[[nodiscard]] std::shared_ptr<Assembly> createAssembly(
    const std::string& name = "",
    const std::string& description = ""
);

} // namespace BendCore::CadCore
