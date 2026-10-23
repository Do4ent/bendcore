// Тесты 2D-эскизов на чертежах (v7.9) - drawing.hpp Sketch2D структура
#include "bendcore/drawing.hpp"

#include <cassert>
#include <cstdio>
#include <iostream>

using namespace bendcore::drawing;

static int test_count = 0;
static int fail_count = 0;

static void test(const char* name, bool condition) {
    ++test_count;
    if (!condition) {
        std::printf("FAIL [%d]: %s\n", test_count, name);
        ++fail_count;
    } else {
        std::printf("PASS [%d]: %s\n", test_count, name);
    }
}

// Test 1: Create empty sketch
static void test_make_empty_sketch() {
    Sketch2D sketch = make_sketch(1, "Test Sketch", "testuser");
    test("sketch has id", !sketch.id.empty());
    test("sketch sheet_number is 1", sketch.sheet_number == 1);
    test("sketch name is correct", sketch.name == "Test Sketch");
    test("sketch created_by is correct", sketch.created_by == "testuser");
    test("sketch is visible", sketch.visible == true);
    test("sketch is not locked", sketch.locked == false);
    test("sketch has no elements", sketch.elements.size() == 0);
    test("sketch has no dimensions", sketch.dimensions.size() == 0);
}

// Test 2: Create line element
static void test_make_sketch_line() {
    Vec2 p0 = {0, 0};
    Vec2 p1 = {10, 20};
    SketchElement line = make_sketch_line(p0, p1, "");
    test("line type is Line", line.type == SketchElementType::Line);
    test("line p0 is correct", line.p0[0] == 0 && line.p0[1] == 0);
    test("line p1 is correct", line.p1[0] == 10 && line.p1[1] == 20);
    test("line id not empty", !line.id.empty());
    test("line style is Contour", line.style == LineStyle::Contour);
}

// Test 3: Create arc element
static void test_make_sketch_arc() {
    Vec2 p0 = {0, 0};
    Vec2 p1 = {10, 0};
    SketchElement arc = make_sketch_arc(p0, p1, 5.0, true, false, "");
    test("arc type is Arc", arc.type == SketchElementType::Arc);
    test("arc radius is correct", arc.radius == 5.0);
    test("arc_large flag is correct", arc.arc_large == true);
    test("arc_sweep flag is correct", arc.arc_sweep == false);
}

// Test 4: Create text element
static void test_make_sketch_text() {
    Vec2 p = {50, 50};
    SketchElement text = make_sketch_text(p, "Hello", 5.5, "");
    test("text type is Text", text.type == SketchElementType::Text);
    test("text content is correct", text.text == "Hello");
    test("text height is correct", text.text_height == 5.5);
    test("text position is correct", text.p0[0] == 50 && text.p0[1] == 50);
}

// Test 5: Create dimension
static void test_make_sketch_dimension() {
    Vec2 p0 = {0, 0};
    Vec2 p1 = {10, 0};
    Vec2 offset = {0, 5};
    SketchDimension dim = make_sketch_dimension(p0, p1, "10.0", offset, "");
    test("dimension id not empty", !dim.id.empty());
    test("dimension text is correct", dim.text == "10.0");
    test("dimension p0 is correct", dim.p0[0] == 0 && dim.p0[1] == 0);
    test("dimension p1 is correct", dim.p1[0] == 10 && dim.p1[1] == 0);
    test("dimension offset is correct", dim.offset_point[0] == 0 && dim.offset_point[1] == 5);
}

// Test 6: Add elements to sketch
static void test_sketch_add_element() {
    Sketch2D sketch = make_sketch(1, "Test", "user");
    Vec2 p0 = {0, 0};
    Vec2 p1 = {10, 10};
    SketchElement line = make_sketch_line(p0, p1, "line_1");

    sketch_add_element(sketch, line);
    test("element added to sketch", sketch.elements.size() == 1);
    test("element id is correct", sketch.elements[0].id == "line_1");
    test("element type is correct", sketch.elements[0].type == SketchElementType::Line);
}

// Test 7: Remove element from sketch
static void test_sketch_remove_element() {
    Sketch2D sketch = make_sketch(1, "Test", "user");
    SketchElement line1 = make_sketch_line({0, 0}, {10, 10}, "line_1");
    SketchElement line2 = make_sketch_line({5, 5}, {15, 15}, "line_2");

    sketch_add_element(sketch, line1);
    sketch_add_element(sketch, line2);
    test("two elements added", sketch.elements.size() == 2);

    bool removed = sketch_remove_element(sketch, "line_1");
    test("element removed successfully", removed);
    test("one element remains", sketch.elements.size() == 1);
    test("remaining element is line_2", sketch.elements[0].id == "line_2");

    bool not_removed = sketch_remove_element(sketch, "nonexistent");
    test("removing nonexistent element fails", !not_removed);
}

// Test 8: Update element in sketch
static void test_sketch_update_element() {
    Sketch2D sketch = make_sketch(1, "Test", "user");
    SketchElement original = make_sketch_line({0, 0}, {10, 10}, "line_1");
    sketch_add_element(sketch, original);

    SketchElement updated = make_sketch_line({5, 5}, {15, 15}, "");
    bool success = sketch_update_element(sketch, "line_1", updated);

    test("element updated successfully", success);
    test("updated element position", sketch.elements[0].p0[0] == 5 && sketch.elements[0].p0[1] == 5);
    test("element id preserved", sketch.elements[0].id == "line_1");
}

