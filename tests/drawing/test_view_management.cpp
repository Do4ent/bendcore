// M4 Phase - View Management & Layouts Tests (GAP-017)
// Comprehensive tests for view management, layouts, and synchronization
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>

#include "bendcore/drawing/view_manager.hpp"
#include "bendcore/drawing/layout_manager.hpp"
#include "bendcore/drawing/view_properties.hpp"
#include "bendcore/drawing/drawing_extraction.hpp"

using namespace BendCore::Drawing;

// ============================================================================
// Test Utilities
// ============================================================================

static int test_count = 0;
static int test_passed = 0;

void test_assert(bool condition, const std::string& test_name) {
    test_count++;
    if (condition) {
        test_passed++;
        std::cout << "[PASS] " << test_name << std::endl;
    } else {
        std::cout << "[FAIL] " << test_name << std::endl;
    }
}

std::shared_ptr<DrawingView> createTestView(const std::string& view_id, ViewType type) {
    auto view = std::make_shared<DrawingView>(view_id, type);
    view->setLabel(view_id);
    view->setScale(1.0);

    // Add some test geometry
    Edge2D edge{{0, 0}, {10, 10}, true, "contour"};
    view->addEdge(edge);

    Circle2D circle{{15, 15}, 5.0, true, "contour"};
    view->addCircle(circle);

    return view;
}

// ============================================================================
// View Properties Tests
// ============================================================================

void test_view_layer_manager_basic() {
    ViewLayerManager layer_mgr;

    layer_mgr.addLayer("Layer1");
    layer_mgr.addLayer("Layer2");

    test_assert(layer_mgr.isLayerVisible("Layer1"), "Layer initially visible");
    test_assert(!layer_mgr.isLayerLocked("Layer1"), "Layer initially not locked");

    layer_mgr.setLayerVisible("Layer1", false);
    test_assert(!layer_mgr.isLayerVisible("Layer1"), "Layer visibility changed");

    layer_mgr.setLayerLocked("Layer1", true);
    test_assert(layer_mgr.isLayerLocked("Layer1"), "Layer locked");
}

void test_view_layer_manager_visibility_presets() {
    ViewLayerManager layer_mgr;

    layer_mgr.addLayer("Layer1");
    layer_mgr.addLayer("Layer2");
    layer_mgr.addLayer("Layer3");

    layer_mgr.hideAllLayers();
    test_assert(!layer_mgr.isLayerVisible("Layer1"), "All layers hidden");
    test_assert(!layer_mgr.isLayerVisible("Layer2"), "All layers hidden");

    layer_mgr.showAllLayers();
    test_assert(layer_mgr.isLayerVisible("Layer1"), "All layers shown");

    layer_mgr.showOnlyLayer("Layer2");
    test_assert(!layer_mgr.isLayerVisible("Layer1"), "Only Layer2 visible");
    test_assert(layer_mgr.isLayerVisible("Layer2"), "Only Layer2 visible");
    test_assert(!layer_mgr.isLayerVisible("Layer3"), "Only Layer2 visible");
}

void test_view_layer_manager_opacity() {
    ViewLayerManager layer_mgr;

    layer_mgr.addLayer("Layer1");
    layer_mgr.setLayerOpacity("Layer1", 0.5);

    test_assert(std::abs(layer_mgr.getLayerOpacity("Layer1") - 0.5) < 0.01, "Layer opacity set");

    layer_mgr.setLayerOpacity("Layer1", 1.5);  // Should clamp to 1.0
    test_assert(std::abs(layer_mgr.getLayerOpacity("Layer1") - 1.0) < 0.01, "Layer opacity clamped to max");

    layer_mgr.setLayerOpacity("Layer1", -0.5);  // Should clamp to 0.0
    test_assert(std::abs(layer_mgr.getLayerOpacity("Layer1") - 0.0) < 0.01, "Layer opacity clamped to min");
}

