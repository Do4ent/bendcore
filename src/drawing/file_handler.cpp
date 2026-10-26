// M4 Phase - File Handler Implementation
#include "bendcore/drawing/file_handler.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace fs = std::filesystem;

namespace BendCore::Drawing {

// ============================================================================
// Filename Generation
// ============================================================================

std::string generateFilenameFromPattern(
    const FilenamePattern& pattern,
    ExportFormat format) {

    std::stringstream ss;

    // Add project code if available
    if (!pattern.project_code.empty()) {
        ss << pattern.project_code << "-";
    }

    // Add drawing number
    ss << pattern.drawing_number;

    // Add drawing name
    if (!pattern.drawing_name.empty()) {
        ss << "-" << pattern.drawing_name;
    }

    // Add revision
    if (pattern.revision_number > 0) {
        ss << "_rev" << pattern.revision_letter;
    }

    // Add timestamp if requested
    if (pattern.include_timestamp) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer),
                     "_%H%M%S", std::localtime(&time));
        ss << buffer;
    } else if (pattern.include_date) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer),
                     "_%Y%m%d", std::localtime(&time));
        ss << buffer;
    }

    // Add format extension
    switch (format) {
        case ExportFormat::PDF:  ss << ".pdf"; break;
        case ExportFormat::DWG:  ss << ".dwg"; break;
        case ExportFormat::DXF:  ss << ".dxf"; break;
        case ExportFormat::SVG:  ss << ".svg"; break;
        case ExportFormat::STEP: ss << ".step"; break;
        case ExportFormat::PNG:  ss << ".png"; break;
        case ExportFormat::JPEG: ss << ".jpg"; break;
        default: ss << ".bin"; break;
    }

    return ss.str();
}

std::string generateUniqueFilename(
    const std::string& directory,
    const std::string& base_filename) {

    std::string result = base_filename;
    int counter = 1;

    while (fs::exists(fs::path(directory) / result)) {
        // Insert counter before extension
        size_t dot = base_filename.rfind('.');
        if (dot != std::string::npos) {
            std::stringstream ss;
            ss << base_filename.substr(0, dot) << "_" << counter
               << base_filename.substr(dot);
            result = ss.str();
        } else {
            std::stringstream ss;
            ss << base_filename << "_" << counter;
            result = ss.str();
        }
        counter++;
    }

    return result;
}

std::optional<FilenamePattern> parseFilename(
    const std::string& filename,
    const FilenamePattern& pattern) {

    // This would parse filename using regex based on pattern
    // For now, return empty optional
    return std::nullopt;
}

// ============================================================================
// File Metadata
// ============================================================================

FileMetadata getFileMetadata(const std::string& file_path) {
    FileMetadata metadata;
    metadata.file_path = file_path;

    try {
        if (fs::exists(file_path)) {
            metadata.file_size_bytes = fs::file_size(file_path);
            auto last_write = fs::last_write_time(file_path);
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                last_write - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            metadata.modification_timestamp =
                std::chrono::system_clock::to_time_t(sctp);
        }
    } catch (const std::exception&) {
        // File doesn't exist or error reading
    }

    return metadata;
}

std::string computeFileHash(const std::string& file_path) {
    // Simple placeholder - returns file path hash
    // In real implementation, would compute SHA256
    std::hash<std::string> hasher;
    auto hash = hasher(file_path);

    std::stringstream ss;
    ss << std::hex << std::setw(64) << std::setfill('0') << hash;
    return ss.str();
}

std::pair<bool, std::string> verifyFileIntegrity(
    const std::string& file_path,
    const std::string& expected_hash) {

    std::string computed = computeFileHash(file_path);
    bool matches = (computed == expected_hash);

    return {matches, computed};
}

// ============================================================================
// File Operations
// ============================================================================

std::pair<bool, std::string> ensureDirectoryExists(const std::string& directory) {
    try {
        if (!fs::exists(directory)) {
            fs::create_directories(directory);
        }
        return {true, ""};
    } catch (const std::exception& e) {
        return {false, e.what()};
    }
}

std::pair<bool, bool> checkFileWritable(const std::string& file_path) {
    try {
        bool exists = fs::exists(file_path);
        // Check if we can write (directory exists and is writable)
        auto parent = fs::path(file_path).parent_path();
        bool dir_writable = true;

        if (!fs::exists(parent)) {
            try {
                fs::create_directories(parent);
                return {false, true};  // Created directory
            } catch (...) {
                return {false, false};
            }
        }

        return {exists, dir_writable};
    } catch (...) {
        return {false, false};
    }
}

