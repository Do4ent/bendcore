// M4 Phase - Drawing Export API (GAP-019)
// Complete export system supporting PDF, DWG, DXF, SVG, STEP with full options
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
 * Exports a DrawingSheet to the specified format with all configured options.
 * Automatically handles format-specific requirements and optimizations.
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
// Format-Specific Export Functions
// ============================================================================

/**
 * @brief Export to PDF format
 *
 * Creates a professional PDF with vector graphics, metadata, watermarks,
 * and security options. Supports multi-page sheets.
 *
 * @param sheet The drawing sheet
 * @param options Export options (pdf field used)
 * @return ExportResult with PDF-specific details
 */
ExportResult exportToPDF(
    const DrawingSheet& sheet,
    const ExportOptions& options
);

/**
 * @brief Export to DWG format (AutoCAD)
 *
 * Exports to AutoCAD DWG format with layer support and geometry preservation.
 *
 * @param sheet The drawing sheet
 * @param options Export options (cad field used)
 * @return ExportResult with DWG-specific details
 */
ExportResult exportToDWG(
    const DrawingSheet& sheet,
    const ExportOptions& options
);

/**
 * @brief Export to DXF format
 *
 * Exports to Drawing Exchange Format for universal CAD compatibility.
 *
 * @param sheet The drawing sheet
 * @param options Export options (cad field used)
 * @return ExportResult with DXF-specific details
 */
ExportResult exportToDXF(
    const DrawingSheet& sheet,
    const ExportOptions& options
);

/**
 * @brief Export to SVG format
 *
 * Creates a scalable vector graphic suitable for web display and editing.
 *
 * @param sheet The drawing sheet
 * @param options Export options (svg field used)
 * @return ExportResult with SVG-specific details
 */
ExportResult exportToSVG(
    const DrawingSheet& sheet,
    const ExportOptions& options
);

/**
 * @brief Export to STEP format
 *
 * Creates a 3D product data exchange file preserving drawing references
 * to the 3D model for bidirectional traceability.
 *
 * @param sheet The drawing sheet
 * @param assembly The original assembly for 3D reference
 * @param options Export options (step field used)
 * @return ExportResult with STEP-specific details
 */
ExportResult exportToSTEP(
    const DrawingSheet& sheet,
    const Assembly& assembly,
    const ExportOptions& options
);

/**
 * @brief Export to PNG format (raster)
 *
 * @param sheet The drawing sheet
 * @param options Export options (raster field used)
 * @return ExportResult with PNG-specific details
 */
ExportResult exportToPNG(
    const DrawingSheet& sheet,
    const ExportOptions& options
);

/**
 * @brief Export to JPEG format (raster)
 *
 * @param sheet The drawing sheet
 * @param options Export options (raster field used)
 * @return ExportResult with JPEG-specific details
 */
ExportResult exportToJPEG(
    const DrawingSheet& sheet,
    const ExportOptions& options
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
 * @brief Get supported export formats
 *
 * @return Vector of supported ExportFormat values
 */
std::vector<ExportFormat> getSupportedFormats();

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
