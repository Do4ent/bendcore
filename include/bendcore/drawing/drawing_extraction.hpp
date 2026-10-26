// M3 Phase - Drawing Extraction (GAP-014): View generation from 3D assemblies
// Supports standard views, isometric views, dimensioning, and multi-format export
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <array>
#include <map>

#include "../vec3.hpp"

namespace BendCore::Drawing {

using Vec2 = std::array<double, 2>;
using Vec3 = bendcore::Vec3;

// ============================================================================
// View types and projection modes
// ============================================================================

enum class ViewType {
    Front,      // Standard front view (looking along -Y axis)
    Top,        // Top view (looking along Z axis, top down)
    Right,      // Right side view (looking along X axis, from right)
    Bottom,     // Bottom view (looking along -Z axis, from bottom)
    Left,       // Left side view (looking along -X axis, from left)
    Back,       // Back view (looking along Y axis, from back)
    IsoTR,      // Isometric - Top-Right (standard)
    IsoTL,      // Isometric - Top-Left
    IsoBR,      // Isometric - Bottom-Right
    IsoBL,      // Isometric - Bottom-Left
    Custom      // Custom viewing angle
};

enum class ProjectionMode {
    Orthographic,   // Standard orthographic projection
    Isometric,      // Isometric projection (equal angles, 120 degrees apart)
    Perspective     // Perspective projection (not yet implemented)
};

// ============================================================================
// 2D Drawing View
// ============================================================================

struct Edge2D {
    Vec2 p0, p1;        // Start and end points in 2D view plane
    bool visible = true; // Whether this edge is visible or hidden
    std::string style = "contour"; // line style (contour, thin, dashed, etc.)
};

struct Circle2D {
    Vec2 center;
    double radius = 0.0;
    bool visible = true;
    std::string style = "contour";
};

struct Arc2D {
    Vec2 center;
    Vec2 p0, p1;        // Start and end points
    double radius = 0.0;
    bool large_arc = false;
    bool sweep = false;
    bool visible = true;
    std::string style = "contour";
};

struct Point2D {
    Vec2 pos;
    std::string label;
    double size = 2.0;  // Point size in mm
};

struct Dimension2D {
    Vec2 p0, p1;        // Reference points
    Vec2 offset;        // Offset for dimension text
    std::string text;   // Dimension value (e.g., "50.0")
    double height = 3.5; // Text height in mm
    bool reference = false;
    std::string style = "dimension";
};

struct TextAnnotation {
    Vec2 position;
    std::string text;
    double height = 3.5;
    std::string anchor = "middle"; // start, middle, end
    std::string style = "annotation";
};

/**
 * @brief DrawingView represents a 2D projection of a 3D assembly
 *
 * Manages all geometric elements, dimensions, and annotations
 * that appear in a single drawing view
 */
class DrawingView {
public:
    DrawingView(const std::string& view_id, ViewType type);

    // View properties
    const std::string& getId() const { return view_id_; }
    ViewType getType() const { return type_; }
    const std::string& getLabel() const { return label_; }
    void setLabel(const std::string& label) { label_ = label; }

    double getScale() const { return scale_; }
    void setScale(double scale) { scale_ = scale; }

    // View origin (position on drawing sheet in mm)
    const Vec2& getOrigin() const { return origin_; }
    void setOrigin(const Vec2& origin) { origin_ = origin; }

    // Geometric elements
    void addEdge(const Edge2D& edge) { edges_.push_back(edge); }
    void addCircle(const Circle2D& circle) { circles_.push_back(circle); }
    void addArc(const Arc2D& arc) { arcs_.push_back(arc); }
    void addPoint(const Point2D& point) { points_.push_back(point); }

    const std::vector<Edge2D>& getEdges() const { return edges_; }
    const std::vector<Circle2D>& getCircles() const { return circles_; }
    const std::vector<Arc2D>& getArcs() const { return arcs_; }
    const std::vector<Point2D>& getPoints() const { return points_; }

    // Dimensions and annotations
    void addDimension(const Dimension2D& dim) { dimensions_.push_back(dim); }
    void addAnnotation(const TextAnnotation& text) { annotations_.push_back(text); }

    const std::vector<Dimension2D>& getDimensions() const { return dimensions_; }
    const std::vector<TextAnnotation>& getAnnotations() const { return annotations_; }

    // Bounding box
    struct BoundingBox {
        Vec2 min_point{0, 0};
        Vec2 max_point{0, 0};

        double width() const { return max_point[0] - min_point[0]; }
        double height() const { return max_point[1] - min_point[1]; }
    };

    BoundingBox calculateBoundingBox() const;

    // Clear all elements
    void clear();

