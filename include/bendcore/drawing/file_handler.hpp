// M4 Phase - File Handler (GAP-019)
// File operations, automatic naming, revision management, backup
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

#include "export_options.hpp"

namespace BendCore::Drawing {

// ============================================================================
// File Naming Configuration
// ============================================================================

struct FilenamePattern {
    // Pattern components
    std::string project_code;
    std::string drawing_number;
    std::string drawing_name;
    int revision_number = 0;
    std::string revision_letter = "A";
    bool include_timestamp = true;      // Add timestamp
    bool include_date = false;          // Add date
    std::string date_format = "YYYYMMDD";
    std::string timestamp_format = "HHmmss";

    // Example: "PROJ-DRW001-MainAssembly_revA_20241026.pdf"
};

// ============================================================================
// Filename Generation
// ============================================================================

/**
 * @brief Generate filename from pattern
 *
 * Creates a filename based on configured pattern and components.
 *
 * @param pattern The filename pattern configuration
 * @param format Export format for extension
 * @return Generated filename without directory path
 */
std::string generateFilenameFromPattern(
    const FilenamePattern& pattern,
    ExportFormat format
);

/**
 * @brief Generate unique filename
 *
 * If file exists, adds number suffix to make it unique.
 *
 * @param directory Output directory
 * @param base_filename Base filename to use
 * @return Unique filename that doesn't exist
 */
std::string generateUniqueFilename(
    const std::string& directory,
    const std::string& base_filename
);

/**
 * @brief Parse filename to extract components
 *
 * Extracts metadata from filename using pattern.
 *
 * @param filename The filename to parse
 * @param pattern The pattern to match against
 * @return Parsed components, or empty if pattern doesn't match
 */
std::optional<FilenamePattern> parseFilename(
    const std::string& filename,
    const FilenamePattern& pattern
);

// ============================================================================
// File Metadata
// ============================================================================

struct FileMetadata {
    std::string file_path;
    std::uint64_t file_size_bytes = 0;
    std::string file_hash;  // SHA256 checksum
    long creation_timestamp = 0;
    long modification_timestamp = 0;
    std::string format_type;
    int page_count = 1;
    std::string drawing_number;
    std::string revision;
};

/**
 * @brief Get file metadata
 *
 * Retrieves detailed file information.
 *
 * @param file_path Path to the file
 * @return FileMetadata with file details
 */
FileMetadata getFileMetadata(const std::string& file_path);

/**
 * @brief Compute file hash (SHA256)
 *
 * Calculates checksum for file integrity verification.
 *
 * @param file_path Path to the file
 * @return SHA256 hash as hex string
 */
std::string computeFileHash(const std::string& file_path);

/**
 * @brief Verify file integrity
 *
 * Checks if file matches expected hash.
 *
 * @param file_path Path to the file
 * @param expected_hash Expected SHA256 hash
 * @return pair of (hash_matches, computed_hash)
 */
std::pair<bool, std::string> verifyFileIntegrity(
    const std::string& file_path,
    const std::string& expected_hash
);

// ============================================================================
// File Operations
// ============================================================================

/**
 * @brief Create directory if it doesn't exist
 *
 * Creates directory and any missing parent directories.
 *
 * @param directory Path to create
 * @return pair of (success, error_message)
 */
std::pair<bool, std::string> ensureDirectoryExists(const std::string& directory);

/**
 * @brief Check if file exists and is writable
 *
 * @param file_path Path to check
 * @return pair of (exists, is_writable)
 */
std::pair<bool, bool> checkFileWritable(const std::string& file_path);

/**
 * @brief Check if directory is accessible and writable
 *
 * @param directory Directory path to check
 * @return pair of (exists, is_writable)
 */
std::pair<bool, bool> checkDirectoryWritable(const std::string& directory);

// ============================================================================
// Backup and Recovery
// ============================================================================

struct BackupInfo {
    std::string backup_file_path;
    long backup_timestamp = 0;
    std::string original_file_hash;
    bool compressed = false;
};

/**
 * @brief Create backup of file
 *
 * Creates a timestamped backup copy before export/modification.
 * Backup is stored in .backups subdirectory.
 *
 * @param file_path Path to the file to backup
 * @param compress Compress backup file
 * @return BackupInfo with backup location
 */
BackupInfo createBackup(
    const std::string& file_path,
    bool compress = false
);

/**
 * @brief Restore file from backup
 *
 * Restores file from its backup copy.
 *
 * @param backup_info The backup information
 * @return pair of (success, error_message)
 */
std::pair<bool, std::string> restoreFromBackup(const BackupInfo& backup_info);

/**
 * @brief Get backup history
 *
 * Lists all available backups for a file.
 *
 * @param file_path Path to the file
 * @return Vector of BackupInfo in reverse chronological order
 */
std::vector<BackupInfo> getBackupHistory(const std::string& file_path);

/**
 * @brief Clean old backups
 *
 * Deletes backup files older than specified days.
 *
 * @param file_path Path to the file
 * @param keep_days Keep backups from last N days
 * @param max_backups Keep at most N recent backups
 * @return Number of backups deleted
 */
int cleanOldBackups(
    const std::string& file_path,
    int keep_days = 30,
    int max_backups = 10
);

// ============================================================================
// Revision Management
// ============================================================================

struct RevisionInfo {
    int version_number = 1;
    std::string revision_letter = "A";
    long created_timestamp = 0;
    std::string created_by;
    std::string description;
};

/**
 * @brief Get next revision number
 *
 * Calculates next revision based on existing files.
 *
 * @param directory Directory containing revision files
 * @param base_filename Base filename without revision
 * @return pair of (next_version_number, next_revision_letter)
 */
std::pair<int, std::string> getNextRevisionNumber(
    const std::string& directory,
    const std::string& base_filename
);

/**
 * @brief Get revision history
 *
 * Lists all revisions of a drawing.
 *
 * @param directory Directory containing revisions
 * @param base_filename Base filename to search for
 * @return Vector of RevisionInfo in chronological order
 */
std::vector<RevisionInfo> getRevisionHistory(
    const std::string& directory,
    const std::string& base_filename
);

/**
 * @brief Archive old revisions
 *
 * Moves older revision files to archive folder.
 *
 * @param directory Directory containing files
 * @param archive_directory Archive destination
 * @param keep_recent Keep this many recent revisions
 * @return Number of files archived
 */
int archiveOldRevisions(
    const std::string& directory,
    const std::string& archive_directory,
    int keep_recent = 5
);

// ============================================================================
// File Utilities
// ============================================================================

/**
 * @brief Get file size
 *
 * @param file_path Path to the file
 * @return File size in bytes, or -1 if error
 */
std::int64_t getFileSize(const std::string& file_path);

/**
 * @brief Format file size for display
 *
 * Converts bytes to human-readable format (KB, MB, GB).
 *
 * @param size_bytes Size in bytes
 * @return Formatted size string
 */
std::string formatFileSize(std::int64_t size_bytes);

/**
 * @brief Sanitize filename
 *
 * Removes invalid characters for filesystem.
 *
 * @param filename Filename to sanitize
 * @return Sanitized filename
 */
std::string sanitizeFilename(const std::string& filename);

/**
 * @brief Get file extension
 *
 * @param file_path Path or filename
 * @return File extension including dot (e.g., ".pdf"), or empty if no extension
 */
std::string getFileExtension(const std::string& file_path);

/**
 * @brief Remove or replace file extension
 *
 * @param file_path Path or filename
 * @param new_extension New extension with dot (e.g., ".png")
 * @return Path with replaced extension
 */
std::string replaceFileExtension(
    const std::string& file_path,
    const std::string& new_extension
);

/**
 * @brief Get filename without extension
 *
 * @param file_path Path or filename
 * @return Filename without extension or directory
 */
std::string getFilenameWithoutExtension(const std::string& file_path);

/**
 * @brief Split path into directory and filename
 *
 * @param file_path Full file path
 * @return pair of (directory, filename)
 */
std::pair<std::string, std::string> splitPath(const std::string& file_path);

/**
 * @brief Get absolute path
 *
 * Resolves relative path to absolute.
 *
 * @param file_path File path (absolute or relative)
 * @return Absolute path
 */
std::string getAbsolutePath(const std::string& file_path);

/**
 * @brief Check if path is absolute
 *
 * @param file_path Path to check
 * @return true if path is absolute
 */
bool isAbsolutePath(const std::string& file_path);

} // namespace BendCore::Drawing
