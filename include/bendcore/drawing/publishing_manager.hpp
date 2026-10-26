// M4 Phase - Publishing Manager (GAP-019)
// Batch export, cloud publishing, email, archival, and versioning
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <chrono>

#include "drawing_export.hpp"
#include "drawing_extraction.hpp"

namespace BendCore::Drawing {

// ============================================================================
// Publishing Destination
// ============================================================================

enum class PublishDestination {
    LocalDirectory,         // Local file system
    NetworkShare,           // Network shared folder
    CloudStorage,           // Cloud storage (S3, etc.)
    EmailRecipients,        // Email distribution
    ArchivalRepository      // Archival/versioning system
};

enum class CloudProvider {
    AmazonS3,
    GoogleCloud,
    AzureBlob,
    MinIO,
    CustomHTTP
};

// ============================================================================
// Publishing Metadata
// ============================================================================

struct PublishingMetadata {
    std::string title;
    std::string drawing_number;
    std::string revision_number;
    std::string author;
    std::string description;
    std::string project_name;
    std::chrono::system_clock::time_point creation_date;
    std::chrono::system_clock::time_point publication_date;
    std::vector<std::string> keywords;
    std::string document_status;  // Draft, Released, Archived, etc.
    bool confidential = false;
};

// ============================================================================
// Destination Configuration
// ============================================================================

struct LocalPublishSettings {
    std::string directory;
    bool create_subdirs = true;      // Create subdirs by project/drawing
    std::string subfolder_pattern;   // e.g., "{project}/{drawing_number}"
    bool use_revision_folders = true;
};

struct NetworkPublishSettings : public LocalPublishSettings {
    std::string network_path;
    std::string username;
    std::string password;
    bool preserve_permissions = true;
};

struct CloudPublishSettings {
    CloudProvider provider;
    std::string bucket_name;
    std::string api_key;
    std::string api_secret;
    std::string endpoint_url;       // For custom HTTP
    std::string folder_prefix;      // Prefix in cloud storage
    bool use_versioning = true;     // Enable cloud versioning
    int retention_days = 0;         // 0 = infinite
};

struct EmailPublishSettings {
    std::vector<std::string> recipients;
    std::string subject_template;   // e.g., "{title} - Rev {revision}"
    std::string message_body;
    std::vector<std::string> cc;
    std::vector<std::string> bcc;
    bool include_pdf = true;
    bool include_dwg = false;
    bool include_thumbnail = true;
    int max_attachment_size_mb = 25;
};

struct ArchivalSettings {
    std::string archive_directory;
    bool compress_files = true;     // ZIP archive
    bool index_for_search = true;
    bool maintain_history = true;   // Keep all versions
    int purge_older_than_days = 0;  // 0 = never purge
    bool digitally_sign = false;    // Sign archived files
};

// ============================================================================
// Publishing Job
// ============================================================================

struct PublishingJob {
    std::string job_id;
    PublishDestination destination;
    std::vector<std::shared_ptr<DrawingSheet>> sheets;
    ExportOptions export_options;
    PublishingMetadata metadata;

    // Destination-specific settings
    std::optional<LocalPublishSettings> local_settings;
    std::optional<CloudPublishSettings> cloud_settings;
    std::optional<EmailPublishSettings> email_settings;
    std::optional<ArchivalSettings> archival_settings;

    // Job control
    bool high_priority = false;
    int max_retries = 3;
    bool notify_on_completion = false;
    std::string notification_email;
};

// ============================================================================
// Publishing Result
// ============================================================================

struct PublishingResult {
    std::string job_id;
    bool success = false;
    PublishDestination destination;
    std::vector<std::string> published_files;
    std::vector<std::string> published_urls;  // For cloud storage
    std::chrono::system_clock::time_point publish_time;
    double publish_duration_seconds = 0.0;
    std::string error_message;
    std::vector<std::string> warnings;