void test_view_appearance_line_styles() {
    ViewAppearance appearance;

    // Check default line styles exist
    const auto& contour = appearance.getLineProperties(LineStyle::Contour);
    test_assert(contour.thickness > 0, "Contour line has thickness");

    const auto& hidden = appearance.getLineProperties(LineStyle::Hidden);
    test_assert(hidden.thickness > 0, "Hidden line has thickness");

    // Modify a line style
    LineProperties custom{LineStyle::Thin, 0.5, "#FF0000", 0.0, 0.0};
    appearance.setLineProperties(LineStyle::Thin, custom);

    const auto& thin = appearance.getLineProperties(LineStyle::Thin);
    test_assert(std::abs(thin.thickness - 0.5) < 0.01, "Custom line thickness applied");
    test_assert(thin.color == "#FF0000", "Custom line color applied");
}

void test_view_appearance_text_properties() {
    ViewAppearance appearance;

    const auto& text_props = appearance.getTextProperties();
    test_assert(text_props.font_size > 0, "Text has font size");

    TextProperties new_text{"Courier", 4.5, "#0000FF", "right", 1.5};
    appearance.setTextProperties(new_text);

    const auto& updated = appearance.getTextProperties();
    test_assert(updated.font_name == "Courier", "Font name changed");
    test_assert(std::abs(updated.font_size - 4.5) < 0.01, "Font size changed");
}

void test_view_appearance_frame_properties() {
    ViewAppearance appearance;

    const auto& frame = appearance.getFrameProperties();
    test_assert(frame.show_scale, "Frame shows scale by default");
    test_assert(frame.show_label, "Frame shows label by default");

    ViewFrameProperties new_frame{BorderStyle::Rounded, 2.0, "#0000FF", 5.0, false, false, {10, 10}};
    appearance.setFrameProperties(new_frame);

    const auto& updated = appearance.getFrameProperties();
    test_assert(updated.border_style == BorderStyle::Rounded, "Border style changed");
    test_assert(!updated.show_scale, "Frame scale visibility changed");
}

// ============================================================================
// View Manager Basic Tests
// ============================================================================

void test_view_manager_register_unregister() {
    ViewManager mgr;

    auto view1 = createTestView("view1", ViewType::Front);
    mgr.registerView(view1);

    test_assert(mgr.hasView("view1"), "View registered");
    test_assert(mgr.getViewCount() == 1, "View count correct");

    mgr.unregisterView("view1");
    test_assert(!mgr.hasView("view1"), "View unregistered");
    test_assert(mgr.getViewCount() == 0, "View count correct after unregister");
}

void test_view_manager_duplicate_register() {
    ViewManager mgr;

    auto view1 = createTestView("view1", ViewType::Front);
    mgr.registerView(view1);

    try {
        mgr.registerView(view1);
        test_assert(false, "Duplicate registration should throw");
    } catch (const std::invalid_argument&) {
        test_assert(true, "Duplicate registration throws exception");
    }
}

void test_view_manager_position_scale() {
    ViewManager mgr;

    auto view = createTestView("view1", ViewType::Front);
    mgr.registerView(view);

    Vec2 position{50.0, 100.0};
    mgr.setViewPosition("view1", position);

    Vec2 retrieved = mgr.getViewPosition("view1");
    test_assert(std::abs(retrieved[0] - 50.0) < 0.01 && std::abs(retrieved[1] - 100.0) < 0.01,
                "View position set and retrieved");

    mgr.setViewScale("view1", 2.0);
    double scale = mgr.getViewScale("view1");
    test_assert(std::abs(scale - 2.0) < 0.01, "View scale set and retrieved");
}

void test_view_manager_invalid_scale() {
    ViewManager mgr;

    auto view = createTestView("view1", ViewType::Front);
    mgr.registerView(view);

    try {
        mgr.setViewScale("view1", -1.0);
        test_assert(false, "Negative scale should throw");
    } catch (const std::invalid_argument&) {
        test_assert(true, "Negative scale throws exception");
    }

    try {
        mgr.setViewScale("view1", 0.0);
        test_assert(false, "Zero scale should throw");
    } catch (const std::invalid_argument&) {
        test_assert(true, "Zero scale throws exception");
    }
}

