// M4 Phase - View Properties & Appearance (GAP-017)
// View-specific properties, styling, and visual attributes
#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <array>
#include <algorithm>
#include "../vec3.hpp"

namespace BendCore::Drawing {

using Vec2 = std::array<double, 2>;
using Vec3 = bendcore::Vec3;

// ============================================================================
// Line Styles and Visual Properties
// ============================================================================

enum class LineStyle {
    Contour,        // Main contour lines (visible edges)
    Hidden,         // Hidden/dashed lines
    Thin,           // Thin construction lines
    Center,         // Center lines
    Dimension,      // Dimension lines
    Cut,            // Cut line (section)
    BreakLine       // Break line
};

enum class HatchingPattern {
    None,
    Solid,          // Solid fill
    CrossHatch,     // Cross hatch (standard)
    DiagonalHatch,  // Diagonal hatch
    DotPattern,     // Dot pattern
    WoodPattern,    // Wood material pattern
    MetalPattern,   // Metal material pattern
    CustomPattern   // Custom defined pattern
};

struct LineProperties {
    LineStyle style = LineStyle::Contour;
    double thickness = 0.7;        // Line thickness in mm
    std::string color = "#000000"; // HEX color code
    double dash_length = 2.0;      // Length of dashes for hidden lines
    double gap_length = 1.0;       // Gap between dashes
};

struct TextProperties {
    std::string font_name = "Arial";
    double font_size = 3.5;        // Font size in mm
    std::string color = "#000000";
    std::string alignment = "center"; // left, center, right
    double line_spacing = 1.2;     // Line spacing multiplier
};

struct HatchProperties {
    HatchingPattern pattern = HatchingPattern::CrossHatch;
    double angle = 45.0;           // Rotation angle in degrees
    double spacing = 2.0;          // Spacing between hatch lines in mm
    std::string foreground_color = "#000000";
    std::string background_color = "#FFFFFF";
    double density = 1.0;          // Hatch density 0.0-1.0
};

// ============================================================================
// View-Specific Layer Management
// ============================================================================

struct LayerVisibility {
    std::string layer_name;
    bool visible = true;
    bool printable = true;
    bool locked = false;
    double opacity = 1.0;
};

class ViewLayerManager {
public:
    ViewLayerManager();

    // Layer visibility control
    void setLayerVisible(const std::string& layer_name, bool visible);
    bool isLayerVisible(const std::string& layer_name) const;

    void setLayerPrintable(const std::string& layer_name, bool printable);
    bool isLayerPrintable(const std::string& layer_name) const;

    void setLayerLocked(const std::string& layer_name, bool locked);
    bool isLayerLocked(const std::string& layer_name) const;

    void setLayerOpacity(const std::string& layer_name, double opacity);
    double getLayerOpacity(const std::string& layer_name) const;

    // Add/remove layers
    void addLayer(const std::string& layer_name);
    void removeLayer(const std::string& layer_name);

    // Get all layer visibilities
    const std::map<std::string, LayerVisibility>& getLayerMap() const;

    // Visibility presets
    void showAllLayers();
    void hideAllLayers();
    void showOnlyLayer(const std::string& layer_name);

    // Clear all layers
    void clear();

private:
    std::map<std::string, LayerVisibility> layers_;
};

// ============================================================================
// View Styling and Appearance
// ============================================================================

enum class BorderStyle {
    None,
    Simple,         // Simple rectangular border
    DoubleFrame,    // Double frame border
    Rounded,        // Rounded corners
    Decorative      // Decorative border
};

struct ViewFrameProperties {
    BorderStyle border_style = BorderStyle::Simple;
    double border_thickness = 1.0;  // mm
    std::string border_color = "#000000";
    double corner_radius = 0.0;     // For rounded borders
    bool show_scale = true;
    bool show_label = true;
    Vec2 label_offset{0, -5};       // Offset from view origin
};

// ============================================================================
// Appearance Manager for a Single View
// ============================================================================

class ViewAppearance {
public:
    ViewAppearance();

    // Line properties
    const LineProperties& getLineProperties(LineStyle style) const;
    void setLineProperties(LineStyle style, const LineProperties& props);

    // Text properties
    const TextProperties& getTextProperties() const { return text_props_; }
    void setTextProperties(const TextProperties& props) { text_props_ = props; }

    // Hatch properties
    const HatchProperties& getHatchProperties() const { return hatch_props_; }
    void setHatchProperties(const HatchProperties& props) { hatch_props_ = props; }

    // Frame and border
    const ViewFrameProperties& getFrameProperties() const { return frame_props_; }
    void setFrameProperties(const ViewFrameProperties& props) { frame_props_ = props; }

    // Layer management
    ViewLayerManager& getLayerManager() { return layer_manager_; }
    const ViewLayerManager& getLayerManager() const { return layer_manager_; }

    // Visibility and rendering
    bool getShowGrid() const { return show_grid_; }
    void setShowGrid(bool show) { show_grid_ = show; }

    bool getShowReferenceGeometry() const { return show_reference_; }
    void setShowReferenceGeometry(bool show) { show_reference_ = show; }

    double getTransparency() const { return transparency_; }
    void setTransparency(double value) { transparency_ = std::clamp(value, 0.0, 1.0); }

    // Background
    std::string getBackgroundColor() const { return background_color_; }
    void setBackgroundColor(const std::string& color) { background_color_ = color; }

private:
    std::map<LineStyle, LineProperties> line_styles_;
    TextProperties text_props_;
    HatchProperties hatch_props_;
    ViewFrameProperties frame_props_;
    ViewLayerManager layer_manager_;

    bool show_grid_ = false;
    bool show_reference_ = true;
    double transparency_ = 0.0;
    std::string background_color_ = "#FFFFFF";
};

} // namespace BendCore::Drawing
