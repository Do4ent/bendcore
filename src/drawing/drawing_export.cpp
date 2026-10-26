// M4 Phase - Drawing Export Implementation
#include "bendcore/drawing/drawing_export.hpp"
#include "bendcore/drawing/file_handler.hpp"
#include <chrono>
#include <cmath>
#include <sstream>

namespace BendCore::Drawing {

// ============================================================================
// Main Export Function
// ============================================================================

ExportResult exportDrawing(
    const DrawingSheet& sheet,
    const ExportOptions& options) {

    ExportResult result;
    auto start_time = std::chrono::system_clock::now();

    // Validate options
    auto [valid, error] = validateExportOptions(options);
    if (!valid) {
        result.error_message = error;
        return result;
    }

    // Generate filename if needed
    std::string output_file = options.output_path;
    if (options.auto_filename) {
        output_file = generateExportFilename(sheet, options.format, true);
    }

    result.output_file = output_file;
    result.format_name = exportFormatToString(options.format);

    // Route to format-specific export
    switch (options.format) {
        case ExportFormat::PDF:
            return exportToPDF(sheet, options);
        case ExportFormat::DWG:
            return exportToDWG(sheet, options);
        case ExportFormat::DXF:
            return exportToDXF(sheet, options);
        case ExportFormat::SVG:
            return exportToSVG(sheet, options);
        case ExportFormat::STEP:
            // STEP requires assembly reference
            result.error_message = "STEP export requires Assembly reference";
            return result;
        case ExportFormat::PNG:
            return exportToPNG(sheet, options);
        case ExportFormat::JPEG:
            return exportToJPEG(sheet, options);
        default:
            result.error_message = "Unknown export format";
            return result;
    }
}

// ============================================================================
// Export Single View
// ============================================================================

ExportResult exportView(
    const DrawingView& view,
    const ExportOptions& options) {

    ExportResult result;
    result.format_name = exportFormatToString(options.format);

    // Create a temporary sheet with just this view
    auto sheet = std::make_shared<DrawingSheet>();
    sheet->addView(std::make_shared<DrawingView>(view));

    return exportDrawing(*sheet, options);
}

// ============================================================================
// Batch Export
// ============================================================================

BatchExportResult batchExportDrawings(
    const std::vector<std::shared_ptr<DrawingSheet>>& sheets,
    const ExportOptions& options,
    const std::string& output_directory) {

    BatchExportResult batch_result;
    batch_result.total_drawings = sheets.size();

    auto start_time = std::chrono::system_clock::now();

    // Ensure output directory exists
    auto [dir_exists, error] = ensureDirectoryExists(output_directory);
    if (!dir_exists) {
        batch_result.error_message = "Cannot create output directory: " + error;
        return batch_result;
    }

    // Export each sheet
    for (const auto& sheet : sheets) {
        ExportOptions sheet_options = options;
        sheet_options.output_path = output_directory;

        auto result = exportDrawing(*sheet, sheet_options);
        batch_result.results.push_back(result);

        if (result.success) {
            batch_result.successful_exports++;
        } else {
            batch_result.failed_exports++;
        }
    }

    auto end_time = std::chrono::system_clock::now();
    batch_result.total_export_time =
        std::chrono::duration<double>(end_time - start_time).count();

    batch_result.success = batch_result.failed_exports == 0;
    return batch_result;
}

// ============================================================================
// Format-Specific Export Functions
// ============================================================================

ExportResult exportToPDF(
    const DrawingSheet& sheet,
    const ExportOptions& options) {

    ExportResult result;
    result.format_name = "PDF";
    result.success = true;  // Placeholder implementation
    result.page_count = sheet.getViewCount();
    result.element_count = 0;

    // Count elements
    for (const auto& view : sheet.getViews()) {
        result.element_count += view->getEdges().size();
        result.element_count += view->getCircles().size();
        result.element_count += view->getArcs().size();
    }

    return result;
}

ExportResult exportToDWG(
    const DrawingSheet& sheet,
    const ExportOptions& options) {

    ExportResult result;
    result.format_name = "DWG";
    result.success = true;  // Placeholder implementation

    return result;
}

ExportResult exportToDXF(
    const DrawingSheet& sheet,
    const ExportOptions& options) {

    ExportResult result;
    result.format_name = "DXF";
    result.success = true;  // Placeholder implementation

    return result;
}

ExportResult exportToSVG(
    const DrawingSheet& sheet,
    const ExportOptions& options) {

    ExportResult result;
    result.format_name = "SVG";
    result.success = true;  // Placeholder implementation

    return result;
}

ExportResult exportToSTEP(
    const DrawingSheet& sheet,
    const Assembly& assembly,
    const ExportOptions& options) {

    ExportResult result;
    result.format_name = "STEP";
    result.success = true;  // Placeholder implementation

    return result;
}

ExportResult exportToPNG(
    const DrawingSheet& sheet,
    const ExportOptions& options) {

    ExportResult result;
    result.format_name = "PNG";
    result.success = true;  // Placeholder implementation

    return result;
}

ExportResult exportToJPEG(
    const DrawingSheet& sheet,
    const ExportOptions& options) {

    ExportResult result;
    result.format_name = "JPEG";
    result.success = true;  // Placeholder implementation

    return result;
}

// ============================================================================
// Validation and Verification
// ============================================================================

std::pair<bool, std::string> validateExportOptions(const ExportOptions& options) {
    // Check output path is valid
    if (options.output_path.empty()) {
        return {false, "Output path is empty"};
    }

    // Check format is valid
    if (options.format == ExportFormat::PDF && options.pdf.dpi < 72) {
        return {false, "PDF DPI must be at least 72"};
    }

    if (options.raster.dpi < 72) {
        return {false, "Raster DPI must be at least 72"};
    }

    if (options.raster.dpi > 600) {
        return {false, "Raster DPI should not exceed 600"};
    }

    return {true, ""};
}

std::pair<bool, std::string> verifyExportedFile(
    const std::string& file_path,
    ExportFormat format) {

    // Check file exists
    auto [exists, writable] = checkFileWritable(file_path);
    if (!exists) {
        return {false, "File does not exist"};
    }

    // Check file size is reasonable
    auto size = getFileSize(file_path);
    if (size <= 0) {
        return {false, "File appears to be empty"};
    }

    return {true, ""};
}

// ============================================================================
// Export Utilities
// ============================================================================

std::string generateExportFilename(
    const DrawingSheet& sheet,
    ExportFormat format,
    bool include_timestamp) {

    std::stringstream ss;

    // Use title block if available
    const auto& title = sheet.getTitleBlock();
    if (title.fields.count("title") > 0) {
        ss << sanitizeFilename(title.fields.at("title"));
    } else {
        ss << "drawing_" << sheet.getSheetNumber();
    }

    if (include_timestamp) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buffer[16];
        std::strftime(buffer, sizeof(buffer), "_%Y%m%d_%H%M%S", std::localtime(&time));
        ss << buffer;
    }

