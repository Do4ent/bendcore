#include "bendcore/assembly/assembly_manager.hpp"
#include "bendcore/document/entity_id.hpp"

namespace BendCore::CadCore {

EntityId AssemblyManager::addPart(const PartInfo& part_info) {
    if (part_info.part_id.isNull()) {
        return EntityId();
    }

    // Check if part already exists
    for (const auto& p : parts_) {
        if (p.part_id == part_info.part_id) {
            return EntityId();  // Part already exists
        }
    }

    parts_.push_back(part_info);
    incrementRevision();
    return part_info.part_id;
}

bool AssemblyManager::removePart(const EntityId& part_id) noexcept {
    auto it = std::find_if(parts_.begin(), parts_.end(),
                          [&](const PartInfo& p) { return p.part_id == part_id; });
    if (it != parts_.end()) {
        parts_.erase(it);
        part_instances_.erase(part_id);
        incrementRevision();
        return true;
    }
    return false;
}

std::optional<PartInfo> AssemblyManager::getPartInfo(const EntityId& part_id) const noexcept {
    for (const auto& p : parts_) {
        if (p.part_id == part_id) {
            return p;
        }
    }
    return std::nullopt;
}

bool AssemblyManager::updatePart(const EntityId& part_id, const PartInfo& part_info) noexcept {
    for (auto& p : parts_) {
        if (p.part_id == part_id) {
            p = part_info;
            p.part_id = part_id;  // Preserve the ID
            incrementRevision();
            return true;
        }
    }
    return false;
}

EntityId AssemblyManager::addSubassembly(const SubassemblyInfo& subassembly_info) {
    if (subassembly_info.subassembly_id.isNull()) {
        return EntityId();
    }

    // Check if subassembly already exists
    for (const auto& s : subassemblies_) {
        if (s.subassembly_id == subassembly_info.subassembly_id) {
            return EntityId();
        }
    }

    subassemblies_.push_back(subassembly_info);
    incrementRevision();
    return subassembly_info.subassembly_id;
}

bool AssemblyManager::removeSubassembly(const EntityId& subassembly_id) noexcept {
    auto it = std::find_if(subassemblies_.begin(), subassemblies_.end(),
                          [&](const SubassemblyInfo& s) { return s.subassembly_id == subassembly_id; });
    if (it != subassemblies_.end()) {
        subassemblies_.erase(it);
        incrementRevision();
        return true;
    }
    return false;
}

std::optional<SubassemblyInfo> AssemblyManager::getSubassemblyInfo(
    const EntityId& subassembly_id) const noexcept {
    for (const auto& s : subassemblies_) {
        if (s.subassembly_id == subassembly_id) {
            return s;
        }
    }
    return std::nullopt;
}

std::vector<BOMEntry> AssemblyManager::generateBOM() const {
    std::vector<BOMEntry> bom;
    std::map<EntityId, uint32_t> part_counts;

    // Count instances of each part
    for (const auto& [part_id, instances] : part_instances_) {
        part_counts[part_id] = static_cast<uint32_t>(instances.size());
    }

    // Create BOM entries
    for (const auto& part : parts_) {
        BOMEntry entry;
        entry.part_id = part.part_id;
        entry.part_number = part.part_number;
        entry.description = part.description;
        entry.unit_weight = part.weight;
        entry.unit_cost = part.cost;
        entry.material = part.material;

        auto count_it = part_counts.find(part.part_id);
        if (count_it != part_counts.end()) {
            entry.quantity = count_it->second;
        } else {
            entry.quantity = 0;
        }

        entry.unit = "each";
        bom.push_back(entry);
    }

    return bom;
}

double AssemblyManager::getTotalWeight() const noexcept {
    double total = 0.0;

    // Weight from parts
    for (const auto& part : parts_) {
        total += part.weight;
    }

    // Weight from subassemblies
    for (const auto& subassembly : subassemblies_) {
        total += subassembly.total_weight;
    }

    return total;
}

double AssemblyManager::getTotalCost() const noexcept {
    double total = 0.0;

    // Cost from parts
    for (const auto& part : parts_) {
        total += part.cost;
    }

    return total;
}

uint32_t AssemblyManager::getTotalPartCount() const noexcept {
    uint32_t count = 0;

    // Count parts
    for (const auto& [_, instances] : part_instances_) {
        count += static_cast<uint32_t>(instances.size());
    }

    // Count subassembly parts
    for (const auto& subassembly : subassemblies_) {
        count += subassembly.part_count;
    }

    return count;
}

bool AssemblyManager::trackPartInstance(const EntityId& part_id, const EntityId& instance_id) {
    if (part_id.isNull() || instance_id.isNull()) {
        return false;
    }

    // Find the part
    auto it = std::find_if(parts_.begin(), parts_.end(),
                          [&](const PartInfo& p) { return p.part_id == part_id; });
    if (it == parts_.end()) {
        return false;
    }

    part_instances_[part_id].push_back(instance_id);
    return true;
}

std::vector<EntityId> AssemblyManager::getPartInstances(const EntityId& part_id) const noexcept {
    auto it = part_instances_.find(part_id);
    if (it != part_instances_.end()) {
        return it->second;
    }
    return {};
}

uint32_t AssemblyManager::getPartInstanceCount(const EntityId& part_id) const noexcept {
    auto it = part_instances_.find(part_id);
    if (it != part_instances_.end()) {
        return static_cast<uint32_t>(it->second.size());
    }
    return 0;
}

} // namespace BendCore::CadCore