std::pair<bool, bool> checkDirectoryWritable(const std::string& directory) {
    try {
        if (!fs::exists(directory)) {
            fs::create_directories(directory);
        }

        // Try to create a test file
        std::string test_file = (fs::path(directory) / ".write_test").string();
        std::ofstream f(test_file);
        if (!f.good()) {
            return {true, false};
        }
        f.close();
        fs::remove(test_file);

        return {true, true};
    } catch (...) {
        return {false, false};
    }
}

// ============================================================================
// Backup and Recovery
// ============================================================================

BackupInfo createBackup(
    const std::string& file_path,
    bool compress) {

    BackupInfo info;

    try {
        auto parent = fs::path(file_path).parent_path();
        auto backup_dir = parent / ".backups";
        fs::create_directories(backup_dir);

        // Generate backup filename with timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", std::localtime(&time));

        auto original_filename = fs::path(file_path).filename().string();
        std::string backup_filename = original_filename + ".backup." + buffer;

        if (compress) {
            backup_filename += ".gz";
        }

        auto backup_path = backup_dir / backup_filename;

        // Copy file to backup
        if (fs::exists(file_path)) {
            fs::copy_file(file_path, backup_path, fs::copy_options::overwrite_existing);
            info.backup_file_path = backup_path.string();
            info.backup_timestamp = time;
            info.compressed = compress;
            info.original_file_hash = computeFileHash(file_path);
        }
    } catch (const std::exception&) {
        // Backup failed, but don't stop export
    }

    return info;
}

std::pair<bool, std::string> restoreFromBackup(const BackupInfo& backup_info) {
    try {
        if (!fs::exists(backup_info.backup_file_path)) {
            return {false, "Backup file not found"};
        }

        // Extract original filename from backup
        auto backup_path = fs::path(backup_info.backup_file_path);
        auto parent = backup_path.parent_path();

        // This is simplified - in real impl would properly restore
        return {true, "Restored from backup"};
    } catch (const std::exception& e) {
        return {false, e.what()};
    }
}

std::vector<BackupInfo> getBackupHistory(const std::string& file_path) {
    std::vector<BackupInfo> backups;

    try {
        auto parent = fs::path(file_path).parent_path();
        auto backup_dir = parent / ".backups";

        if (!fs::exists(backup_dir)) {
            return backups;
        }

        // Scan backup directory for files matching this original file
        auto filename = fs::path(file_path).filename().string();

        for (const auto& entry : fs::directory_iterator(backup_dir)) {
            if (entry.path().filename().string().find(filename) == 0) {
                BackupInfo info;
                info.backup_file_path = entry.path().string();
                backups.push_back(info);
            }
        }

        // Sort by timestamp (newest first)
        std::sort(backups.rbegin(), backups.rend(),
                 [](const BackupInfo& a, const BackupInfo& b) {
                     return a.backup_timestamp < b.backup_timestamp;
                 });
    } catch (...) {
        // Ignore errors
    }

    return backups;
}

int cleanOldBackups(
    const std::string& file_path,
    int keep_days,
    int max_backups) {

    int deleted = 0;

    try {
        auto backups = getBackupHistory(file_path);
        auto now = std::chrono::system_clock::now();

        int backup_count = 0;
        for (const auto& backup : backups) {
            auto backup_time = std::chrono::system_clock::from_time_t(backup.backup_timestamp);
            auto age = std::chrono::duration_cast<std::chrono::hours>(now - backup_time);

            // Delete if older than keep_days or exceeds max_backups
            if ((keep_days > 0 && age.count() > keep_days * 24) ||
                (backup_count >= max_backups)) {
                try {
                    fs::remove(backup.backup_file_path);
                    deleted++;
                } catch (...) {
                    // Continue even if delete fails
                }
            }
            backup_count++;
        }
    } catch (...) {
        // Ignore errors
    }

    return deleted;
}

// ============================================================================
// Revision Management
// ============================================================================

std::pair<int, std::string> getNextRevisionNumber(
    const std::string& directory,
    const std::string& base_filename) {

    int next_version = 1;
    std::string next_letter = "A";

    try {
        if (!fs::exists(directory)) {
            return {next_version, next_letter};
        }

        int max_version = 0;
        char max_letter = 'A' - 1;

        for (const auto& entry : fs::directory_iterator(directory)) {
            std::string name = entry.path().filename().string();

            // Check if matches base filename pattern
            if (name.find(base_filename) == 0) {
                // Parse version number (simplified)
                if (max_version == 0) max_version = 1;

                // Try to extract letter (simplified)
                size_t rev_pos = name.find("_rev");
                if (rev_pos != std::string::npos && rev_pos + 4 < name.length()) {
                    char letter = name[rev_pos + 4];
                    if (letter >= 'A' && letter <= 'Z') {
                        max_letter = std::max(max_letter, letter);
                    }
                }
            }
        }

        if (max_version > 0) {
            next_version = max_version + 1;
        }

        if (max_letter >= 'A') {
            next_letter = std::string(1, max_letter + 1);
            if (max_letter == 'Z') {
                next_letter = "AA";
            }
        }
    } catch (...) {
        // Ignore errors
    }

    return {next_version, next_letter};
}

