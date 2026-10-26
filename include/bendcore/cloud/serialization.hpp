#pragma once
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>

namespace bendcore::cloud {

// Serialization format
enum class SerializationFormat {
    JSON,
    MessagePack,
    BSON,
    ProtoBuf,
    Binary
};

// Compression algorithm
enum class CompressionType {
    None,
    Gzip,
    Brotli,
    Zstd
};

// Serialization context
struct SerializationContext {
    SerializationFormat format = SerializationFormat::JSON;
    CompressionType compression = CompressionType::Gzip;
    bool includeMetadata = true;
    bool validateSchema = true;
};

// Data serializer
class DataSerializer {
public:
    explicit DataSerializer(
        SerializationFormat format = SerializationFormat::JSON,
        CompressionType compression = CompressionType::Gzip
    );
    ~DataSerializer();

    // Project serialization
    std::string serializeProject(
        const std::string& projectId,
        const std::string& projectData,
        const SerializationContext& context = {}
    );

    std::optional<std::string> deserializeProject(
        const std::string& serializedData
    );

    // Model export
    std::string exportAsJson(
        const std::string& modelData,
        bool pretty = true
    );

    std::string exportAsBinary(
        const std::string& modelData
    );

    // History export
    struct HistoryEntry {
        std::string id;
        int64_t timestamp = 0;
        std::string userId;
        std::string action;
        std::string details;
    };

    std::string serializeHistory(
        const std::vector<HistoryEntry>& history
    );

    std::vector<HistoryEntry> deserializeHistory(
        const std::string& serializedHistory
    );

    // Change log serialization
    struct ChangeLogEntry {
        std::string changeId;
        std::string objectId;
        std::string userId;
        std::string changeType;
        int64_t timestamp = 0;
        std::string beforeValue;
        std::string afterValue;
    };

    std::string serializeChangelog(
        const std::vector<ChangeLogEntry>& changelog
    );

    std::vector<ChangeLogEntry> deserializeChangelog(
        const std::string& serializedLog
    );

    // Backup serialization
    struct BackupSnapshot {
        std::string projectId;
        int64_t timestamp = 0;
        std::string projectState;
        std::string metadata;
        std::vector<HistoryEntry> history;
    };

    std::string serializeBackup(
        const BackupSnapshot& snapshot
    );

    std::optional<BackupSnapshot> deserializeBackup(
        const std::string& serializedBackup
    );

    // Compression
    std::string compress(const std::string& data);

    std::optional<std::string> decompress(const std::string& compressedData);

    // Checksum/Hash
    std::string calculateHash(const std::string& data);

    bool verifyHash(
        const std::string& data,
        const std::string& hash
    );

    // Schema validation
    bool validateSchema(
        const std::string& jsonData,
        const std::string& schemaName
    );

    // Format conversion
    std::string convertFormat(
        const std::string& data,
        SerializationFormat fromFormat,
        SerializationFormat toFormat
    );

    // Metadata extraction
    struct Metadata {
        std::string projectId;
        std::string projectName;
        std::string owner;
        int64_t createdAt = 0;
        int64_t modifiedAt = 0;
        int64_t dataSize = 0;
        std::string hash;
        std::map<std::string, std::string> customTags;
    };

    Metadata extractMetadata(const std::string& serializedData);

    // Statistics
    struct SerializationStats {
        int64_t originalSize = 0;
        int64_t compressedSize = 0;
        double compressionRatio = 0.0;
        int64_t serializationTimeMs = 0;
        int64_t deserializationTimeMs = 0;
    };

    SerializationStats getStats() const;

private:
    SerializationFormat format;
    CompressionType compression;
    int64_t totalOriginalSize = 0;
    int64_t totalCompressedSize = 0;
};

}  // namespace bendcore::cloud
