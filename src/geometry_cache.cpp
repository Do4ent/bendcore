#include "bendcore/geometry/geometry_cache.hpp"

namespace BendCore::CadCore {

void GeometryCache::cacheGeometry(const EntityId& entityId, const CachedGeometry& geom) noexcept {
    cache_[entityId] = geom;
    cache_[entityId].isDirty = false;
}

const CachedGeometry* GeometryCache::getGeometry(const EntityId& entityId) const noexcept {
    auto it = cache_.find(entityId);
    return it != cache_.end() ? &it->second : nullptr;
}

void GeometryCache::markDirty(const EntityId& entityId) noexcept {
    auto it = cache_.find(entityId);
    if (it != cache_.end()) {
        it->second.isDirty = true;
    }
}

void GeometryCache::markAllDirty() noexcept {
    for (auto& [_, geom] : cache_) {
        geom.isDirty = true;
    }
}

bool GeometryCache::isCached(const EntityId& entityId) const noexcept {
    return cache_.find(entityId) != cache_.end();
}

void GeometryCache::invalidate(const EntityId& entityId) noexcept {
    cache_.erase(entityId);
}

std::vector<EntityId> GeometryCache::getDirtyEntities() const noexcept {
    std::vector<EntityId> dirty;
    for (const auto& [id, geom] : cache_) {
        if (geom.isDirty) {
            dirty.push_back(id);
        }
    }
    return dirty;
}

} // namespace BendCore::CadCore
