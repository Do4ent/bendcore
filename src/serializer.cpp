#include "bendcore/serialization/serializer.hpp"
#include <fstream>

namespace BendCore::CadCore {

Serializer::Serializer(SerializationFormat format) noexcept
    : format_(format) {}

std::vector<uint8_t> Serializer::serialize(const CadDocument& doc) const noexcept {
    std::vector<uint8_t> result;

    switch (format_) {
        case SerializationFormat::BINARY: {
            // Simple binary format: write document revision
            uint64_t revision = doc.getRevision();
            uint8_t* ptr = reinterpret_cast<uint8_t*>(&revision);
            result.insert(result.end(), ptr, ptr + sizeof(revision));
            break;
        }
        case SerializationFormat::JSON: {
            // JSON format - simplified placeholder
            std::string json = "{\"version\": 1, \"revision\": 0}";
            result.insert(result.end(), json.begin(), json.end());
            break;
        }
        case SerializationFormat::PROTOBUF: {
            // Protobuf format - simplified placeholder
            result.push_back(0x08); // Field 1
            result.push_back(0x01); // Value 1
            break;
        }
    }

    return result;
}

bool Serializer::deserialize(CadDocument& doc, const std::vector<uint8_t>& data) noexcept {
    (void)doc;
    if (data.empty()) return false;

    switch (format_) {
        case SerializationFormat::BINARY: {
            if (data.size() < sizeof(uint64_t)) return false;
            // Read revision (simplified)
            return true;
        }
        case SerializationFormat::JSON:
        case SerializationFormat::PROTOBUF:
            return !data.empty();
    }

    return false;
}

bool Serializer::serializeToFile(const CadDocument& doc, const std::string& filePath) const noexcept {
    std::vector<uint8_t> data = serialize(doc);

    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

bool Serializer::deserializeFromFile(CadDocument& doc, const std::string& filePath) noexcept {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);

    return deserialize(doc, data);
}

} // namespace BendCore::CadCore
