#pragma once

#include <string>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <functional>

namespace BendCore::CadCore {

/**
 * @brief Stable permanent identifier for all CAD objects
 *
 * Requirements:
 * - Does not change after renaming
 * - Does not change after moving in tree
 * - Persists across save/load cycles
 * - Does not depend on memory address
 * - Copy creates new instance ID but preserves reference to shared part
 * - Format validity can be checked
 * - Serializes without loss of precision
 *
 * Implementation: UUID v4 (128-bit random)
 * Format: "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx" (RFC 4122)
 */
class EntityId {
public:
    // 16 bytes for UUID
    using UuidBytes = std::array<uint8_t, 16>;

    /**
     * @brief Generate a new random EntityId (UUID v4)
     */
    static EntityId generate() noexcept;

    /**
     * @brief Parse EntityId from string representation
     * @throws std::invalid_argument if format is invalid
     */
    static EntityId parse(const std::string& str);

    /**
     * @brief Check if string is valid EntityId format
     */
    static bool isValid(const std::string& str) noexcept;

    /**
     * @brief Default constructor - creates null ID
     */
    EntityId() noexcept;

    /**
     * @brief Construct from UUID bytes
     */
    explicit EntityId(const UuidBytes& bytes) noexcept;

    /**
     * @brief Copy constructor
     */
    EntityId(const EntityId&) noexcept = default;

    /**
     * @brief Move constructor
     */
    EntityId(EntityId&&) noexcept = default;

    /**
     * @brief Copy assignment
     */
    EntityId& operator=(const EntityId&) noexcept = default;

    /**
     * @brief Move assignment
     */
    EntityId& operator=(EntityId&&) noexcept = default;

    /**
     * @brief Convert to string representation
     */
    std::string toString() const;

    /**
     * @brief Get raw UUID bytes
     */
    const UuidBytes& getBytes() const noexcept { return uuid_; }

    /**
     * @brief Check if ID is null (default constructed)
     */
    bool isNull() const noexcept;

    /**
     * @brief Equality comparison
     */
    bool operator==(const EntityId& other) const noexcept;

    /**
     * @brief Inequality comparison
     */
    bool operator!=(const EntityId& other) const noexcept;

    /**
     * @brief Less-than comparison (for use in maps/sets)
     */
    bool operator<(const EntityId& other) const noexcept;

private:
    UuidBytes uuid_;

    /**
     * @brief Check UUID format (RFC 4122 v4)
     */
    static bool validateUuidFormat(const std::string& str) noexcept;

    /**
     * @brief Convert hex string to byte
     */
    static uint8_t hexToByte(const std::string& hex);
};

} // namespace BendCore::CadCore

/**
 * @brief Hash function for EntityId (for use in unordered_map/set)
 */
namespace std {
template<>
struct hash<BendCore::CadCore::EntityId> {
    size_t operator()(const BendCore::CadCore::EntityId& id) const noexcept;
};
} // namespace std