// Test 9: Add dimension to sketch
static void test_sketch_add_dimension() {
    Sketch2D sketch = make_sketch(1, "Test", "user");
    SketchDimension dim = make_sketch_dimension({0, 0}, {10, 0}, "10.0", {0, 5}, "dim_1");

    sketch_add_dimension(sketch, dim);
    test("dimension added to sketch", sketch.dimensions.size() == 1);
    test("dimension text is correct", sketch.dimensions[0].text == "10.0");
}

// Test 10: Remove dimension from sketch
static void test_sketch_remove_dimension() {
    Sketch2D sketch = make_sketch(1, "Test", "user");
    SketchDimension dim1 = make_sketch_dimension({0, 0}, {10, 0}, "10.0", {0, 5}, "dim_1");
    SketchDimension dim2 = make_sketch_dimension({5, 5}, {15, 5}, "10.0", {5, 10}, "dim_2");

    sketch_add_dimension(sketch, dim1);
    sketch_add_dimension(sketch, dim2);
    test("two dimensions added", sketch.dimensions.size() == 2);

    bool removed = sketch_remove_dimension(sketch, "dim_1");
    test("dimension removed successfully", removed);
    test("one dimension remains", sketch.dimensions.size() == 1);
    test("remaining dimension is dim_2", sketch.dimensions[0].id == "dim_2");
}

// Test 11: Validate sketch
static void test_validate_sketch() {
    Sketch2D sketch = make_sketch(1, "Test", "user");
    SketchElement line = make_sketch_line({0, 0}, {10, 10}, "line_1");
    sketch_add_element(sketch, line);

    SketchValidationOptions opts;
    opts.check_element_ids = true;
    SketchValidationResult result = validate_sketch(sketch, opts);

    test("valid sketch passes validation", result.valid);
    test("no errors in valid sketch", result.errors.empty());
}

// Test 12: Validate sketch with duplicate IDs
static void test_validate_sketch_duplicate_ids() {
    Sketch2D sketch = make_sketch(1, "Test", "user");
    SketchElement line1 = make_sketch_line({0, 0}, {10, 10}, "line_1");
    SketchElement line2 = make_sketch_line({5, 5}, {15, 15}, "line_1");  // Same ID

    sketch_add_element(sketch, line1);
    sketch_add_element(sketch, line2);

    SketchValidationOptions opts;
    opts.check_element_ids = true;
    SketchValidationResult result = validate_sketch(sketch, opts);

    test("invalid sketch fails validation", !result.valid);
    test("duplicate ID error detected", !result.errors.empty());
}

// Test 13: Sketch bounding box
static void test_sketch_bbox() {
    Sketch2D sketch = make_sketch(1, "Test", "user");

    SketchElement line1 = make_sketch_line({0, 0}, {10, 0}, "");
    SketchElement line2 = make_sketch_line({10, 0}, {10, 20}, "");
    sketch_add_element(sketch, line1);
    sketch_add_element(sketch, line2);

    Box2 bbox = sketch_bbox(sketch);
    test("bbox not empty", !bbox.empty);
    test("bbox x0 is 0", bbox.lo[0] == 0);
    test("bbox y0 is 0", bbox.lo[1] == 0);
    test("bbox x1 is 10", bbox.hi[0] == 10);
    test("bbox y1 is 20", bbox.hi[1] == 20);
    test("bbox width is 10", bbox.w() == 10);
    test("bbox height is 20", bbox.h() == 20);
}

// Test 14: Sketch persistence - verify timestamps
static void test_sketch_timestamps() {
    Sketch2D sketch = make_sketch(1, "Test", "user");

    test("sketch has created_at", !sketch.created_at.empty());
    test("sketch has modified_at", !sketch.modified_at.empty());
    test("timestamps are ISO 8601", sketch.created_at.find('T') != std::string::npos);
    test("timestamps have Z suffix", sketch.created_at.find('Z') != std::string::npos);
}

// Test 15: Multiple sketches on different sheets
static void test_multiple_sketches() {
    Sketch2D sketch1 = make_sketch(1, "Sheet1Sketch", "user");
    Sketch2D sketch2 = make_sketch(2, "Sheet2Sketch", "user");

    test("sketch1 sheet_number is 1", sketch1.sheet_number == 1);
    test("sketch2 sheet_number is 2", sketch2.sheet_number == 2);
    test("sketch1 and sketch2 have different ids", sketch1.id != sketch2.id);
}

int main() {
    std::printf("Running 2D Sketch API tests...\n\n");

    test_make_empty_sketch();
    test_make_sketch_line();
    test_make_sketch_arc();
    test_make_sketch_text();
    test_make_sketch_dimension();
    test_sketch_add_element();
    test_sketch_remove_element();
    test_sketch_update_element();
    test_sketch_add_dimension();
    test_sketch_remove_dimension();
    test_validate_sketch();
    test_validate_sketch_duplicate_ids();
    test_sketch_bbox();
    test_sketch_timestamps();
    test_multiple_sketches();

    std::printf("\n=======================================\n");
    std::printf("Total tests: %d\n", test_count);
    std::printf("Passed: %d\n", test_count - fail_count);
    std::printf("Failed: %d\n", fail_count);
    std::printf("=======================================\n");

    return fail_count > 0 ? 1 : 0;
}
