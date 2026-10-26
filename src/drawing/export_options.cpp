// M4 Phase - Export Options Implementation
#include "bendcore/drawing/export_options.hpp"
#include <algorithm>

namespace BendCore::Drawing {

// ============================================================================
// Format to String conversions
// ============================================================================

std::string exportFormatToString(ExportFormat format) {
    switch (format) {
        case ExportFormat::PDF:  return "PDF";
        case ExportFormat::DWG:  return "DWG";
        case ExportFormat::DXF:  return "DXF";
        case ExportFormat::SVG:  return "SVG";
        case ExportFormat::STEP: return "STEP";
        case ExportFormat::PNG:  return "PNG";
        case ExportFormat::JPEG: return "JPEG";
        default: return "UNKNOWN";
    }
}

ExportFormat stringToExportFormat(const std::string& str) {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper == "PDF")  return ExportFormat::PDF;
    if (upper == "DWG")  return ExportFormat::DWG;
    if (upper == "DXF")  return ExportFormat::DXF;
    if (upper == "SVG")  return ExportFormat::SVG;
    if (upper == "STEP") return ExportFormat::STEP;
    if (upper == "PNG")  return ExportFormat::PNG;
    if (upper == "JPEG" || upper == "JPG") return ExportFormat::JPEG;

    return ExportFormat::PDF;  // Default
}

// ============================================================================
// Page Size conversions
// ============================================================================

std::string pageSizeToString(PageSize size) {
    switch (size) {
        case PageSize::A0: return "A0";
        case PageSize::A1: return "A1";
        case PageSize::A2: return "A2";
        case PageSize::A3: return "A3";
        case PageSize::A4: return "A4";
        case PageSize::A5: return "A5";
        case PageSize::A6: return "A6";
        case PageSize::B0: return "B0";
        case PageSize::B1: return "B1";
        case PageSize::B2: return "B2";
        case PageSize::B3: return "B3";
        case PageSize::B4: return "B4";
        case PageSize::B5: return "B5";
        case PageSize::B6: return "B6";
        case PageSize::ANSI_A: return "ANSI A";
        case PageSize::ANSI_B: return "ANSI B";
        case PageSize::ANSI_C: return "ANSI C";
        case PageSize::ANSI_D: return "ANSI D";
        case PageSize::ANSI_E: return "ANSI E";
        case PageSize::Letter: return "Letter";
        case PageSize::Legal: return "Legal";
        case PageSize::Tabloid: return "Tabloid";
        default: return "UNKNOWN";
    }
}

std::pair<double, double> getPageDimensions(PageSize size, PageOrientation orientation) {
    // ISO A series (mm)
    double width = 210.0, height = 297.0;  // Default A4

    switch (size) {
        case PageSize::A0:  width = 841.0;  height = 1189.0; break;
        case PageSize::A1:  width = 594.0;  height = 841.0;  break;
        case PageSize::A2:  width = 420.0;  height = 594.0;  break;
        case PageSize::A3:  width = 297.0;  height = 420.0;  break;
        case PageSize::A4:  width = 210.0;  height = 297.0;  break;
        case PageSize::A5:  width = 148.0;  height = 210.0;  break;
        case PageSize::A6:  width = 105.0;  height = 148.0;  break;

        // ISO B series (mm)
        case PageSize::B0:  width = 1000.0; height = 1414.0; break;
        case PageSize::B1:  width = 707.0;  height = 1000.0; break;
        case PageSize::B2:  width = 500.0;  height = 707.0;  break;
        case PageSize::B3:  width = 353.0;  height = 500.0;  break;
        case PageSize::B4:  width = 250.0;  height = 353.0;  break;
        case PageSize::B5:  width = 176.0;  height = 250.0;  break;
        case PageSize::B6:  width = 125.0;  height = 176.0;  break;

        // ANSI sizes (inches converted to mm)
        case PageSize::ANSI_A: width = 215.9; height = 279.4; break;  // Letter
        case PageSize::ANSI_B: width = 279.4; height = 431.8; break;  // Ledger
        case PageSize::ANSI_C: width = 431.8; height = 558.8; break;
        case PageSize::ANSI_D: width = 558.8; height = 863.6; break;
        case PageSize::ANSI_E: width = 863.6; height = 1117.6; break;

        // US standard (inches to mm)
        case PageSize::Letter: width = 215.9; height = 279.4; break;
        case PageSize::Legal:  width = 215.9; height = 355.6; break;
        case PageSize::Tabloid: width = 279.4; height = 431.8; break;

        default: break;
    }

    // Apply orientation
    if (orientation == PageOrientation::Landscape) {
        std::swap(width, height);
    }

    return {width, height};
}

// ============================================================================
// Unit Type conversions
// ============================================================================

std::string unitTypeToString(UnitType unit) {
    switch (unit) {
        case UnitType::Millimeter:  return "mm";
        case UnitType::Centimeter:  return "cm";
        case UnitType::Meter:       return "m";
        case UnitType::Inch:        return "in";
        case UnitType::Foot:        return "ft";
        default: return "unknown";
    }
}

UnitType stringToUnitType(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "mm" || lower == "millimeter" || lower == "millimetres") return UnitType::Millimeter;
    if (lower == "cm" || lower == "centimeter" || lower == "centimetres") return UnitType::Centimeter;
    if (lower == "m" || lower == "meter" || lower == "metre") return UnitType::Meter;
    if (lower == "in" || lower == "inch" || lower == "inches") return UnitType::Inch;
    if (lower == "ft" || lower == "foot" || lower == "feet") return UnitType::Foot;

    return UnitType::Millimeter;  // Default
}

double convertUnits(double value, UnitType from, UnitType to) {
    if (from == to) return value;

    // Convert to millimeters as base unit
    double mm_value = 0.0;
    switch (from) {
        case UnitType::Millimeter:  mm_value = value; break;
        case UnitType::Centimeter:  mm_value = value * 10.0; break;
        case UnitType::Meter:       mm_value = value * 1000.0; break;
        case UnitType::Inch:        mm_value = value * 25.4; break;
        case UnitType::Foot:        mm_value = value * 304.8; break;
    }

    // Convert from millimeters to target unit
    switch (to) {
        case UnitType::Millimeter:  return mm_value;
        case UnitType::Centimeter:  return mm_value / 10.0;
        case UnitType::Meter:       return mm_value / 1000.0;
        case UnitType::Inch:        return mm_value / 25.4;
        case UnitType::Foot:        return mm_value / 304.8;
    }

    return value;
}

} // namespace BendCore::Drawing
