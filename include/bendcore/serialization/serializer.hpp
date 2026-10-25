#pragma once

#include "../document/entity_id.hpp"
#include "../document/cad_document.hpp"
#include <string>
#include <vector>
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief Serialization format
 */
enum class SerializationFormat {
    BINARY,
    JSON,
    PROTOBUF
};

/**
 * @brief Serializer for CAD documents
 */
class Serializer {
public:
    Serializer(SerializationFormat format = SerializationFormat::BINARY) noexcept;

    /**
     * @brief Serialize document to bytes
     */
    std::vector<uint8_t> serialize(const CadDocument& doc) const noexcept;

    /**
     * @brief Deserialize document from bytes
     */
    bool deserialize(CadDocument& doc, const std::vector<uint8_t>& data) noexcept;

    /**
     * @brief Serialize to file
     */
    bool serializeToFile(const CadDocument& doc, const std::string& filePath) const noexcept;

    /**
     * @brief Deserialize from file
     */
    bool deserializeFromFile(CadDocument& doc, const std::string& filePath) noexcept;

    /**
     * @brief Set serialization format
     */
    void setFormat(SerializationFormat format) noexcept { format_ = format; }

    /**
     * @brief Get serialization format
     */
    SerializationFormat getFormat() const noexcept { return format_; }

private:
    SerializationFormat format_;
};

} // namespace BendCore::CadCore
