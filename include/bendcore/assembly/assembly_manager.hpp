#pragma once

#include "../document/entity_id.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <optional>
#include <cstdint>

namespace BendCore::CadCore {

/**
 * @brief Part information for BOM and component tracking
 */
struct PartInfo {
    EntityId part_id;
    std::string part_number;
    std::string description;
    std::string material;
    double weight = 0.0;  // kg
    double cost = 0.0;    // currency units
    uint32_t revision = 0;
    std::map<std::string, std::string> properties;
};

/**
 * @brief Bill of Materials entry
 */
struct BOMEntry {
    EntityId part_id;
    std::string part_number;
    std::string description;
    uint32_t quantity = 1;
    std::string unit;
    double unit_weight = 0.0;
    double unit_cost = 0.0;
    std::string material;
    std::string notes;
};

/**
 * @brief Subassembly reference with component tracking
 */
struct SubassemblyInfo {
    EntityId subassembly_id;
    std::string name;
    uint32_t component_count = 0;
    uint32_t part_count = 0;
    double total_weight = 0.0;
    uint32_t revision = 0;
};

/**
 * @brief Assembly Manager - Manages components, parts, and BOM
 *
 * Provides:
 * - Component organization and tracking
 * - Part reference management
 * - Revision control
 * - BOM generation and management
 * - Subassembly hierarchy
 */
class AssemblyManager {
public:
    /**
     * @brief Constructor
     * @param assembly_id ID of the assembly being managed
     */
    explicit AssemblyManager(const EntityId& assembly_id) noexcept
        : assembly_id_(assembly_id) {}

    /**
     * @brief Add a part to the assembly
     * @param part_info Part information
     * @return Part ID, or null ID if failed
     */
    EntityId addPart(const PartInfo& part_info);

    /**
     * @brief Remove a part by ID
     * @param part_id Part ID to remove
     * @return true if removed, false if not found
     */
    bool removePart(const EntityId& part_id) noexcept;

    /**
     * @brief Get part information
     * @param part_id Part ID
     * @return Part info, or empty optional if not found
     */
    [[nodiscard]] std::optional<PartInfo> getPartInfo(const EntityId& part_id) const noexcept;

    /**
     * @brief Get all parts in assembly
     */
    [[nodiscard]] const std::vector<PartInfo>& getParts() const noexcept { return parts_; }

    /**
     * @brief Update part information
     * @param part_id Part ID to update
     * @param part_info Updated part information
     * @return true if updated, false if not found
     */
    bool updatePart(const EntityId& part_id, const PartInfo& part_info) noexcept;

    /**
     * @brief Add a subassembly reference
     * @param subassembly_info Subassembly information
     * @return Subassembly ID, or null ID if failed
     */
    EntityId addSubassembly(const SubassemblyInfo& subassembly_info);

    /**
     * @brief Remove a subassembly
     * @param subassembly_id Subassembly ID to remove
     * @return true if removed, false if not found
     */
    bool removeSubassembly(const EntityId& subassembly_id) noexcept;

    /**
     * @brief Get subassembly information
     * @param subassembly_id Subassembly ID
     * @return Subassembly info, or empty optional if not found
     */
    [[nodiscard]] std::optional<SubassemblyInfo> getSubassemblyInfo(
        const EntityId& subassembly_id) const noexcept;

    /**
     * @brief Get all subassemblies
     */
    [[nodiscard]] const std::vector<SubassemblyInfo>& getSubassemblies() const noexcept {
        return subassemblies_;
    }

    /**
     * @brief Generate Bill of Materials
     * @return Vector of BOM entries
     */
    [[nodiscard]] std::vector<BOMEntry> generateBOM() const;

    /**
     * @brief Get total assembly weight
     */
    [[nodiscard]] double getTotalWeight() const noexcept;

    /**
     * @brief Get total assembly cost
     */
    [[nodiscard]] double getTotalCost() const noexcept;

    /**
     * @brief Get total part count (including subassemblies)
     */
    [[nodiscard]] uint32_t getTotalPartCount() const noexcept;

    /**
     * @brief Increment assembly revision
     */
    void incrementRevision() noexcept { assembly_revision_++; }

    /**
     * @brief Get current assembly revision
     */
    [[nodiscard]] uint32_t getRevision() const noexcept { return assembly_revision_; }

    /**
     * @brief Track part instances in assembly (quantity tracking)
     * @param part_id Part ID
     * @param instance_id Instance ID
     * @return true if tracked, false if failed
     */
    bool trackPartInstance(const EntityId& part_id, const EntityId& instance_id);

    /**
     * @brief Get instances of a specific part
     * @param part_id Part ID
     * @return Vector of instance IDs
     */
    [[nodiscard]] std::vector<EntityId> getPartInstances(const EntityId& part_id) const noexcept;

    /**
     * @brief Get part count for specific part ID
     * @param part_id Part ID
     * @return Number of instances of this part
     */
    [[nodiscard]] uint32_t getPartInstanceCount(const EntityId& part_id) const noexcept;

private:
    EntityId assembly_id_;
    std::vector<PartInfo> parts_;
    std::vector<SubassemblyInfo> subassemblies_;
    std::map<EntityId, std::vector<EntityId>> part_instances_;  // part_id -> instances
    uint32_t assembly_revision_ = 0;
};

} // namespace BendCore::CadCore