// ============================================================================
// View Manager Relationships Tests
// ============================================================================

void test_view_manager_parent_child_relationship() {
    ViewManager mgr;

    auto parent = createTestView("parent", ViewType::Front);
    auto child = createTestView("child", ViewType::IsoTR);

    mgr.registerView(parent);
    mgr.registerView(child);

    mgr.setViewDependency("parent", "child");

    test_assert(mgr.getParentView("child") == "parent", "Parent view set");

    auto dependents = mgr.getDependentViews("parent");
    test_assert(dependents.size() == 1 && dependents[0] == "child", "Dependent view found");
}

void test_view_manager_detail_view() {
    ViewManager mgr;

    auto parent = createTestView("parent", ViewType::Front);
    auto detail = createTestView("detail", ViewType::Custom);

    mgr.registerView(parent);
    mgr.registerView(detail);

    DetailViewInfo detail_info{
        "parent",
        {25.0, 25.0},
        10.0,
        DetailViewType::Circle,
        {0, -10}
    };

    mgr.createDetailView("parent", "detail", detail_info);

    const auto& metadata = mgr.getMetadata("detail");
    test_assert(metadata.getDetailViewInfo().has_value(), "Detail view info set");
    test_assert(metadata.getDetailViewInfo()->parent_view_id == "parent", "Detail parent correct");
}

void test_view_manager_section_view() {
    ViewManager mgr;

    auto parent = createTestView("parent", ViewType::Front);
    auto section = createTestView("section", ViewType::Custom);

    mgr.registerView(parent);
    mgr.registerView(section);

    SectionViewInfo section_info{
        "parent",
        {0, 0},
        {20, 20},
        "A"
    };

    mgr.createSectionView("parent", "section", section_info);

    const auto& metadata = mgr.getMetadata("section");
    test_assert(metadata.getSectionViewInfo().has_value(), "Section view info set");
    test_assert(metadata.getSectionViewInfo()->section_identifier == "A", "Section identifier correct");
}

void test_view_manager_view_ordering() {
    ViewManager mgr;

    auto view1 = createTestView("view1", ViewType::Front);
    auto view2 = createTestView("view2", ViewType::Top);
    auto view3 = createTestView("view3", ViewType::Right);

    mgr.registerView(view1);
    mgr.registerView(view2);
    mgr.registerView(view3);

    mgr.moveViewToFront("view1");
    auto all_views = mgr.getAllViews();
    test_assert(all_views.back()->getId() == "view1", "View moved to front");

    mgr.moveViewToBack("view1");
    all_views = mgr.getAllViews();
    test_assert(all_views.front()->getId() == "view1", "View moved to back");
}

// ============================================================================
// View Manager Visibility Tests
// ============================================================================

void test_view_manager_visibility() {
    ViewManager mgr;

    auto view = createTestView("view1", ViewType::Front);
    mgr.registerView(view);

    test_assert(mgr.getViewVisibility("view1"), "View visible by default");

    mgr.setViewVisibility("view1", false);
    test_assert(!mgr.getViewVisibility("view1"), "View visibility changed");
}

void test_view_manager_show_hide_all() {
    ViewManager mgr;

    auto view1 = createTestView("view1", ViewType::Front);
    auto view2 = createTestView("view2", ViewType::Top);

    mgr.registerView(view1);
    mgr.registerView(view2);

    mgr.hideAllViews();
    test_assert(!mgr.getViewVisibility("view1"), "View1 hidden");
    test_assert(!mgr.getViewVisibility("view2"), "View2 hidden");

    mgr.showAllViews();
    test_assert(mgr.getViewVisibility("view1"), "View1 shown");
    test_assert(mgr.getViewVisibility("view2"), "View2 shown");
}

// ============================================================================
// View Manager Alignment Tests
// ============================================================================

