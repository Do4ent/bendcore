// M4 Phase - Publishing Manager Implementation
#include "bendcore/drawing/publishing_manager.hpp"
#include "bendcore/drawing/file_handler.hpp"
#include <cctype>
#include <chrono>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace BendCore::Drawing {

namespace {

std::string unsupportedDestinationMessage(PublishDestination destination) {
    switch (destination) {
        case PublishDestination::CloudStorage:
            return "Publishing to cloud storage is not supported: bendcore has no cloud upload client";
        case PublishDestination::EmailRecipients:
            return "Publishing via e-mail is not supported: bendcore has no mail client";
        default:
            return "";
    }
}

}  // namespace

// ============================================================================
// Local Publishing
// ============================================================================

PublishingResult publishToLocal(const PublishingJob& job) {
    PublishingResult result;
    result.job_id = job.job_id;
    result.destination = PublishDestination::LocalDirectory;

    auto start_time = std::chrono::system_clock::now();

    // Get local settings
    if (!job.local_settings.has_value()) {
        result.error_message = "Local publishing settings not provided";
        return result;
    }

    const auto& local_settings = job.local_settings.value();

    // Ensure directory exists
    auto [dir_exists, dir_error] = ensureDirectoryExists(local_settings.directory);
    if (!dir_exists) {
        result.error_message = "Cannot create directory: " + dir_error;
        return result;
    }

    // Export each sheet
    for (const auto& sheet : job.sheets) {
        ExportOptions export_opts = job.export_options;
        export_opts.output_path = local_settings.directory;

        auto export_result = exportDrawing(*sheet, export_opts);
        if (export_result.success) {
            result.published_files.push_back(export_result.output_file);
            result.sheets_published++;
            result.total_bytes_published += static_cast<long>(export_result.file_size_bytes);
        } else {
            result.error_message = export_result.error_message;
            return result;
        }
    }

    auto end_time = std::chrono::system_clock::now();
    result.publish_duration_seconds =
        std::chrono::duration<double>(end_time - start_time).count();
    result.success = true;
    result.publish_time = end_time;

    return result;
}

// ============================================================================
// Network Publishing
// ============================================================================

PublishingResult publishToNetwork(const PublishingJob& job) {
    PublishingResult result;
    result.job_id = job.job_id;
    result.destination = PublishDestination::NetworkShare;

    // A network share is published through its mount point in local_settings.directory.
    if (!job.local_settings.has_value()) {
        result.error_message = "Network publishing settings not provided";
        return result;
    }

    PublishingResult local = publishToLocal(job);
    local.destination = PublishDestination::NetworkShare;
    return local;
}

// ============================================================================
// Archival
// ============================================================================

PublishingResult archiveDrawings(const PublishingJob& job) {
    PublishingResult result;
    result.job_id = job.job_id;
    result.destination = PublishDestination::ArchivalRepository;

    auto start_time = std::chrono::system_clock::now();

    if (!job.archival_settings.has_value()) {
        result.error_message = "Archival settings not provided";
        return result;
    }

    const auto& archival_settings = job.archival_settings.value();

    // Ensure archive directory exists
    auto [dir_exists, dir_error] = ensureDirectoryExists(archival_settings.archive_directory);
    if (!dir_exists) {
        result.error_message = "Cannot create archive directory: " + dir_error;
        return result;
    }

    // Export and archive each sheet
    for (const auto& sheet : job.sheets) {
        ExportOptions export_opts = job.export_options;
        export_opts.output_path = archival_settings.archive_directory;

        auto export_result = exportDrawing(*sheet, export_opts);
        if (export_result.success) {
            result.published_files.push_back(export_result.output_file);
            result.sheets_published++;
        }
    }

    auto end_time = std::chrono::system_clock::now();
    result.publish_duration_seconds =
        std::chrono::duration<double>(end_time - start_time).count();
    result.success = result.sheets_published == job.sheets.size();
    result.publish_time = end_time;
    result.archive_id = job.job_id;

    return result;
}

// ============================================================================
// Multi-Destination Publishing
// ============================================================================

