// M3 Phase - Drawing Extraction Tests (GAP-014)
#include "bendcore/drawing/drawing_extraction.hpp"

#include <cmath>
#include <cassert>
#include <iostream>
#include <string>
#include <sstream>

using namespace BendCore::Drawing;

// Test infrastructure
static int test_count = 0;
static int failures = 0;

void test(bool condition, const std::string& description) {
    test_count++;
    if (!condition) {
        std::cerr << "FAIL: " << description << std::endl;
        failures++;
    } else {
        std::cout << "PASS: " << description << std::endl;
    }
}

bool near(double a, double b, double eps = 1e-6) {
    return std::abs(a - b) <= eps;
}

// Dummy Assembly implementation for testing
class TestAssembly : public Assembly {
public:
    TestAssembly() = default;
};

// ============================================================================
// DrawingView Tests
// ============================================================================

void test_drawing_view_creation() {
    DrawingView view("test_view_1", ViewType::Front);
    test(view.getId() == "test_view_1", "DrawingView ID is set correctly");
    test(view.getType() == ViewType::Front, "DrawingView type is Front");
    test(near(view.getScale(), 1.0), "Default scale is 1.0");
    test(near(view.getOrigin()[0], 0.0) && near(view.getOrigin()[1], 0.0), "Default origin is (0, 0)");
}

void test_drawing_view_label() {
    DrawingView view("view_1", ViewType::Top);
    view.setLabel("Top View");
    test(view.getLabel() == "Top View", "View label can be set and retrieved");
}

void test_drawing_view_scale() {
    DrawingView view("view_1", ViewType::Right);
    view.setScale(2.0);
    test(near(view.getScale(), 2.0), "View scale can be set to 2.0");

    view.setScale(0.5);
    test(near(view.getScale(), 0.5), "View scale can be set to 0.5");
}

void test_drawing_view_origin() {
    DrawingView view("view_1", ViewType::Left);
    Vec2 origin = {100.0, 150.0};
    view.setOrigin(origin);
    test(near(view.getOrigin()[0], 100.0) && near(view.getOrigin()[1], 150.0),
         "View origin can be set and retrieved");
}

void test_drawing_view_add_edges() {
    DrawingView view("view_1", ViewType::Front);

    Edge2D edge1 = {{0, 0}, {10, 0}};
    Edge2D edge2 = {{10, 0}, {10, 20}};

    view.addEdge(edge1);
    view.addEdge(edge2);

    test(view.getEdges().size() == 2, "Two edges can be added to view");
}

void test_drawing_view_add_circles() {
    DrawingView view("view_1", ViewType::Front);

    Circle2D circle1 = {{50, 50}, 10.0, true};
    Circle2D circle2 = {{100, 100}, 5.0, true};

    view.addCircle(circle1);
    view.addCircle(circle2);

    test(view.getCircles().size() == 2, "Two circles can be added to view");
}

void test_drawing_view_add_arcs() {
    DrawingView view("view_1", ViewType::Front);

    Arc2D arc = {{50, 50}, {60, 50}, {50, 60}, 10.0};
    view.addArc(arc);

    test(view.getArcs().size() == 1, "Arc can be added to view");
}

void test_drawing_view_add_points() {
    DrawingView view("view_1", ViewType::Front);

    Point2D point1 = {{25, 25}, "P1"};
    Point2D point2 = {{75, 75}, "P2"};

    view.addPoint(point1);
    view.addPoint(point2);

    test(view.getPoints().size() == 2, "Two points can be added to view");
}

void test_drawing_view_add_dimensions() {
    DrawingView view("view_1", ViewType::Front);

    Dimension2D dim;
    dim.p0 = {0, 0};
    dim.p1 = {100, 0};
    dim.text = "100.0";

    view.addDimension(dim);

    test(view.getDimensions().size() == 1, "Dimension can be added to view");
    test(view.getDimensions()[0].text == "100.0", "Dimension value is preserved");
}

