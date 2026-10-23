// Parametric Dimension Builder Tests (Q3 2027, Task 1, Subtask 4)
#include <bendcore/parametric_builder.hpp>
#include <bendcore/geometry_analyzer.hpp>

#include <cassert>
#include <iostream>
#include <cmath>
#include <iomanip>

using namespace bendcore::parametric;
using namespace bendcore::geometry;

int test_od_dimension_created() {
    std::cout << "Test 1: OD dimension created... ";

    ParametricDimensionBuilder builder;
    DimensionFormula od = builder.create_od_dimension(25.4);

    assert(od.id.find("DIM_") == 0);
    assert(od.is_valid);
    assert(od.result > 0);
    assert(std::abs(od.result - 25.4) < 0.01);

    std::cout << "PASS (ID=" << od.id << ", Value=" << std::fixed << std::setprecision(2)
              << od.result << " mm)" << std::endl;
    return 0;
}

int test_wall_dimension_created() {
    std::cout << "Test 2: Wall dimension created... ";

    ParametricDimensionBuilder builder;
    DimensionFormula wall = builder.create_wall_dimension(2.11);

    assert(wall.id.find("DIM_") == 0);
    assert(wall.is_valid);
    assert(wall.result > 0);
    assert(std::abs(wall.result - 2.11) < 0.01);

    std::cout << "PASS (ID=" << wall.id << ", Value=" << std::fixed << std::setprecision(2)
              << wall.result << " mm)" << std::endl;
    return 0;
}

int test_id_formula_created_and_evaluated() {
    std::cout << "Test 3: ID formula created and evaluated... ";

    ParametricDimensionBuilder builder;

    DimensionFormula od = builder.create_od_dimension(25.4);
    DimensionFormula wall = builder.create_wall_dimension(2.11);
    DimensionFormula id = builder.create_id_formula(od, wall);

    assert(id.formula_text.find("OD") != std::string::npos);
    assert(id.formula_text.find("Wall") != std::string::npos);
    assert(id.is_valid);

    // ID should be OD - 2*Wall = 25.4 - 2*2.11 = 21.18
    double expected_id = 25.4 - 2.0 * 2.11;
    assert(std::abs(id.result - expected_id) < 0.01);

    std::cout << "PASS (ID = " << std::fixed << std::setprecision(2)
              << id.result << " mm)" << std::endl;
    return 0;
}

int test_length_dimension_created() {
    std::cout << "Test 4: Length dimension created... ";

    ParametricDimensionBuilder builder;
    DimensionFormula length = builder.create_length_dimension(150.0);

    assert(length.id.find("DIM_") == 0);
    assert(length.is_valid);
    assert(std::abs(length.result - 150.0) < 0.01);

    std::cout << "PASS (Value=" << std::fixed << std::setprecision(2)
              << length.result << " mm)" << std::endl;
    return 0;
}

int test_bend_angle_dimensions_created() {
    std::cout << "Test 5: Bend angle dimensions created... ";

    ParametricDimensionBuilder builder;

    DimensionFormula bend1 = builder.create_bend_angle_dimension(90.0, 0);
    DimensionFormula bend2 = builder.create_bend_angle_dimension(45.0, 1);

    assert(bend1.id.find("BendAngle_1") != std::string::npos);
    assert(bend2.id.find("BendAngle_2") != std::string::npos);
    assert(bend1.is_valid);
    assert(bend2.is_valid);
    assert(std::abs(bend1.result - 90.0) < 0.01);
    assert(std::abs(bend2.result - 45.0) < 0.01);

    std::cout << "PASS (Bend1=" << std::fixed << std::setprecision(1)
              << bend1.result << "°, Bend2=" << bend2.result << "°)" << std::endl;
    return 0;
}

int test_bend_radius_dimensions_created() {
    std::cout << "Test 6: Bend radius dimensions created... ";

    ParametricDimensionBuilder builder;

    DimensionFormula radius1 = builder.create_bend_radius_dimension(50.0, 0);
    DimensionFormula radius2 = builder.create_bend_radius_dimension(35.0, 1);

    assert(radius1.id.find("BendRadius_1") != std::string::npos);
    assert(radius2.id.find("BendRadius_2") != std::string::npos);
    assert(radius1.is_valid);
    assert(radius2.is_valid);
    assert(std::abs(radius1.result - 50.0) < 0.01);
    assert(std::abs(radius2.result - 35.0) < 0.01);

    std::cout << "PASS (Radius1=" << std::fixed << std::setprecision(1)
              << radius1.result << " mm, Radius2=" << radius2.result << " mm)" << std::endl;
    return 0;
}

int test_all_formulas_validate() {
    std::cout << "Test 7: All formulas validate... ";

    ParametricDimensionBuilder builder;

    // Create a dimension set
    ExtractedParams params;
    params.outer_diameter = 25.4;
    params.wall_thickness = 2.11;
    params.total_length = 150.0;
    params.bend_angles = {90.0};
    params.bend_radii = {50.0};
    params.shape_type = "BEND";
    params.confidence = 0.9;

    DimensionSet dim_set = builder.build(params, "test_file.step", "test_author");

    // All dimensions should be valid
    assert(dim_set.dimensions.size() >= 4);  // At least OD, Wall, Length, ID

    for (const auto& dim : dim_set.dimensions) {
        assert(dim.is_valid);
        assert(!dim.formula_text.empty());
    }

    std::cout << "PASS (Created " << dim_set.dimensions.size() << " dimensions)" << std::endl;
    return 0;
}

int test_timestamp_and_author_set_correctly() {
    std::cout << "Test 8: Timestamp and author set correctly... ";

    ParametricDimensionBuilder builder;

    ExtractedParams params;
    params.outer_diameter = 25.4;
    params.wall_thickness = 2.11;
    params.total_length = 150.0;
    params.bend_angles = {};
    params.bend_radii = {};

    std::string author = "TestAuthor";
    DimensionSet dim_set = builder.build(params, "test.step", author);

    assert(dim_set.author == author);
    assert(!dim_set.creation_timestamp.empty());
    assert(dim_set.creation_timestamp.find("T") != std::string::npos);  // ISO 8601 format

    // All dimensions should have the same author and timestamp
    for (const auto& dim : dim_set.dimensions) {
        assert(dim.author == author);
        assert(dim.created_at == dim_set.creation_timestamp);
    }

    std::cout << "PASS (Author=" << author << ", Timestamp=" << dim_set.creation_timestamp << ")" << std::endl;
    return 0;
}

int main() {
    std::cout << "=== Parametric Dimension Builder Tests ===" << std::endl;

    int failed = 0;

    try {
        failed += test_od_dimension_created();
        failed += test_wall_dimension_created();
        failed += test_id_formula_created_and_evaluated();
        failed += test_length_dimension_created();
        failed += test_bend_angle_dimensions_created();
        failed += test_bend_radius_dimensions_created();
        failed += test_all_formulas_validate();
        failed += test_timestamp_and_author_set_correctly();
    } catch (const std::exception& e) {
        std::cout << "FATAL ERROR: " << e.what() << std::endl;
        failed++;
    } catch (...) {
        std::cout << "FATAL ERROR: Unknown exception" << std::endl;
        failed++;
    }

    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Total tests: 8" << std::endl;
    std::cout << "Failed: " << failed << std::endl;
    std::cout << "Passed: " << (8 - failed) << std::endl;

    return failed;
}
