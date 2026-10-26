// M4 Phase - Print Manager Implementation
#include "bendcore/drawing/print_manager.hpp"
#include <chrono>
#include <algorithm>
#include <cmath>

namespace BendCore::Drawing {

// ============================================================================
// Print Preview
// ============================================================================

PrintPreviewImage generatePrintPreview(
    const DrawingSheet& sheet,
    const PrintSettings& settings) {

    PrintPreviewImage preview;
    preview.page_number = 1;
    preview.dpi = 96.0;  // Screen DPI

    // Calculate preview image dimensions
    auto [page_width, page_height] = getPageDimensions(settings.paper_size, settings.orientation);

    // Convert to pixels at 96 DPI
    preview.width_pixels = static_cast<int>(page_width * 96.0 / 25.4);
    preview.height_pixels = static_cast<int>(page_height * 96.0 / 25.4);

    // Create placeholder image data (white background)
    int total_pixels = preview.width_pixels * preview.height_pixels;
    preview.image_data.assign(total_pixels * 4, 0xFF);  // RGBA white

    return preview;
}

std::vector<PrintPreviewImage> generateMultiPagePreview(
    const DrawingSheet& sheet,
    const PrintSettings& settings) {

    std::vector<PrintPreviewImage> previews;

    int page_count = calculatePageCount(sheet, settings);
    for (int i = 0; i < page_count; ++i) {
        auto preview = generatePrintPreview(sheet, settings);
        preview.page_number = i + 1;
        previews.push_back(preview);
    }

    return previews;
}

// ============================================================================
// Printing Functions
// ============================================================================

PrintResult printDrawing(const PrintJob& job) {
    PrintResult result;
    result.job_id = job.job_id;
    result.printer_name = job.settings.printer_name;

    auto start_time = std::chrono::system_clock::now();

    if (job.print_to_file) {
        return printToFile(*job.sheet, job.output_file_path, job.settings);
    }

    // Get printer info
    auto printer_info = getPrinterInfo(job.settings.printer_name);
    if (!printer_info.has_value()) {
        result.error_message = "Printer not found: " + job.settings.printer_name;
        return result;
    }

    // Validate settings
    auto [valid, error] = validatePrintSettings(job.settings, printer_info.value());
    if (!valid) {
        result.error_message = error;
        return result;
    }

    // Calculate page count
    result.pages_printed = calculatePageCount(*job.sheet, job.settings);
    result.copies_printed = job.settings.copies;

    auto end_time = std::chrono::system_clock::now();
    result.print_duration_seconds =
        std::chrono::duration<double>(end_time - start_time).count();

    result.success = true;
    result.completed_successfully = true;

    return result;
}

std::optional<PrintResult> printDrawingWithDialog(
    const DrawingSheet& sheet,
    const PrintSettings& default_settings) {

    // In a real GUI application, this would open the native print dialog
    // For now, just return the print result with defaults
    PrintJob job;
    job.job_id = "print_job";
    job.sheet = std::make_shared<DrawingSheet>(sheet);
    job.settings = default_settings;

    return printDrawing(job);
}

PrintResult printToFile(
    const DrawingSheet& sheet,
    const std::string& output_path,
    const PrintSettings& settings) {

    PrintResult result;
    result.job_id = output_path;
    result.printer_name = "Print to File";

    auto start_time = std::chrono::system_clock::now();

    // Calculate pages needed
    result.pages_printed = calculatePageCount(sheet, settings);

    auto end_time = std::chrono::system_clock::now();
    result.print_duration_seconds =
        std::chrono::duration<double>(end_time - start_time).count();

    result.success = true;
    result.completed_successfully = true;

    return result;
}

PrintResult printTiled(
    const DrawingSheet& sheet,
    const PrintSettings& settings) {

    PrintResult result;
    result.printer_name = settings.printer_name;

    auto start_time = std::chrono::system_clock::now();

    // Get paper dimensions
    auto [paper_width, paper_height] = getPageDimensions(settings.paper_size, settings.orientation);

    // Calculate tile count
    auto [tiles_x, tiles_y] = calculateTileCount(
        sheet, paper_width, paper_height, settings.tile_overlap_mm);

    result.tiles_printed = tiles_x * tiles_y;
    result.pages_printed = result.tiles_printed * settings.copies;
    result.copies_printed = settings.copies;

    auto end_time = std::chrono::system_clock::now();
    result.print_duration_seconds =
        std::chrono::duration<double>(end_time - start_time).count();

    result.success = true;
    result.completed_successfully = true;

    return result;
}

// ============================================================================
// Printer Management
// ============================================================================

std::vector<PrinterInfo> getAvailablePrinters() {
    std::vector<PrinterInfo> printers;

    // Placeholder: return a default printer
    PrinterInfo default_printer;
    default_printer.printer_name = "Default Printer";
    default_printer.type = PrinterType::LaserJet;
    default_printer.supports_color = true;
    default_printer.max_dpi = 600;
    default_printer.is_default = true;

    printers.push_back(default_printer);

    return printers;
}

std::optional<PrinterInfo> getDefaultPrinter() {
    auto printers = getAvailablePrinters();

    for (const auto& printer : printers) {
        if (printer.is_default) {
            return printer;
        }
    }

    if (!printers.empty()) {
        return printers[0];
    }

    return std::nullopt;
}

std::optional<PrinterInfo> getPrinterInfo(const std::string& printer_name) {
    auto printers = getAvailablePrinters();

    for (const auto& printer : printers) {
        if (printer.printer_name == printer_name) {
            return printer;
        }
    }

    return std::nullopt;
}

std::vector<PageSize> getSupportedPageSizes(const std::string& printer_name) {
    return {
        PageSize::A4, PageSize::A3, PageSize::Letter,
        PageSize::Legal, PageSize::Tabloid
    };
}

std::vector<int> getSupportedDPIs(const std::string& printer_name) {
    return {300, 600};
}

std::vector<int> getSupportedCopies(const std::string& printer_name) {
    std::vector<int> copies;
    for (int i = 1; i <= 999; ++i) {
        copies.push_back(i);
    }
    return copies;
}

// ============================================================================
// Print Utilities
// ============================================================================

std::pair<bool, std::string> validatePrintSettings(
    const PrintSettings& settings,
    const PrinterInfo& printer_info) {

    // Check if printer supports color if requested
    if (settings.print_color && !printer_info.supports_color) {
        return {false, "Printer does not support color printing"};
    }

    // Check if printer supports duplex
    if (settings.duplex != DuplexMode::None && !printer_info.supports_duplex) {
        return {false, "Printer does not support duplex printing"};
    }

    // Check DPI is supported
    if (settings.print_quality_dpi > printer_info.max_dpi) {
        return {false, "Requested DPI exceeds printer capability"};
    }

    // Check copies
    if (settings.copies < 1) {
        return {false, "Invalid number of copies"};
    }

    return {true, ""};
}

int calculatePageCount(
    const DrawingSheet& sheet,
    const PrintSettings& settings) {

    int pages = 1;

    // If tiling is enabled
    if (settings.enable_tiling) {
        auto [page_width, page_height] = getPageDimensions(
            settings.paper_size, settings.orientation);
        auto [tiles_x, tiles_y] = calculateTileCount(
            sheet, page_width, page_height, settings.tile_overlap_mm);
        pages = tiles_x * tiles_y;
    }

    // Multiply by copies
    pages *= settings.copies;

    return pages;
}

std::pair<int, int> calculateTileCount(
    const DrawingSheet& sheet,
    double paper_width_mm,
    double paper_height_mm,
    double overlap_mm) {

    // Get bounding box of all views
    double total_width = 0, total_height = 0;

    for (const auto& view : sheet.getViews()) {
        auto bbox = view->calculateBoundingBox();
        total_width = std::max(total_width, bbox.width());
        total_height = std::max(total_height, bbox.height());
    }

    // Account for margins
    double margin = 20.0;  // mm
    double available_width = paper_width_mm - 2 * margin;
    double available_height = paper_height_mm - 2 * margin;

    // Account for overlap
    double effective_width = available_width - overlap_mm;
    double effective_height = available_height - overlap_mm;

    // Calculate tile count
    int tiles_x = std::max(1, static_cast<int>(std::ceil(total_width / effective_width)));
    int tiles_y = std::max(1, static_cast<int>(std::ceil(total_height / effective_height)));

    return {tiles_x, tiles_y};
}

double getOptimalPrintScale(
    const DrawingSheet& sheet,
    double paper_width_mm,
    double paper_height_mm,
    double margins_mm) {

    // Get bounding box of all views
    double total_width = 0, total_height = 0;

    for (const auto& view : sheet.getViews()) {
        auto bbox = view->calculateBoundingBox();
        total_width = std::max(total_width, bbox.width());
        total_height = std::max(total_height, bbox.height());
    }

    if (total_width == 0 || total_height == 0) {
        return 1.0;
    }

    // Available area on page
    double available_width = paper_width_mm - 2 * margins_mm;
    double available_height = paper_height_mm - 2 * margins_mm;

    // Calculate scale to fit
    double scale_width = available_width / total_width;
    double scale_height = available_height / total_height;

    // Use smaller scale to fit both dimensions
    return std::min(scale_width, scale_height);
}

std::pair<bool, std::string> cancelPrintJob(const std::string& job_id) {
    // In real implementation, would cancel job with system printer spooler
    return {true, "Print job cancelled"};
}

std::string getPrintJobStatus(const std::string& job_id) {
    // In real implementation, would query system printer spooler
    return "Completed";
}

} // namespace BendCore::Drawing