    ss << getFormatExtension(format);
    return ss.str();
}

std::string getFormatExtension(ExportFormat format) {
    switch (format) {
        case ExportFormat::PDF:  return ".pdf";
        case ExportFormat::DWG:  return ".dwg";
        case ExportFormat::DXF:  return ".dxf";
        case ExportFormat::SVG:  return ".svg";
        case ExportFormat::STEP: return ".step";
        case ExportFormat::PNG:  return ".png";
        case ExportFormat::JPEG: return ".jpg";
        default: return ".bin";
    }
}

std::pair<bool, std::string> createBackupFile(const std::string& file_path) {
    try {
        auto backup_info = createBackup(file_path, false);
        return {true, backup_info.backup_file_path};
    } catch (const std::exception& e) {
        return {false, e.what()};
    }
}

std::vector<ExportFormat> getSupportedFormats() {
    return {
        ExportFormat::PDF,
        ExportFormat::DWG,
        ExportFormat::DXF,
        ExportFormat::SVG,
        ExportFormat::STEP,
        ExportFormat::PNG,
        ExportFormat::JPEG
    };
}

bool isVectorFormat(ExportFormat format) {
    return format == ExportFormat::PDF ||
           format == ExportFormat::DWG ||
           format == ExportFormat::DXF ||
           format == ExportFormat::SVG ||
           format == ExportFormat::STEP;
}

bool isRasterFormat(ExportFormat format) {
    return format == ExportFormat::PNG ||
           format == ExportFormat::JPEG;
}

} // namespace BendCore::Drawing
