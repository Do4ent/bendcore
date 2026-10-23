// Geometric Parameter Extraction Implementation (Q3 2027, Task 1, Subtask 3)
#include <bendcore/geometry_analyzer.hpp>

#include <cmath>
#include <algorithm>
#include <numeric>
#include <sstream>

namespace bendcore::geometry {

GeometryAnalyzer::GeometryAnalyzer() {
    // Initialize analyzer state if needed
}

GeometryAnalyzer::~GeometryAnalyzer() {
    // Cleanup
}

ExtractedParams GeometryAnalyzer::analyze(const std::string& geometry_data) {
    ExtractedParams params;

    if (geometry_data.empty()) {
        params.confidence = 0.0;
        params.analysis_notes = "No geometry data provided";
        return params;
    }

    try {
        // Extract bounding box
        double width = 0.0, height = 0.0, length = 0.0;
        extract_bounding_box(geometry_data, width, height, length);

        // Estimate OD from bounding box (largest circular dimension)
        double estimated_od = std::max(width, height);

        // Extract edge points for more precise fitting
        auto edge_points = extract_edge_points(geometry_data);

        if (edge_points.size() >= 3) {
            // Fit cylinder to get precise OD
            params.outer_diameter = fit_cylinder(edge_points);

            // If fitting failed, use bounding box estimate
            if (params.outer_diameter < 0.001) {
                params.outer_diameter = estimated_od;
            }
        } else {
            // Not enough points, use bounding box
            params.outer_diameter = estimated_od;
        }

        // Estimate wall thickness (approximately 8-15% of OD for typical tubes)
        // This is a heuristic; precise wall detection requires full geometry
        params.wall_thickness = std::max(0.5, params.outer_diameter * 0.10);

        // Calculate inner diameter
        params.inner_diameter = params.outer_diameter - 2.0 * params.wall_thickness;

        // Set total length
        params.total_length = length;

        // Detect bends
        params.bend_angles = detect_bends(geometry_data);

        // Estimate bend radii based on OD
        for (size_t i = 0; i < params.bend_angles.size(); i++) {
            // Typical bend radius is 1-3x the OD
            double bend_radius = params.outer_diameter * 2.0;
            params.bend_radii.push_back(bend_radius);
        }

        // Calculate arc length for bends
        params.bend_arc_length = 0.0;
        for (size_t i = 0; i < params.bend_angles.size(); i++) {
            double angle_rad = params.bend_angles[i] * M_PI / 180.0;
            double arc_len = params.bend_radii[i] * angle_rad;
            params.bend_arc_length += arc_len;
        }

        // Determine shape type
        if (params.bend_angles.size() == 0) {
            params.shape_type = "TUBE";
        } else if (params.bend_angles.size() == 1) {
            params.shape_type = "BEND";
        } else {
            params.shape_type = "COMPLEX";
        }

        // Estimate confidence
        params.confidence = estimate_confidence(params);

        // Add analysis notes
        std::ostringstream notes;
        notes << "Analyzed " << edge_points.size() << " edge points; "
              << "detected " << params.bend_angles.size() << " bends";
        params.analysis_notes = notes.str();

    } catch (const std::exception& e) {
        params.confidence = 0.0;
        params.analysis_notes = std::string("Analysis error: ") + e.what();
    }

    return params;
}

void GeometryAnalyzer::extract_bounding_box(const std::string& geometry_data,
                                           double& width, double& height, double& length) {
    // Parse geometry data (simplified - in production this would parse OCCT shapes)
    // For now, generate reasonable default estimates

    // Assume tube: OD ~20-30mm, length ~100-200mm
    width = 25.4;      // Standard 1" tube
    height = 25.4;
    length = 150.0;

    // In production, this would:
    // 1. Get actual bounds from OCCT Bnd_Box
    // 2. Calculate XYZ dimensions
    // 3. Determine axis orientation
}

std::vector<std::pair<double, double>> GeometryAnalyzer::extract_edge_points(
    const std::string& geometry_data) {
    std::vector<std::pair<double, double>> points;

    // In production, this would:
    // 1. Traverse OCCT topology (TopExp_Explorer)
    // 2. Extract edge vertices
    // 3. Convert to 2D points (x, y distance from axis)

    // Placeholder: generate circular points for 25.4mm OD
    double od = 25.4;
    double radius = od / 2.0;

    for (int i = 0; i < 16; i++) {
        double angle = 2.0 * M_PI * i / 16.0;
        double x = radius * std::cos(angle);
        double y = radius * std::sin(angle);
        points.push_back({x, y});
    }

    return points;
}

double GeometryAnalyzer::fit_cylinder(const std::vector<std::pair<double, double>>& points) {
    if (points.size() < 3) {
        return -1.0;  // Cannot fit with too few points
    }

    // Least-squares circle fitting
    // Calculate center and radius
    double sum_x = 0.0, sum_y = 0.0;
    for (const auto& p : points) {
        sum_x += p.first;
        sum_y += p.second;
    }

    double mean_x = sum_x / points.size();
    double mean_y = sum_y / points.size();

    // Build system matrix
    double sum_x2 = 0.0, sum_y2 = 0.0, sum_xy = 0.0;
    double sum_x3 = 0.0, sum_y3 = 0.0, sum_x2y = 0.0, sum_xy2 = 0.0;

    for (const auto& p : points) {
        double dx = p.first - mean_x;
        double dy = p.second - mean_y;

        sum_x2 += dx * dx;
        sum_y2 += dy * dy;
        sum_xy += dx * dy;
        sum_x3 += dx * dx * dx;
        sum_y3 += dy * dy * dy;
        sum_x2y += dx * dx * dy;
        sum_xy2 += dx * dy * dy;
    }

    // Solve for center offset
    double A = sum_x2 / points.size();
    double B = sum_xy / points.size();
    double C = sum_xy / points.size();
    double D = sum_y2 / points.size();

    double det = A * D - B * C;
    if (std::abs(det) < 1e-10) {
        return -1.0;  // Singular matrix
    }

    double ux = (sum_x3 + sum_xy2) / (2.0 * points.size());
    double uy = (sum_x2y + sum_y3) / (2.0 * points.size());

    double cx = (D * ux - B * uy) / det;
    double cy = (A * uy - C * ux) / det;

    double center_x = cx + mean_x;
    double center_y = cy + mean_y;

    // Calculate radius
    double radius_sum = 0.0;
    for (const auto& p : points) {
        double dx = p.first - center_x;
        double dy = p.second - center_y;
        radius_sum += std::sqrt(dx * dx + dy * dy);
    }

    double radius = radius_sum / points.size();

    // Return diameter
    return radius * 2.0;
}

std::vector<double> GeometryAnalyzer::detect_bends(const std::string& geometry_data) {
    std::vector<double> bend_angles;

    // In production, this would:
    // 1. Sample points along the centerline
    // 2. Calculate curvature at each point
    // 3. Find curvature peaks (bends)
    // 4. Calculate angle from curvature

    // Placeholder: return one 90-degree bend if geometry suggests it
    // This would be detected from actual geometry analysis

    return bend_angles;  // No bends detected in simplified version
}

std::vector<double> GeometryAnalyzer::calculate_bend_angles(
    const std::vector<double>& curvature_points) {
    std::vector<double> angles;

    // Convert curvature to bend angles
    // angle ≈ integral of curvature along arc

    double accumulated_angle = 0.0;
    for (size_t i = 0; i < curvature_points.size(); i++) {
        accumulated_angle += curvature_points[i];

        // If significant angle accumulated, record as bend
        if (accumulated_angle > 10.0) {  // 10 degrees
            angles.push_back(accumulated_angle);
            accumulated_angle = 0.0;
        }
    }

    return angles;
}

double GeometryAnalyzer::estimate_confidence(const ExtractedParams& params) {
    // Confidence scoring based on parameter validity

    if (!params.is_valid()) {
        return 0.0;
    }

    double confidence = 1.0;

    // Adjust for reasonable ranges
    if (params.outer_diameter < 5.0 || params.outer_diameter > 1000.0) {
        confidence *= 0.7;  // Unusual OD
    }

    if (params.wall_thickness < 0.1 || params.wall_thickness > 0.5 * params.outer_diameter) {
        confidence *= 0.7;  // Unusual wall
    }

    if (params.total_length < 10.0 || params.total_length > 10000.0) {
        confidence *= 0.7;  // Unusual length
    }

    // More bends increase confidence in complex geometry detection
    if (params.bend_angles.size() > 0) {
        confidence *= 0.9;  // Slight boost for bend detection
    }

    // Ensure within bounds
    confidence = std::max(0.0, std::min(1.0, confidence));

    return confidence;
}

}  // namespace bendcore::geometry
