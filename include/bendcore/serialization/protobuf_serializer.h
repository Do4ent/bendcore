#pragma once

#include <string>
#include <vector>
#include <memory>

namespace bendcore::serialization {

/**
 * Protobuf Serializer wrapper
 * Provides efficient binary serialization with version compatibility
 */
class ProtobufSerializer {
public:
    /**
     * Serialize data to binary format
     */
    static std::vector<uint8_t> serialize_model(const std::string& model_name,
                                                const std::string& json_data);

    /**
     * Deserialize data from binary format
     */
    static std::string deserialize_model(const std::vector<uint8_t>& binary_data);

    /**
     * Serialize with compression
     */
    static std::vector<uint8_t> serialize_compressed(const std::string& data);

    /**
     * Deserialize with decompression
     */
    static std::string deserialize_compressed(const std::vector<uint8_t>& data);

    /**
     * Save to file
     */
    static bool save_to_file(const std::string& filepath,
                            const std::vector<uint8_t>& data);

    /**
     * Load from file
     */
    static std::vector<uint8_t> load_from_file(const std::string& filepath);

    /**
     * Get schema version
     */
    static uint32_t get_schema_version();

    /**
     * Check compatibility between versions
     */
    static bool is_compatible(uint32_t version1, uint32_t version2);

    /**
     * Migrate data from older version
     */
    static std::vector<uint8_t> migrate_data(const std::vector<uint8_t>& old_data,
                                            uint32_t from_version,
                                            uint32_t to_version);

    /**
     * Get size of serialized data
     */
    static size_t get_serialized_size(const std::vector<uint8_t>& data);

    /**
     * Estimate compression ratio
     */
    static double estimate_compression_ratio(const std::vector<uint8_t>& original,
                                             const std::vector<uint8_t>& compressed);

    /**
     * Validate binary data
     */
    static bool validate_binary(const std::vector<uint8_t>& data);

    /**
     * Merge multiple serialized objects
     */
    static std::vector<uint8_t> merge_objects(
        const std::vector<std::vector<uint8_t>>& objects);

    /**
     * Split serialized object into multiple parts
     */
    static std::vector<std::vector<uint8_t>> split_object(
        const std::vector<uint8_t>& data,
        size_t num_parts);

    /**
     * Calculate checksum for integrity verification
     */
    static uint32_t calculate_checksum(const std::vector<uint8_t>& data);

    /**
     * Verify data integrity
     */
    static bool verify_checksum(const std::vector<uint8_t>& data,
                               uint32_t expected_checksum);

    /**
     * Get memory footprint reduction (vs JSON)
     */
    static double get_memory_reduction_vs_json(const std::vector<uint8_t>& binary,
                                              const std::string& json_data);
};

} // namespace bendcore::serialization
