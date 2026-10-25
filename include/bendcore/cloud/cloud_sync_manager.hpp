#pragma once
#include "../document/entity_id.hpp"
namespace BendCore::CadCore {
class CloudSyncManager {
public:
    CloudSyncManager(const EntityId& project_id) : project_id_(project_id), synced_(false) {}
    bool syncToCloud() noexcept;
    [[nodiscard]] bool isSynced() const noexcept { return synced_; }
private:
    EntityId project_id_;
    bool synced_;
};
}
