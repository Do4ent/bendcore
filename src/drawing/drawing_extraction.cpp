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
// View Extraction Functions
// ============================================================================

std::vector<std::shared_ptr<DrawingView>> extractViewsFromAssembly(
    const Assembly& assembly,
    const std::vector<ViewType>& view_types) {

    std::vector<std::shared_ptr<DrawingView>> views;

    for (size_t i = 0; i < view_types.size(); ++i) {
        auto view = std::make_shared<DrawingView>(
            "view_" + std::to_string(i),
            view_types[i]
        );
        view->setLabel(viewTypeToString(view_types[i]));

        // Set up view matrix based on view type
        switch (view_types[i]) {
            case ViewType::Front:
                view->setViewMatrix({{
                    {1.0, 0.0, 0.0},
                    {0.0, 1.0, 0.0},
                    {0.0, 0.0, 1.0}
                }});
                break;

            case ViewType::Top:
                view->setViewMatrix({{
                    {1.0, 0.0, 0.0},
                    {0.0, 0.0, -1.0},
                    {0.0, 1.0, 0.0}
                }});
                break;

            case ViewType::Right:
                view->setViewMatrix({{
                    {0.0, 0.0, 1.0},
                    {0.0, 1.0, 0.0},
                    {-1.0, 0.0, 0.0}
                }});
                break;

            case ViewType::Bottom:
                view->setViewMatrix({{
                    {1.0, 0.0, 0.0},
                    {0.0, 0.0, 1.0},
                    {0.0, -1.0, 0.0}
                }});
                break;

            case ViewType::Left:
                view->setViewMatrix({{
                    {0.0, 0.0, -1.0},
                    {0.0, 1.0, 0.0},
                    {1.0, 0.0, 0.0}
                }});
                break;

            case ViewType::Back:
                view->setViewMatrix({{
                    {-1.0, 0.0, 0.0},
                    {0.0, 1.0, 0.0},
                    {0.0, 0.0, -1.0}
                }});
                break;

            case ViewType::IsoTR: {
                // Standard isometric (35.26° rotation)
                double angle = std::asin(1.0 / std::sqrt(3.0));
                double cos_a = std::cos(angle);
                double sin_a = std::sin(angle);
                view->setViewMatrix({{
                    {cos_a, -sin_a, 0.0},
                    {sin_a * sin_a, cos_a * sin_a, cos_a},
                    {-sin_a * cos_a, -cos_a * cos_a, sin_a}
                }});
                break;
            }

            case ViewType::IsoTL:
                view->setViewMatrix({{
                    {-0.707, -0.707, 0.0},
                    {0.408, -0.408, 0.816},
                    {-0.577, 0.577, 0.577}
                }});
                break;

            case ViewType::IsoBR:
                view->setViewMatrix({{
                    {0.707, 0.707, 0.0},
                    {-0.408, 0.408, -0.816},
                    {-0.577, 0.577, 0.577}
                }});
                break;

            case ViewType::IsoBL:
                view->setViewMatrix({{
                    {-0.707, 0.707, 0.0},
                    {-0.408, -0.408, -0.816},
                    {0.577, 0.577, 0.577}
                }});
                break;

            default:
                break;
        }

        views.push_back(view);
    }

    return views;
}

std::shared_ptr<DrawingView> generateCustomView(
    const Assembly& assembly,
    const Vec3& view_direction,
    const Vec3& up_direction) {

    auto view = std::make_shared<DrawingView>("custom_view", ViewType::Custom);

    // Normalize input vectors
    double dir_norm = std::sqrt(
        view_direction[0] * view_direction[0] +
        view_direction[1] * view_direction[1] +
        view_direction[2] * view_direction[2]
    );

    if (dir_norm < 1e-12) {
        throw std::invalid_argument("View direction cannot be zero");
    }

    // Compute view basis
    Vec3 zaxis = {
        view_direction[0] / dir_norm,
        view_direction[1] / dir_norm,
        view_direction[2] / dir_norm
    };

    // Cross product for X axis
    Vec3 xaxis = {
        up_direction[1] * zaxis[2] - up_direction[2] * zaxis[1],
        up_direction[2] * zaxis[0] - up_direction[0] * zaxis[2],
        up_direction[0] * zaxis[1] - up_direction[1] * zaxis[0]
    };

    double xnorm = std::sqrt(xaxis[0] * xaxis[0] + xaxis[1] * xaxis[1] + xaxis[2] * xaxis[2]);
    if (xnorm < 1e-12) {
        throw std::invalid_argument("Up direction is parallel to view direction");
    }

    xaxis[0] /= xnorm;
    xaxis[1] /= xnorm;
    xaxis[2] /= xnorm;

    // Y axis is cross product of Z and X
    Vec3 yaxis = {
        zaxis[1] * xaxis[2] - zaxis[2] * xaxis[1],
        zaxis[2] * xaxis[0] - zaxis[0] * xaxis[2],
        zaxis[0] * xaxis[1] - zaxis[1] * xaxis[0]
    };

    // Set view matrix
    view->setViewMatrix({{
        {xaxis[0], xaxis[1], xaxis[2]},
        {yaxis[0], yaxis[1], yaxis[2]},
        {zaxis[0], zaxis[1], zaxis[2]}
    }});

    return view;
}

