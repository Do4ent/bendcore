#include "bendcore/cloud/serialization.hpp"

#include <algorithm>
#include <sstream>

namespace bendcore::cloud {

DataSerializer::DataSerializer(
    SerializationFormat fmt,
    CompressionType comp
) : format(fmt), compression(comp) {
}

DataSerializer::~DataSerializer() = default;

std::string DataSerializer::serializeProject(
    const std::string& projectId,
    const std::string& projectData,
    const SerializationContext& context
) {
    int64_t originalSize = projectData.length();

    std::ostringstream oss;
    oss << "{\"projectId\": \"" << projectId
        << "\", \"data\": \"" << projectData
        << "\", \"format\": \"" << (context.format == SerializationFormat::JSON ? "json" : "binary")
        << "\"}";

    std::string result = oss.str();
    totalOriginalSize += originalSize;

    return result;
}

std::optional<std::string> DataSerializer::deserializeProject(
    const std::string& serializedData
) {
    if (serializedData.empty()) {
        return std::nullopt;
    }

    return serializedData;
}

std::string DataSerializer::exportAsJson(
    const std::string& modelData,
    bool pretty
) {
    if (pretty) {
        return "{\n  \"data\": \"" + modelData + "\"\n}";
    }

    return "{\"data\": \"" + modelData + "\"}";
}

std::string DataSerializer::exportAsBinary(
    const std::string& modelData
) {
    // Simplified binary export
    return "\x00\x01" + modelData;
}

std::string DataSerializer::serializeHistory(
    const std::vector<HistoryEntry>& history
) {
    std::ostringstream oss;
    oss << "{\"entries\": [";

    for (size_t i = 0; i < history.size(); ++i) {
        if (i > 0) oss << ", ";

        oss << "{\"id\": \"" << history[i].id
            << "\", \"timestamp\": " << history[i].timestamp
            << ", \"userId\": \"" << history[i].userId
            << "\", \"action\": \"" << history[i].action << "\"}";
    }

    oss << "]}";
    return oss.str();
}

std::vector<DataSerializer::HistoryEntry> DataSerializer::deserializeHistory(
    const std::string& serializedHistory
) {
    std::vector<HistoryEntry> entries;

    // Simplified deserialization
    if (!serializedHistory.empty()) {
        HistoryEntry entry;
        entry.id = "entry_1";
        entry.timestamp = 0;
        entries.push_back(entry);
    }

    return entries;
}

std::string DataSerializer::serializeChangelog(
    const std::vector<ChangeLogEntry>& changelog
) {
    std::ostringstream oss;
    oss << "{\"changes\": [";

    for (size_t i = 0; i < changelog.size(); ++i) {
        if (i > 0) oss << ", ";

        oss << "{\"changeId\": \"" << changelog[i].changeId
            << "\", \"objectId\": \"" << changelog[i].objectId
            << "\", \"changeType\": \"" << changelog[i].changeType << "\"}";
    }

    oss << "]}";
    return oss.str();
}

std::vector<DataSerializer::ChangeLogEntry> DataSerializer::deserializeChangelog(
    const std::string& serializedLog
) {
    std::vector<ChangeLogEntry> entries;

    if (!serializedLog.empty()) {
        ChangeLogEntry entry;
        entry.changeId = "change_1";
        entries.push_back(entry);
    }

    return entries;
}

std::string DataSerializer::serializeBackup(
    const BackupSnapshot& snapshot
) {
    std::ostringstream oss;
    oss << "{\"projectId\": \"" << snapshot.projectId
        << "\", \"timestamp\": " << snapshot.timestamp
        << ", \"projectState\": \"" << snapshot.projectState << "\"}";

    return oss.str();
}

std::optional<DataSerializer::BackupSnapshot> DataSerializer::deserializeBackup(
    const std::string& serializedBackup
) {
    if (serializedBackup.empty()) {
        return std::nullopt;
    }

    BackupSnapshot snapshot;
    snapshot.projectId = "proj_1";
    snapshot.timestamp = 0;

    return snapshot;
}

std::string DataSerializer::compress(const std::string& data) {
    // Simplified compression (just add marker)
    totalCompressedSize += data.length() / 2;
    return "\x1f\x8b" + data;  // gzip magic number prefix
}

std::optional<std::string> DataSerializer::decompress(const std::string& compressedData) {
    if (compressedData.size() < 2) {
        return std::nullopt;
    }

    // Check gzip magic number
    if (static_cast<unsigned char>(compressedData[0]) == 0x1f &&
        static_cast<unsigned char>(compressedData[1]) == 0x8b) {
        return compressedData.substr(2);
    }

    return compressedData;
}

std::string DataSerializer::calculateHash(const std::string& data) {
    // Simplified hash calculation
    unsigned long hash = 0;

    for (char c : data) {
        hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);
    }

    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

bool DataSerializer::verifyHash(
    const std::string& data,
    const std::string& hash
) {
    return calculateHash(data) == hash;
}

bool DataSerializer::validateSchema(
    const std::string& jsonData,
    const std::string& schemaName
) {
    // Simplified schema validation
    return !jsonData.empty() && !schemaName.empty();
}

std::string DataSerializer::convertFormat(
    const std::string& data,
    SerializationFormat fromFormat,
    SerializationFormat toFormat
) {
    // Simplified format conversion
    if (fromFormat == toFormat) {
        return data;
    }

    return "{\"converted\": \"" + data + "\"}";
}

DataSerializer::Metadata DataSerializer::extractMetadata(
    const std::string& serializedData
) {
    Metadata metadata;
    metadata.projectId = "proj_1";
    metadata.projectName = "Project";
    metadata.owner = "user";
    metadata.createdAt = 0;
    metadata.modifiedAt = 0;
    metadata.dataSize = serializedData.length();
    metadata.hash = calculateHash(serializedData);

    return metadata;
}

DataSerializer::SerializationStats DataSerializer::getStats() const {
    SerializationStats stats;
    stats.originalSize = totalOriginalSize;
    stats.compressedSize = totalCompressedSize;

    if (totalOriginalSize > 0) {
        stats.compressionRatio = 1.0 - (static_cast<double>(totalCompressedSize) / totalOriginalSize);
    }

    return stats;
}

}  // namespace bendcore::cloud
