#include "bendcore/serialization/document_serializer.hpp"
#include "nlohmann/json.hpp"
#include <fstream>
#include <sstream>
#include <zlib.h>
#include <cstring>
#include <filesystem>

namespace BendCore::CadCore {

using json = nlohmann::json;
namespace fs = std::filesystem;

// CRC32 implementation
uint32_t DocumentSerializer::calculateCRC32(const std::vector<uint8_t>& data) {
    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, data.data(), data.size());
    return crc;
}

bool DocumentSerializer::verifyCRC32(const std::vector<uint8_t>& data, uint32_t expected_crc) {
    uint32_t calculated = calculateCRC32(data);
    return calculated == expected_crc;
}

DocumentSerializer::DocumentSerializer(Format format)
    : format_(format)
    , compression_enabled_(true)
{}

SerializationResult DocumentSerializer::serialize(
    const CadDocument& doc,
    const DocumentMetadata& metadata) noexcept
{
    try {
        std::vector<uint8_t> serialized;

        if (format_ == Format::JSON) {
            serialized = serializeToJSON(doc, metadata);
        } else {
            serialized = serializeToBinary(doc, metadata);
        }

        if (serialized.empty()) {
            return SerializationResult(false, "Serialization produced empty result");
        }

        [[maybe_unused]] uint32_t crc = calculateCRC32(serialized);

        return SerializationResult(true);
    } catch (const std::exception& e) {
        return SerializationResult(false, std::string("Serialization failed: ") + e.what());
    }
}

SerializationResult DocumentSerializer::deserialize(
    CadDocument& doc,
    const std::vector<uint8_t>& data) noexcept
{
    try {
        if (data.size() < 4) {
            return SerializationResult(false, "Data too small");
        }

        if (format_ == Format::JSON) {
            if (!deserializeFromJSON(doc, data)) {
                return SerializationResult(false, "JSON deserialization failed");
            }
        } else {
            if (!deserializeFromBinary(doc, data)) {
                return SerializationResult(false, "Binary deserialization failed");
            }
        }

        return SerializationResult(true);
    } catch (const std::exception& e) {
        return SerializationResult(false, std::string("Deserialization failed: ") + e.what());
    }
}

SerializationResult DocumentSerializer::serializeToFile(
    const CadDocument& doc,
    const std::string& filepath,
    const DocumentMetadata& metadata) noexcept
{
    try {
        // Create backup
        if (fs::exists(filepath)) {
            auto backup_result = createBackup(filepath);
            if (!backup_result.success) {
                return backup_result;
            }
        }

        auto serialized = serialize(doc, metadata);
        if (!serialized.success) {
            return serialized;
        }

        // Open file for writing
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return SerializationResult(false, "Cannot open file for writing: " + filepath);
        }

        // Serialize to binary first
        auto binary_data = serializeToBinary(doc, metadata);

        // Write header with CRC32
        uint32_t crc = calculateCRC32(binary_data);
        file.write(reinterpret_cast<const char*>(&crc), sizeof(uint32_t));
        file.write(reinterpret_cast<const char*>(binary_data.data()), binary_data.size());

        if (!file.good()) {
            file.close();
            return SerializationResult(false, "Write failed");
        }

        file.close();

        SerializationResult result(true);
        result.crc32 = crc;
        result.size = binary_data.size();
        return result;
    } catch (const std::exception& e) {
        return SerializationResult(false, std::string("File serialization failed: ") + e.what());
    }
}

SerializationResult DocumentSerializer::deserializeFromFile(
    CadDocument& doc,
    const std::string& filepath) noexcept
{
    try {
        if (!fs::exists(filepath)) {
            return SerializationResult(false, "File not found: " + filepath);
        }

        // Read file
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return SerializationResult(false, "Cannot open file for reading: " + filepath);
        }

        // Read CRC32
        uint32_t stored_crc;
        file.read(reinterpret_cast<char*>(&stored_crc), sizeof(uint32_t));

        // Read rest of file
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
        file.close();

        // Verify CRC32
        if (!verifyCRC32(data, stored_crc)) {
            return SerializationResult(false, "CRC32 verification failed - file may be corrupted");
        }

        return deserialize(doc, data);
    } catch (const std::exception& e) {
        return SerializationResult(false, std::string("File deserialization failed: ") + e.what());
    }
}

SerializationResult DocumentSerializer::createBackup(const std::string& original_filepath) noexcept {
    try {
        if (!fs::exists(original_filepath)) {
            return SerializationResult(true);  // Nothing to backup
        }

        std::string backup_filepath = original_filepath + ".backup";
        fs::copy_file(original_filepath, backup_filepath, fs::copy_options::overwrite_existing);

        return SerializationResult(true);
    } catch (const std::exception& e) {
        return SerializationResult(false, std::string("Backup failed: ") + e.what());
    }
}

SerializationResult DocumentSerializer::recoverFromBackup(
    const std::string& backup_filepath,
    const std::string& target_filepath) noexcept
{
    try {
        if (!fs::exists(backup_filepath)) {
            return SerializationResult(false, "Backup file not found: " + backup_filepath);
        }

        fs::copy_file(backup_filepath, target_filepath, fs::copy_options::overwrite_existing);

        return SerializationResult(true);
    } catch (const std::exception& e) {
        return SerializationResult(false, std::string("Recovery failed: ") + e.what());
    }
}

