#pragma once

#include "bendcore/document/cad_document.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <chrono>

namespace BendCore::CadCore {

/**
 * @brief Serialization result with error handling
 */
struct SerializationResult {
    bool success = false;
    std::string error;
    uint32_t crc32 = 0;
    size_t size = 0;

    SerializationResult() = default;
    explicit SerializationResult(bool ok, const std::string& err = "")
        : success(ok), error(err) {}
};

/**
 * @brief Document format version
 */
struct DocumentVersion {
    uint16_t major = 1;
    uint16_t minor = 0;
    uint32_t revision = 0;

    [[nodiscard]] uint64_t toUint64() const noexcept {
        return (static_cast<uint64_t>(major) << 48) |
               (static_cast<uint64_t>(minor) << 32) | revision;
    }

    static DocumentVersion fromUint64(uint64_t v) noexcept {
        return {
            static_cast<uint16_t>(v >> 48),
            static_cast<uint16_t>((v >> 32) & 0xFFFF),
            static_cast<uint32_t>(v & 0xFFFFFFFF)
        };
    }
};

/**
 * @brief Document metadata for serialization
 */
struct DocumentMetadata {
    std::string title;
    std::string author;
    std::string description;
    std::chrono::system_clock::time_point created_time;
    std::chrono::system_clock::time_point modified_time;
    DocumentVersion version;
    std::map<std::string, std::string> custom_properties;
};

/**
 * @brief Document serialization with version control (GAP-018: Save/Load)
 *
 * Supports:
 * - Multiple formats (JSON, Binary)
 * - Version management with migrations
 * - CRC32 integrity checks
 * - Emergency recovery
 */
class DocumentSerializer {
public:
    /**
     * @brief Create serializer
     * @param format BINARY or JSON
     */
    enum class Format { BINARY, JSON };

    explicit DocumentSerializer(Format format = Format::BINARY);

    /**
     * @brief Serialize document to bytes
     */
    [[nodiscard]] SerializationResult serialize(
        const CadDocument& doc,
        const DocumentMetadata& metadata = DocumentMetadata()
    ) noexcept;

    /**
     * @brief Deserialize document from bytes
     */
    [[nodiscard]] SerializationResult deserialize(
        CadDocument& doc,
        const std::vector<uint8_t>& data
    ) noexcept;

    /**
     * @brief Save document to file
     */
    [[nodiscard]] SerializationResult serializeToFile(
        const CadDocument& doc,
        const std::string& filepath,
        const DocumentMetadata& metadata = DocumentMetadata()
    ) noexcept;

    /**
     * @brief Load document from file
     */
    [[nodiscard]] SerializationResult deserializeFromFile(
        CadDocument& doc,
        const std::string& filepath
    ) noexcept;

    /**
     * @brief Create backup before save
     */
    [[nodiscard]] SerializationResult createBackup(
        const std::string& original_filepath
    ) noexcept;

    /**
     * @brief Recover from backup
     */
    [[nodiscard]] SerializationResult recoverFromBackup(
        const std::string& backup_filepath,
        const std::string& target_filepath
    ) noexcept;

    /**
     * @brief Set format
     */
    void setFormat(Format format) noexcept { format_ = format; }

    /**
     * @brief Get format
     */
    [[nodiscard]] Format getFormat() const noexcept { return format_; }

    /**
     * @brief Set compression enabled
     */
    void setCompressionEnabled(bool enabled) noexcept { compression_enabled_ = enabled; }

    /**
     * @brief Check if file is valid
     */
    [[nodiscard]] bool isValidFile(const std::string& filepath) noexcept;

private:
    Format format_;
    bool compression_enabled_;

    // Serialization helpers
    [[nodiscard]] std::vector<uint8_t> serializeToBinary(
        const CadDocument& doc,
        const DocumentMetadata& metadata
    ) const;

    [[nodiscard]] bool deserializeFromBinary(
        CadDocument& doc,
        const std::vector<uint8_t>& data
    );

    [[nodiscard]] std::vector<uint8_t> serializeToJSON(
        const CadDocument& doc,
        const DocumentMetadata& metadata
    ) const;

    [[nodiscard]] bool deserializeFromJSON(
        CadDocument& doc,
        const std::vector<uint8_t>& data
    );

    // Integrity checks
    static uint32_t calculateCRC32(const std::vector<uint8_t>& data);
    static bool verifyCRC32(const std::vector<uint8_t>& data, uint32_t expected_crc);
};

} // namespace BendCore::CadCore
