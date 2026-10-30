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

namespace {
GeometryBackend& backend_slot() {
    static GeometryBackend backend;
    return backend;
}
}  // namespace

void set_geometry_backend(GeometryBackend backend) { backend_slot() = std::move(backend); }
bool has_geometry_backend() { return static_cast<bool>(backend_slot()); }

ExtractedParams GeometryAnalyzer::params_from_features(const BRepTubeFeatures& f) {
    ExtractedParams params;
    if (!f.ok) {
        params.analysis_notes = "Geometry backend error: " + f.error;
        return params;
    }
    params.outer_diameter = 2.0 * f.outer_radius;
    if (f.inner_radius > 0.0 && f.inner_radius < f.outer_radius) {
        params.inner_diameter = 2.0 * f.inner_radius;
        params.wall_thickness = f.outer_radius - f.inner_radius;
    } else {
        params.inner_diameter = 0.0;
        params.wall_thickness = f.outer_radius;  // сплошной пруток: стенка = радиус
    }

    double straight = 0.0;
    for (double l : f.straight_lengths) straight += l;
    for (const auto& [angle_deg, radius] : f.bends) {
        params.bend_angles.push_back(angle_deg);
        params.bend_radii.push_back(radius);
        params.bend_arc_length += radius * angle_deg * M_PI / 180.0;
    }
    params.total_length = straight + params.bend_arc_length;  // длина по оси

    params.shape_type = params.bend_angles.empty() ? "TUBE"
                        : params.bend_angles.size() == 1 ? "BEND" : "COMPLEX";
    params.confidence = estimate_confidence(params);

    std::ostringstream notes;
    notes << "B-rep: " << f.straight_lengths.size() << " straight segment(s), " << f.bends.size()
          << " bend(s); bbox " << f.bbox_size[0] << " x " << f.bbox_size[1] << " x " << f.bbox_size[2];
    params.analysis_notes = notes.str();
    return params;
}

ExtractedParams GeometryAnalyzer::analyze(const std::string& geometry_data) {
    ExtractedParams params;
    if (geometry_data.empty()) {
        params.analysis_notes = "No geometry data provided";
        return params;
    }
    const GeometryBackend& backend = backend_slot();
    if (!backend) {
        params.analysis_notes =
            "Geometry backend not available: build with BENDCORE_WITH_OCCT=ON and call "
            "bendcore::occt::install_geometry_backend()";
        return params;
    }
    try {
        return params_from_features(backend(geometry_data));
    } catch (const std::exception& e) {
        params.analysis_notes = std::string("Analysis error: ") + e.what();
        return params;
    }
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
    return analyze(geometry_data).bend_angles;
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

    // Геометрическая полнота без учёта самого confidence (is_valid() требует confidence >= 0.5,
    // а здесь он ещё не вычислен).
    if (!(params.outer_diameter > 0.001 && params.wall_thickness >= 0.001 && params.total_length > 0.1)) {
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

    // Гибы по B-rep распознаются по тороидальным граням; сложнее формы — чуть ниже уверенность.
    if (params.bend_angles.size() > 0) {
        confidence *= 0.9;
    }

    // Ensure within bounds
    confidence = std::max(0.0, std::min(1.0, confidence));

    return confidence;
}

TubeAxisResult GeometryAnalyzer::find_tube_axis_pca(
    const std::vector<std::vector<double>>& points_3d) {

    TubeAxisResult result;

    // Need at least 3 points for PCA
    if (points_3d.size() < 3) {
        result.confidence = 0.0;
        result.axis_direction = {1.0, 0.0, 0.0};  // Default X axis
        result.axis_origin = {0.0, 0.0, 0.0};
        return result;
    }

    // Step 1: Calculate centroid (mean of all points)
    std::vector<double> centroid(3, 0.0);
    for (const auto& pt : points_3d) {
        if (pt.size() >= 3) {
            centroid[0] += pt[0];
            centroid[1] += pt[1];
            centroid[2] += pt[2];
        }
    }

    double n = static_cast<double>(points_3d.size());
    centroid[0] /= n;
    centroid[1] /= n;
    centroid[2] /= n;

    // Step 2: Build covariance matrix
    std::vector<std::vector<double>> cov(3, std::vector<double>(3, 0.0));

    for (const auto& pt : points_3d) {
        if (pt.size() >= 3) {
            double x = pt[0] - centroid[0];
            double y = pt[1] - centroid[1];
            double z = pt[2] - centroid[2];

            cov[0][0] += x * x;
            cov[0][1] += x * y;
            cov[0][2] += x * z;
            cov[1][1] += y * y;
            cov[1][2] += y * z;
            cov[2][2] += z * z;
        }
    }

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            cov[i][j] /= n;
            cov[j][i] = cov[i][j];  // Make symmetric
        }
    }

    // Step 3: Find dominant eigenvector using power iteration
    // Initialize with random vector
    std::vector<double> v = {1.0, 0.0, 0.0};

    // 10 power iterations
    for (int iter = 0; iter < 10; iter++) {
        // Compute Av
        std::vector<double> av(3, 0.0);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                av[i] += cov[i][j] * v[j];
            }
        }

        // Normalize
        double norm = std::sqrt(av[0] * av[0] + av[1] * av[1] + av[2] * av[2]);
        if (norm > 1e-10) {
            v[0] = av[0] / norm;
            v[1] = av[1] / norm;
            v[2] = av[2] / norm;
        }
    }

    // Set results
    result.axis_direction = v;
    result.axis_origin = centroid;

    // Calculate confidence based on point cloud linearity
    // If all points are close to the fitted axis, confidence is high
    double total_deviation = 0.0;
    for (const auto& pt : points_3d) {
        if (pt.size() >= 3) {
            // Distance from point to axis
            double dx = pt[0] - centroid[0];
            double dy = pt[1] - centroid[1];
            double dz = pt[2] - centroid[2];

            // Dot product with axis
            double dot = dx * v[0] + dy * v[1] + dz * v[2];

            // Perpendicular distance
            double perp_dist = std::sqrt(
                (dx - dot * v[0]) * (dx - dot * v[0]) +
                (dy - dot * v[1]) * (dy - dot * v[1]) +
                (dz - dot * v[2]) * (dz - dot * v[2])
            );

            total_deviation += perp_dist;
        }
    }

    double avg_deviation = total_deviation / n;

    // Confidence decreases with deviation
    // Assume typical tube radius, good confidence if deviation < 10% of radius
    result.confidence = 1.0 / (1.0 + avg_deviation / 1.0);  // Sigmoid-like
    result.confidence = std::max(0.0, std::min(1.0, result.confidence));

    return result;
}

}  // namespace bendcore::geometry