    // Publication details
    int sheets_published = 0;
    long total_bytes_published = 0;
    std::string archive_id;         // For archival
    bool email_sent = false;        // For email distribution
};

// ============================================================================
// Batch Publishing Manager
// ============================================================================

/**
 * @brief Publish drawings to local directory
 *
 * @param job The publishing job configuration
 * @return PublishingResult with details
 */
PublishingResult publishToLocal(const PublishingJob& job);

/**
 * @brief Publish drawings to network share
 *
 * @param job The publishing job configuration
 * @return PublishingResult with details
 */
PublishingResult publishToNetwork(const PublishingJob& job);

/**
 * @brief Publish drawings to cloud storage
 *
 * Uploads drawings to cloud provider with metadata and versioning.
 *
 * @param job The publishing job configuration
 * @return PublishingResult with cloud URLs
 */
PublishingResult publishToCloud(const PublishingJob& job);

/**
 * @brief Publish drawings via email
 *
 * Sends drawing exports to email recipients with optional compression.
 *
 * @param job The publishing job configuration
 * @return PublishingResult with email delivery status
 */
PublishingResult publishViaEmail(const PublishingJob& job);

/**
 * @brief Archive drawings with versioning
 *
 * Stores drawings in archival repository with version tracking and
 * change history.
 *
 * @param job The publishing job configuration
 * @return PublishingResult with archive ID and location
 */
PublishingResult archiveDrawings(const PublishingJob& job);

/**
 * @brief Publish to multiple destinations simultaneously
 *
 * @param job The publishing job configuration
 * @param destinations Vector of destinations to publish to
 * @return Vector of PublishingResult for each destination
 */
std::vector<PublishingResult> publishToMultiple(
    const PublishingJob& job,
    const std::vector<PublishDestination>& destinations
);

// ============================================================================
// Batch Export Manager
// ============================================================================

/**
 * @brief Export multiple drawings with different formats
 *
 * Batch export drawings to multiple formats in a single operation.
 *
 * @param sheets Vector of drawing sheets to export
 * @param formats Vector of export formats
 * @param base_directory Output directory
 * @return BatchExportResult with aggregate statistics
 */
BatchExportResult batchExportMultipleFormats(
    const std::vector<std::shared_ptr<DrawingSheet>>& sheets,
    const std::vector<ExportFormat>& formats,
    const std::string& base_directory
);

// ============================================================================
// Change Tracking and Versioning
// ============================================================================

struct DrawingVersion {
    int version_number = 1;
    std::string revision_letter = "A";  // A, B, C, etc.
    std::chrono::system_clock::time_point timestamp;
    std::string changed_by;
    std::string change_description;
    std::string file_path;
    std::string file_hash;  // For integrity verification
};

/**
 * @brief Get version history for a drawing
 *
 * @param archive_directory The archival directory
 * @param drawing_number The drawing number to query
 * @return Vector of DrawingVersion in chronological order
 */
std::vector<DrawingVersion> getDrawingVersionHistory(
    const std::string& archive_directory,
    const std::string& drawing_number
);

/**
 * @brief Generate revision number for new publication
 *
 * Automatically increments revision number based on history.
 *
 * @param archive_directory The archival directory
 * @param drawing_number The drawing number
 * @return Next revision identifier (e.g., "B", "v1.2")
 */
std::string generateNextRevision(
    const std::string& archive_directory,
    const std::string& drawing_number
);

// ============================================================================
// Document Numbering
// ============================================================================

struct NumberingScheme {
    std::string prefix;             // e.g., "DRW"
    std::string separator;          // e.g., "-"
    int sequential_start = 1000;
    bool include_date = false;      // Add date to number
    std::string date_format;        // e.g., "YYYYMM"
};

/**
 * @brief Generate next drawing number
 *
 * @param scheme The numbering scheme configuration
 * @param archive_directory Directory containing existing drawings
 * @return Generated drawing number
 */
std::string generateDrawingNumber(
    const NumberingScheme& scheme,
    const std::string& archive_directory
);

// ============================================================================
// Publishing Utilities
// ============================================================================

/**
 * @brief Embed metadata in exported file
 *
 * Adds metadata to the exported drawing (PDF, DWG properties, etc.)
 *
 * @param file_path Path to exported file
 * @param metadata The metadata to embed
 * @param format The export format
 * @return pair of (success, error_message)
 */
std::pair<bool, std::string> embedMetadata(
    const std::string& file_path,
    const PublishingMetadata& metadata,
    ExportFormat format
);

/**
 * @brief Create document digest/summary
 *
 * Generates a summary of the drawing contents.
 *
 * @param sheet The drawing sheet
 * @param metadata The metadata to include
 * @return Summary string in human-readable format
 */
std::string createDocumentDigest(
    const DrawingSheet& sheet,
    const PublishingMetadata& metadata
);

/**
 * @brief Validate publishing destination
 *
 * Checks if destination is accessible and writable.
 *
 * @param destination Type of destination
 * @param settings Destination-specific settings
 * @return pair of (is_valid, error_message)
 */
std::pair<bool, std::string> validatePublishingDestination(
    PublishDestination destination,
    const PublishingJob& job
);

} // namespace BendCore::Drawing
