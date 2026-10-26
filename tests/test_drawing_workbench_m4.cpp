// M4 Phase: Drawing Workbench Foundation Tests
// Complete test suite for drawing, sheet, view, and manager functionality
#include "bendcore/drawing/drawing.hpp"
#include "bendcore/drawing/drawing_manager.hpp"

#include <cstdio>
#include <string>
#include <vector>
#include <cassert>

using namespace BendCore::CadCore;

static int test_count = 0;
static int pass_count = 0;

void check(bool ok, const char* test_name) {
    test_count++;
    if (ok) {
        pass_count++;
        std::printf("PASS: %s\n", test_name);
    } else {
        std::printf("FAIL: %s\n", test_name);
    }
}

// ============ SheetProperties Tests ============

void test_sheet_properties_default() {
    SheetProperties props;
    check(props.width_mm == 210.0, "SheetProperties default width");
    check(props.height_mm == 297.0, "SheetProperties default height");
    check(props.size == SheetSize::ISO_A4, "SheetProperties default size");
    check(props.has_border == true, "SheetProperties has border");
    check(props.has_title_block == true, "SheetProperties has title block");
    check(props.sheet_number == 1, "SheetProperties sheet number");
}

void test_sheet_properties_iso_a3() {
    SheetProperties props(SheetSize::ISO_A3);
    check(props.width_mm == 297.0, "SheetProperties A3 width");
    check(props.height_mm == 420.0, "SheetProperties A3 height");
    check(props.size == SheetSize::ISO_A3, "SheetProperties A3 size");
}

void test_sheet_properties_ansi_b() {
    SheetProperties props(SheetSize::ANSI_B);
    check(props.width_mm == 279.0, "SheetProperties ANSI B width");
    check(props.height_mm == 432.0, "SheetProperties ANSI B height");
}

void test_sheet_properties_custom() {
    SheetProperties props(SheetSize::CUSTOM);
    check(props.width_mm == 0.0, "SheetProperties custom width");
    check(props.height_mm == 0.0, "SheetProperties custom height");
}

// ============ ViewProperties Tests ============

void test_view_properties_default() {
    ViewProperties props;
    check(props.position_x == 0.0, "ViewProperties default x");
    check(props.position_y == 0.0, "ViewProperties default y");
    check(props.width == 100.0, "ViewProperties default width");
    check(props.height == 100.0, "ViewProperties default height");
    check(props.scale == 1.0, "ViewProperties default scale");
    check(props.is_visible == true, "ViewProperties visible");
}

// ============ DrawingSheet Tests ============

void test_drawing_sheet_creation() {
    EntityId sheet_id = EntityId::generate();
    DrawingSheet sheet(sheet_id, "Sheet1");
    check(sheet.getId() == sheet_id, "DrawingSheet ID");
    check(sheet.getName() == "Sheet1", "DrawingSheet name");
    check(sheet.getRevision() == 0, "DrawingSheet initial revision");
}

void test_drawing_sheet_properties() {
    EntityId sheet_id = EntityId::generate();
    DrawingSheet sheet(sheet_id, "Sheet1");

    SheetProperties props(SheetSize::ISO_A3);
    sheet.setProperties(props);

    const auto& retrieved = sheet.getProperties();
    check(retrieved.width_mm == 297.0, "DrawingSheet properties width");
    check(retrieved.height_mm == 420.0, "DrawingSheet properties height");
}

void test_drawing_sheet_add_view() {
    EntityId sheet_id = EntityId::generate();
    EntityId view_id = EntityId::generate();
    DrawingSheet sheet(sheet_id, "Sheet1");

    ViewProperties props;
    props.position_x = 10.0;
    props.position_y = 20.0;
    sheet.addView(view_id, props);

    check(sheet.getViewCount() == 1, "DrawingSheet view count after add");

    auto retrieved = sheet.getViewProperties(view_id);
    check(retrieved.has_value(), "DrawingSheet get view properties");
    check(retrieved->position_x == 10.0, "DrawingSheet view position x");
    check(retrieved->position_y == 20.0, "DrawingSheet view position y");
}

void test_drawing_sheet_duplicate_view() {
    EntityId sheet_id = EntityId::generate();
    EntityId view_id = EntityId::generate();
    DrawingSheet sheet(sheet_id, "Sheet1");

    ViewProperties props;
    sheet.addView(view_id, props);

    bool threw = false;
    try {
        sheet.addView(view_id, props);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "DrawingSheet duplicate view throws");
}

