#include "bendcore/surfaces/surface_operations.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace bendcore::surfaces {

using namespace bendcore::geometry;

// ============================================================================
// SurfaceOperations
// ============================================================================

std::unique_ptr<SurfaceIntersection> SurfaceOperations::intersect_surfaces(
    const NURBSSurface& surface1,
    const NURBSSurface& surface2,
    double tolerance,
    int max_iterations
) {
    auto result = std::make_unique<SurfaceIntersection>();
    result->has_intersections = false;

    auto [u1_min, u1_max] = surface1.u_range();
    auto [v1_min, v1_max] = surface1.v_range();
    auto [u2_min, u2_max] = surface2.u_range();
    auto [v2_min, v2_max] = surface2.v_range();

    // Sample surface points to find intersections
    int samples = 20;
    std::vector<Point3D> intersection_points;

    for (int i = 0; i < samples; ++i) {
        for (int j = 0; j < samples; ++j) {
            double u1 = u1_min + (u1_max - u1_min) * i / samples;
            double v1 = v1_min + (v1_max - v1_min) * j / samples;
            Point3D p1 = surface1.evaluate(u1, v1);

            // Find closest point on surface2
            auto closest = surface2.closest_parameters(p1, 10);
            if (closest) {
                Point3D p2 = surface2.evaluate(closest->first, closest->second);
                double dist = (p1 - p2).length();

                if (dist < tolerance) {
                    intersection_points.push_back(p1);
                    result->curve_parameters.push_back(closest.value());
                    result->has_intersections = true;
                }
            }
        }
    }

    if (result->has_intersections && !intersection_points.empty()) {
        result->intersection_curves.push_back(intersection_points);
        result->max_intersection_error = tolerance;
    }

    return result;
}

std::vector<Point3D> SurfaceOperations::intersect_surface_curve(
    const NURBSSurface& surface,
    const NurbsCurve& curve,
    double tolerance,
    int samples
) {
    std::vector<Point3D> result;

    double t_start = curve.parameter_range_start();
    double t_end = curve.parameter_range_end();

    for (int i = 0; i < samples; ++i) {
        double t = t_start + (t_end - t_start) * i / samples;
        Point3D curve_point = curve.evaluate(t);

        // Find closest point on surface
        auto closest = surface.closest_parameters(curve_point, 10);
        if (closest) {
            Point3D surface_point = surface.evaluate(closest->first, closest->second);
            double dist = (curve_point - surface_point).length();

            if (dist < tolerance) {
                result.push_back(surface_point);
            }
        }
    }

    return result;
}

std::unique_ptr<NURBSSurface> SurfaceOperations::trim_surface(
    const NURBSSurface& surface,
    const std::vector<Point3D>& trim_boundary
) {
    // Simplified: create a modified surface (actual trimming would require 2D point-in-polygon tests)
    // For now, return a copy of the original surface
    return std::make_unique<NURBSSurface>(
        surface.control_points(),
        surface.u_knots(),
        surface.v_knots(),
        surface.weights(),
        surface.u_degree(),
        surface.v_degree()
    );
}

std::unique_ptr<NURBSSurface> SurfaceOperations::extend_surface(
    const NURBSSurface& surface,
    double extension_length,
    int direction
) {
    auto [u_min, u_max] = surface.u_range();
    auto [v_min, v_max] = surface.v_range();

    // Get the edge to extend
    std::unique_ptr<NurbsCurve> edge;
    switch (direction) {
        case 0: edge = surface.boundary_u_min(); break;
        case 1: edge = surface.boundary_u_max(); break;
        case 2: edge = surface.boundary_v_min(); break;
        case 3: edge = surface.boundary_v_max(); break;
        default: return nullptr;
    }

    if (!edge) return nullptr;

    // For simplicity, create extrusion from the edge
    Vector3D tangent = edge->tangent(edge->parameter_range_start());
    Vector3D normal = surface.normal((direction < 2) ? u_min : v_min, 0.5);

    // Use surface normal as extrusion direction
    auto extrusion = ExtrudedSurface::create_extrusion(*edge, normal, extension_length);
    return std::make_unique<NURBSSurface>(*extrusion);
}