std::vector<PublishingResult> publishToMultiple(
    const PublishingJob& job,
    const std::vector<PublishDestination>& destinations) {

    std::vector<PublishingResult> results;

    for (const auto& dest : destinations) {
        PublishingJob dest_job = job;

        PublishingResult result;
        switch (dest) {
            case PublishDestination::LocalDirectory:
                result = publishToLocal(dest_job);
                break;
            case PublishDestination::NetworkShare:
                result = publishToNetwork(dest_job);
                break;
            case PublishDestination::CloudStorage:
            case PublishDestination::EmailRecipients:
                result.job_id = job.job_id;
                result.destination = dest;
                result.error_message = unsupportedDestinationMessage(dest);
                break;
            case PublishDestination::ArchivalRepository:
                result = archiveDrawings(dest_job);
                break;
        }

        results.push_back(result);
    }

    return results;
}

// ============================================================================
// Batch Export Multiple Formats
// ============================================================================

BatchExportResult batchExportMultipleFormats(
    const std::vector<std::shared_ptr<DrawingSheet>>& sheets,
    const std::vector<ExportFormat>& formats,
    const std::string& base_directory) {

    BatchExportResult result;

    // Ensure base directory exists
    auto [dir_exists, error] = ensureDirectoryExists(base_directory);
    if (!dir_exists) {
        result.error_message = "Cannot create directory: " + error;
        return result;
    }

    // Export each sheet in each format
    for (const auto& sheet : sheets) {
        for (const auto& format : formats) {
            ExportOptions opts;
            opts.format = format;
            opts.output_path = base_directory;
            opts.auto_filename = true;

            auto export_result = exportDrawing(*sheet, opts);
            result.results.push_back(export_result);

            if (export_result.success) {
                result.successful_exports++;
            } else {
                result.failed_exports++;
            }
        }
    }

    result.total_drawings = sheets.size() * formats.size();
    result.success = result.failed_exports == 0;

    return result;
}

// ============================================================================
// Change Tracking and Versioning
// ============================================================================

namespace {

// Revision letters follow A..Z, AA..AZ, BA..: bijective base-26.
long revisionToIndex(const std::string& rev) {
    long n = 0;
    for (char c : rev) n = n * 26 + (c - 'A' + 1);
    return n;
}

std::string indexToRevision(long n) {
    std::string rev;
    while (n > 0) {
        --n;
        rev.insert(rev.begin(), static_cast<char>('A' + n % 26));
        n /= 26;
    }
    return rev;
}

}  // namespace