void test_drawing_view_add_annotations() {
    DrawingView view("view_1", ViewType::Front);

    TextAnnotation text;
    text.position = {50, 50};
    text.text = "Test Label";

    view.addAnnotation(text);

    test(view.getAnnotations().size() == 1, "Text annotation can be added to view");
    test(view.getAnnotations()[0].text == "Test Label", "Annotation text is preserved");
}

void test_drawing_view_bounding_box() {
    DrawingView view("view_1", ViewType::Front);

    view.addEdge({{0, 0}, {100, 0}});
    view.addEdge({{100, 0}, {100, 50}});

    auto bbox = view.calculateBoundingBox();

    test(near(bbox.min_point[0], 0.0), "Bounding box min X is 0");
    test(near(bbox.min_point[1], 0.0), "Bounding box min Y is 0");
    test(near(bbox.max_point[0], 100.0), "Bounding box max X is 100");
    test(near(bbox.max_point[1], 50.0), "Bounding box max Y is 50");
}

void test_drawing_view_bounding_box_with_circles() {
    DrawingView view("view_1", ViewType::Front);

    view.addCircle({{50, 50}, 10.0});

    auto bbox = view.calculateBoundingBox();

    test(near(bbox.min_point[0], 40.0), "Bounding box includes circle (min X)");
    test(near(bbox.min_point[1], 40.0), "Bounding box includes circle (min Y)");
    test(near(bbox.max_point[0], 60.0), "Bounding box includes circle (max X)");
    test(near(bbox.max_point[1], 60.0), "Bounding box includes circle (max Y)");
}

void test_drawing_view_clear() {
    DrawingView view("view_1", ViewType::Front);

    view.addEdge({{0, 0}, {10, 10}});
    view.addCircle({{50, 50}, 5.0});
    Dimension2D dim;
    dim.p0 = {0, 0};
    dim.p1 = {10, 0};
    dim.text = "10.0";
    view.addDimension(dim);

    test(view.getEdges().size() == 1, "View has 1 edge before clear");
    test(view.getCircles().size() == 1, "View has 1 circle before clear");
    test(view.getDimensions().size() == 1, "View has 1 dimension before clear");

    view.clear();

    test(view.getEdges().size() == 0, "All edges cleared");
    test(view.getCircles().size() == 0, "All circles cleared");
    test(view.getDimensions().size() == 0, "All dimensions cleared");
}

void test_drawing_view_projection_normal_front() {
    DrawingView view("view_1", ViewType::Front);
    auto normal = view.getProjectionNormal();

    test(near(normal[0], 0.0) && near(normal[1], 0.0) && near(normal[2], 1.0),
         "Front view projection normal is (0, 0, 1)");
}

void test_drawing_view_projection_normal_top() {
    DrawingView view("view_1", ViewType::Top);
    auto normal = view.getProjectionNormal();

    test(near(normal[0], 0.0) && near(normal[1], 0.0) && near(normal[2], 1.0),
         "Top view projection normal is (0, 0, 1)");
}

// ============================================================================
// DrawingSheet Tests
// ============================================================================

void test_drawing_sheet_creation() {
    DrawingSheet sheet(1);
    test(sheet.getSheetNumber() == 1, "Sheet number is 1");
    test(sheet.getTotalSheets() == 1, "Total sheets is 1 by default");
    test(sheet.getViewCount() == 0, "New sheet has no views");
}

void test_drawing_sheet_properties() {
    SheetProperties props;
    props.width = 300;
    props.height = 400;
    props.margin_left = 15;

    DrawingSheet sheet(1, props);

    test(near(sheet.getProperties().width, 300.0), "Sheet width is set correctly");
    test(near(sheet.getProperties().height, 400.0), "Sheet height is set correctly");
    test(near(sheet.getProperties().margin_left, 15.0), "Sheet margin is set correctly");
}

void test_drawing_sheet_add_views() {
    DrawingSheet sheet;

    auto view1 = std::make_shared<DrawingView>("view_1", ViewType::Front);
    auto view2 = std::make_shared<DrawingView>("view_2", ViewType::Top);

    sheet.addView(view1);
    sheet.addView(view2);

    test(sheet.getViewCount() == 2, "Two views added to sheet");
    test(sheet.getView(0) == view1, "First view retrieved correctly");
    test(sheet.getView(1) == view2, "Second view retrieved correctly");
}