bool DocumentSerializer::isValidFile(const std::string& filepath) noexcept {
    try {
        if (!fs::exists(filepath)) {
            return false;
        }

        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        // Check minimum size
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        if (size < sizeof(uint32_t) + 100) {  // At least CRC + some data
            file.close();
            return false;
        }

        // Try to read CRC32
        file.seekg(0, std::ios::beg);
        uint32_t stored_crc;
        file.read(reinterpret_cast<char*>(&stored_crc), sizeof(uint32_t));

        // Read rest
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
        file.close();

        // Verify CRC32
        return verifyCRC32(data, stored_crc);
    } catch (...) {
        return false;
    }
}

std::vector<uint8_t> DocumentSerializer::serializeToBinary(
    const CadDocument& doc,
    const DocumentMetadata& metadata) const
{
    std::vector<uint8_t> buffer;
    buffer.reserve(102400);  // 100KB initial

    try {
        // Write version
        uint64_t version = metadata.version.toUint64();
        size_t pos = buffer.size();
        buffer.resize(pos + sizeof(uint64_t));
        *reinterpret_cast<uint64_t*>(buffer.data() + pos) = version;

        // Write metadata
        auto write_string = [&](const std::string& str) {
            uint32_t size = str.size();
            pos = buffer.size();
            buffer.resize(pos + sizeof(uint32_t) + size);
            *reinterpret_cast<uint32_t*>(buffer.data() + pos) = size;
            if (size > 0) {
                std::copy(str.begin(), str.end(), buffer.begin() + pos + sizeof(uint32_t));
            }
        };

        write_string(metadata.title);
        write_string(metadata.author);
        write_string(metadata.description);

        // Write document revision
        uint64_t doc_revision = doc.getRevision();
        pos = buffer.size();
        buffer.resize(pos + sizeof(uint64_t));
        *reinterpret_cast<uint64_t*>(buffer.data() + pos) = doc_revision;

        // Write entities
        auto entities = doc.getEntityIds();
        uint32_t entity_count = entities.size();
        pos = buffer.size();
        buffer.resize(pos + sizeof(uint32_t));
        *reinterpret_cast<uint32_t*>(buffer.data() + pos) = entity_count;

        for (const auto& entity_id : entities) {
            const auto& entity = doc.getEntity(entity_id);

            // Entity ID
            std::string id_str = entity_id.toString();
            write_string(id_str);

            // Entity type
            uint32_t type = static_cast<uint32_t>(entity.type);
            pos = buffer.size();
            buffer.resize(pos + sizeof(uint32_t));
            *reinterpret_cast<uint32_t*>(buffer.data() + pos) = type;

            // Entity name
            write_string(entity.name);

            // Parent ID
            std::string parent_str = entity.parent_id.toString();
            write_string(parent_str);

            // Revision and state
            uint64_t entity_rev = entity.revision;
            uint32_t state = static_cast<uint32_t>(entity.state);
            pos = buffer.size();
            buffer.resize(pos + sizeof(uint64_t) + sizeof(uint32_t));
            *reinterpret_cast<uint64_t*>(buffer.data() + pos) = entity_rev;
            *reinterpret_cast<uint32_t*>(buffer.data() + pos + sizeof(uint64_t)) = state;
        }

    } catch (const std::exception&) {
        return std::vector<uint8_t>();
    }

    return buffer;
}

bool DocumentSerializer::deserializeFromBinary(
    CadDocument& /*doc*/,
    const std::vector<uint8_t>& data)
{
    try {
        if (data.size() < sizeof(uint64_t)) {
            return false;
        }

        size_t pos = 0;

        // Read version
        [[maybe_unused]] uint64_t version = *reinterpret_cast<const uint64_t*>(data.data() + pos);
        pos += sizeof(uint64_t);

        // TODO: Version migration logic here

        // For now, just validate structure
        if (pos >= data.size()) {
            return false;
        }

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::vector<uint8_t> DocumentSerializer::serializeToJSON(
    const CadDocument& doc,
    const DocumentMetadata& metadata) const
{
    try {
        json j;

        // Version
        j["version"]["major"] = metadata.version.major;
        j["version"]["minor"] = metadata.version.minor;
        j["version"]["revision"] = metadata.version.revision;

        // Metadata
        j["metadata"]["title"] = metadata.title;
        j["metadata"]["author"] = metadata.author;
        j["metadata"]["description"] = metadata.description;

        // Document info
        j["document"]["revision"] = doc.getRevision();

        // Entities
        auto entities = doc.getEntityIds();
        for (const auto& entity_id : entities) {
            const auto& entity = doc.getEntity(entity_id);

            json entity_obj;
            entity_obj["id"] = entity_id.toString();
            entity_obj["type"] = static_cast<int>(entity.type);
            entity_obj["name"] = entity.name;
            entity_obj["parent_id"] = entity.parent_id.toString();
            entity_obj["revision"] = entity.revision;
            entity_obj["state"] = static_cast<int>(entity.state);

            j["entities"].push_back(entity_obj);
        }

        // Convert to bytes
        std::string json_str = j.dump();
        std::vector<uint8_t> result(json_str.begin(), json_str.end());

        return result;
    } catch (const std::exception&) {
        return std::vector<uint8_t>();
    }
}

bool DocumentSerializer::deserializeFromJSON(
    CadDocument& /*doc*/,
    const std::vector<uint8_t>& data)
{
    try {
        std::string json_str(data.begin(), data.end());
        json j = json::parse(json_str);

        // TODO: Deserialize entities and restore document state

        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace BendCore::CadCore