    // View transformation matrix (from 3D to 2D)
    const std::array<std::array<double, 3>, 3>& getViewMatrix() const { return view_matrix_; }
    void setViewMatrix(const std::array<std::array<double, 3>, 3>& matrix) { view_matrix_ = matrix; }

    // Projection normal vector in 3D
    Vec3 getProjectionNormal() const;

private:
    std::string view_id_;
    ViewType type_;
    std::string label_;
    double scale_ = 1.0;
    Vec2 origin_{0, 0};

    std::vector<Edge2D> edges_;
    std::vector<Circle2D> circles_;
    std::vector<Arc2D> arcs_;
    std::vector<Point2D> points_;

    std::vector<Dimension2D> dimensions_;
    std::vector<TextAnnotation> annotations_;

    // 3x3 view transformation matrix (orthonormal)
    std::array<std::array<double, 3>, 3> view_matrix_;
};

// ============================================================================
// Drawing Sheet
// ============================================================================

enum class SheetSize {
    A0, A1, A2, A3, A4, A5,      // ISO sizes
    ANSI_A, ANSI_B, ANSI_C, ANSI_D, ANSI_E // ANSI sizes
};

enum class Orientation {
    Portrait,
    Landscape
};

struct SheetProperties {
    SheetSize size = SheetSize::A3;
    Orientation orientation = Orientation::Landscape;
    double width = 420;   // mm
    double height = 297;  // mm
    double margin_left = 10.0;
    double margin_right = 10.0;
    double margin_top = 10.0;
    double margin_bottom = 10.0;
};

struct TitleBlock {
    std::map<std::string, std::string> fields; // e.g., title, part_number, scale, etc.
    Vec2 position;
    double width = 100;
    double height = 50;
};

/**
 * @brief DrawingSheet represents a complete drawing sheet with multiple views
 */
class DrawingSheet {
public:
    DrawingSheet(int sheet_number = 1, const SheetProperties& props = SheetProperties());

    // Sheet properties
    int getSheetNumber() const { return sheet_number_; }
    int getTotalSheets() const { return total_sheets_; }
    void setTotalSheets(int count) { total_sheets_ = count; }

    const SheetProperties& getProperties() const { return properties_; }
    void setProperties(const SheetProperties& props) { properties_ = props; }

    // Views on sheet
    void addView(std::shared_ptr<DrawingView> view) { views_.push_back(view); }
    const std::vector<std::shared_ptr<DrawingView>>& getViews() const { return views_; }
    std::shared_ptr<DrawingView> getView(size_t index) const;
    size_t getViewCount() const { return views_.size(); }

    // Title block
    void setTitleBlock(const TitleBlock& title) { title_block_ = title; }
    const TitleBlock& getTitleBlock() const { return title_block_; }