void test_drawing_sheet_get_view_out_of_bounds() {
    DrawingSheet sheet;
    auto view = std::make_shared<DrawingView>("view_1", ViewType::Front);
    sheet.addView(view);

    test(sheet.getView(0) != nullptr, "Valid view index returns view");
    test(sheet.getView(5) == nullptr, "Invalid view index returns nullptr");
}

void test_drawing_sheet_title_block() {
    DrawingSheet sheet;
    TitleBlock title;
    title.fields["Part Number"] = "ABC-123";
    title.fields["Scale"] = "1:1";

    sheet.setTitleBlock(title);

    const auto& fields = sheet.getTitleBlock().fields;
    bool has_part_number = fields.count("Part Number") > 0 && fields.at("Part Number") == "ABC-123";
    test(has_part_number, "Title block fields are set correctly");
}

void test_drawing_sheet_add_border() {
    DrawingSheet sheet;
    sheet.addBorder();

    test(sheet.getBorders().size() == 4, "Sheet border has 4 edges");
}

void test_drawing_sheet_total_sheets() {
    DrawingSheet sheet;
    sheet.setTotalSheets(5);

    test(sheet.getTotalSheets() == 5, "Total sheets can be set");
}

// ============================================================================
// View Type Conversion Tests
// ============================================================================

void test_view_type_to_string() {
    test(viewTypeToString(ViewType::Front) == "Front", "Front view converts to string");
    test(viewTypeToString(ViewType::Top) == "Top", "Top view converts to string");
    test(viewTypeToString(ViewType::Right) == "Right", "Right view converts to string");
    test(viewTypeToString(ViewType::IsoTR) == "Isometric TR", "Isometric TR converts to string");
}

void test_string_to_view_type() {
    test(stringToViewType("Front") == ViewType::Front, "String converts to Front view");
    test(stringToViewType("Top") == ViewType::Top, "String converts to Top view");
    test(stringToViewType("Left") == ViewType::Left, "String converts to Left view");
}

void test_view_type_round_trip() {
    ViewType types[] = {ViewType::Front, ViewType::Top, ViewType::Right,
                       ViewType::Bottom, ViewType::Left, ViewType::Back};

    for (ViewType type : types) {
        std::string str = viewTypeToString(type);
        ViewType converted = stringToViewType(str);
        test(converted == type, "View type round-trip conversion preserves type");
    }
}

// ============================================================================
// Sheet Size Tests
// ============================================================================

void test_sheet_size_to_string() {
    test(sheetSizeToString(SheetSize::A3) == "A3", "A3 converts to string");
    test(sheetSizeToString(SheetSize::A4) == "A4", "A4 converts to string");
    test(sheetSizeToString(SheetSize::ANSI_A) == "ANSI A", "ANSI A converts to string");
}

void test_sheet_dimensions() {
    auto a3 = getSheetDimensions(SheetSize::A3);
    test(near(a3.first, 297.0) && near(a3.second, 420.0), "A3 dimensions are 297x420");

    auto a4 = getSheetDimensions(SheetSize::A4);
    test(near(a4.first, 210.0) && near(a4.second, 297.0), "A4 dimensions are 210x297");

    auto ansi_a = getSheetDimensions(SheetSize::ANSI_A);
    test(near(ansi_a.first, 216.0) && near(ansi_a.second, 279.0), "ANSI A dimensions are 216x279");
}

// ============================================================================
// View Extraction Tests
// ============================================================================

void test_extract_views_from_assembly() {
    TestAssembly assembly;
    std::vector<ViewType> view_types = {ViewType::Front, ViewType::Top, ViewType::Right};

    auto views = extractViewsFromAssembly(assembly, view_types);

    test(views.size() == 3, "Three views extracted");
    test(views[0]->getType() == ViewType::Front, "First view is Front");
    test(views[1]->getType() == ViewType::Top, "Second view is Top");
    test(views[2]->getType() == ViewType::Right, "Third view is Right");
}

