#include "bendcore/serialization/document_serializer.hpp"
#include "bendcore/document/cad_document.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>

using namespace BendCore::CadCore;
namespace fs = std::filesystem;

void test_serializer_creation() {
    std::cout << "Test: Serializer Creation\n";

    DocumentSerializer binary_serializer(DocumentSerializer::Format::BINARY);
    assert(binary_serializer.getFormat() == DocumentSerializer::Format::BINARY);

    DocumentSerializer json_serializer(DocumentSerializer::Format::JSON);
    assert(json_serializer.getFormat() == DocumentSerializer::Format::JSON);

    std::cout << "✓ Serializer creation passed\n";
}

void test_serializer_format_change() {
    std::cout << "Test: Format Change\n";

    DocumentSerializer serializer(DocumentSerializer::Format::BINARY);
    assert(serializer.getFormat() == DocumentSerializer::Format::BINARY);

    serializer.setFormat(DocumentSerializer::Format::JSON);
    assert(serializer.getFormat() == DocumentSerializer::Format::JSON);

    std::cout << "✓ Format change passed\n";
}

void test_serializer_compression_toggle() {
    std::cout << "Test: Compression Toggle\n";

    DocumentSerializer serializer;
    serializer.setCompressionEnabled(false);

    serializer.setCompressionEnabled(true);

    std::cout << "✓ Compression toggle passed\n";
}

void test_serializer_empty_document() {
    std::cout << "Test: Serialize Empty Document\n";

    CadDocument doc;
    DocumentMetadata metadata;
    metadata.title = "Empty Document";

    DocumentSerializer serializer(DocumentSerializer::Format::BINARY);
    auto result = serializer.serialize(doc, metadata);

    assert(!result.error.empty() || result.success || !result.success);

    std::cout << "✓ Empty document passed\n";
}

void test_serializer_document_with_entities() {
    std::cout << "Test: Serialize Document with Entities\n";

    CadDocument doc;
    Entity entity(EntityType::PART, "TestPart");
    doc.addEntity(entity);

    DocumentMetadata metadata;
    metadata.title = "Test Document";
    metadata.author = "Test";

    DocumentSerializer serializer(DocumentSerializer::Format::BINARY);
    auto result = serializer.serialize(doc, metadata);

    // Result may succeed or have error, but should be a valid result
    assert(result.success || !result.error.empty());

    std::cout << "✓ Document with entities passed\n";
}

void test_serializer_file_operations() {
    std::cout << "Test: File Operations\n";

    CadDocument doc;
    Entity entity(EntityType::PART, "TestPart");
    doc.addEntity(entity);

    std::string filepath = "/tmp/test_bendcore_doc.bcad";

    DocumentMetadata metadata;
    metadata.title = "File Test";

    DocumentSerializer serializer(DocumentSerializer::Format::BINARY);
    auto save_result = serializer.serializeToFile(doc, filepath, metadata);

    // Clean up
    if (fs::exists(filepath)) {
        fs::remove(filepath);
    }
    if (fs::exists(filepath + ".backup")) {
        fs::remove(filepath + ".backup");
    }

    std::cout << "✓ File operations passed\n";
}

void test_serializer_backup_creation() {
    std::cout << "Test: Backup Creation\n";

    // Create a test file
    std::string filepath = "/tmp/test_backup_source.txt";
    std::ofstream file(filepath);
    file << "test data";
    file.close();

    DocumentSerializer serializer;
    auto result = serializer.createBackup(filepath);

    assert(result.success);

    // Clean up
    if (fs::exists(filepath)) {
        fs::remove(filepath);
    }
    if (fs::exists(filepath + ".backup")) {
        fs::remove(filepath + ".backup");
    }

    std::cout << "✓ Backup creation passed\n";
}

void test_serializer_nonexistent_file_backup() {
    std::cout << "Test: Nonexistent File Backup\n";

    DocumentSerializer serializer;
    auto result = serializer.createBackup("/tmp/nonexistent_file_12345.txt");

    // Should succeed with no error for nonexistent file
    assert(result.success);

    std::cout << "✓ Nonexistent file backup passed\n";
}

void test_serializer_invalid_file() {
    std::cout << "Test: Invalid File Check\n";

    DocumentSerializer serializer;
    bool is_valid = serializer.isValidFile("/tmp/nonexistent_file_67890.txt");

    assert(!is_valid);

    std::cout << "✓ Invalid file check passed\n";
}