std::vector<DrawingVersion> getDrawingVersionHistory(
    const std::string& archive_directory,
    const std::string& drawing_number) {

    namespace fs = std::filesystem;
    std::vector<DrawingVersion> versions;

    std::error_code ec;
    if (drawing_number.empty() || !fs::is_directory(archive_directory, ec)) {
        return versions;
    }

    // Archived files are named "<drawing_number>_rev<LETTERS>.<ext>"
    const std::string prefix = drawing_number + "_rev";
    for (const auto& entry : fs::directory_iterator(archive_directory, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const std::string stem = entry.path().stem().string();
        if (stem.size() <= prefix.size() || stem.compare(0, prefix.size(), prefix) != 0) {
            continue;
        }
        const std::string rev = stem.substr(prefix.size());
        if (!std::all_of(rev.begin(), rev.end(), [](char c) { return c >= 'A' && c <= 'Z'; })) {
            continue;
        }

        DrawingVersion v;
        v.revision_letter = rev;
        v.file_path = entry.path().string();
        auto mtime = fs::last_write_time(entry.path(), ec);
        if (!ec) {
            // C++20 compatible: convert file_time to system_clock via duration
            auto dur = mtime.time_since_epoch();
            v.timestamp = std::chrono::system_clock::time_point(
                std::chrono::duration_cast<std::chrono::system_clock::duration>(dur)
            );
        }
        versions.push_back(std::move(v));
    }

    // Chronological = revision order. Same revision in several formats
    // (e.g. revA.pdf + revA.dwg) collapses to one entry.
    std::sort(versions.begin(), versions.end(), [](const auto& a, const auto& b) {
        return revisionToIndex(a.revision_letter) < revisionToIndex(b.revision_letter);
    });
    versions.erase(std::unique(versions.begin(), versions.end(), [](const auto& a, const auto& b) {
        return a.revision_letter == b.revision_letter;
    }), versions.end());

    for (size_t i = 0; i < versions.size(); ++i) {
        versions[i].version_number = static_cast<int>(i + 1);
    }
    return versions;
}

std::string generateNextRevision(
    const std::string& archive_directory,
    const std::string& drawing_number) {

    auto versions = getDrawingVersionHistory(archive_directory, drawing_number);
    if (versions.empty()) {
        return "A";
    }
    return indexToRevision(revisionToIndex(versions.back().revision_letter) + 1);
}

// ============================================================================
// Document Numbering
// ============================================================================

std::string generateDrawingNumber(
    const NumberingScheme& scheme,
    const std::string& archive_directory) {

    namespace fs = std::filesystem;

    // Next sequential number: one past the largest "<prefix><separator><digits>" found.
    const std::string stem_prefix = scheme.prefix + scheme.separator;
    long next_number = scheme.sequential_start;
    std::error_code ec;
    if (fs::is_directory(archive_directory, ec)) {
        for (const auto& entry : fs::directory_iterator(archive_directory, ec)) {
            const std::string name = entry.path().filename().string();
            if (name.size() <= stem_prefix.size() || name.compare(0, stem_prefix.size(), stem_prefix) != 0) {
                continue;
            }
            std::size_t end = stem_prefix.size();
            while (end < name.size() && std::isdigit(static_cast<unsigned char>(name[end]))) ++end;
            if (end == stem_prefix.size() || end - stem_prefix.size() > 9) continue;
            const long n = std::stol(name.substr(stem_prefix.size(), end - stem_prefix.size()));
            next_number = std::max(next_number, n + 1);
        }
    }

    std::string number = stem_prefix + std::to_string(next_number);

    if (scheme.include_date) {
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm{};
        localtime_r(&now, &tm);
        auto two = [](int v) { return (v < 10 ? "0" : "") + std::to_string(v); };
        std::string date = scheme.date_format.empty() ? "YYYYMMDD" : scheme.date_format;
        auto replace = [&date](const std::string& token, const std::string& value) {
            for (std::size_t pos = date.find(token); pos != std::string::npos; pos = date.find(token, pos + value.size())) {
                date.replace(pos, token.size(), value);
            }
        };
        replace("YYYY", std::to_string(tm.tm_year + 1900));
        replace("YY", two((tm.tm_year + 1900) % 100));
        replace("MM", two(tm.tm_mon + 1));
        replace("DD", two(tm.tm_mday));
        number += scheme.separator + date;
    }

    return number;
}

// ============================================================================
// Publishing Utilities
// ============================================================================

std::string createDocumentDigest(
    const DrawingSheet& sheet,
    const PublishingMetadata& metadata) {

    std::stringstream ss;
    ss << "Drawing: " << metadata.title << "\n";
    ss << "Number: " << metadata.drawing_number << "\n";
    ss << "Revision: " << metadata.revision_number << "\n";
    ss << "Author: " << metadata.author << "\n";
    ss << "Description: " << metadata.description << "\n";
    ss << "Project: " << metadata.project_name << "\n";
    ss << "Views: " << sheet.getViewCount() << "\n";

    return ss.str();
}

std::pair<bool, std::string> validatePublishingDestination(
    PublishDestination destination,
    const PublishingJob& job) {

    switch (destination) {
        case PublishDestination::LocalDirectory:
            if (!job.local_settings.has_value()) {
                return {false, "Local settings not provided"};
            } {
                auto [exists, writable] = checkDirectoryWritable(job.local_settings.value().directory);
                if (!exists || !writable) {
                    return {false, "Directory not accessible or not writable"};
                }
                return {true, ""};
            }

        case PublishDestination::NetworkShare:
            // publishToNetwork writes to local_settings.directory (a mounted share).
            if (!job.local_settings.has_value()) {
                return {false, "Network settings not provided"};
            } {
                auto [exists, writable] = checkDirectoryWritable(job.local_settings.value().directory);
                if (!exists || !writable) {
                    return {false, "Network directory not accessible or not writable"};
                }
                return {true, ""};
            }

        case PublishDestination::CloudStorage:
        case PublishDestination::EmailRecipients:
            return {false, unsupportedDestinationMessage(destination)};

        case PublishDestination::ArchivalRepository:
            if (!job.archival_settings.has_value()) {
                return {false, "Archival settings not provided"};
            } {
                auto [exists, writable] = checkDirectoryWritable(job.archival_settings.value().archive_directory);
                if (!exists || !writable) {
                    return {false, "Archive directory not accessible or not writable"};
                }
                return {true, ""};
            }

        default:
            return {false, "Unknown destination type"};
    }
}

} // namespace BendCore::Drawing
