// M3 Phase - Drawing Extraction (GAP-014): Implementation
#include "bendcore/drawing/drawing_extraction.hpp"

#include <cmath>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <numbers>

namespace BendCore::Drawing {

// ============================================================================
// DrawingView Implementation
// ============================================================================

DrawingView::DrawingView(const std::string& view_id, ViewType type)
    : view_id_(view_id), type_(type) {
    // Initialize view matrix as identity
    view_matrix_ = {{
        {1.0, 0.0, 0.0},
        {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}
    }};
}

DrawingView::BoundingBox DrawingView::calculateBoundingBox() const {
    BoundingBox bbox;
    bool first = true;

    // Include edges
    for (const auto& edge : edges_) {
        if (first) {
            bbox.min_point = edge.p0;
            bbox.max_point = edge.p0;
            first = false;
        }
        bbox.min_point[0] = std::min(bbox.min_point[0], std::min(edge.p0[0], edge.p1[0]));
        bbox.min_point[1] = std::min(bbox.min_point[1], std::min(edge.p0[1], edge.p1[1]));
        bbox.max_point[0] = std::max(bbox.max_point[0], std::max(edge.p0[0], edge.p1[0]));
        bbox.max_point[1] = std::max(bbox.max_point[1], std::max(edge.p0[1], edge.p1[1]));
    }

    // Include circles
    for (const auto& circle : circles_) {
        if (first) {
            bbox.min_point = {circle.center[0] - circle.radius, circle.center[1] - circle.radius};
            bbox.max_point = {circle.center[0] + circle.radius, circle.center[1] + circle.radius};
            first = false;
        } else {
            bbox.min_point[0] = std::min(bbox.min_point[0], circle.center[0] - circle.radius);
            bbox.min_point[1] = std::min(bbox.min_point[1], circle.center[1] - circle.radius);
            bbox.max_point[0] = std::max(bbox.max_point[0], circle.center[0] + circle.radius);
            bbox.max_point[1] = std::max(bbox.max_point[1], circle.center[1] + circle.radius);
        }
    }

    // Include arcs
    for (const auto& arc : arcs_) {
        if (first) {
            bbox.min_point = {arc.center[0] - arc.radius, arc.center[1] - arc.radius};
            bbox.max_point = {arc.center[0] + arc.radius, arc.center[1] + arc.radius};
            first = false;
        } else {
            bbox.min_point[0] = std::min(bbox.min_point[0], arc.center[0] - arc.radius);
            bbox.min_point[1] = std::min(bbox.min_point[1], arc.center[1] - arc.radius);
            bbox.max_point[0] = std::max(bbox.max_point[0], arc.center[0] + arc.radius);
            bbox.max_point[1] = std::max(bbox.max_point[1], arc.center[1] + arc.radius);
        }
    }

    // Include points
    for (const auto& point : points_) {
        if (first) {
            bbox.min_point = point.pos;
            bbox.max_point = point.pos;
            first = false;
        } else {
            bbox.min_point[0] = std::min(bbox.min_point[0], point.pos[0]);
            bbox.min_point[1] = std::min(bbox.min_point[1], point.pos[1]);
            bbox.max_point[0] = std::max(bbox.max_point[0], point.pos[0]);
            bbox.max_point[1] = std::max(bbox.max_point[1], point.pos[1]);
        }
    }

    return bbox;
}

Vec3 DrawingView::getProjectionNormal() const {
    // Return the normal to the projection plane
    // This is the third row of the view matrix (Z direction)
    return Vec3{view_matrix_[2][0], view_matrix_[2][1], view_matrix_[2][2]};
}

void DrawingView::clear() {
    edges_.clear();
    circles_.clear();
    arcs_.clear();
    points_.clear();
    dimensions_.clear();
    annotations_.clear();
}

// ============================================================================
// DrawingSheet Implementation
// ============================================================================

DrawingSheet::DrawingSheet(int sheet_number, const SheetProperties& props)
    : sheet_number_(sheet_number), properties_(props) {

    // Set title block position (bottom right of sheet)
    title_block_.position = {
        properties_.width - 100 - properties_.margin_right,
        properties_.margin_bottom
    };
}

std::shared_ptr<DrawingView> DrawingSheet::getView(size_t index) const {
    if (index < views_.size()) {
        return views_[index];
    }
    return nullptr;
}

void DrawingSheet::addBorder() {
    // Create sheet borders
    double left = properties_.margin_left;
    double right = properties_.width - properties_.margin_right;
    double top = properties_.height - properties_.margin_top;
    double bottom = properties_.margin_bottom;

    // Top border
    borders_.push_back({{left, top}, {right, top}});

    // Right border
    borders_.push_back({{right, top}, {right, bottom}});

    // Bottom border
    borders_.push_back({{right, bottom}, {left, bottom}});

    // Left border
    borders_.push_back({{left, bottom}, {left, top}});
}

// ============================================================================
// Dimensioning Functions
// ============================================================================

void addDimensionsToView(
    DrawingView& view,
    int precision,
    char decimal_separator) {

    // Get the bounding box
    auto bbox = view.calculateBoundingBox();

    if (bbox.width() < 0.1 || bbox.height() < 0.1) {
        return; // View is too small to dimension
    }

    // Add horizontal dimension (width)
    if (bbox.width() > 0.1) {
        Dimension2D dim_w;
        dim_w.p0 = {bbox.min_point[0], bbox.min_point[1]};
        dim_w.p1 = {bbox.max_point[0], bbox.min_point[1]};
        dim_w.offset = {bbox.min_point[0] + bbox.width() / 2, bbox.min_point[1] - 15.0};

        // Format dimension text
        std::ostringstream oss;
        oss.precision(precision);
        oss << std::fixed << bbox.width();
        dim_w.text = oss.str();
        if (decimal_separator == ',') {
            std::string text = dim_w.text;
            std::replace(text.begin(), text.end(), '.', ',');
            dim_w.text = text;
        }

        view.addDimension(dim_w);
    }

    // Add vertical dimension (height)
    if (bbox.height() > 0.1) {
        Dimension2D dim_h;
        dim_h.p0 = {bbox.min_point[0], bbox.min_point[1]};
        dim_h.p1 = {bbox.min_point[0], bbox.max_point[1]};
        dim_h.offset = {bbox.min_point[0] - 15.0, bbox.min_point[1] + bbox.height() / 2};

        // Format dimension text
        std::ostringstream oss;
        oss.precision(precision);
        oss << std::fixed << bbox.height();
        dim_h.text = oss.str();
        if (decimal_separator == ',') {
            std::string text = dim_h.text;
            std::replace(text.begin(), text.end(), '.', ',');
            dim_h.text = text;
        }

        view.addDimension(dim_h);
    }
}

void addReferenceDimensions(
    DrawingView& view,
    const std::map<std::string, double>& references) {

    auto bbox = view.calculateBoundingBox();
    double y_offset = bbox.max_point[1] + 30.0;

    int index = 0;
    for (const auto& [name, value] : references) {
        TextAnnotation annotation;
        annotation.position = {
            bbox.min_point[0] + 10.0,
            y_offset + (index * 10.0)
        };

        std::ostringstream oss;
        oss << name << ": " << value;
        annotation.text = oss.str();
        annotation.style = "reference";

        view.addAnnotation(annotation);
        ++index;
    }
}

// ============================================================================
// Layout and Arrangement Functions
// ============================================================================

bool autoArrangeViews(DrawingSheet& sheet, ProjectionMode projection_mode) {
    const auto& props = sheet.getProperties();

    double available_width = props.width - props.margin_left - props.margin_right - 110;
    double available_height = props.height - props.margin_top - props.margin_bottom - 60;

    size_t view_count = sheet.getViewCount();
    if (view_count == 0) return true;

    // Calculate grid dimensions for standard arrangement
    size_t cols = std::ceil(std::sqrt(view_count));
    size_t rows = (view_count + cols - 1) / cols;

    double cell_width = available_width / cols;
    double cell_height = available_height / rows;

    // Arrange views
    for (size_t i = 0; i < view_count; ++i) {
        auto view = sheet.getView(i);
        if (!view) continue;

        size_t row = i / cols;
        size_t col = i % cols;

        // Calculate position
        double x = props.margin_left + col * cell_width + cell_width / 4;
        double y = props.margin_top + row * cell_height + cell_height / 4;

        view->setOrigin({x, y});

        // Calculate scale to fit in cell
        double scale = calculateOptimalScale(
            *view,
            cell_width * 0.8,
            cell_height * 0.8
        );

        view->setScale(scale);
    }

    return true;
}

double calculateOptimalScale(
    const DrawingView& view,
    double available_width,
    double available_height) {

    auto bbox = view.calculateBoundingBox();

    if (bbox.width() < 0.1 || bbox.height() < 0.1) {
        return 1.0;
    }

    double scale_x = available_width / bbox.width();
    double scale_y = available_height / bbox.height();

    return std::min(scale_x, scale_y);
}

void applyScaleToView(DrawingView& view, double scale_factor) {
    // Store all current elements before clearing
    auto edges = view.getEdges();
    auto circles = view.getCircles();
    auto arcs = view.getArcs();
    auto points = view.getPoints();
    auto dims = view.getDimensions();
    auto annots = view.getAnnotations();

    // Clear the view
    view.clear();

    // Rebuild with scaled elements
    for (const auto& edge : edges) {
        Edge2D scaled;
        scaled.p0 = {edge.p0[0] * scale_factor, edge.p0[1] * scale_factor};
        scaled.p1 = {edge.p1[0] * scale_factor, edge.p1[1] * scale_factor};
        scaled.visible = edge.visible;
        scaled.style = edge.style;
        view.addEdge(scaled);
    }

    for (const auto& circle : circles) {
        Circle2D scaled;
        scaled.center = {circle.center[0] * scale_factor, circle.center[1] * scale_factor};
        scaled.radius = circle.radius * scale_factor;
        scaled.visible = circle.visible;
        scaled.style = circle.style;
        view.addCircle(scaled);
    }

    for (const auto& arc : arcs) {
        Arc2D scaled;
        scaled.center = {arc.center[0] * scale_factor, arc.center[1] * scale_factor};
        scaled.p0 = {arc.p0[0] * scale_factor, arc.p0[1] * scale_factor};
        scaled.p1 = {arc.p1[0] * scale_factor, arc.p1[1] * scale_factor};
        scaled.radius = arc.radius * scale_factor;
        scaled.large_arc = arc.large_arc;
        scaled.sweep = arc.sweep;
        scaled.visible = arc.visible;
        scaled.style = arc.style;
        view.addArc(scaled);
    }

    for (const auto& point : points) {
        Point2D scaled;
        scaled.pos = {point.pos[0] * scale_factor, point.pos[1] * scale_factor};
        scaled.label = point.label;
        scaled.size = point.size * scale_factor;
        view.addPoint(scaled);
    }

    for (const auto& dim : dims) {
        Dimension2D scaled;
        scaled.p0 = {dim.p0[0] * scale_factor, dim.p0[1] * scale_factor};
        scaled.p1 = {dim.p1[0] * scale_factor, dim.p1[1] * scale_factor};
        scaled.offset = {dim.offset[0] * scale_factor, dim.offset[1] * scale_factor};
        scaled.text = dim.text;
        scaled.height = dim.height * scale_factor;
        scaled.reference = dim.reference;
        scaled.style = dim.style;
        view.addDimension(scaled);
    }

    for (const auto& annot : annots) {
        TextAnnotation scaled;
        scaled.position = {annot.position[0] * scale_factor, annot.position[1] * scale_factor};
        scaled.text = annot.text;
        scaled.height = annot.height * scale_factor;
        scaled.anchor = annot.anchor;
        scaled.style = annot.style;
        view.addAnnotation(scaled);
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string viewTypeToString(ViewType type) {
    switch (type) {
        case ViewType::Front:  return "Front";
        case ViewType::Top:    return "Top";
        case ViewType::Right:  return "Right";
        case ViewType::Bottom: return "Bottom";
        case ViewType::Left:   return "Left";
        case ViewType::Back:   return "Back";
        case ViewType::IsoTR:  return "Isometric TR";
        case ViewType::IsoTL:  return "Isometric TL";
        case ViewType::IsoBR:  return "Isometric BR";
        case ViewType::IsoBL:  return "Isometric BL";
        case ViewType::Custom: return "Custom";
        default:               return "Unknown";
    }
}

ViewType stringToViewType(const std::string& str) {
    if (str == "Front")       return ViewType::Front;
    if (str == "Top")         return ViewType::Top;
    if (str == "Right")       return ViewType::Right;
    if (str == "Bottom")      return ViewType::Bottom;
    if (str == "Left")        return ViewType::Left;
    if (str == "Back")        return ViewType::Back;
    if (str == "Isometric TR") return ViewType::IsoTR;
    if (str == "Isometric TL") return ViewType::IsoTL;
    if (str == "Isometric BR") return ViewType::IsoBR;
    if (str == "Isometric BL") return ViewType::IsoBL;
    if (str == "Custom")      return ViewType::Custom;
    throw std::invalid_argument("Unknown view type: " + str);
}

std::string sheetSizeToString(SheetSize size) {
    switch (size) {
        case SheetSize::A0: return "A0";
        case SheetSize::A1: return "A1";
        case SheetSize::A2: return "A2";
        case SheetSize::A3: return "A3";
        case SheetSize::A4: return "A4";
        case SheetSize::A5: return "A5";
        case SheetSize::ANSI_A: return "ANSI A";
        case SheetSize::ANSI_B: return "ANSI B";
        case SheetSize::ANSI_C: return "ANSI C";
        case SheetSize::ANSI_D: return "ANSI D";
        case SheetSize::ANSI_E: return "ANSI E";
        default: return "Unknown";
    }
}

std::pair<double, double> getSheetDimensions(SheetSize size) {
    switch (size) {
        case SheetSize::A0: return {841, 1189};
        case SheetSize::A1: return {594, 841};
        case SheetSize::A2: return {420, 594};
        case SheetSize::A3: return {297, 420};
        case SheetSize::A4: return {210, 297};
        case SheetSize::A5: return {148, 210};
        case SheetSize::ANSI_A: return {216, 279};   // 8.5" x 11"
        case SheetSize::ANSI_B: return {279, 432};   // 11" x 17"
        case SheetSize::ANSI_C: return {432, 559};   // 17" x 22"
        case SheetSize::ANSI_D: return {559, 864};   // 22" x 34"
        case SheetSize::ANSI_E: return {864, 1118};  // 34" x 44"
        default: return {0, 0};
    }
}

} // namespace BendCore::Drawing