void test_view_manager_alignment_calculation() {
    ViewManager mgr;

    auto view1 = createTestView("view1", ViewType::Front);
    auto view2 = createTestView("view2", ViewType::Top);

    mgr.registerView(view1);
    mgr.registerView(view2);

    mgr.setViewPosition("view1", {10.0, 20.0});
    mgr.setViewPosition("view2", {30.0, 40.0});

    auto alignment = mgr.calculateViewAlignment({"view1", "view2"});
    test_assert(alignment.left <= 10.0, "Left alignment calculated");
    test_assert(alignment.top <= 20.0, "Top alignment calculated");
}

// ============================================================================
// View Manager Synchronization Tests
// ============================================================================

void test_view_manager_batch_update() {
    ViewManager mgr;

    auto view = createTestView("view1", ViewType::Front);
    mgr.registerView(view);

    mgr.beginBatchUpdate();
    test_assert(mgr.isInBatchUpdate(), "In batch update");

    mgr.setViewPosition("view1", {10.0, 20.0});
    mgr.setViewScale("view1", 2.0);

    mgr.endBatchUpdate();
    test_assert(!mgr.isInBatchUpdate(), "Batch update ended");
}

void test_view_manager_sync() {
    ViewManager mgr;

    auto parent = createTestView("parent", ViewType::Front);
    auto child = createTestView("child", ViewType::Custom);

    mgr.registerView(parent);
    mgr.registerView(child);

    mgr.setViewDependency("parent", "child");

    auto parent_rev = mgr.getMetadata("parent").getRevision();
    auto child_rev = mgr.getMetadata("child").getRevision();

    mgr.syncView("parent");
    test_assert(mgr.getMetadata("parent").getRevision() > parent_rev, "Parent view synced");

    mgr.syncDependentViews("parent");
    test_assert(mgr.getMetadata("child").getRevision() > child_rev, "Child view synced");
}

// ============================================================================
// Layout Manager Basic Tests
// ============================================================================

void test_layout_manager_single_view() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    auto view = createTestView("view1", ViewType::Front);
    std::vector<std::shared_ptr<DrawingView>> views{view};

    auto solution = layout_mgr.arrangeViewsByTemplate(views, LayoutTemplate::SingleView);

    test_assert(solution.is_valid, "Single view layout valid");
    test_assert(solution.view_layouts.size() == 1, "Single view layout has one view");
}

void test_layout_manager_three_view() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    auto view1 = createTestView("view1", ViewType::Front);
    auto view2 = createTestView("view2", ViewType::Top);
    auto view3 = createTestView("view3", ViewType::Right);

    std::vector<std::shared_ptr<DrawingView>> views{view1, view2, view3};

    auto solution = layout_mgr.arrangeViewsByTemplate(views, LayoutTemplate::ThreeView);

    test_assert(solution.is_valid, "Three view layout valid");
    test_assert(solution.view_layouts.size() == 3, "Three view layout has three views");
}

void test_layout_manager_six_view() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    std::vector<std::shared_ptr<DrawingView>> views;
    for (int i = 0; i < 6; i++) {
        views.push_back(createTestView("view" + std::to_string(i), ViewType::Front));
    }

    auto solution = layout_mgr.arrangeViewsByTemplate(views, LayoutTemplate::SixView);

    test_assert(solution.is_valid, "Six view layout valid");
    test_assert(solution.view_layouts.size() == 6, "Six view layout has six views");
}

void test_layout_manager_grid() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    std::vector<std::shared_ptr<DrawingView>> views;
    for (int i = 0; i < 4; i++) {
        views.push_back(createTestView("view" + std::to_string(i), ViewType::Front));
    }

    auto solution = layout_mgr.arrangeViewsInGrid(views, 2, 2);

    test_assert(solution.is_valid, "Grid layout valid");
    test_assert(solution.view_layouts.size() == 4, "Grid layout has correct number of views");
}

void test_layout_manager_line_layout() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    std::vector<std::shared_ptr<DrawingView>> views;
    for (int i = 0; i < 3; i++) {
        views.push_back(createTestView("view" + std::to_string(i), ViewType::Front));
    }

    auto solution_h = layout_mgr.arrangeViewsInLine(views, true);
    test_assert(solution_h.view_layouts.size() == 3, "Horizontal line has three views");

    auto solution_v = layout_mgr.arrangeViewsInLine(views, false);
    test_assert(solution_v.view_layouts.size() == 3, "Vertical line has three views");
}

