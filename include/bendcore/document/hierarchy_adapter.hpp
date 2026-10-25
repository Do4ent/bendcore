#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "bendcore/document/cad_document.hpp"
#include "bendcore/instances.hpp"  // Old Registry

namespace bendcore::document {

/**
 * HierarchyAdapter — мост между старой bendcore::assembly::Registry
 * и новой CadDocument для миграции данных и обратной совместимости.
 *
 * CAD-CORE-004: Unified Entity Hierarchy
 */
class HierarchyAdapter {
public:
    explicit HierarchyAdapter(CadDocument& doc);
    ~HierarchyAdapter() = default;

    // Миграция из Registry
    [[nodiscard]] bool import_from_registry(const bendcore::assembly::Registry& old_registry);

    // Экспорт в Registry (для обратной совместимости)
    [[nodiscard]] std::unique_ptr<bendcore::assembly::Registry> export_to_registry() const;

    // Вспомогательные методы
    [[nodiscard]] Part* get_part_by_designation(const std::string& designation);
    [[nodiscard]] std::vector<EntityId> find_instances_of_part(EntityId part_id) const;

private:
    CadDocument& document_;

    // Маппинг от старых строковых ID к новым EntityId (для импорта)
    std::map<std::string, EntityId> old_to_new_id_;
};

}  // namespace bendcore::document
