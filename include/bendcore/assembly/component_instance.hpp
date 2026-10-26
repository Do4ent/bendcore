#pragma once

#include "../document/entity_id.hpp"
#include "../geometry/vector.hpp"
#include <string>
#include <memory>
#include <map>
#include <optional>
#include <cstdint>

namespace BendCore::CadCore {

/**
 * @brief Component Instance - Reference to a Part in an Assembly
 *
 * Represents a single occurrence/instance of a Part in an Assembly.
 * Each instance has:
 * - Reference to the Part being instanced
 * - Unique ID for the instance
 * - Position and orientation in assembly space
 * - Optional name override
 * - Visibility and selection state
 *
 * Analogous to:
 * - SOLIDWORKS component instance
 * - Inventor occurrence
 * - NX component
 * - Creo assembly component
 */
class ComponentInstance {
public:
    /**
     * @brief Constructor
     * @param instance_id Unique ID for this instance
     * @param part_id ID of the Part being referenced
     * @param position Initial position (default: origin)
     * @param orientation Initial orientation as Euler angles (default: identity)
     */
    ComponentInstance(
        const EntityId& instance_id,
        const EntityId& part_id,
        const Vector3D& position = Vector3D(0, 0, 0),
        const Vector3D& orientation = Vector3D(0, 0, 0)
    );

    /**
     * @brief Get instance ID
     */
    [[nodiscard]] const EntityId& getId() const noexcept { return instance_id_; }

    /**
     * @brief Get referenced Part ID
     */
    [[nodiscard]] const EntityId& getPartId() const noexcept { return part_id_; }

    /**
     * @brief Get instance name (or part name if not overridden)
     */
    [[nodiscard]] const std::string& getName() const noexcept { return name_; }

    /**
     * @brief Set instance name
     */
    void setName(const std::string& name) noexcept { name_ = name; }

    /**
     * @brief Get instance position
     */
    [[nodiscard]] const Vector3D& getPosition() const noexcept { return position_; }

    /**
     * @brief Set instance position
     */
    void setPosition(const Vector3D& position) noexcept;

    /**
     * @brief Get instance orientation (Euler angles in degrees)
     */
    [[nodiscard]] const Vector3D& getOrientation() const noexcept { return orientation_; }

    /**
     * @brief Set instance orientation (Euler angles in degrees)
     */
    void setOrientation(const Vector3D& orientation) noexcept;

    /**
     * @brief Get instance transformation matrix as 4x4
     * @return 16 values in row-major order
     */
    [[nodiscard]] std::array<double, 16> getTransformMatrix() const noexcept;

    /**
     * @brief Get instance visibility state
     */
    [[nodiscard]] bool isVisible() const noexcept { return is_visible_; }

    /**
     * @brief Set instance visibility
     */
    void setVisible(bool visible) noexcept { is_visible_ = visible; }

    /**
     * @brief Get instance selection state
     */
    [[nodiscard]] bool isSelected() const noexcept { return is_selected_; }

    /**
     * @brief Set instance selection state
     */
    void setSelected(bool selected) noexcept { is_selected_ = selected; }

    /**
     * @brief Get instance suppression state (suppressed instances are not included in assembly)
     */
    [[nodiscard]] bool isSuppressed() const noexcept { return is_suppressed_; }

    /**
     * @brief Set instance suppression state
     */
    void setSuppressed(bool suppressed) noexcept { is_suppressed_ = suppressed; }

    /**
     * @brief Get instance revision number
     */
    [[nodiscard]] uint64_t getRevision() const noexcept { return revision_; }

    /**
     * @brief Increment instance revision
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
     * @brief Check if instance is primary (original, not a copy)
     * In assemblies, typically the first instance is primary and editable
     */
    [[nodiscard]] bool isPrimary() const noexcept { return is_primary_; }

    /**
     * @brief Set instance as primary
     */
    void setPrimary(bool primary) noexcept { is_primary_ = primary; }

    /**
     * @brief Get description for debugging
     */
    [[nodiscard]] std::string getDescription() const noexcept;

    /**
     * @brief Validate instance
     * @return error message or empty string if valid
     */
    [[nodiscard]] std::string validate() const noexcept;

private:
    EntityId instance_id_;
    EntityId part_id_;
    std::string name_;
    Vector3D position_;      // Position in assembly space
    Vector3D orientation_;   // Orientation as Euler angles (in degrees)
    bool is_visible_ = true;
    bool is_selected_ = false;
    bool is_suppressed_ = false;
    bool is_primary_ = false;
    uint64_t revision_ = 0;
    std::map<std::string, std::string> metadata_;

    /**
     * @brief Convert Euler angles to transformation matrix
     */
    std::array<double, 16> eulerToMatrix(
        const Vector3D& position,
        const Vector3D& orientation
    ) const noexcept;
};

/**
 * @brief Factory function to create a component instance
 * @param part_id Part ID to instance
 * @param position Position in assembly space
 * @param orientation Orientation (Euler angles)
 * @return shared_ptr to new ComponentInstance
 */
[[nodiscard]] std::shared_ptr<ComponentInstance> createComponentInstance(
    const EntityId& part_id,
    const Vector3D& position = Vector3D(0, 0, 0),
    const Vector3D& orientation = Vector3D(0, 0, 0)
);

} // namespace BendCore::CadCore
