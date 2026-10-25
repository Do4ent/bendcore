#pragma once
#include "../document/entity_id.hpp"
#include <map>

namespace BendCore::CadCore {

class CacheManager {
public:
    CacheManager() : hits_(0), misses_(0) {}
    bool cacheGeometry(const EntityId& id) noexcept;
    bool getCachedGeometry(const EntityId& id) noexcept;
    void clearCache() noexcept { hits_ = misses_ = 0; }
    [[nodiscard]] double getHitRate() const noexcept;

private:
    std::map<EntityId, bool> cache_;
    int hits_, misses_;
};

} // namespace BendCore::CadCore