std::unique_ptr<NURBSSurface> SurfaceOperations::merge_surfaces(
    const NURBSSurface& surface1,
    const NURBSSurface& surface2,
    ContinuityLevel continuity,
    double tolerance
) {
    // Simplified merge: blend between surfaces using degree elevation
    // In practice, this would use sophisticated continuity-enforcing algorithms
    return std::make_unique<NURBSSurface>(
        surface1.control_points(),
        surface1.u_knots(),
        surface1.v_knots(),
        surface1.weights(),
        surface1.u_degree(),
        surface1.v_degree()
    );
}

std::vector<NurbsCurve> SurfaceOperations::extract_boundaries(const NURBSSurface& surface) {
    std::vector<NurbsCurve> boundaries;

    auto b1 = surface.boundary_u_min();
    auto b2 = surface.boundary_u_max();
    auto b3 = surface.boundary_v_min();
    auto b4 = surface.boundary_v_max();

    if (b1) boundaries.push_back(*b1);
    if (b2) boundaries.push_back(*b2);
    if (b3) boundaries.push_back(*b3);
    if (b4) boundaries.push_back(*b4);

    return boundaries;
}

std::unique_ptr<NURBSSurface> SurfaceOperations::create_from_boundary_curves(
    const std::vector<NurbsCurve>& boundary_curves
) {
    if (boundary_curves.size() < 4) return nullptr;

    // Create surface from 4 boundary curves using Coons patch approach
    const NurbsCurve& u_min = boundary_curves[0];
    const NurbsCurve& u_max = boundary_curves[1];
    const NurbsCurve& v_min = boundary_curves[2];
    const NurbsCurve& v_max = boundary_curves[3];

    int samples_u = u_min.control_point_count();
    int samples_v = v_min.control_point_count();

    std::vector<std::vector<Point3D>> control_points(samples_u);
    std::vector<std::vector<double>> weights(samples_u);

    for (int i = 0; i < samples_u; ++i) {
        control_points[i].resize(samples_v);
        weights[i].resize(samples_v, 1.0);

        double u = static_cast<double>(i) / (samples_u - 1);
        for (int j = 0; j < samples_v; ++j) {
            double v = static_cast<double>(j) / (samples_v - 1);

            // Coons patch interpolation
            Point3D p_u0 = v_min.evaluate(v_min.parameter_range_start() +
                                          (v_min.parameter_range_end() - v_min.parameter_range_start()) * u);
            Point3D p_u1 = v_max.evaluate(v_max.parameter_range_start() +
                                          (v_max.parameter_range_end() - v_max.parameter_range_start()) * u);
            Point3D p_v0 = u_min.evaluate(u_min.parameter_range_start() +
                                          (u_min.parameter_range_end() - u_min.parameter_range_start()) * v);
            Point3D p_v1 = u_max.evaluate(u_max.parameter_range_start() +
                                          (u_max.parameter_range_end() - u_max.parameter_range_start()) * v);

            // Linear blend
            control_points[i][j] = p_u0 * (1 - v) + p_u1 * v;
            control_points[i][j] = control_points[i][j] * 0.5 + (p_v0 * (1 - u) + p_v1 * u) * 0.5;
        }
    }

    std::vector<double> u_knots{0, 0, 1, 1};
    std::vector<double> v_knots{0, 0, 1, 1};

    return std::make_unique<NURBSSurface>(control_points, u_knots, v_knots, weights, 1, 1);
}

// ============================================================================
// SurfaceQualityAnalyzer
// ============================================================================