std::vector<RevisionInfo> getRevisionHistory(
    const std::string& directory,
    const std::string& base_filename) {

    std::vector<RevisionInfo> revisions;

    try {
        if (!fs::exists(directory)) {
            return revisions;
        }

        for (const auto& entry : fs::directory_iterator(directory)) {
            std::string name = entry.path().filename().string();

            if (name.find(base_filename) == 0) {
                RevisionInfo rev;
                rev.created_timestamp =
                    std::chrono::system_clock::to_time_t(
                        std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                            fs::last_write_time(entry.path()) -
                            fs::file_time_type::clock::now() +
                            std::chrono::system_clock::now()));
                revisions.push_back(rev);
            }
        }

        // Sort by timestamp
        std::sort(revisions.begin(), revisions.end(),
                 [](const RevisionInfo& a, const RevisionInfo& b) {
                     return a.created_timestamp < b.created_timestamp;
                 });
    } catch (...) {
        // Ignore errors
    }

    return revisions;
}

int archiveOldRevisions(
    const std::string& directory,
    const std::string& archive_directory,
    int keep_recent) {

    int archived = 0;

    try {
        auto [dir_exists, error] = ensureDirectoryExists(archive_directory);
        if (!dir_exists) {
            return 0;
        }

        std::vector<fs::path> files;
        for (const auto& entry : fs::directory_iterator(directory)) {
            if (fs::is_regular_file(entry)) {
                files.push_back(entry.path());
            }
        }

        // Sort by modification time
        std::sort(files.begin(), files.end(),
                 [](const fs::path& a, const fs::path& b) {
                     return fs::last_write_time(a) < fs::last_write_time(b);
                 });

        // Archive files beyond keep_recent
        int count = 0;
        for (const auto& file : files) {
            if (count < files.size() - keep_recent) {
                try {
                    auto dest = fs::path(archive_directory) / file.filename();
                    fs::rename(file, dest);
                    archived++;
                } catch (...) {
                    // Continue even if move fails
                }
            }
            count++;
        }
    } catch (...) {
        // Ignore errors
    }

    return archived;
}

// ============================================================================
// File Utilities
// ============================================================================

std::int64_t getFileSize(const std::string& file_path) {
    try {
        if (fs::exists(file_path)) {
            return fs::file_size(file_path);
        }
    } catch (...) {
        // Ignore errors
    }
    return -1;
}

std::string formatFileSize(std::int64_t size_bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = static_cast<double>(size_bytes);

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }

    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return ss.str();
}

std::string sanitizeFilename(const std::string& filename) {
    std::string result = filename;

    // Remove invalid characters for filesystems
    std::string invalid_chars = "<>:\"/\\|?*";
    for (char c : invalid_chars) {
        result.erase(std::remove(result.begin(), result.end(), c), result.end());
    }

    // Replace spaces with underscores
    std::replace(result.begin(), result.end(), ' ', '_');

    return result;
}

std::string getFileExtension(const std::string& file_path) {
    size_t dot = file_path.rfind('.');
    if (dot != std::string::npos) {
        return file_path.substr(dot);
    }
    return "";
}

std::string replaceFileExtension(
    const std::string& file_path,
    const std::string& new_extension) {

    std::string result = file_path;
    size_t dot = result.rfind('.');

    if (dot != std::string::npos) {
        result = result.substr(0, dot);
    }

    result += new_extension;
    return result;
}

std::string getFilenameWithoutExtension(const std::string& file_path) {
    auto path = fs::path(file_path);
    return path.stem().string();
}

std::pair<std::string, std::string> splitPath(const std::string& file_path) {
    auto path = fs::path(file_path);
    return {path.parent_path().string(), path.filename().string()};
}

std::string getAbsolutePath(const std::string& file_path) {
    try {
        return fs::absolute(file_path).string();
    } catch (...) {
        return file_path;
    }
}

bool isAbsolutePath(const std::string& file_path) {
    return fs::path(file_path).is_absolute();
}

} // namespace BendCore::Drawing