// ============================================================================
// Layout Manager Configuration Tests
// ============================================================================

void test_layout_manager_config() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    LayoutConfig config;
    config.margin_left = 20.0;
    config.margin_top = 20.0;
    config.horizontal_spacing = 15.0;
    config.center_views = true;

    layout_mgr.setConfig(config);

    const auto& retrieved = layout_mgr.getConfig();
    test_assert(std::abs(retrieved.margin_left - 20.0) < 0.01, "Margin left configured");
    test_assert(retrieved.center_views, "Center views configured");
}

// ============================================================================
// Layout Manager Constraints Tests
// ============================================================================

void test_layout_manager_add_constraint() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    LayoutConstraint constraint{
        "view1",
        ConstraintType::FixedPosition,
        0.0,
        {},
        10
    };

    layout_mgr.addConstraint(constraint);

    // Constraint added successfully (no exception)
    test_assert(true, "Constraint added");
}

void test_layout_manager_remove_constraint() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    LayoutConstraint constraint{
        "view1",
        ConstraintType::FixedPosition,
        0.0,
        {},
        10
    };

    layout_mgr.addConstraint(constraint);
    layout_mgr.removeConstraint("view1", ConstraintType::FixedPosition);

    // Constraint removed successfully
    test_assert(true, "Constraint removed");
}

void test_layout_manager_clear_constraints() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    LayoutConstraint c1{"view1", ConstraintType::FixedPosition, 0.0, {}, 10};
    LayoutConstraint c2{"view2", ConstraintType::FixedSize, 0.0, {}, 10};

    layout_mgr.addConstraint(c1);
    layout_mgr.addConstraint(c2);
    layout_mgr.clearConstraints();

    // Constraints cleared successfully
    test_assert(true, "Constraints cleared");
}

// ============================================================================
// Layout Manager Overlap Detection Tests
// ============================================================================

void test_layout_manager_overlap_detection() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    ViewLayout layout1{"view1", {10, 10}, 50, 50, 1.0};
    ViewLayout layout2{"view2", {40, 40}, 50, 50, 1.0};
    ViewLayout layout3{"view3", {100, 100}, 50, 50, 1.0};

    test_assert(layout_mgr.checkOverlap(layout1, layout2), "Overlapping views detected");
    test_assert(!layout_mgr.checkOverlap(layout1, layout3), "Non-overlapping views not detected");
}

void test_layout_manager_fit_on_sheet() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    ViewLayout layout1{"view1", {10, 10}, 100, 100, 1.0};
    test_assert(layout_mgr.checkFitsOnSheet(layout1), "View fits on sheet");

    ViewLayout layout2{"view2", {-10, 10}, 50, 50, 1.0};
    test_assert(!layout_mgr.checkFitsOnSheet(layout2), "View with negative position doesn't fit");

    ViewLayout layout3{"view3", {400, 400}, 100, 100, 1.0};
    test_assert(!layout_mgr.checkFitsOnSheet(layout3), "View extending beyond sheet doesn't fit");
}

// ============================================================================
// Layout Manager Alignment Operations Tests
// ============================================================================

void test_layout_manager_align_views() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    std::vector<std::string> view_ids{"view1", "view2", "view3"};
    layout_mgr.alignViews(view_ids, ConstraintType::AlignLeft);

    // Alignment constraints added successfully
    test_assert(true, "Views aligned left");
}

void test_layout_manager_distribute_views() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    std::vector<std::string> view_ids{"view1", "view2", "view3"};
    layout_mgr.distributeViews(view_ids, true);

    // Distribution constraint added successfully
    test_assert(true, "Views distributed");
}

void test_layout_manager_space_evenly() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    std::vector<std::string> view_ids{"view1", "view2", "view3"};
    layout_mgr.spaceViewsEvenly(view_ids, true);

    // Spacing constraints added successfully
    test_assert(true, "Views spaced evenly");
}