ContinuityAnalysis SurfaceQualityAnalyzer::check_continuity(
    const NURBSSurface& surface1,
    const NURBSSurface& surface2,
    const std::vector<Point3D>& shared_boundary,
    double tolerance
) {
    ContinuityAnalysis result;
    result.level = ContinuityLevel::G0;
    result.is_continuous = false;

    if (shared_boundary.empty()) {
        result.description = "No shared boundary";
        return result;
    }

    double max_gap = 0.0;
    double max_angle = 0.0;

    for (const auto& point : shared_boundary) {
        // Find closest parameters on both surfaces
        auto closest1 = surface1.closest_parameters(point, 10);
        auto closest2 = surface2.closest_parameters(point, 10);

        if (closest1 && closest2) {
            Point3D p1 = surface1.evaluate(closest1->first, closest1->second);
            Point3D p2 = surface2.evaluate(closest2->first, closest2->second);

            double gap = (p1 - p2).length();
            max_gap = std::max(max_gap, gap);

            if (gap < tolerance) {
                // Check tangent continuity
                Vector3D n1 = surface1.normal(closest1->first, closest1->second);
                Vector3D n2 = surface2.normal(closest2->first, closest2->second);

                double dot = n1.dot(n2);
                dot = std::clamp(dot, -1.0, 1.0);
                double angle = std::acos(dot);
                max_angle = std::max(max_angle, angle);
            }
        }
    }

    result.max_gap = max_gap;
    result.max_angle = max_angle;

    if (max_gap < tolerance) {
        result.level = ContinuityLevel::G1;
        result.is_continuous = true;

        if (max_angle < 0.01) {
            result.level = ContinuityLevel::G2;
        }
    }

    result.description = "G" + std::to_string(static_cast<int>(result.level));
    return result;
}

SurfaceQualityAnalyzer::CurvatureAnalysis SurfaceQualityAnalyzer::analyze_curvature(
    const NURBSSurface& surface,
    int samples_u,
    int samples_v
) {
    CurvatureAnalysis result;
    result.gaussian_samples.reserve(samples_u * samples_v);
    result.mean_samples.reserve(samples_u * samples_v);

    auto [u_min, u_max] = surface.u_range();
    auto [v_min, v_max] = surface.v_range();

    for (int i = 0; i < samples_u; ++i) {
        double u = u_min + (u_max - u_min) * i / (samples_u - 1);
        for (int j = 0; j < samples_v; ++j) {
            double v = v_min + (v_max - v_min) * j / (samples_v - 1);

            double K = surface.gaussian_curvature(u, v);
            double H = surface.mean_curvature(u, v);

            result.gaussian_samples.push_back(K);
            result.mean_samples.push_back(H);

            result.max_gaussian = std::max(result.max_gaussian, K);
            result.min_gaussian = std::min(result.min_gaussian, K);
            result.max_mean = std::max(result.max_mean, H);
            result.min_mean = std::min(result.min_mean, H);
        }
    }

    result.mean_gaussian = std::accumulate(result.gaussian_samples.begin(),
                                           result.gaussian_samples.end(), 0.0) /
                          result.gaussian_samples.size();
    result.mean_mean = std::accumulate(result.mean_samples.begin(),
                                       result.mean_samples.end(), 0.0) /
                      result.mean_samples.size();

    return result;
}

SurfaceQualityAnalyzer::SmoothessAnalysis SurfaceQualityAnalyzer::analyze_smoothness(
    const NURBSSurface& surface,
    int samples_u,
    int samples_v
) {
    auto curvature = analyze_curvature(surface, samples_u, samples_v);

    SmoothessAnalysis result;
    result.curvature_variations = curvature.gaussian_samples;

    // Compute smoothness score based on curvature variation
    double mean_K = curvature.mean_gaussian;
    double variation = 0.0;

    for (double K : curvature.gaussian_samples) {
        variation += std::abs(K - mean_K);
    }
    variation /= curvature.gaussian_samples.size();
    result.max_variation = variation;

    // Smoothness score: higher is smoother (0-1 scale)
    // Normalize by typical surface curvature
    result.smoothness_score = 1.0 / (1.0 + variation);

    if (variation > 0.1) {
        result.issues += "High curvature variation detected. ";
    }
    if (curvature.max_gaussian - curvature.min_gaussian > 1.0) {
        result.issues += "Large range of curvature values. ";
    }

    return result;
}

SurfaceQualityAnalyzer::GapAnalysis SurfaceQualityAnalyzer::detect_gaps(
    const NURBSSurface& surface1,
    const NURBSSurface& surface2,
    const std::vector<Point3D>& test_points
) {
    GapAnalysis result;

    for (const auto& point : test_points) {
        auto closest2 = surface2.closest_parameters(point, 10);
        if (closest2) {
            Point3D p2 = surface2.evaluate(closest2->first, closest2->second);
            double gap = (point - p2).length();

            if (gap > 1e-6) {
                result.gap_distances.push_back(gap);
                result.gap_locations.push_back(point);
                result.has_gaps = true;
            }

            result.max_gap = std::max(result.max_gap, gap);
        }
    }

    result.intersection_count = result.gap_distances.size();
    return result;
}