    // Add border and frame
    void addBorder();
    const std::vector<Edge2D>& getBorders() const { return borders_; }

private:
    int sheet_number_ = 1;
    int total_sheets_ = 1;
    SheetProperties properties_;
    TitleBlock title_block_;
    std::vector<std::shared_ptr<DrawingView>> views_;
    std::vector<Edge2D> borders_;
};

// ============================================================================
// Forward declarations for assembly types
// ============================================================================

// These would be actual BendCore types - for now using placeholders
// In real integration, these should be from bendcore/assembly/assembly.hpp
class Assembly {
public:
    virtual ~Assembly() = default;
};

// ============================================================================
// Drawing Extraction API
// ============================================================================

/**
 * @brief Extract 2D views from 3D assembly
 *
 * Generates standard orthographic views (Front, Top, Right, etc.)
 * and isometric views from a 3D assembly model.
 *
 * @param assembly The 3D assembly to extract views from
 * @param view_types Vector of view types to generate
 * @return Vector of extracted DrawingView objects
 */
std::vector<std::shared_ptr<DrawingView>> extractViewsFromAssembly(
    const Assembly& assembly,
    const std::vector<ViewType>& view_types
);

/**
 * @brief Generate a single view with custom viewing angle
 *
 * @param assembly The 3D assembly
 * @param view_direction Custom viewing direction (normalized)
 * @param up_direction Up direction for the view plane
 * @return Extracted DrawingView
 */
std::shared_ptr<DrawingView> generateCustomView(
    const Assembly& assembly,
    const Vec3& view_direction,
    const Vec3& up_direction
);

/**
 * @brief Create a complete drawing sheet with multiple views
 *
 * Automatically arranges views on the sheet according to
 * orthographic projection standards (first/third angle).
 *
 * @param assembly The 3D assembly
 * @param views Vector of views to place on sheet
 * @return Pointer to created DrawingSheet
 */
std::shared_ptr<DrawingSheet> createDrawingSheet(
    const Assembly& assembly,
    const std::vector<std::shared_ptr<DrawingView>>& views
);

/**
 * @brief Auto-dimension a drawing view
 *
 * Extracts key dimensions from the assembly and adds them
 * to the view with intelligent placement and formatting.
 *
 * @param view The view to dimension
 * @param precision Decimal places for dimensions
 * @param decimal_separator Character for decimal separator ('.' or ',')
 */
void addDimensionsToView(
    DrawingView& view,
    int precision = 1,
    char decimal_separator = '.'
);

/**
 * @brief Add reference dimensions (for information only)
 *
 * @param view The view to annotate
 * @param references Map of reference names to dimension values
 */
void addReferenceDimensions(
    DrawingView& view,
    const std::map<std::string, double>& references
);

// ============================================================================
// Export Functions
// ============================================================================

enum class ExportFormat {
    DWG,    // AutoCAD format
    DXF,    // Drawing Exchange Format
    PDF,    // Portable Document Format
    SVG,    // Scalable Vector Graphics
    STEP,   // 3D product data exchange
    PNG,    // Raster image format
    JPEG    // Raster image format
};

/**
 * @brief Export drawing sheet to file
 *
 * @param sheet The drawing sheet to export
 * @param file_path Path where to save the file
 * @param format Export format
 * @return true if export was successful
 */
bool exportDrawing(
    const DrawingSheet& sheet,
    const std::string& file_path,
    ExportFormat format
);

/**
 * @brief Export single view to file
 *
 * @param view The view to export
 * @param file_path Path where to save the file
 * @param format Export format
 * @return true if export was successful
 */
bool exportView(
    const DrawingView& view,
    const std::string& file_path,
    ExportFormat format
);

/**
 * @brief Export to DWG format (AutoCAD)
 *
 * @param sheet The drawing sheet
 * @param file_path Output file path
 * @return true if successful
 */
bool exportToDWG(const DrawingSheet& sheet, const std::string& file_path);

/**
 * @brief Export to DXF format (Drawing Exchange Format)
 *
 * @param sheet The drawing sheet
 * @param file_path Output file path
 * @return true if successful
 */
bool exportToDXF(const DrawingSheet& sheet, const std::string& file_path);

/**
 * @brief Export to PDF format
 *
 * @param sheet The drawing sheet
 * @param file_path Output file path
 * @return true if successful
 */
bool exportToPDF(const DrawingSheet& sheet, const std::string& file_path);

/**
 * @brief Export to SVG format (Scalable Vector Graphics)
 *
 * @param sheet The drawing sheet
 * @param file_path Output file path
 * @return true if successful
 */
bool exportToSVG(const DrawingSheet& sheet, const std::string& file_path);

/**
 * @brief Export to PNG format (raster)
 *
 * @param sheet The drawing sheet
 * @param file_path Output file path
 * @param dpi Resolution in dots per inch (default 300)
 * @return true if successful
 */
bool exportToPNG(
    const DrawingSheet& sheet,
    const std::string& file_path,
    int dpi = 300
);

/**
 * @brief Export to JPEG format (raster)
 *
 * @param sheet The drawing sheet
 * @param file_path Output file path
 * @param dpi Resolution in dots per inch (default 300)
 * @param quality JPEG quality 1-100 (default 85)
 * @return true if successful
 */
bool exportToJPEG(
    const DrawingSheet& sheet,
    const std::string& file_path,
    int dpi = 300,
    int quality = 85
);

// ============================================================================
// View Layout and Arrangement
// ============================================================================

/**
 * @brief Auto-arrange views on sheet according to projection standard
 *
 * @param sheet The drawing sheet
 * @param projection_mode First angle or third angle projection
 * @return true if arrangement was successful
 */
bool autoArrangeViews(DrawingSheet& sheet, ProjectionMode projection_mode);

/**
 * @brief Calculate optimal view scale to fit on sheet
 *
 * @param view The view to scale
 * @param available_width Available width in mm
 * @param available_height Available height in mm
 * @return Optimal scale factor
 */
double calculateOptimalScale(
    const DrawingView& view,
    double available_width,
    double available_height
);

/**
 * @brief Apply scale factor to all dimensions and text in view
 *
 * @param view The view to scale
 * @param scale_factor The scale factor to apply
 */
void applyScaleToView(DrawingView& view, double scale_factor);

// ============================================================================
// Utility Functions
// ============================================================================

std::string viewTypeToString(ViewType type);
ViewType stringToViewType(const std::string& str);
std::string sheetSizeToString(SheetSize size);
std::pair<double, double> getSheetDimensions(SheetSize size);

} // namespace BendCore::Drawing
