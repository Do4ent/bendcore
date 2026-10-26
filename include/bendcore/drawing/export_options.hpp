// M4 Phase - Export Options (GAP-019): Configuration for export operations
// Supports quality settings, scale, units, and element filtering
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <map>

#include "drawing_extraction.hpp"

namespace BendCore::Drawing {

// Note: ExportFormat is already defined in drawing_extraction.hpp
// It includes: DWG, DXF, PDF, SVG, STEP, PNG, JPEG

// ============================================================================
// Quality and Resolution Settings
// ============================================================================

enum class QualityLevel {
    Draft,       // Lower quality, faster
    Standard,    // Balanced quality and speed
    High,        // High quality output
    Maximum      // Maximum quality output
};

struct QualitySettings {
    QualityLevel level = QualityLevel::Standard;
    int dpi = 300;                  // DPI for raster formats
    int jpeg_quality = 85;          // JPEG quality 1-100
    bool compression = true;        // Enable compression
    bool anti_alias = true;         // Anti-aliasing for raster
};

// ============================================================================
// Scale and Unit Settings
// ============================================================================

enum class UnitType {
    Millimeter,   // mm
    Centimeter,   // cm
    Meter,        // m
    Inch,         // in
    Foot          // ft
};

struct ScaleSettings {
    double scale_factor = 1.0;      // 1.0 = 1:1, 2.0 = 1:2, etc.
    UnitType unit = UnitType::Millimeter;
    bool fit_to_page = false;       // Auto-scale to fit page
    bool preserve_aspect = true;    // Keep aspect ratio when scaling
};

// ============================================================================
// Page and Margin Settings
// ============================================================================

enum class PageSize {
    A0, A1, A2, A3, A4, A5, A6,    // ISO A series
    B0, B1, B2, B3, B4, B5, B6,    // ISO B series
    ANSI_A, ANSI_B, ANSI_C,        // ANSI sizes
    ANSI_D, ANSI_E,
    Letter, Legal, Tabloid          // US standard
};

enum class PageOrientation {
    Portrait,
    Landscape
};

struct PageSettings {
    PageSize size = PageSize::A3;
    PageOrientation orientation = PageOrientation::Landscape;
    double margin_left = 10.0;      // mm
    double margin_right = 10.0;     // mm
    double margin_top = 10.0;       // mm
    double margin_bottom = 10.0;    // mm
    bool add_border = true;         // Add frame border
    bool add_title_block = true;    // Include title block
};

// ============================================================================
// Color Settings
// ============================================================================

enum class ColorMode {
    FullColor,          // Full RGB colors
    Grayscale,          // Grayscale output
    Monochrome,         // Pure black and white
    PlotterOptimized    // Optimized for pen plotters
};

struct ColorSettings {
    ColorMode mode = ColorMode::FullColor;
    bool preserve_colors = true;
    bool print_optimized = false;   // Optimize for printing
    int color_depth = 24;           // Bits per pixel
};

// ============================================================================
// Layer and Element Filtering
// ============================================================================

struct ElementFilterSettings {
    bool include_dimensions = true;
    bool include_annotations = true;
    bool include_construction_lines = false;
    bool include_hidden_lines = true;
    bool include_dashed_lines = true;
    bool include_center_marks = true;
    std::vector<std::string> layer_filter;  // Empty = all layers
};

// ============================================================================
// PDF-Specific Settings
// ============================================================================

struct PDFSettings : public QualitySettings, public PageSettings, public ColorSettings {
    bool embed_fonts = true;        // Embed fonts in PDF
    bool add_metadata = true;       // Add document metadata
    bool add_watermark = false;     // Add watermark
    std::string watermark_text;     // Watermark text
    bool allow_printing = true;     // Allow user to print
    bool allow_copying = true;      // Allow copy/paste
    bool password_protected = false;
    std::string user_password;      // For restricting access
    std::string owner_password;     // For document control
    bool compress_images = true;
};

// ============================================================================
// DWG/DXF Settings
// ============================================================================

struct CADSettings : public ScaleSettings {
    bool include_layers = true;
    bool preserve_layer_names = true;
    bool convert_splines = false;   // Convert splines to polylines
    bool include_blocks = false;    // DWG blocks
    bool explode_text = false;      // Convert text to geometry
};

// ============================================================================
// SVG Settings
// ============================================================================

struct SVGSettings : public ScaleSettings, public ColorSettings {
    bool embed_images = true;
    bool use_css_styles = true;
    bool include_metadata = true;
    int decimal_places = 2;         // Precision of coordinates
    bool optimize = true;           // SVG optimization
};

// ============================================================================
// STEP Settings
// ============================================================================

struct STEPSettings : public ScaleSettings {
    bool include_colors = true;
    bool include_names = true;
    std::string schema_version = "AP242";  // STEP schema
    bool validate_output = true;
    bool optimize_geometry = true;
};

// ============================================================================
// Raster Settings
// ============================================================================

struct RasterSettings : public QualitySettings {
    bool trim_whitespace = false;
    int background_color = 0xFFFFFF;  // White background
    bool transparent_background = false;
};

// ============================================================================
// Unified Export Options
// ============================================================================

struct ExportOptions {
    ExportFormat format = ExportFormat::PDF;
    std::string output_path;
    std::string filename;
    bool auto_filename = true;      // Generate filename automatically

    // Format-specific settings
    PDFSettings pdf;
    CADSettings cad;
    SVGSettings svg;
    STEPSettings step;
    RasterSettings raster;
    ElementFilterSettings elements;

    // Common settings
    bool create_backup = true;      // Backup existing file
    bool overwrite = false;         // Overwrite existing file
    bool verify_output = true;      // Verify file after export
};

// ============================================================================
// Helper Functions
// ============================================================================

std::string exportFormatToString(ExportFormat format);
ExportFormat stringToExportFormat(const std::string& str);

std::string pageSizeToString(PageSize size);
std::pair<double, double> getPageDimensions(PageSize size, PageOrientation orientation);

std::string unitTypeToString(UnitType unit);
UnitType stringToUnitType(const std::string& str);
double convertUnits(double value, UnitType from, UnitType to);

} // namespace BendCore::Drawing