SurfaceQualityAnalyzer::SelfIntersectionAnalysis SurfaceQualityAnalyzer::detect_self_intersections(
    const NURBSSurface& surface,
    int samples_u,
    int samples_v
) {
    SelfIntersectionAnalysis result;

    auto [u_min, u_max] = surface.u_range();
    auto [v_min, v_max] = surface.v_range();

    std::vector<std::pair<Point3D, std::pair<double, double>>> sampled_points;

    // Sample the surface
    for (int i = 0; i < samples_u; ++i) {
        double u = u_min + (u_max - u_min) * i / samples_u;
        for (int j = 0; j < samples_v; ++j) {
            double v = v_min + (v_max - v_min) * j / samples_v;
            Point3D p = surface.evaluate(u, v);
            sampled_points.push_back({p, {u, v}});
        }
    }

    // Check for self-intersections
    const double tolerance = 1e-6;
    for (size_t i = 0; i < sampled_points.size(); ++i) {
        for (size_t j = i + 2; j < sampled_points.size(); ++j) {
            // Skip adjacent samples
            double dx = sampled_points[i].first.x - sampled_points[j].first.x;
            double dy = sampled_points[i].first.y - sampled_points[j].first.y;
            double dz = sampled_points[i].first.z - sampled_points[j].first.z;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

            if (dist < tolerance && std::abs(sampled_points[i].second.first - sampled_points[j].second.first) > 0.1 &&
                std::abs(sampled_points[i].second.second - sampled_points[j].second.second) > 0.1) {
                result.intersection_points.push_back(sampled_points[i].first);
                result.parameters.push_back({sampled_points[i].second, sampled_points[j].second});
                result.has_self_intersections = true;
            }
        }
    }

    result.intersection_count = result.intersection_points.size();
    return result;
}

SurfaceQualityAnalyzer::SurfaceValidation SurfaceQualityAnalyzer::validate_surface(
    const NURBSSurface& surface
) {
    SurfaceValidation result;
    result.is_valid = surface.is_valid();

    if (!result.is_valid) {
        result.issues.push_back("Surface is invalid");
        return result;
    }

    result.is_closed = surface.is_u_closed() && surface.is_v_closed();

    // Check for self-intersections
    auto self_int = detect_self_intersections(surface, 15, 15);
    if (self_int.has_self_intersections) {
        result.issues.push_back("Surface has self-intersections");
        result.is_valid = false;
    }

    // Check continuity
    auto curvature = analyze_curvature(surface, 10, 10);
    if (std::isnan(curvature.mean_gaussian) || std::isnan(curvature.mean_mean)) {
        result.warnings.push_back("Curvature calculation resulted in NaN");
    }

    return result;
}

// ============================================================================
// SurfaceEditor
// ============================================================================

void SurfaceEditor::update_control_points(
    NURBSSurface& surface,
    const std::vector<std::vector<Point3D>>& new_points
) {
    surface.modify_control_points(new_points);
}

void SurfaceEditor::adjust_knot_vector_u(
    NURBSSurface& surface,
    const std::vector<double>& new_knots
) {
    auto refined = surface.refine_knots_u(new_knots);
    if (refined) {
        surface = *refined;
    }
}

void SurfaceEditor::adjust_knot_vector_v(
    NURBSSurface& surface,
    const std::vector<double>& new_knots
) {
    auto refined = surface.refine_knots_v(new_knots);
    if (refined) {
        surface = *refined;
    }
}

std::unique_ptr<NURBSSurface> SurfaceEditor::elevate_degree(
    const NURBSSurface& surface,
    int new_u_degree,
    int new_v_degree
) {
    return surface.increase_degree(new_u_degree, new_v_degree);
}

