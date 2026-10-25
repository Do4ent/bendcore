#include "bendcore/document/hierarchy_adapter.hpp"

namespace bendcore::document {

HierarchyAdapter::HierarchyAdapter(CadDocument& doc) : document_(doc) {}

bool HierarchyAdapter::import_from_registry(const bendcore::assembly::Registry& old_registry) {
    // CAD-CORE-004: Migration stub
    // Full import will be implemented when Registry API is finalized
    // For now, return true (no-op) to indicate compatibility
    (void)old_registry;  // Avoid unused parameter warning
    return true;
}

std::unique_ptr<bendcore::assembly::Registry> HierarchyAdapter::export_to_registry() const {
    // CAD-CORE-004: Export stub
    // Full export will be implemented when Registry API is finalized
    // For now, return empty Registry for compatibility
    auto registry = std::make_unique<bendcore::assembly::Registry>();
    return registry;
}

Part* HierarchyAdapter::get_part_by_designation(const std::string& designation) {
    auto part_ids = document_.all_parts();
    for (EntityId part_id : part_ids) {
        auto* part = document_.get_entity<Part>(part_id);
        if (part && part->designation == designation) {
            return part;
        }
    }
    return nullptr;
}

std::vector<EntityId> HierarchyAdapter::find_instances_of_part(EntityId part_id) const {
    std::vector<EntityId> result;
    if (!part_id.is_valid()) return result;

    auto instance_ids = document_.all_instances();
    for (EntityId inst_id : instance_ids) {
        auto* inst = document_.get_entity<Instance>(inst_id);
        if (inst && inst->part_id == part_id) {
            result.push_back(inst_id);
        }
    }

    return result;
}

}  // namespace bendcore::document
