// M4 Phase - Print Manager (GAP-019)
// Print preview, print dialog configuration, multi-page printing
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>

#include "drawing_extraction.hpp"
#include "export_options.hpp"

namespace BendCore::Drawing {

// ============================================================================
// Print Settings
// ============================================================================

enum class PrinterType {
    InkJet,
    LaserJet,
    PlotterPen,
    PlotterInkjet,
    ThirdPartyDriver,
    PDF_Virtual,           // PDF printer
    PostScript_Virtual     // PostScript printer
};

enum class PrintScaling {
    PercentOfPageSize,
    FitToPage,
    FitToHeight,
    FitToWidth,
    Actual
};

enum class DuplexMode {
    None,
    Simplex,
    Duplex_LongEdge,
    Duplex_ShortEdge
};

struct PrinterInfo {
    std::string printer_name;
    PrinterType type = PrinterType::InkJet;
    std::string device_id;
    double max_paper_width_mm = 216;   // A4 width
    double max_paper_height_mm = 279;  // A4 height
    bool supports_color = true;
    bool supports_duplex = false;
    int max_dpi = 600;
    bool is_default = false;
};

struct PrintSettings {
    // Printer selection
    std::string printer_name;
    PrinterType printer_type = PrinterType::LaserJet;

    // Page setup
    PageSize paper_size = PageSize::A4;
    PageOrientation orientation = PageOrientation::Portrait;
    int copies = 1;

    // Scaling and fit
    PrintScaling scaling = PrintScaling::Actual;
    double scaling_percent = 100.0;    // For PercentOfPageSize
    bool fit_to_page = false;

    // Margins and spacing
    double margin_left_mm = 10.0;
    double margin_right_mm = 10.0;
    double margin_top_mm = 10.0;
    double margin_bottom_mm = 10.0;

    // Content options
    bool print_color = true;
    bool print_background = false;
    bool include_title_block = true;
    bool include_dimensions = true;
    bool include_annotations = true;
    bool include_borders = true;

    // Advanced options
    DuplexMode duplex = DuplexMode::None;
    int print_quality_dpi = 300;
    bool high_quality = true;

    // Page ranges
    bool print_all_pages = true;
    int first_page = 1;
    int last_page = -1;  // -1 = last page
    std::vector<int> specific_pages;  // For custom page selection

    // Tiling for large drawings
    bool enable_tiling = false;
    int tile_overlap_mm = 10;
    bool print_tile_labels = true;
};

// ============================================================================
// Print Preview
// ============================================================================

struct PrintPreviewImage {
    std::vector<uint8_t> image_data;  // RGBA image bytes
    int width_pixels = 0;
    int height_pixels = 0;
    double dpi = 96.0;
    int page_number = 1;
};

/**
 * @brief Generate print preview
 *
 * Creates a preview image showing how the drawing will print.
 *
 * @param sheet The drawing sheet to preview
 * @param settings Print settings to apply
 * @return PrintPreviewImage with preview data
 */
PrintPreviewImage generatePrintPreview(
    const DrawingSheet& sheet,
    const PrintSettings& settings
);

/**
 * @brief Generate multi-page print preview
 *
 * For large drawings that span multiple pages.
 *
 * @param sheet The drawing sheet
 * @param settings Print settings
 * @return Vector of PrintPreviewImage for each page
 */
std::vector<PrintPreviewImage> generateMultiPagePreview(
    const DrawingSheet& sheet,
    const PrintSettings& settings
);

// ============================================================================
// Print Job and Result
// ============================================================================

struct PrintJob {
    std::string job_id;
    std::shared_ptr<DrawingSheet> sheet;
    PrintSettings settings;
    bool print_to_file = false;
    std::string output_file_path;  // For print-to-file
};

struct PrintResult {
    bool success = false;
    std::string job_id;
    std::string printer_name;
    int pages_printed = 0;
    int copies_printed = 0;
    double print_duration_seconds = 0.0;
    std::string error_message;
    std::vector<std::string> warnings;
    std::string spooler_job_id;    // Job ID from printer spooler