void test_drawing_sheet_remove_view() {
    EntityId sheet_id = EntityId::generate();
    EntityId view_id = EntityId::generate();
    DrawingSheet sheet(sheet_id, "Sheet1");

    ViewProperties props;
    sheet.addView(view_id, props);
    check(sheet.getViewCount() == 1, "DrawingSheet view count before remove");

    sheet.removeView(view_id);
    check(sheet.getViewCount() == 0, "DrawingSheet view count after remove");
}

void test_drawing_sheet_multiple_views() {
    EntityId sheet_id = EntityId::generate();
    DrawingSheet sheet(sheet_id, "Sheet1");

    ViewProperties props;
    EntityId view1 = EntityId::generate();
    EntityId view2 = EntityId::generate();
    EntityId view3 = EntityId::generate();

    sheet.addView(view1, props);
    sheet.addView(view2, props);
    sheet.addView(view3, props);

    check(sheet.getViewCount() == 3, "DrawingSheet multiple views count");

    auto view_ids = sheet.getViewIds();
    check(view_ids.size() == 3, "DrawingSheet get all view IDs");
}

void test_drawing_sheet_revision_tracking() {
    EntityId sheet_id = EntityId::generate();
    DrawingSheet sheet(sheet_id, "Sheet1");

    uint64_t rev1 = sheet.getRevision();
    check(rev1 == 0, "DrawingSheet initial revision");

    sheet.setName("Sheet2");
    uint64_t rev2 = sheet.getRevision();
    check(rev2 == 1, "DrawingSheet revision after name change");
}

void test_drawing_sheet_metadata() {
    EntityId sheet_id = EntityId::generate();
    DrawingSheet sheet(sheet_id, "Sheet1");

    sheet.setMetadata("key1", "value1");
    auto retrieved = sheet.getMetadata("key1");
    check(retrieved.has_value(), "DrawingSheet metadata get");
    check(retrieved.value() == "value1", "DrawingSheet metadata value");

    auto missing = sheet.getMetadata("nonexistent");
    check(!missing.has_value(), "DrawingSheet missing metadata");
}

// ============ DrawingView Tests ============

void test_drawing_view_creation() {
    EntityId view_id = EntityId::generate();
    EntityId source_id = EntityId::generate();
    DrawingView view(view_id, source_id, "View1");

    check(view.getId() == view_id, "DrawingView ID");
    check(view.getSourceGeometryId() == source_id, "DrawingView source ID");
    check(view.getName() == "View1", "DrawingView name");
}

void test_drawing_view_properties() {
    EntityId view_id = EntityId::generate();
    EntityId source_id = EntityId::generate();
    DrawingView view(view_id, source_id, "View1");

    ViewProperties props;
    props.position_x = 50.0;
    props.position_y = 60.0;
    props.scale = 2.0;

    view.setProperties(props);
    const auto& retrieved = view.getProperties();

    check(retrieved.position_x == 50.0, "DrawingView property x");
    check(retrieved.position_y == 60.0, "DrawingView property y");
    check(retrieved.scale == 2.0, "DrawingView property scale");
}

void test_drawing_view_clipping() {
    EntityId view_id = EntityId::generate();
    EntityId source_id = EntityId::generate();
    DrawingView view(view_id, source_id, "View1");

    check(!view.hasClippingRegion(), "DrawingView no clipping initially");

    view.setClippingRegion(10, 20, 100, 200);
    check(view.hasClippingRegion(), "DrawingView has clipping after set");

    auto [x, y, w, h] = view.getClippingBounds();
    check(x == 10 && y == 20 && w == 100 && h == 200, "DrawingView clipping bounds");

    view.clearClippingRegion();
    check(!view.hasClippingRegion(), "DrawingView clipping cleared");
}

void test_drawing_view_dirty_flag() {
    EntityId view_id = EntityId::generate();
    EntityId source_id = EntityId::generate();
    DrawingView view(view_id, source_id, "View1");

    check(view.isDirty(), "DrawingView initially dirty");

    view.markClean();
    check(!view.isDirty(), "DrawingView marked clean");

    view.markDirty();
    check(view.isDirty(), "DrawingView marked dirty again");
}

void test_drawing_view_metadata() {
    EntityId view_id = EntityId::generate();
    EntityId source_id = EntityId::generate();
    DrawingView view(view_id, source_id, "View1");

    view.setMetadata("type", "projection");
    auto retrieved = view.getMetadata("type");
    check(retrieved.has_value(), "DrawingView metadata get");
    check(retrieved.value() == "projection", "DrawingView metadata value");
}

// ============ Drawing Tests ============