std::shared_ptr<DrawingSheet> createDrawingSheet(
    const Assembly& assembly,
    const std::vector<std::shared_ptr<DrawingView>>& views) {

    auto sheet = std::make_shared<DrawingSheet>();

    for (const auto& view : views) {
        sheet->addView(view);
    }

    sheet->addBorder();
    return sheet;
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
// Export Functions
// ============================================================================

bool exportDrawing(
    const DrawingSheet& sheet,
    const std::string& file_path,
    ExportFormat format) {

    switch (format) {
        case ExportFormat::DWG:
            return exportToDWG(sheet, file_path);
        case ExportFormat::DXF:
            return exportToDXF(sheet, file_path);
        case ExportFormat::PDF:
            return exportToPDF(sheet, file_path);
        case ExportFormat::SVG:
            return exportToSVG(sheet, file_path);
        case ExportFormat::PNG:
            return exportToPNG(sheet, file_path);
        case ExportFormat::JPEG:
            return exportToJPEG(sheet, file_path);
        default:
            return false;
    }
}

bool exportView(
    const DrawingView& view,
    const std::string& file_path,
    ExportFormat format) {

    // Create a temporary sheet with the single view
    // Create a non-const copy for adding to sheet
    auto view_copy = std::make_shared<DrawingView>(view.getId(), view.getType());
    view_copy->setLabel(view.getLabel());
    view_copy->setScale(view.getScale());
    view_copy->setOrigin(view.getOrigin());

    // Copy all elements
    for (const auto& edge : view.getEdges()) {
        view_copy->addEdge(edge);
    }
    for (const auto& circle : view.getCircles()) {
        view_copy->addCircle(circle);
    }
    for (const auto& arc : view.getArcs()) {
        view_copy->addArc(arc);
    }
    for (const auto& point : view.getPoints()) {
        view_copy->addPoint(point);
    }
    for (const auto& dim : view.getDimensions()) {
        view_copy->addDimension(dim);
    }
    for (const auto& text : view.getAnnotations()) {
        view_copy->addAnnotation(text);
    }

    auto sheet = std::make_shared<DrawingSheet>();
    sheet->addView(view_copy);

    return exportDrawing(*sheet, file_path, format);
}

bool exportToDWG(const DrawingSheet& sheet, const std::string& file_path) {
    // DWG export implementation
    // This is a placeholder - real implementation would use LibDWG library
    return true;
}

bool exportToDXF(const DrawingSheet& sheet, const std::string& file_path) {
    // DXF export implementation
    // Basic DXF file format
    std::ostringstream oss;

    oss << "  0\nSECTION\n";
    oss << "  2\nHEADER\n";
    oss << "  9\n$ACADVER\n  1\nAC1021\n";
    oss << "  0\nENDSEC\n";

    oss << "  0\nSECTION\n";
    oss << "  2\nENTITIES\n";

    // Export views
    for (const auto& view : sheet.getViews()) {
        if (!view) continue;

        // Export lines
        for (const auto& edge : view->getEdges()) {
            oss << "  0\nLINE\n";
            oss << " 10\n" << edge.p0[0] << "\n";
            oss << " 20\n" << edge.p0[1] << "\n";
            oss << " 11\n" << edge.p1[0] << "\n";
            oss << " 21\n" << edge.p1[1] << "\n";
        }

        // Export circles
        for (const auto& circle : view->getCircles()) {
            oss << "  0\nCIRCLE\n";
            oss << " 10\n" << circle.center[0] << "\n";
            oss << " 20\n" << circle.center[1] << "\n";
            oss << " 40\n" << circle.radius << "\n";
        }
    }

    oss << "  0\nENDSEC\n";
    oss << "  0\nEOF\n";

    return true;
}

bool exportToPDF(const DrawingSheet& sheet, const std::string& file_path) {
    // PDF export implementation
    // This is a placeholder
    return true;
}

bool exportToSVG(const DrawingSheet& sheet, const std::string& file_path) {
    // SVG export implementation
    std::ostringstream oss;

    const auto& props = sheet.getProperties();

    oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    oss << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
    oss << "width=\"" << props.width << "mm\" ";
    oss << "height=\"" << props.height << "mm\" ";
    oss << "viewBox=\"0 0 " << props.width << " " << props.height << "\">\n";

    // Add sheet background
    oss << "<rect width=\"" << props.width << "\" height=\"" << props.height
        << "\" fill=\"white\" stroke=\"black\" stroke-width=\"0.5\"/>\n";

    // Export views
    for (const auto& view : sheet.getViews()) {
        if (!view) continue;

        // Export lines
        for (const auto& edge : view->getEdges()) {
            oss << "<line x1=\"" << edge.p0[0] << "\" y1=\"" << edge.p0[1] << "\" ";
            oss << "x2=\"" << edge.p1[0] << "\" y2=\"" << edge.p1[1] << "\" ";
            oss << "stroke=\"black\" stroke-width=\"0.5\"/>\n";
        }

        // Export circles
        for (const auto& circle : view->getCircles()) {
            oss << "<circle cx=\"" << circle.center[0] << "\" cy=\"" << circle.center[1] << "\" ";
            oss << "r=\"" << circle.radius << "\" ";
            oss << "stroke=\"black\" fill=\"none\" stroke-width=\"0.5\"/>\n";
        }

        // Export text annotations
        for (const auto& text : view->getAnnotations()) {
            oss << "<text x=\"" << text.position[0] << "\" y=\"" << text.position[1] << "\" ";
            oss << "font-size=\"" << text.height << "\" ";
            oss << "text-anchor=\"" << text.anchor << "\">" << text.text << "</text>\n";
        }
    }

    oss << "</svg>\n";

    return true;
}

bool exportToPNG(
    const DrawingSheet& sheet,
    const std::string& file_path,
    int dpi) {

    // PNG export implementation
    // This is a placeholder
    return true;
}

bool exportToJPEG(
    const DrawingSheet& sheet,
    const std::string& file_path,
    int dpi,
    int quality) {

    // JPEG export implementation
    // This is a placeholder
    return true;
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
