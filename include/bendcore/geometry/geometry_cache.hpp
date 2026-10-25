#pragma once

#include "../document/entity_id.hpp"
#include "vector.hpp"
#include <map>
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief Cached geometry data for performance
 */
struct CachedGeometry {
    EntityId entityId;
    Vector3D centroid;
    double volume = 0.0;
    double surfaceArea = 0.0;
    Vector3D boundingBoxMin;
    Vector3D boundingBoxMax;
    uint64_t revision = 0;
    bool isDirty = false;
};

/**
 * @brief Geometry cache manager for performance optimization
 */
class GeometryCache {
public:
    GeometryCache() noexcept = default;

    /**
     * @brief Cache geometry for entity
     */
    void cacheGeometry(const EntityId& entityId, const CachedGeometry& geom) noexcept;

    /**
     * @brief Get cached geometry
     */
    const CachedGeometry* getGeometry(const EntityId& entityId) const noexcept;

    /**
     * @brief Mark entity as dirty (needs recalculation)
     */
    void markDirty(const EntityId& entityId) noexcept;

    /**
     * @brief Mark all as dirty
     */
    void markAllDirty() noexcept;

    /**
     * @brief Check if entity is cached
     */
    bool isCached(const EntityId& entityId) const noexcept;

    /**
     * @brief Invalidate cache entry
     */
    void invalidate(const EntityId& entityId) noexcept;

    /**
     * @brief Get cache size
     */
    size_t getCacheSize() const noexcept { return cache_.size(); }

    /**
     * @brief Clear all cache
     */
    void clear() noexcept { cache_.clear(); }

    /**
     * @brief Get dirty entities
     */
    std::vector<EntityId> getDirtyEntities() const noexcept;

private:
    std::map<EntityId, CachedGeometry> cache_;
};

} // namespace BendCore::CadCore