    // Print statistics
    int tiles_printed = 0;         // For tiled printing
    bool completed_successfully = false;
};

// ============================================================================
// Printing Functions
// ============================================================================

/**
 * @brief Send drawing to printer
 *
 * Sends a DrawingSheet to the specified printer with configuration.
 *
 * @param job The print job configuration
 * @return PrintResult with print status
 */
PrintResult printDrawing(const PrintJob& job);

/**
 * @brief Print with dialog
 *
 * Opens native print dialog for user to configure printer and settings.
 *
 * @param sheet The drawing sheet
 * @param default_settings Default print settings (dialog can modify)
 * @return PrintResult if user clicked OK, or nullptr if cancelled
 */
std::optional<PrintResult> printDrawingWithDialog(
    const DrawingSheet& sheet,
    const PrintSettings& default_settings
);

/**
 * @brief Print to file
 *
 * Prints drawing to a file (PDF, PostScript, etc.)
 *
 * @param sheet The drawing sheet
 * @param output_path Output file path
 * @param settings Print settings
 * @return PrintResult with file location
 */
PrintResult printToFile(
    const DrawingSheet& sheet,
    const std::string& output_path,
    const PrintSettings& settings
);

/**
 * @brief Print tiled (for large drawings)
 *
 * Splits large drawing across multiple pages with overlap.
 * Used for printing on standard paper size.
 *
 * @param sheet The drawing sheet
 * @param settings Print settings (tile parameters)
 * @return PrintResult with tile information
 */
PrintResult printTiled(
    const DrawingSheet& sheet,
    const PrintSettings& settings
);

// ============================================================================
// Printer Management
// ============================================================================

/**
 * @brief Get list of available printers
 *
 * @return Vector of PrinterInfo for available printers
 */
std::vector<PrinterInfo> getAvailablePrinters();

/**
 * @brief Get default printer
 *
 * @return PrinterInfo of default printer, or nullptr if none available
 */
std::optional<PrinterInfo> getDefaultPrinter();

/**
 * @brief Get printer information
 *
 * @param printer_name Name of printer
 * @return PrinterInfo if found, or nullptr
 */
std::optional<PrinterInfo> getPrinterInfo(const std::string& printer_name);

/**
 * @brief Query printer capabilities
 *
 * @param printer_name Name of printer
 * @return Capabilities (supported paper sizes, resolutions, etc.)
 */
std::vector<PageSize> getSupportedPageSizes(const std::string& printer_name);
std::vector<int> getSupportedDPIs(const std::string& printer_name);
std::vector<int> getSupportedCopies(const std::string& printer_name);

// ============================================================================
// Print Utilities
// ============================================================================

/**
 * @brief Validate print settings
 *
 * Checks if print settings are compatible with printer.
 *
 * @param settings The print settings
 * @param printer_info Information about target printer
 * @return pair of (is_valid, error_message)
 */
std::pair<bool, std::string> validatePrintSettings(
    const PrintSettings& settings,
    const PrinterInfo& printer_info
);

/**
 * @brief Calculate page count for print job
 *
 * Determines how many pages will be printed based on settings.
 *
 * @param sheet The drawing sheet
 * @param settings Print settings
 * @return Number of pages that will be printed
 */
int calculatePageCount(
    const DrawingSheet& sheet,
    const PrintSettings& settings
);

/**
 * @brief Calculate tile count for large drawing
 *
 * For tiled printing of large drawings.
 *
 * @param sheet The drawing sheet
 * @param paper_width_mm Paper width in mm
 * @param paper_height_mm Paper height in mm
 * @param overlap_mm Overlap between tiles in mm
 * @return pair of (horizontal_tiles, vertical_tiles)
 */
std::pair<int, int> calculateTileCount(
    const DrawingSheet& sheet,
    double paper_width_mm,
    double paper_height_mm,
    double overlap_mm
);

/**
 * @brief Get optimal print scale
 *
 * Calculates best scale to fit drawing on selected paper.
 *
 * @param sheet The drawing sheet
 * @param paper_width_mm Paper width in mm
 * @param paper_height_mm Paper height in mm
 * @param margins_mm Margins in mm
 * @return Optimal scale factor
 */
double getOptimalPrintScale(
    const DrawingSheet& sheet,
    double paper_width_mm,
    double paper_height_mm,
    double margins_mm
);

/**
 * @brief Cancel print job
 *
 * Cancels an in-progress print job.
 *
 * @param job_id The spooler job ID
 * @return pair of (success, error_message)
 */
std::pair<bool, std::string> cancelPrintJob(const std::string& job_id);

/**
 * @brief Get print job status
 *
 * Retrieves status of a print job.
 *
 * @param job_id The spooler job ID
 * @return Status string (Pending, Printing, Completed, Cancelled, etc.)
 */
std::string getPrintJobStatus(const std::string& job_id);

} // namespace BendCore::Drawing