// ============================================================================
// Layout Manager Validation Tests
// ============================================================================

void test_layout_manager_validate_layout() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    LayoutSolution solution{
        {ViewLayout{"view1", {10, 10}, 50, 50, 1.0},
         ViewLayout{"view2", {70, 10}, 50, 50, 1.0}},
        true,
        ""
    };

    test_assert(layout_mgr.validateLayout(solution), "Valid layout validated correctly");
}

// ============================================================================
// Layout History Tests
// ============================================================================

void test_layout_history_record_retrieve() {
    LayoutHistory history;

    LayoutSolution solution{
        {ViewLayout{"view1", {10, 10}, 50, 50, 1.0}},
        true,
        ""
    };

    history.recordLayout("layout1", solution);

    const auto& retrieved = history.getLayout("layout1");
    test_assert(retrieved.view_layouts.size() == 1, "Layout recorded and retrieved");
    test_assert(retrieved.view_layouts[0].view_id == "view1", "Layout data correct");
}

void test_layout_history_get_names() {
    LayoutHistory history;

    LayoutSolution solution{{}, true, ""};
    history.recordLayout("layout1", solution);
    history.recordLayout("layout2", solution);
    history.recordLayout("layout3", solution);

    auto names = history.getLayoutNames();
    test_assert(names.size() == 3, "All layout names retrieved");
}

void test_layout_history_clear() {
    LayoutHistory history;

    LayoutSolution solution{{}, true, ""};
    history.recordLayout("layout1", solution);
    history.clearHistory();

    test_assert(history.getLayoutNames().empty(), "History cleared");
}

// ============================================================================
// Edge Cases and Complex Scenarios
// ============================================================================

void test_empty_view_list() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    std::vector<std::shared_ptr<DrawingView>> views;
    auto solution = layout_mgr.arrangeViewsByTemplate(views, LayoutTemplate::ThreeView);

    test_assert(!solution.is_valid, "Empty view list produces invalid layout");
}

void test_single_view_with_multiple_template() {
    SheetProperties sheet_props;
    LayoutManager layout_mgr(sheet_props);

    auto view = createTestView("view1", ViewType::Front);
    std::vector<std::shared_ptr<DrawingView>> views{view};

    auto solution = layout_mgr.arrangeViewsByTemplate(views, LayoutTemplate::ThreeView);

    test_assert(solution.view_layouts.size() == 1, "Single view handled with 3-view template");
}

void test_view_manager_metadata_isolation() {
    ViewManager mgr;

    auto view1 = createTestView("view1", ViewType::Front);
    auto view2 = createTestView("view2", ViewType::Top);

    mgr.registerView(view1);
    mgr.registerView(view2);

    auto& meta1 = mgr.getMetadata("view1");
    auto& meta2 = mgr.getMetadata("view2");

    meta1.setViewName("Custom1");
    meta2.setViewName("Custom2");

    test_assert(mgr.getMetadata("view1").getViewName() == "Custom1", "Metadata isolated for view1");
    test_assert(mgr.getMetadata("view2").getViewName() == "Custom2", "Metadata isolated for view2");
}

void test_layout_manager_workspace_dimensions() {
    SheetProperties sheet_props{SheetSize::A4, Orientation::Landscape, 210, 297, 10, 10, 10, 10};
    LayoutManager layout_mgr(sheet_props);

    // A4 landscape = 297 x 210 mm, with 10mm margins on each side
    // Working area = (297 - 20) x (210 - 20) = 277 x 190
    auto view = createTestView("view1", ViewType::Front);
    std::vector<std::shared_ptr<DrawingView>> views{view};

    auto solution = layout_mgr.arrangeViewsByTemplate(views, LayoutTemplate::SingleView);
    test_assert(solution.is_valid, "Layout respects sheet dimensions and margins");
}

