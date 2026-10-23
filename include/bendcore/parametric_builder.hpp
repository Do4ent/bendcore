// Parametric Dimension Builder (Q3 2027, Task 1, Subtask 4)
// Creates DimensionFormula objects from extracted geometry parameters
#pragma once

#include <bendcore/geometry_analyzer.hpp>
#include <bendcore/formula.hpp>

#include <vector>
#include <string>
#include <map>

namespace bendcore::parametric {

using DimensionFormula = bendcore::formula::DimensionFormula;

// Set of parametric dimensions created from geometry analysis
struct DimensionSet {
    std::vector<DimensionFormula> dimensions;
    std::map<std::string, double> extracted_values;
    std::string source_file;
    std::string creation_timestamp;
    std::string author;
};

// Builder for parametric dimensions
class ParametricDimensionBuilder {
public:
    ParametricDimensionBuilder();
    ~ParametricDimensionBuilder();

    // Build dimension set from extracted geometry parameters
    DimensionSet build(
        const bendcore::geometry::ExtractedParams& params,
        const std::string& source_file,
        const std::string& author
    );

    // Create individual dimension formulas
    DimensionFormula create_od_dimension(double value);
    DimensionFormula create_wall_dimension(double value);
    DimensionFormula create_length_dimension(double value);
    DimensionFormula create_bend_angle_dimension(double value, int bend_index);
    DimensionFormula create_bend_radius_dimension(double value, int bend_index);
    DimensionFormula create_id_formula(const DimensionFormula& od,
                                      const DimensionFormula& wall);

    // Validate all formulas in a set
    bool validate_dimensions(const DimensionSet& dim_set);

private:
    // Generate unique ID for dimension
    static std::string generate_dimension_id(const std::string& dimension_name);

    // Get current ISO 8601 timestamp
    static std::string get_iso_timestamp();

    // Counter for unique dimension IDs
    static int dimension_counter_;
};

}  // namespace bendcore::parametric
