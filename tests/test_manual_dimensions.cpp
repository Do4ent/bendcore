// Тесты для ручных размеров на чертежах (Task 2.1-2.7)
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

// Helper: Create a dummy sheet for testing
static DrawingSheet make_dummy_sheet() {
    DrawingSheet sheet;
    sheet.number = 1;
    sheet.count = 1;
    sheet.standard = Standard::GOST;
    sheet.projection = Projection::FirstAngle;
    sheet.frame.drawable = {{0, 0}, {420, 297}};  // A3 format
    sheet.frame.border = {{5, 5}, {415, 292}};
    return sheet;
}

// Test 1: Create and add manual dimension without conflicts
static void test_add_manual_dimension_valid() {
    DrawingSheet sheet = make_dummy_sheet();
    ManualDimensionTable table;

    Vec2 position{100, 150};
    std::string id = add_manual_dimension(sheet, table, position, "100.5", false, "user1");

    test("dimension added successfully", !id.empty());
    test("dimension table has 1 item", table.dimensions.size() == 1);
    test("dimension text is correct", table.dimensions[0].text == "100.5");
    test("dimension position is correct",
         table.dimensions[0].position[0] == 100 && table.dimensions[0].position[1] == 150);
    test("dimension reference flag is false", table.dimensions[0].reference == false);
    test("dimension author is correct", table.dimensions[0].author == "user1");
}

// Test 2: Reject dimension outside sheet bounds
static void test_add_manual_dimension_out_of_bounds() {
    DrawingSheet sheet = make_dummy_sheet();
    ManualDimensionTable table;

    // Position outside drawable area
    Vec2 position{-10, 150};
    std::string id = add_manual_dimension(sheet, table, position, "50.0", false, "user1");

    test("dimension rejected for out of bounds", id.empty());
    test("dimension table still empty", table.dimensions.size() == 0);
}

// Test 3: Reject dimension conflicting with another manual dimension
static void test_add_manual_dimension_conflict() {
    DrawingSheet sheet = make_dummy_sheet();
    ManualDimensionTable table;

    // Add first dimension
    Vec2 pos1{100, 100};
    std::string id1 = add_manual_dimension(sheet, table, pos1, "100.0", false, "user1");
    test("first dimension added", !id1.empty());

    // Try to add second dimension too close (< 5mm)
    Vec2 pos2{102, 102};
    std::string id2 = add_manual_dimension(sheet, table, pos2, "50.0", false, "user2");

    test("conflicting dimension rejected", id2.empty());
    test("table has only 1 dimension", table.dimensions.size() == 1);
}

// Test 4: Find dimension by id
static void test_find_manual_dimension() {
    ManualDimensionTable table;

    ManualDimension dim1;
    dim1.id = "dim_001";
    dim1.text = "Test Dimension";
    dim1.position = {100, 100};
    table.dimensions.push_back(dim1);

    auto* found = table.find("dim_001");
    test("dimension found by id", found != nullptr);
    if (found) {
        test("found dimension text is correct", found->text == "Test Dimension");
    }

    auto* not_found = table.find("nonexistent");
    test("nonexistent dimension returns nullptr", not_found == nullptr);
}

// Test 5: Update dimension text
static void test_update_manual_dimension_text() {
    ManualDimensionTable table;

    ManualDimension dim;
    dim.id = "dim_001";
    dim.text = "Original";
    table.dimensions.push_back(dim);

    bool success = update_manual_dimension_text(table, "dim_001", "Updated");
    test("update returns true", success);
    test("dimension text updated", table.dimensions[0].text == "Updated");

    bool fail = update_manual_dimension_text(table, "nonexistent", "Text");
    test("update nonexistent returns false", !fail);
}

// Test 6: Remove dimension
static void test_remove_manual_dimension() {
    ManualDimensionTable table;

    ManualDimension dim1, dim2;
    dim1.id = "dim_001";
    dim1.text = "Dimension 1";
    dim2.id = "dim_002";
    dim2.text = "Dimension 2";
    table.dimensions.push_back(dim1);
    table.dimensions.push_back(dim2);

    test("table has 2 dimensions", table.dimensions.size() == 2);

    bool success = remove_manual_dimension(table, "dim_001");
    test("remove returns true", success);
    test("table has 1 dimension", table.dimensions.size() == 1);
    test("remaining dimension is correct", table.dimensions[0].id == "dim_002");

    bool fail = remove_manual_dimension(table, "nonexistent");
    test("remove nonexistent returns false", !fail);
}