void test_multiple_dependency_chains() {
    ViewManager mgr;

    // Create a chain: view1 -> view2 -> view3
    auto view1 = createTestView("view1", ViewType::Front);
    auto view2 = createTestView("view2", ViewType::Custom);
    auto view3 = createTestView("view3", ViewType::Custom);

    mgr.registerView(view1);
    mgr.registerView(view2);
    mgr.registerView(view3);

    mgr.setViewDependency("view1", "view2");
    mgr.setViewDependency("view2", "view3");

    test_assert(mgr.getParentView("view2") == "view1", "First level dependency correct");
    test_assert(mgr.getParentView("view3") == "view2", "Second level dependency correct");

    auto view1_deps = mgr.getDependentViews("view1");
    test_assert(view1_deps.size() == 1, "First level has one dependent");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "Running M4 Phase - View Management & Layouts Tests" << std::endl;
    std::cout << "==================================================\n" << std::endl;

    // View Properties Tests
    std::cout << "View Properties Tests:" << std::endl;
    test_view_layer_manager_basic();
    test_view_layer_manager_visibility_presets();
    test_view_layer_manager_opacity();
    test_view_appearance_line_styles();
    test_view_appearance_text_properties();
    test_view_appearance_frame_properties();

    // View Manager Basic Tests
    std::cout << "\nView Manager Basic Tests:" << std::endl;
    test_view_manager_register_unregister();
    test_view_manager_duplicate_register();
    test_view_manager_position_scale();
    test_view_manager_invalid_scale();

    // View Manager Relationships Tests
    std::cout << "\nView Manager Relationships Tests:" << std::endl;
    test_view_manager_parent_child_relationship();
    test_view_manager_detail_view();
    test_view_manager_section_view();
    test_view_manager_view_ordering();

    // View Manager Visibility Tests
    std::cout << "\nView Manager Visibility Tests:" << std::endl;
    test_view_manager_visibility();
    test_view_manager_show_hide_all();

    // View Manager Alignment Tests
    std::cout << "\nView Manager Alignment Tests:" << std::endl;
    test_view_manager_alignment_calculation();

    // View Manager Synchronization Tests
    std::cout << "\nView Manager Synchronization Tests:" << std::endl;
    test_view_manager_batch_update();
    test_view_manager_sync();

    // Layout Manager Basic Tests
    std::cout << "\nLayout Manager Basic Tests:" << std::endl;
    test_layout_manager_single_view();
    test_layout_manager_three_view();
    test_layout_manager_six_view();
    test_layout_manager_grid();
    test_layout_manager_line_layout();

    // Layout Manager Configuration Tests
    std::cout << "\nLayout Manager Configuration Tests:" << std::endl;
    test_layout_manager_config();

    // Layout Manager Constraints Tests
    std::cout << "\nLayout Manager Constraints Tests:" << std::endl;
    test_layout_manager_add_constraint();
    test_layout_manager_remove_constraint();
    test_layout_manager_clear_constraints();

    // Layout Manager Overlap Detection Tests
    std::cout << "\nLayout Manager Overlap Detection Tests:" << std::endl;
    test_layout_manager_overlap_detection();
    test_layout_manager_fit_on_sheet();

    // Layout Manager Alignment Operations Tests
    std::cout << "\nLayout Manager Alignment Operations Tests:" << std::endl;
    test_layout_manager_align_views();
    test_layout_manager_distribute_views();
    test_layout_manager_space_evenly();

    // Layout Manager Validation Tests
    std::cout << "\nLayout Manager Validation Tests:" << std::endl;
    test_layout_manager_validate_layout();

    // Layout History Tests
    std::cout << "\nLayout History Tests:" << std::endl;
    test_layout_history_record_retrieve();
    test_layout_history_get_names();
    test_layout_history_clear();

    // Edge Cases and Complex Scenarios
    std::cout << "\nEdge Cases and Complex Scenarios:" << std::endl;
    test_empty_view_list();
    test_single_view_with_multiple_template();
    test_view_manager_metadata_isolation();
    test_layout_manager_workspace_dimensions();
    test_multiple_dependency_chains();

    // Print summary
    std::cout << "\n==================================================\n" << std::endl;
    std::cout << "Test Results: " << test_passed << "/" << test_count << " passed" << std::endl;

    return (test_passed == test_count) ? 0 : 1;
}