void test_drawing_creation() {
    EntityId drawing_id = EntityId::generate();
    Drawing drawing(drawing_id, "Drawing1");

    check(drawing.getId() == drawing_id, "Drawing ID");
    check(drawing.getName() == "Drawing1", "Drawing name");
    check(drawing.getScale() == 1.0, "Drawing default scale");
}

void test_drawing_title_and_source() {
    EntityId drawing_id = EntityId::generate();
    EntityId source_id = EntityId::generate();
    Drawing drawing(drawing_id, "Drawing1");

    drawing.setTitle("My Assembly");
    drawing.setSourceId(source_id);

    check(drawing.getTitle() == "My Assembly", "Drawing title");
    check(drawing.getSourceId() == source_id, "Drawing source ID");
}

void test_drawing_scale() {
    EntityId drawing_id = EntityId::generate();
    Drawing drawing(drawing_id, "Drawing1");

    drawing.setScale(0.5);
    check(drawing.getScale() == 0.5, "Drawing scale 0.5");

    drawing.setScale(2.0);
    check(drawing.getScale() == 2.0, "Drawing scale 2.0");
}

void test_drawing_units() {
    EntityId drawing_id = EntityId::generate();
    Drawing drawing(drawing_id, "Drawing1");

    drawing.setUnits(DrawingUnits::INCHES);
    check(drawing.getUnits() == DrawingUnits::INCHES, "Drawing units");
}

void test_drawing_projection() {
    EntityId drawing_id = EntityId::generate();
    Drawing drawing(drawing_id, "Drawing1");

    drawing.setProjectionMethod(ProjectionMethod::FIRST_ANGLE);
    check(drawing.getProjectionMethod() == ProjectionMethod::FIRST_ANGLE, "Drawing projection");
}

void test_drawing_add_sheet() {
    EntityId drawing_id = EntityId::generate();
    Drawing drawing(drawing_id, "Drawing1");

    check(drawing.getSheetCount() == 0, "Drawing initial sheet count");

    auto sheet = drawing.addSheet("Sheet1");
    check(drawing.getSheetCount() == 1, "Drawing sheet count after add");
    check(sheet != nullptr, "Drawing add sheet returns sheet");
    check(sheet->getName() == "Sheet1", "Drawing sheet name");
}

void test_drawing_multiple_sheets() {
    EntityId drawing_id = EntityId::generate();
    Drawing drawing(drawing_id, "Drawing1");

    drawing.addSheet("Sheet1");
    drawing.addSheet("Sheet2");
    drawing.addSheet("Sheet3");

    check(drawing.getSheetCount() == 3, "Drawing multiple sheets count");

    auto sheet_ids = drawing.getSheetIds();
    check(sheet_ids.size() == 3, "Drawing get all sheet IDs");
}

void test_drawing_add_view() {
    EntityId drawing_id = EntityId::generate();
    EntityId source_id = EntityId::generate();
    Drawing drawing(drawing_id, "Drawing1");

    check(drawing.getViewCount() == 0, "Drawing initial view count");

    auto view = drawing.addView(source_id, "View1");
    check(drawing.getViewCount() == 1, "Drawing view count after add");
    check(view != nullptr, "Drawing add view returns view");
    check(view->getName() == "View1", "Drawing view name");
}

void test_drawing_mark_views_dirty() {
    EntityId drawing_id = EntityId::generate();
    EntityId source_id = EntityId::generate();
    Drawing drawing(drawing_id, "Drawing1");

    auto view1 = drawing.addView(source_id, "View1");
    auto view2 = drawing.addView(source_id, "View2");

    view1->markClean();
    view2->markClean();

    check(!view1->isDirty(), "Drawing view1 clean before");
    check(!view2->isDirty(), "Drawing view2 clean before");

    drawing.markAllViewsDirty();

    check(view1->isDirty(), "Drawing view1 dirty after mark all");
    check(view2->isDirty(), "Drawing view2 dirty after mark all");
}

void test_drawing_metadata() {
    EntityId drawing_id = EntityId::generate();
    Drawing drawing(drawing_id, "Drawing1");

    drawing.setMetadata("author", "John Doe");
    drawing.setMetadata("revision", "A");

    auto author = drawing.getMetadata("author");
    check(author.has_value(), "Drawing metadata get author");
    check(author.value() == "John Doe", "Drawing metadata author value");
}

void test_drawing_validate() {
    EntityId drawing_id = EntityId::generate();
    EntityId source_id = EntityId::generate();
    Drawing drawing(drawing_id, "Drawing1");

    drawing.setSourceId(source_id);

    // Should fail: no sheets
    auto error1 = drawing.validate();
    check(!error1.empty(), "Drawing validate fails with no sheets");

    // Add sheet - should pass
    drawing.addSheet("Sheet1");
    auto error2 = drawing.validate();
    check(error2.empty(), "Drawing validate passes with sheet");
}