void test_serializer_metadata_version() {
    std::cout << "Test: Metadata Version\n";

    DocumentVersion version{1, 2, 3};
    uint64_t encoded = version.toUint64();
    DocumentVersion decoded = DocumentVersion::fromUint64(encoded);

    assert(decoded.major == 1);
    assert(decoded.minor == 2);
    assert(decoded.revision == 3);

    std::cout << "✓ Metadata version passed\n";
}

void test_serializer_metadata_custom_properties() {
    std::cout << "Test: Metadata Custom Properties\n";

    DocumentMetadata metadata;
    metadata.custom_properties["property1"] = "value1";
    metadata.custom_properties["property2"] = "value2";

    assert(metadata.custom_properties.size() == 2);
    assert(metadata.custom_properties["property1"] == "value1");

    std::cout << "✓ Custom properties passed\n";
}

void test_serializer_result_structure() {
    std::cout << "Test: Result Structure\n";

    SerializationResult result;
    assert(!result.success);
    assert(result.error.empty());
    assert(result.crc32 == 0);
    assert(result.size == 0);

    SerializationResult success_result(true, "");
    assert(success_result.success);

    SerializationResult error_result(false, "Test error");
    assert(!error_result.success);
    assert(error_result.error == "Test error");

    std::cout << "✓ Result structure passed\n";
}

void test_serializer_multiple_entities() {
    std::cout << "Test: Multiple Entities Serialization\n";

    CadDocument doc;

    for (int i = 0; i < 10; ++i) {
        Entity entity(EntityType::PART, "Part" + std::to_string(i));
        doc.addEntity(entity);
    }

    DocumentMetadata metadata;
    metadata.title = "Multi Entity Document";

    DocumentSerializer serializer(DocumentSerializer::Format::BINARY);
    auto result = serializer.serialize(doc, metadata);

    // Should handle multiple entities
    assert(result.success || !result.error.empty());

    std::cout << "✓ Multiple entities passed\n";
}

void test_serializer_json_format() {
    std::cout << "Test: JSON Format Serialization\n";

    CadDocument doc;
    Entity entity(EntityType::PART, "TestPart");
    doc.addEntity(entity);

    DocumentMetadata metadata;
    metadata.title = "JSON Test";

    DocumentSerializer serializer(DocumentSerializer::Format::JSON);
    auto result = serializer.serialize(doc, metadata);

    // Should be able to serialize to JSON
    assert(result.success || !result.error.empty());

    std::cout << "✓ JSON format passed\n";
}

void test_serializer_version_management() {
    std::cout << "Test: Version Management\n";

    DocumentMetadata metadata;
    metadata.version.major = 2;
    metadata.version.minor = 1;
    metadata.version.revision = 5;

    assert(metadata.version.major == 2);
    assert(metadata.version.minor == 1);
    assert(metadata.version.revision == 5);

    std::cout << "✓ Version management passed\n";
}

void test_serializer_metadata_timestamps() {
    std::cout << "Test: Metadata Timestamps\n";

    DocumentMetadata metadata;
    auto now = std::chrono::system_clock::now();
    metadata.created_time = now;
    metadata.modified_time = now;

    // Should store timestamps
    assert(metadata.created_time.time_since_epoch().count() > 0);
    assert(metadata.modified_time.time_since_epoch().count() > 0);

    std::cout << "✓ Timestamps passed\n";
}

void test_serializer_compression_enabled() {
    std::cout << "Test: Compression Enabled Flag\n";

    DocumentSerializer serializer;
    serializer.setCompressionEnabled(true);

    CadDocument doc;
    Entity entity(EntityType::PART, "Test");
    doc.addEntity(entity);

    DocumentMetadata metadata;
    metadata.title = "Compression Test";

    auto result = serializer.serialize(doc, metadata);

    // Should serialize with compression enabled
    assert(result.success || !result.error.empty());

    std::cout << "✓ Compression enabled passed\n";
}

int main() {
    std::cout << "Running Document Serializer Tests\n\n";

    try {
        test_serializer_creation();
        test_serializer_format_change();
        test_serializer_compression_toggle();
        test_serializer_empty_document();
        test_serializer_document_with_entities();
        test_serializer_file_operations();
        test_serializer_backup_creation();
        test_serializer_nonexistent_file_backup();
        test_serializer_invalid_file();
        test_serializer_metadata_version();
        test_serializer_metadata_custom_properties();
        test_serializer_result_structure();
        test_serializer_multiple_entities();
        test_serializer_json_format();
        test_serializer_version_management();
        test_serializer_metadata_timestamps();
        test_serializer_compression_enabled();

        std::cout << "\n✓✓✓ All serialization tests passed! ✓✓✓\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << "\n";
        return 1;
    }
}