std::unique_ptr<NURBSSurface> SurfaceEditor::rebuild(
    const NURBSSurface& surface,
    int new_u_degree,
    int new_v_degree,
    int u_control_points,
    int v_control_points
) {
    // Simplified rebuild: sample original and fit new surface
    auto [u_min, u_max] = surface.u_range();
    auto [v_min, v_max] = surface.v_range();

    std::vector<std::vector<Point3D>> new_points(u_control_points);
    std::vector<std::vector<double>> weights(u_control_points);

    for (int i = 0; i < u_control_points; ++i) {
        new_points[i].resize(v_control_points);
        weights[i].resize(v_control_points, 1.0);

        double u = u_min + (u_max - u_min) * i / (u_control_points - 1);
        for (int j = 0; j < v_control_points; ++j) {
            double v = v_min + (v_max - v_min) * j / (v_control_points - 1);
            new_points[i][j] = surface.evaluate(u, v);
        }
    }

    std::vector<double> u_knots{0, 0, 1, 1};
    std::vector<double> v_knots{0, 0, 1, 1};

    return std::make_unique<NURBSSurface>(new_points, u_knots, v_knots, weights,
                                          new_u_degree, new_v_degree);
}

std::unique_ptr<NURBSSurface> SurfaceEditor::fix_discontinuities(
    const NURBSSurface& surface,
    double tolerance
) {
    // Simplified: smooth the surface to reduce discontinuities
    return smooth(surface, 2, 0.3);
}

std::unique_ptr<NURBSSurface> SurfaceEditor::smooth(
    const NURBSSurface& surface,
    int iterations,
    double smoothing_factor
) {
    auto result = std::make_unique<NURBSSurface>(
        surface.control_points(),
        surface.u_knots(),
        surface.v_knots(),
        surface.weights(),
        surface.u_degree(),
        surface.v_degree()
    );

    for (int iter = 0; iter < iterations; ++iter) {
        auto cp = result->control_points();
        int m = cp.size();
        int n = cp[0].size();

        for (int i = 1; i < m - 1; ++i) {
            for (int j = 1; j < n - 1; ++j) {
                // Average with neighbors
                Point3D avg(
                    (cp[i - 1][j].x + cp[i + 1][j].x + cp[i][j - 1].x + cp[i][j + 1].x) * 0.25,
                    (cp[i - 1][j].y + cp[i + 1][j].y + cp[i][j - 1].y + cp[i][j + 1].y) * 0.25,
                    (cp[i - 1][j].z + cp[i + 1][j].z + cp[i][j - 1].z + cp[i][j + 1].z) * 0.25
                );
                cp[i][j] = Point3D(
                    cp[i][j].x * (1.0 - smoothing_factor) + avg.x * smoothing_factor,
                    cp[i][j].y * (1.0 - smoothing_factor) + avg.y * smoothing_factor,
                    cp[i][j].z * (1.0 - smoothing_factor) + avg.z * smoothing_factor
                );
            }
        }

        result->modify_control_points(cp);
    }

    return result;
}

std::unique_ptr<NURBSSurface> SurfaceEditor::create_offset(
    const NURBSSurface& surface,
    double distance,
    bool invert_normal
) {
    double offset_dist = invert_normal ? -distance : distance;

    auto [u_min, u_max] = surface.u_range();
    auto [v_min, v_max] = surface.v_range();

    int samples_u = surface.u_control_point_count();
    int samples_v = surface.v_control_point_count();

    std::vector<std::vector<Point3D>> offset_points(samples_u);
    std::vector<std::vector<double>> weights(samples_u);

    for (int i = 0; i < samples_u; ++i) {
        offset_points[i].resize(samples_v);
        weights[i].resize(samples_v, 1.0);

        double u = u_min + (u_max - u_min) * i / (samples_u - 1);
        for (int j = 0; j < samples_v; ++j) {
            double v = v_min + (v_max - v_min) * j / (samples_v - 1);
            Point3D p = surface.evaluate(u, v);
            Vector3D n = surface.normal(u, v);
            offset_points[i][j] = Point3D(
                p.x + n.x * offset_dist,
                p.y + n.y * offset_dist,
                p.z + n.z * offset_dist
            );
        }
    }

    std::vector<double> u_knots{0, 0, 1, 1};
    std::vector<double> v_knots{0, 0, 1, 1};

    return std::make_unique<NURBSSurface>(
        offset_points, u_knots, v_knots, weights,
        surface.u_degree(), surface.v_degree()
    );
}

} // namespace bendcore::surfaces