// ============ DrawingManager Tests ============

void test_drawing_manager_create() {
    DrawingManager manager;
    EntityId source_id = EntityId::generate();

    auto result = manager.createDrawing("TestDrawing", source_id, "Test Title");
    check(result.success, "DrawingManager create drawing success");
    check(manager.getDrawingCount() == 1, "DrawingManager drawing count");
}

void test_drawing_manager_get() {
    DrawingManager manager;
    EntityId source_id = EntityId::generate();

    auto result = manager.createDrawing("TestDrawing", source_id);
    auto drawing = manager.getDrawing(result.created_entity_id);

    check(drawing != nullptr, "DrawingManager get drawing");
    check(drawing->getName() == "TestDrawing", "DrawingManager drawing name");
}

void test_drawing_manager_add_sheet() {
    DrawingManager manager;
    EntityId source_id = EntityId::generate();

    auto draw_result = manager.createDrawing("TestDrawing", source_id);
    auto sheet_result = manager.addSheet(draw_result.created_entity_id, "Sheet1", SheetSize::ISO_A3);

    check(sheet_result.success, "DrawingManager add sheet success");

    auto drawing = manager.getDrawing(draw_result.created_entity_id);
    check(drawing->getSheetCount() == 1, "DrawingManager sheet count");
}

void test_drawing_manager_add_view() {
    DrawingManager manager;
    EntityId source_id = EntityId::generate();

    auto draw_result = manager.createDrawing("TestDrawing", source_id);
    auto view_result = manager.addView(draw_result.created_entity_id, source_id, "View1");

    check(view_result.success, "DrawingManager add view success");

    auto drawing = manager.getDrawing(draw_result.created_entity_id);
    check(drawing->getViewCount() == 1, "DrawingManager view count");
}

void test_drawing_manager_json_serialization() {
    DrawingManager manager;
    EntityId source_id = EntityId::generate();

    auto draw_result = manager.createDrawing("TestDrawing", source_id, "Title");
    manager.addSheet(draw_result.created_entity_id, "Sheet1");
    manager.addView(draw_result.created_entity_id, source_id, "View1");

    auto json_str = manager.serializeToJson(draw_result.created_entity_id);
    check(!json_str.empty(), "DrawingManager serialize to JSON");
    check(json_str.find("TestDrawing") != std::string::npos, "DrawingManager JSON contains name");
}

void test_drawing_manager_json_deserialization() {
    DrawingManager manager1;
    EntityId source_id = EntityId::generate();

    auto draw_result = manager1.createDrawing("TestDrawing", source_id, "Title");
    auto sheet_result = manager1.addSheet(draw_result.created_entity_id, "Sheet1");
    auto view_result = manager1.addView(draw_result.created_entity_id, source_id, "View1");

    auto json_str = manager1.serializeToJson(draw_result.created_entity_id);

    // Deserialize in new manager
    DrawingManager manager2;
    auto des_result = manager2.deserializeFromJson(json_str);

    check(des_result.success, "DrawingManager deserialize JSON success");

    auto drawing = manager2.getDrawing(des_result.created_entity_id);
    check(drawing != nullptr, "DrawingManager deserialized drawing exists");
    check(drawing->getName() == "TestDrawing", "DrawingManager deserialized name");
    check(drawing->getTitle() == "Title", "DrawingManager deserialized title");
}

void test_drawing_manager_file_operations() {
    DrawingManager manager;
    EntityId source_id = EntityId::generate();

    auto draw_result = manager.createDrawing("FileTest", source_id);
    manager.addSheet(draw_result.created_entity_id, "Sheet1");

    const char* filepath = "/tmp/test_drawing.json";

    auto save_result = manager.saveDrawingToFile(draw_result.created_entity_id, filepath);
    check(save_result.success, "DrawingManager save to file");

    // Load in new manager
    DrawingManager manager2;
    auto load_result = manager2.loadDrawingFromFile(filepath);
    check(load_result.success, "DrawingManager load from file");

    auto drawing = manager2.getDrawing(load_result.created_entity_id);
    check(drawing != nullptr, "DrawingManager loaded drawing exists");
}

void test_drawing_manager_validate_all() {
    DrawingManager manager;
    EntityId source_id = EntityId::generate();

    auto result = manager.createDrawing("TestDrawing", source_id);
    manager.addSheet(result.created_entity_id, "Sheet1");

    auto errors = manager.validateAllDrawings();
    check(errors.empty(), "DrawingManager validate all clean");
}

// ============ Integration Tests ============

