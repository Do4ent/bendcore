// Geometric Parameter Extraction (Q3 2027, Task 1, Subtask 3)
// Extracts dimensions (OD, wall thickness, length, bends) from CAD geometry
#pragma once

#include <vector>
#include <string>
#include <cmath>

namespace bendcore::geometry {

// Extracted parameters from geometric analysis
struct ExtractedParams {
    double outer_diameter = 0.0;      // OD (outer diameter) in mm
    double inner_diameter = 0.0;      // ID = OD - 2*wall
    double wall_thickness = 0.0;      // Wall thickness in mm
    double total_length = 0.0;        // Total length in mm
    std::vector<double> bend_angles;  // Bend angles in degrees
    std::vector<double> bend_radii;   // Bend radii in mm
    double bend_arc_length = 0.0;     // Total arc length of bends in mm
    std::string shape_type;           // "TUBE", "PIPE", "BEND", "COMPLEX"
    double confidence = 0.0;          // 0.0-1.0 confidence level
    std::string analysis_notes;       // Additional analysis information

    // Validate extracted parameters
    bool is_valid() const {
        return outer_diameter > 0.001 && wall_thickness >= 0.001 &&
               total_length > 0.1 && confidence >= 0.5;
    }
};

// Geometry analyzer class
class GeometryAnalyzer {
public:
    GeometryAnalyzer();
    ~GeometryAnalyzer();

    // Analyze geometry and extract parameters
    // geometry_data: serialized shape data from STEP reader
    ExtractedParams analyze(const std::string& geometry_data);

    // Fit cylinder to geometry for OD/ID detection
    double fit_cylinder(const std::vector<std::pair<double, double>>& points);

    // Detect bends in geometry
    std::vector<double> detect_bends(const std::string& geometry_data);

    // Calculate bend angles from curvature data
    std::vector<double> calculate_bend_angles(const std::vector<double>& curvature_points);

    // Estimate confidence score based on analysis quality
    static double estimate_confidence(const ExtractedParams& params);

private:
    // Tolerance for dimension matching (in mm)
    static constexpr double TOLERANCE_MM = 0.01;

    // Minimum confidence to accept analysis results
    static constexpr double MIN_CONFIDENCE = 0.50;

    // Extract bounding box dimensions
    void extract_bounding_box(const std::string& geometry_data,
                             double& width, double& height, double& length);

    // Extract edge points for cylinder fitting
    std::vector<std::pair<double, double>> extract_edge_points(const std::string& geometry_data);

    // Calculate curvature at points along geometry
    std::vector<double> calculate_curvature(const std::vector<std::pair<double, double>>& points);
};

}  // namespace bendcore::geometry