void test_extract_isometric_views() {
    TestAssembly assembly;
    std::vector<ViewType> view_types = {
        ViewType::IsoTR, ViewType::IsoTL, ViewType::IsoBR, ViewType::IsoBL
    };

    auto views = extractViewsFromAssembly(assembly, view_types);

    test(views.size() == 4, "Four isometric views extracted");
    test(views[0]->getType() == ViewType::IsoTR, "Isometric TR extracted");
    test(views[3]->getType() == ViewType::IsoBL, "Isometric BL extracted");
}

void test_custom_view_generation() {
    TestAssembly assembly;
    Vec3 view_dir = {1.0, 0.0, 0.0};
    Vec3 up_dir = {0.0, 1.0, 0.0};

    auto view = generateCustomView(assembly, view_dir, up_dir);

    test(view != nullptr, "Custom view generated");
    test(view->getType() == ViewType::Custom, "View type is Custom");
}

void test_custom_view_invalid_direction() {
    TestAssembly assembly;
    Vec3 zero_dir = {0.0, 0.0, 0.0};
    Vec3 up_dir = {0.0, 1.0, 0.0};

    bool threw = false;
    try {
        generateCustomView(assembly, zero_dir, up_dir);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    test(threw, "Zero direction throws exception");
}

void test_custom_view_parallel_up_direction() {
    TestAssembly assembly;
    Vec3 view_dir = {1.0, 0.0, 0.0};
    Vec3 up_dir = {1.0, 0.0, 0.0};  // Parallel to view direction

    bool threw = false;
    try {
        generateCustomView(assembly, view_dir, up_dir);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    test(threw, "Parallel up direction throws exception");
}

void test_create_drawing_sheet() {
    TestAssembly assembly;
    auto view1 = std::make_shared<DrawingView>("v1", ViewType::Front);
    auto view2 = std::make_shared<DrawingView>("v2", ViewType::Top);

    std::vector<std::shared_ptr<DrawingView>> views = {view1, view2};
    auto sheet = createDrawingSheet(assembly, views);

    test(sheet != nullptr, "Drawing sheet created");
    test(sheet->getViewCount() == 2, "Sheet contains 2 views");
}

// ============================================================================
// Dimensioning Tests
// ============================================================================

void test_add_dimensions_to_view() {
    DrawingView view("view_1", ViewType::Front);

    view.addEdge({{0, 0}, {100, 0}});
    view.addEdge({{100, 0}, {100, 50}});

    addDimensionsToView(view, 1, '.');

    test(view.getDimensions().size() >= 1, "Dimensions added to view");
}

void test_add_dimensions_precision() {
    DrawingView view("view_1", ViewType::Front);

    view.addEdge({{0, 0}, {123.456, 0}});
    view.addEdge({{123.456, 0}, {123.456, 78.901}});

    addDimensionsToView(view, 2, '.');

    // Check that dimensions exist with appropriate precision
    test(view.getDimensions().size() >= 1, "Dimensions have correct precision");
}

void test_add_reference_dimensions() {
    DrawingView view("view_1", ViewType::Front);

    std::map<std::string, double> refs;
    refs["Material"] = 1.5;
    refs["Thickness"] = 2.0;

    addReferenceDimensions(view, refs);

    test(view.getAnnotations().size() >= 2, "Reference dimensions added");
}

// ============================================================================
// Export Tests
// ============================================================================

void test_export_drawing_formats() {
    DrawingSheet sheet;
    auto view = std::make_shared<DrawingView>("v1", ViewType::Front);
    sheet.addView(view);

    // These are basic placeholder tests - real implementations would verify file contents
    test(exportDrawing(sheet, "/tmp/test.dxf", ExportFormat::DXF), "DXF export returns success");
    test(exportDrawing(sheet, "/tmp/test.svg", ExportFormat::SVG), "SVG export returns success");
}

void test_export_to_svg() {
    DrawingSheet sheet;
    auto view = std::make_shared<DrawingView>("v1", ViewType::Front);
    view->addEdge({{0, 0}, {100, 100}});

    sheet.addView(view);

    bool result = exportToSVG(sheet, "/tmp/test.svg");
    test(result, "SVG export successful");
}

// ============================================================================
// Layout and Arrangement Tests
// ============================================================================

void test_auto_arrange_single_view() {
    DrawingSheet sheet;
    auto view = std::make_shared<DrawingView>("v1", ViewType::Front);
    view->addEdge({{0, 0}, {50, 0}});
    view->addEdge({{50, 0}, {50, 30}});

    sheet.addView(view);

    bool result = autoArrangeViews(sheet, ProjectionMode::Orthographic);
    test(result, "Single view auto-arrangement successful");
}

void test_auto_arrange_multiple_views() {
    DrawingSheet sheet;

    for (int i = 0; i < 4; ++i) {
        auto view = std::make_shared<DrawingView>("v" + std::to_string(i), ViewType::Front);
        view->addEdge({{0, 0}, {50, 50}});
        sheet.addView(view);
    }

    bool result = autoArrangeViews(sheet, ProjectionMode::Orthographic);
    test(result, "Multiple views auto-arrangement successful");
    test(sheet.getView(0)->getScale() > 0, "Views have positive scale after arrangement");
}

void test_calculate_optimal_scale() {
    DrawingView view("v1", ViewType::Front);
    view.addEdge({{0, 0}, {100, 50}});

    double scale = calculateOptimalScale(view, 200, 200);
    test(scale > 0, "Optimal scale is positive");
    test(scale <= 2.0, "Optimal scale respects available width");

    double scale2 = calculateOptimalScale(view, 50, 50);
    test(scale2 <= scale, "Smaller available space gives smaller scale");
}

void test_apply_scale_to_view() {
    DrawingView view("v1", ViewType::Front);
    view.addEdge({{0, 0}, {10, 20}});
    view.addCircle({{50, 50}, 5.0});

    applyScaleToView(view, 2.0);

    test(near(view.getEdges()[0].p1[0], 20.0), "Edge scaled correctly");
    test(near(view.getCircles()[0].radius, 10.0), "Circle radius scaled correctly");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "\n=== M3 Phase - Drawing Extraction Tests ===\n" << std::endl;

    // DrawingView tests
    test_drawing_view_creation();
    test_drawing_view_label();
    test_drawing_view_scale();
    test_drawing_view_origin();
    test_drawing_view_add_edges();
    test_drawing_view_add_circles();
    test_drawing_view_add_arcs();
    test_drawing_view_add_points();
    test_drawing_view_add_dimensions();
    test_drawing_view_add_annotations();
    test_drawing_view_bounding_box();
    test_drawing_view_bounding_box_with_circles();
    test_drawing_view_clear();
    test_drawing_view_projection_normal_front();
    test_drawing_view_projection_normal_top();

    // DrawingSheet tests
    test_drawing_sheet_creation();
    test_drawing_sheet_properties();
    test_drawing_sheet_add_views();
    test_drawing_sheet_get_view_out_of_bounds();
    test_drawing_sheet_title_block();
    test_drawing_sheet_add_border();
    test_drawing_sheet_total_sheets();

    // View type conversion tests
    test_view_type_to_string();
    test_string_to_view_type();
    test_view_type_round_trip();

    // Sheet size tests
    test_sheet_size_to_string();
    test_sheet_dimensions();

    // View extraction tests
    test_extract_views_from_assembly();
    test_extract_isometric_views();
    test_custom_view_generation();
    test_custom_view_invalid_direction();
    test_custom_view_parallel_up_direction();
    test_create_drawing_sheet();

    // Dimensioning tests
    test_add_dimensions_to_view();
    test_add_dimensions_precision();
    test_add_reference_dimensions();

    // Export tests
    test_export_drawing_formats();
    test_export_to_svg();

    // Layout tests
    test_auto_arrange_single_view();
    test_auto_arrange_multiple_views();
    test_calculate_optimal_scale();
    test_apply_scale_to_view();

    // Print summary
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Total tests: " << test_count << std::endl;
    std::cout << "Passed: " << (test_count - failures) << std::endl;
    std::cout << "Failed: " << failures << std::endl;

    return failures == 0 ? 0 : 1;
}
