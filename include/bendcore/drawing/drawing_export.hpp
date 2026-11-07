// M4 Phase - Drawing Export API (GAP-019)
// Export options, validation and filename utilities. No format writer is implemented:
// exportDrawing() returns an error for every format until roadmap phase 4 (drawings).
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <map>

#include "drawing_extraction.hpp"
#include "export_options.hpp"

namespace BendCore::Drawing {

// ============================================================================
// Export Result and Status
// ============================================================================

struct ExportResult {
    bool success = false;
    std::string output_file;
    std::string format_name;
    double file_size_bytes = 0.0;
    std::string error_message;
    std::vector<std::string> warnings;
    bool verification_passed = false;
    std::map<std::string, std::string> metadata;  // Export metadata

    // Export statistics
    int page_count = 1;
    int element_count = 0;
    double export_time_seconds = 0.0;
};

// ============================================================================
// Batch Export Result
// ============================================================================

struct BatchExportResult {
    bool success = false;
    int total_drawings = 0;
    int successful_exports = 0;
    int failed_exports = 0;
    double total_export_time = 0.0;
    std::vector<ExportResult> results;
    std::string error_message;
};

// ============================================================================
// Main Export API
// ============================================================================

/**
 * @brief Export a single drawing sheet to file
 *
 * Validates the options, then fails with an explicit error: no format writer
 * is implemented until roadmap phase 4 (docs/ROADMAP_GENERAL_CAD.md).
 *
 * @param sheet The drawing sheet to export
 * @param options Export configuration options
 * @return ExportResult with success status and details
 */
ExportResult exportDrawing(
    const DrawingSheet& sheet,
    const ExportOptions& options
);

/**
 * @brief Export a single drawing view
 *
 * @param view The drawing view to export
 * @param options Export configuration options
 * @return ExportResult with success status and details
 */
ExportResult exportView(
    const DrawingView& view,
    const ExportOptions& options
);

/**
 * @brief Batch export multiple drawings
 *
 * Efficiently exports multiple drawings in sequence with error handling.
 * Useful for batch processing of designs.
 *
 * @param sheets Vector of drawing sheets to export
 * @param options Export configuration (applied to all sheets)
 * @param output_directory Base directory for output files
 * @return BatchExportResult with aggregate statistics
 */
BatchExportResult batchExportDrawings(
    const std::vector<std::shared_ptr<DrawingSheet>>& sheets,
    const ExportOptions& options,
    const std::string& output_directory
);

// ============================================================================
// Validation and Verification
// ============================================================================

/**
 * @brief Validate export options before export
 *
 * Checks if the export options are valid and consistent.
 *
 * @param options The options to validate
 * @return pair of (is_valid, error_message)
 */
std::pair<bool, std::string> validateExportOptions(const ExportOptions& options);

/**
 * @brief Verify exported file integrity
 *
 * Checks if the exported file is valid and readable.
 *
 * @param file_path Path to the exported file
 * @param format The export format
 * @return pair of (is_valid, error_message)
 */
std::pair<bool, std::string> verifyExportedFile(
    const std::string& file_path,
    ExportFormat format
);

// ============================================================================
// Export Utilities
// ============================================================================

/**
 * @brief Generate automatic filename for export
 *
 * Creates a filename with timestamp, revision number, and format extension.
 *
 * @param sheet The drawing sheet (for title/number)
 * @param format The export format
 * @param include_timestamp Include timestamp in filename
 * @return Generated filename without directory path
 */
std::string generateExportFilename(
    const DrawingSheet& sheet,
    ExportFormat format,
    bool include_timestamp = true
);

/**
 * @brief Get file extension for export format
 *
 * @param format The export format
 * @return File extension with dot (e.g., ".pdf")
 */
std::string getFormatExtension(ExportFormat format);

/**
 * @brief Create backup of existing file before export
 *
 * @param file_path Path to the file to backup
 * @return pair of (success, backup_file_path)
 */
std::pair<bool, std::string> createBackupFile(const std::string& file_path);

/**
 * @brief Check if format is vector-based
 *
 * @param format The export format
 * @return true if vector format (PDF, DWG, DXF, SVG, STEP)
 */
bool isVectorFormat(ExportFormat format);

/**
 * @brief Check if format is raster-based
 *
 * @param format The export format
 * @return true if raster format (PNG, JPEG)
 */
bool isRasterFormat(ExportFormat format);

} // namespace BendCore::Drawing
