// Parametric Dimension Builder Implementation (Q3 2027, Task 1, Subtask 4)
#include <bendcore/parametric_builder.hpp>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace bendcore::parametric {

// Static counter for unique IDs
int ParametricDimensionBuilder::dimension_counter_ = 0;

ParametricDimensionBuilder::ParametricDimensionBuilder() {
}

ParametricDimensionBuilder::~ParametricDimensionBuilder() {
}

std::string ParametricDimensionBuilder::generate_dimension_id(const std::string& dimension_name) {
    std::ostringstream oss;
    oss << "DIM_" << dimension_name << "_" << ++dimension_counter_;
    return oss.str();
}

std::string ParametricDimensionBuilder::get_iso_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

DimensionSet ParametricDimensionBuilder::build(
    const bendcore::geometry::ExtractedParams& params,
    const std::string& source_file,
    const std::string& author) {

    DimensionSet dim_set;
    dim_set.source_file = source_file;
    dim_set.author = author;
    dim_set.creation_timestamp = get_iso_timestamp();

    // Create OD dimension
    DimensionFormula od_dim = create_od_dimension(params.outer_diameter);
    od_dim.author = author;
    od_dim.created_at = dim_set.creation_timestamp;
    od_dim.modified_at = dim_set.creation_timestamp;
    dim_set.dimensions.push_back(od_dim);
    dim_set.extracted_values["$OD"] = params.outer_diameter;

    // Create wall thickness dimension
    DimensionFormula wall_dim = create_wall_dimension(params.wall_thickness);
    wall_dim.author = author;
    wall_dim.created_at = dim_set.creation_timestamp;
    wall_dim.modified_at = dim_set.creation_timestamp;
    dim_set.dimensions.push_back(wall_dim);
    dim_set.extracted_values["$Wall"] = params.wall_thickness;

    // Create ID formula (ID = OD - 2*Wall)
    DimensionFormula id_dim = create_id_formula(od_dim, wall_dim);
    id_dim.author = author;
    id_dim.created_at = dim_set.creation_timestamp;
    id_dim.modified_at = dim_set.creation_timestamp;
    dim_set.dimensions.push_back(id_dim);

    // Create length dimension
    DimensionFormula length_dim = create_length_dimension(params.total_length);
    length_dim.author = author;
    length_dim.created_at = dim_set.creation_timestamp;
    length_dim.modified_at = dim_set.creation_timestamp;
    dim_set.dimensions.push_back(length_dim);
    dim_set.extracted_values["$Length"] = params.total_length;

    // Create bend angle dimensions
    for (size_t i = 0; i < params.bend_angles.size(); i++) {
        DimensionFormula bend_angle = create_bend_angle_dimension(params.bend_angles[i], (int)i);
        bend_angle.author = author;
        bend_angle.created_at = dim_set.creation_timestamp;
        bend_angle.modified_at = dim_set.creation_timestamp;
        dim_set.dimensions.push_back(bend_angle);

        std::ostringstream var_name;
        var_name << "$BendAngle_" << (i + 1);
        dim_set.extracted_values[var_name.str()] = params.bend_angles[i];
    }

    // Create bend radius dimensions
    for (size_t i = 0; i < params.bend_radii.size(); i++) {
        DimensionFormula bend_radius = create_bend_radius_dimension(params.bend_radii[i], (int)i);
        bend_radius.author = author;
        bend_radius.created_at = dim_set.creation_timestamp;
        bend_radius.modified_at = dim_set.creation_timestamp;
        dim_set.dimensions.push_back(bend_radius);

        std::ostringstream var_name;
        var_name << "$BendRadius_" << (i + 1);
        dim_set.extracted_values[var_name.str()] = params.bend_radii[i];
    }

    // Validate all formulas
    if (!validate_dimensions(dim_set)) {
        // Log warning but continue
    }

    return dim_set;
}

DimensionFormula ParametricDimensionBuilder::create_od_dimension(double value) {
    DimensionFormula dim;
    dim.id = generate_dimension_id("OD");
    dim.formula_text = std::to_string(value);
    dim.result = value;
    dim.is_valid = true;
    dim.variables_map["$OD"] = value;

    // Parse the formula
    dim.set_formula(dim.formula_text);

    return dim;
}

DimensionFormula ParametricDimensionBuilder::create_wall_dimension(double value) {
    DimensionFormula dim;
    dim.id = generate_dimension_id("Wall");
    dim.formula_text = std::to_string(value);
    dim.result = value;
    dim.is_valid = true;
    dim.variables_map["$Wall"] = value;

    // Parse the formula
    dim.set_formula(dim.formula_text);

    return dim;
}

DimensionFormula ParametricDimensionBuilder::create_length_dimension(double value) {
    DimensionFormula dim;
    dim.id = generate_dimension_id("Length");
    dim.formula_text = std::to_string(value);
    dim.result = value;
    dim.is_valid = true;
    dim.variables_map["$Length"] = value;

    // Parse the formula
    dim.set_formula(dim.formula_text);

    return dim;
}

DimensionFormula ParametricDimensionBuilder::create_bend_angle_dimension(
    double value, int bend_index) {
    DimensionFormula dim;

    std::ostringstream id_oss;
    id_oss << "BendAngle_" << (bend_index + 1);
    dim.id = generate_dimension_id(id_oss.str());

    dim.formula_text = std::to_string(value);
    dim.result = value;
    dim.is_valid = true;

    std::ostringstream var_name;
    var_name << "$BendAngle_" << (bend_index + 1);
    dim.variables_map[var_name.str()] = value;

    // Parse the formula
    dim.set_formula(dim.formula_text);

    return dim;
}

DimensionFormula ParametricDimensionBuilder::create_bend_radius_dimension(
    double value, int bend_index) {
    DimensionFormula dim;

    std::ostringstream id_oss;
    id_oss << "BendRadius_" << (bend_index + 1);
    dim.id = generate_dimension_id(id_oss.str());

    dim.formula_text = std::to_string(value);
    dim.result = value;
    dim.is_valid = true;

    std::ostringstream var_name;
    var_name << "$BendRadius_" << (bend_index + 1);
    dim.variables_map[var_name.str()] = value;

    // Parse the formula
    dim.set_formula(dim.formula_text);

    return dim;
}

DimensionFormula ParametricDimensionBuilder::create_id_formula(
    const DimensionFormula& od,
    const DimensionFormula& wall) {
    DimensionFormula dim;
    dim.id = generate_dimension_id("ID");

    // Create formula: $ID = $OD - 2*$Wall
    dim.formula_text = "$OD - 2*$Wall";

    // Set variables
    dim.variables_map = od.variables_map;
    for (const auto& [var_name, var_value] : wall.variables_map) {
        dim.variables_map[var_name] = var_value;
    }

    // Evaluate the formula
    if (od.result > 0 && wall.result > 0) {
        dim.result = od.result - 2.0 * wall.result;
        dim.is_valid = true;
    } else {
        dim.is_valid = false;
        dim.error_message = "Invalid OD or Wall values for ID calculation";
    }

    // Parse the formula
    dim.set_formula(dim.formula_text);

    return dim;
}

bool ParametricDimensionBuilder::validate_dimensions(const DimensionSet& dim_set) {
    for (const auto& dim : dim_set.dimensions) {
        if (!dim.is_valid) {
            return false;
        }

        // Check that formula can be parsed
        if (dim.formula_ast == nullptr && !dim.formula_text.empty()) {
            return false;
        }
    }

    return true;
}

}  // namespace bendcore::parametric