// Test 7: Move dimension to valid position
static void test_move_manual_dimension_valid() {
    DrawingSheet sheet = make_dummy_sheet();
    ManualDimensionTable table;

    // Add initial dimension
    Vec2 pos1{100, 100};
    std::string id = add_manual_dimension(sheet, table, pos1, "100.0", false, "user");
    test("dimension added", !id.empty());

    // Move to new valid position
    Vec2 pos2{200, 200};
    bool success = move_manual_dimension(sheet, table, id, pos2);
    test("move returns true", success);
    test("dimension position updated",
         table.dimensions[0].position[0] == 200 && table.dimensions[0].position[1] == 200);
}

// Test 8: Reject move to invalid position
static void test_move_manual_dimension_invalid() {
    DrawingSheet sheet = make_dummy_sheet();
    ManualDimensionTable table;

    ManualDimension dim;
    dim.id = "dim_001";
    dim.position = {100, 100};
    dim.text = "Test";
    table.dimensions.push_back(dim);

    // Try to move outside bounds
    Vec2 out_pos{-10, 100};
    bool fail = move_manual_dimension(sheet, table, "dim_001", out_pos);
    test("move outside bounds rejected", !fail);
    test("position unchanged", table.dimensions[0].position[0] == 100);
}

// Test 9: Reject move that causes conflict
static void test_move_manual_dimension_conflict() {
    DrawingSheet sheet = make_dummy_sheet();
    ManualDimensionTable table;

    // Add two dimensions
    Vec2 pos1{100, 100};
    Vec2 pos2{200, 200};
    std::string id1 = add_manual_dimension(sheet, table, pos1, "Dim1", false, "user");
    std::string id2 = add_manual_dimension(sheet, table, pos2, "Dim2", false, "user");

    test("two dimensions added", table.dimensions.size() == 2);

    // Try to move dim2 too close to dim1
    Vec2 conflict_pos{101, 101};
    bool fail = move_manual_dimension(sheet, table, id2, conflict_pos);
    test("move causing conflict rejected", !fail);
}

// Test 10: Validate dimension - checks position bounds
static void test_validate_dimension_position() {
    DrawingSheet sheet = make_dummy_sheet();
    ManualDimensionTable table;

    ManualDimension dim1;
    dim1.position = {10, 150};
    dim1.text = "Valid position";

    auto validation1 = validate_manual_dimension(dim1, sheet, table);
    test("valid position passes validation", validation1.valid);
    test("no out_of_bounds errors", validation1.out_of_bounds.empty());

    ManualDimension dim2;
    dim2.position = {-5, 150};
    dim2.text = "Invalid position";

    auto validation2 = validate_manual_dimension(dim2, sheet, table);
    test("invalid position fails validation", !validation2.valid);
    test("out_of_bounds error reported", validation2.out_of_bounds.size() > 0);
}

// Test 11: Reference dimension flag
static void test_reference_dimension() {
    DrawingSheet sheet = make_dummy_sheet();
    ManualDimensionTable table;

    Vec2 position{100, 150};
    std::string id = add_manual_dimension(sheet, table, position, "100.5", true, "user1");

    test("reference dimension added", !id.empty());
    test("reference flag is true", table.dimensions[0].reference == true);
}

// Test 12: Multiple dimensions without conflicts
static void test_add_multiple_dimensions() {
    DrawingSheet sheet = make_dummy_sheet();
    ManualDimensionTable table;

    // Add several dimensions with adequate spacing
    Vec2 positions[] = {{50, 50}, {100, 100}, {150, 150}, {200, 200}};
    int added = 0;

    for (auto& pos : positions) {
        std::string id = add_manual_dimension(sheet, table, pos, "Test", false, "user");
        if (!id.empty()) added++;
    }

    test("multiple dimensions added", added == 4);
    test("table has 4 dimensions", table.dimensions.size() == 4);
}

// Run all tests
int main() {
    test_add_manual_dimension_valid();
    test_add_manual_dimension_out_of_bounds();
    test_add_manual_dimension_conflict();
    test_find_manual_dimension();
    test_update_manual_dimension_text();
    test_remove_manual_dimension();
    test_move_manual_dimension_valid();
    test_move_manual_dimension_invalid();
    test_move_manual_dimension_conflict();
    test_validate_dimension_position();
    test_reference_dimension();
    test_add_multiple_dimensions();

    std::printf("\n========================================\n");
    std::printf("Tests run: %d\n", test_count);
    std::printf("Tests failed: %d\n", fail_count);
    std::printf("Tests passed: %d\n", test_count - fail_count);
    std::printf("========================================\n");

    return fail_count > 0 ? 1 : 0;
}