void test_integration_complete_drawing_workflow() {
    // Create manager and drawing
    DrawingManager manager;
    EntityId assembly_id = EntityId::generate();

    // Create drawing
    auto draw_result = manager.createDrawing("AssemblyDrawing", assembly_id, "Main Assembly");
    EntityId drawing_id = draw_result.created_entity_id;
    check(draw_result.success, "Integration: create drawing");

    // Add multiple sheets
    auto sheet1 = manager.addSheet(drawing_id, "Overview", SheetSize::ISO_A1);
    auto sheet2 = manager.addSheet(drawing_id, "Details", SheetSize::ISO_A2);
    check(sheet1.success && sheet2.success, "Integration: add sheets");

    // Add views
    auto view1 = manager.addView(drawing_id, assembly_id, "Front");
    auto view2 = manager.addView(drawing_id, assembly_id, "Top");
    check(view1.success && view2.success, "Integration: add views");

    // Add views to sheets
    ViewProperties props1;
    props1.position_x = 10; props1.position_y = 20;
    auto av1 = manager.addViewToSheet(drawing_id, sheet1.created_entity_id, view1.created_entity_id, props1);
    check(av1.success, "Integration: add view to sheet");

    // Validate
    auto errors = manager.validateAllDrawings();
    check(errors.empty(), "Integration: validate all");

    // Serialize
    auto json = manager.serializeToJson(drawing_id);
    check(!json.empty(), "Integration: serialize");

    // Verify drawing state
    auto drawing = manager.getDrawing(drawing_id);
    check(drawing->getSheetCount() == 2, "Integration: final sheet count");
    check(drawing->getViewCount() == 2, "Integration: final view count");
}

void test_integration_large_drawing() {
    DrawingManager manager;
    EntityId assembly_id = EntityId::generate();

    auto draw_result = manager.createDrawing("LargeDrawing", assembly_id);
    EntityId drawing_id = draw_result.created_entity_id;

    // Add many sheets
    for (int i = 0; i < 10; i++) {
        manager.addSheet(drawing_id, std::string("Sheet") + std::to_string(i+1));
    }

    // Add many views
    for (int i = 0; i < 20; i++) {
        manager.addView(drawing_id, assembly_id, std::string("View") + std::to_string(i+1));
    }

    auto drawing = manager.getDrawing(drawing_id);
    check(drawing->getSheetCount() == 10, "Integration: large drawing sheets");
    check(drawing->getViewCount() == 20, "Integration: large drawing views");

    // Validate
    auto errors = manager.validateAllDrawings();
    check(errors.empty(), "Integration: large drawing validate");
}

int main() {
    std::printf("=== M4 Phase: Drawing Workbench Foundation Tests ===\n");

    // SheetProperties tests
    test_sheet_properties_default();
    test_sheet_properties_iso_a3();
    test_sheet_properties_ansi_b();
    test_sheet_properties_custom();

    // ViewProperties tests
    test_view_properties_default();

    // DrawingSheet tests
    test_drawing_sheet_creation();
    test_drawing_sheet_properties();
    test_drawing_sheet_add_view();
    test_drawing_sheet_duplicate_view();
    test_drawing_sheet_remove_view();
    test_drawing_sheet_multiple_views();
    test_drawing_sheet_revision_tracking();
    test_drawing_sheet_metadata();

    // DrawingView tests
    test_drawing_view_creation();
    test_drawing_view_properties();
    test_drawing_view_clipping();
    test_drawing_view_dirty_flag();
    test_drawing_view_metadata();

    // Drawing tests
    test_drawing_creation();
    test_drawing_title_and_source();
    test_drawing_scale();
    test_drawing_units();
    test_drawing_projection();
    test_drawing_add_sheet();
    test_drawing_multiple_sheets();
    test_drawing_add_view();
    test_drawing_mark_views_dirty();
    test_drawing_metadata();
    test_drawing_validate();

    // DrawingManager tests
    test_drawing_manager_create();
    test_drawing_manager_get();
    test_drawing_manager_add_sheet();
    test_drawing_manager_add_view();
    test_drawing_manager_json_serialization();
    test_drawing_manager_json_deserialization();
    test_drawing_manager_file_operations();
    test_drawing_manager_validate_all();

    // Integration tests
    test_integration_complete_drawing_workflow();
    test_integration_large_drawing();

    std::printf("\n=== Test Results ===\n");
    std::printf("Passed: %d/%d\n", pass_count, test_count);

    if (pass_count == test_count) {
        std::printf("All tests passed!\n");
        return 0;
    } else {
        std::printf("Some tests failed!\n");
        return 1;
    }
}
