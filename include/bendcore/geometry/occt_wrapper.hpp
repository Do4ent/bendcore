#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../document/entity_id.hpp"
#include "vector.hpp"

namespace BendCore::CadCore {

/**
 * @brief Open CASCADE Technology (OCCT) Wrapper
 *
 * Provides integration with OCCT for:
 * - Advanced geometry operations
 * - STEP/IGES import/export
 * - Surface modeling
 * - Direct editing (face operations)
 * - Shape repair and healing
 *
 * OCCT is optional: if not available, falls back to CGAL/Eigen
 */
class OccWrapper {
public:
    /**
     * @brief Check if OCCT is available
     */
    [[nodiscard]] static bool isAvailable() noexcept;

    /**
     * @brief Get OCCT version string
     */
    [[nodiscard]] static std::string getVersion() noexcept;

    /**
     * @brief Create STEP exporter
     */
    class StepExporter {
    public:
        /**
         * @brief Export shape to STEP file
         */
        bool exportShape(const std::string& filename,
                        const EntityId& shape_id) noexcept;

        /**
         * @brief Export assembly to STEP file
         */
        bool exportAssembly(const std::string& filename,
                           const std::vector<EntityId>& components) noexcept;

    private:
        // OCCT internals
    };

    /**
     * @brief Create IGES exporter
     */
    class IgesExporter {
    public:
        bool exportShape(const std::string& filename,
                        const EntityId& shape_id) noexcept;
    };

    /**
     * @brief Create STEP importer
     */
    class StepImporter {
    public:
        /**
         * @brief Import STEP file
         * Returns: list of imported entity IDs
         */
        std::vector<EntityId> importShape(const std::string& filename) noexcept;

        bool hasErrors() const noexcept { return !errors_.empty(); }
        std::string getErrors() const noexcept { return errors_; }

    private:
        std::string errors_;
    };

    /**
     * @brief Shape repair and healing
     */
    class ShapeHealer {
    public:
        /**
         * @brief Heal shape (fix topology issues)
         */
        bool healShape(const EntityId& shape_id) noexcept;

        /**
         * @brief Remove unused faces
         */
        bool removeFaces(const EntityId& shape_id,
                        const std::vector<size_t>& face_indices) noexcept;

        /**
         * @brief Offset face
         */
        bool offsetFace(const EntityId& shape_id,
                       size_t face_index,
                       double offset_distance) noexcept;

    private:
        // OCCT internals
    };

    /**
     * @brief Surface modeling operations
     */
    class SurfaceOps {
    public:
        /**
         * @brief Create surface from profiles (loft)
         */
        EntityId loftProfiles(const std::vector<EntityId>& profiles) noexcept;

        /**
         * @brief Create surface by sweeping profile
         */
        EntityId sweepProfile(const EntityId& profile,
                             const EntityId& path) noexcept;

    private:
        // OCCT internals
    };

    /**
     * @brief Boolean operations
     */
    class BooleanOps {
    public:
        /**
         * @brief Union (combine) two shapes
         */
        EntityId unite(const EntityId& shape1,
                      const EntityId& shape2) noexcept;

        /**
         * @brief Difference (cut) shape1 by shape2
         */
        EntityId cut(const EntityId& shape1,
                    const EntityId& shape2) noexcept;

        /**
         * @brief Intersection of two shapes
         */
        EntityId intersect(const EntityId& shape1,
                          const EntityId& shape2) noexcept;

    private:
        // OCCT internals
    };
};

} // namespace BendCore::CadCore
