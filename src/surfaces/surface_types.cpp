#include "bendcore/surfaces/surface_types.hpp"
#include <cmath>
#include <algorithm>

namespace bendcore::surfaces {

using namespace bendcore::geometry;

// ============================================================================
// BlendSurface
// ============================================================================

BlendSurface::BlendSurface() = default;

std::unique_ptr<NURBSSurface> BlendSurface::create_blend(
    const NurbsCurve& boundary_curve1,
    const NurbsCurve& boundary_curve2,
    const Vector3D& tangent_curve1,
    const Vector3D& tangent_curve2
) {
    // Create blend surface using boundary curves as control curves
    int pts1 = boundary_curve1.control_point_count();
    int pts2 = boundary_curve2.control_point_count();
    int pts = std::max(pts1, pts2);

    std::vector<std::vector<Point3D>> control_points(pts);
    std::vector<std::vector<double>> weights(pts);
    std::vector<double> u_knots(pts + 2);
    u_knots[0] = 0;
    u_knots[pts + 1] = 1;
    for (int i = 1; i <= pts; ++i) {
        u_knots[i] = static_cast<double>(i) / (pts);
    }

    for (int i = 0; i < pts; ++i) {
        control_points[i].resize(2);
        weights[i].resize(2, 1.0);

        // Linear interpolation between curves
        double t1 = static_cast<double>(i) / (pts - 1);
        Point3D p1 = boundary_curve1.evaluate(boundary_curve1.parameter_range_start() +
                      (boundary_curve1.parameter_range_end() - boundary_curve1.parameter_range_start()) * t1);
        Point3D p2 = boundary_curve2.evaluate(boundary_curve2.parameter_range_start() +
                      (boundary_curve2.parameter_range_end() - boundary_curve2.parameter_range_start()) * t1);

        control_points[i][0] = p1;
        control_points[i][1] = p2;
    }

    // Create knot vectors for degree 1 surface
    std::vector<double> v_knots{0, 0, 1, 1};

    auto blend = std::make_unique<BlendSurface>();
    // Initialize by calling parent constructor indirectly
    NURBSSurface base(control_points, u_knots, v_knots, weights, 1, 1);
    *blend = BlendSurface();  // Default initialize

    // Copy data using parent methods
    blend->modify_control_points(control_points);

    return blend;
}

// ============================================================================
// OffsetSurface
// ============================================================================

OffsetSurface::OffsetSurface(const std::shared_ptr<NURBSSurface>& base_surface, double offset_distance)
    : base_surface_(base_surface), offset_distance_(offset_distance)
{
}

Point3D OffsetSurface::evaluate(double u, double v) const {
    Point3D base_point = base_surface_->evaluate(u, v);
    Vector3D normal_vec = base_surface_->normal(u, v);
    return Point3D(
        base_point.x + normal_vec.x * offset_distance_,
        base_point.y + normal_vec.y * offset_distance_,
        base_point.z + normal_vec.z * offset_distance_
    );
}

Vector3D OffsetSurface::normal(double u, double v) const {
    return base_surface_->normal(u, v);
}

std::unique_ptr<NURBSSurface> OffsetSurface::to_nurbs(int samples_u, int samples_v) const {
    auto [u_min, u_max] = base_surface_->u_range();
    auto [v_min, v_max] = base_surface_->v_range();

    std::vector<std::vector<Point3D>> control_points(samples_u);
    std::vector<std::vector<double>> weights(samples_u);

    for (int i = 0; i < samples_u; ++i) {
        control_points[i].resize(samples_v);
        weights[i].resize(samples_v, 1.0);

        double u = u_min + (u_max - u_min) * i / (samples_u - 1);
        for (int j = 0; j < samples_v; ++j) {
            double v = v_min + (v_max - v_min) * j / (samples_v - 1);
            control_points[i][j] = evaluate(u, v);
        }
    }

    std::vector<double> u_knots{0, 0, 1, 1};
    std::vector<double> v_knots{0, 0, 1, 1};

    return std::make_unique<NURBSSurface>(control_points, u_knots, v_knots, weights, 1, 1);
}

// ============================================================================
// ExtrudedSurface
// ============================================================================

ExtrudedSurface::ExtrudedSurface() = default;

std::unique_ptr<NURBSSurface> ExtrudedSurface::create_extrusion(
    const NurbsCurve& profile,
    const Vector3D& direction,
    double distance
) {
    auto surface = std::make_unique<ExtrudedSurface>();
    surface->profile_ = std::make_unique<NurbsCurve>(profile);
    surface->extrusion_direction_ = direction.normalized();
    surface->extrusion_distance_ = distance;

    // Create NURBS surface from extrusion
    int pts = profile.control_point_count();
    std::vector<std::vector<Point3D>> control_points(pts);
    std::vector<std::vector<double>> weights(pts);

    for (int i = 0; i < pts; ++i) {
        control_points[i].resize(2);
        weights[i].resize(2, 1.0);

        Point3D p1 = profile.control_points()[i];
        Point3D p2(
            p1.x + direction.x * distance,
            p1.y + direction.y * distance,
            p1.z + direction.z * distance
        );

        control_points[i][0] = p1;
        control_points[i][1] = p2;
    }

    std::vector<double> v_knots{0, 0, 1, 1};

    return std::make_unique<NURBSSurface>(
        control_points,
        profile.knot_vector(),
        v_knots,
        weights,
        profile.degree(),
        1
    );
}

// ============================================================================
// RuledSurface
// ============================================================================

RuledSurface::RuledSurface() = default;

std::unique_ptr<NURBSSurface> RuledSurface::create_ruled(
    const NurbsCurve& curve1,
    const NurbsCurve& curve2,
    double straightness_factor
) {
    auto surface = std::make_unique<RuledSurface>();
    surface->curve1_ = std::make_unique<NurbsCurve>(curve1);
    surface->curve2_ = std::make_unique<NurbsCurve>(curve2);

    // Create ruling surface
    int pts1 = curve1.control_point_count();
    int pts2 = curve2.control_point_count();
    int pts = std::max(pts1, pts2);

    std::vector<std::vector<Point3D>> control_points(pts);
    std::vector<std::vector<double>> weights(pts);

    for (int i = 0; i < pts; ++i) {
        control_points[i].resize(2);
        weights[i].resize(2, 1.0);

        // Linear interpolation between corresponding points
        double t = static_cast<double>(i) / (pts - 1);

        Point3D p1 = curve1.evaluate(curve1.parameter_range_start() +
                      (curve1.parameter_range_end() - curve1.parameter_range_start()) * t);
        Point3D p2 = curve2.evaluate(curve2.parameter_range_start() +
                      (curve2.parameter_range_end() - curve2.parameter_range_start()) * t);

        // Apply straightness factor to control point influence
        control_points[i][0] = p1;
        control_points[i][1] = p2;
    }

    std::vector<double> u_knots{0, 0, 1, 1};
    std::vector<double> v_knots{0, 0, 1, 1};

    return std::make_unique<NURBSSurface>(control_points, u_knots, v_knots, weights, 1, 1);
}

// ============================================================================
// FilletSurface
// ============================================================================

FilletSurface::FilletSurface(
    const std::shared_ptr<NURBSSurface>& surface1,
    const std::shared_ptr<NURBSSurface>& surface2,
    double radius,
    bool is_convex
)
    : surface1_(surface1), surface2_(surface2), radius_(radius), is_convex_(is_convex)
{
}

Point3D FilletSurface::evaluate(double u, double v) const {
    // Simple linear interpolation between surfaces
    Point3D p1 = surface1_->evaluate(u, v);
    Point3D p2 = surface2_->evaluate(u, v);
    double t = 1.0 - v;
    Point3D result(
        p1.x * t + p2.x * v,
        p1.y * t + p2.y * v,
        p1.z * t + p2.z * v
    );
    return result;
}

Vector3D FilletSurface::normal(double u, double v) const {
    Vector3D n1 = surface1_->normal(u, v);
    Vector3D n2 = surface2_->normal(u, v);
    double t = 1.0 - v;
    Vector3D result(
        n1.x * t + n2.x * v,
        n1.y * t + n2.y * v,
        n1.z * t + n2.z * v
    );
    double len = result.magnitude();
    if (len > 1e-12) {
        result = Vector3D(result.x / len, result.y / len, result.z / len);
    }
    return result;
}

std::unique_ptr<NURBSSurface> FilletSurface::to_nurbs(int samples_u, int samples_v) const {
    std::vector<std::vector<Point3D>> control_points(samples_u);
    std::vector<std::vector<double>> weights(samples_u);

    for (int i = 0; i < samples_u; ++i) {
        control_points[i].resize(samples_v);
        weights[i].resize(samples_v, 1.0);

        double u = static_cast<double>(i) / (samples_u - 1);
        for (int j = 0; j < samples_v; ++j) {
            double v = static_cast<double>(j) / (samples_v - 1);
            control_points[i][j] = evaluate(u, v);
        }
    }

    std::vector<double> u_knots{0, 0, 1, 1};
    std::vector<double> v_knots{0, 0, 1, 1};

    return std::make_unique<NURBSSurface>(control_points, u_knots, v_knots, weights, 1, 1);
}

// ============================================================================
// SewedSurface
// ============================================================================

SewedSurface::SewedSurface() = default;

void SewedSurface::add_surface(
    const std::shared_ptr<NURBSSurface>& surface,
    ContinuityLevel continuity
) {
    surfaces_.push_back(surface);
    continuities_.push_back(continuity);
}

Point3D SewedSurface::evaluate(double u, double v) const {
    if (surfaces_.empty()) return Point3D(0, 0, 0);
    // Default: use first surface
    return surfaces_[0]->evaluate(u, v);
}

ContinuityAnalysis SewedSurface::check_continuity(int surface_index1, int surface_index2) const {
    ContinuityAnalysis result;
    result.is_continuous = false;
    result.level = ContinuityLevel::G0;

    if (surface_index1 >= static_cast<int>(surfaces_.size()) ||
        surface_index2 >= static_cast<int>(surfaces_.size())) {
        result.description = "Invalid surface indices";
        return result;
    }

    auto& surf1 = surfaces_[surface_index1];
    auto& surf2 = surfaces_[surface_index2];

    // Sample boundary for continuity check
    int samples = 10;
    double max_gap = 0.0;
    double max_angle = 0.0;

    for (int i = 0; i < samples; ++i) {
        double t = static_cast<double>(i) / (samples - 1);

        // Check boundary points
        auto [u1_min, u1_max] = surf1->u_range();
        Point3D p1 = surf1->evaluate(u1_max, t);

        auto [u2_min, u2_max] = surf2->u_range();
        Point3D p2 = surf2->evaluate(u2_min, t);

        double gap = std::sqrt((p1.x - p2.x) * (p1.x - p2.x) +
                               (p1.y - p2.y) * (p1.y - p2.y) +
                               (p1.z - p2.z) * (p1.z - p2.z));
        max_gap = std::max(max_gap, gap);

        // Check tangent alignment for G1
        Vector3D n1 = surf1->normal(u1_max, t);
        Vector3D n2 = surf2->normal(u2_min, t);
        double angle = std::acos(std::clamp(n1.dot(n2), -1.0, 1.0));
        max_angle = std::max(max_angle, angle);
    }

    result.max_gap = max_gap;
    result.max_angle = max_angle;
    result.is_continuous = max_gap < 1e-6 && max_angle < 1e-6;
    result.level = result.is_continuous ? ContinuityLevel::G1 : ContinuityLevel::G0;

    return result;
}

std::vector<ContinuityAnalysis> SewedSurface::analyze_all_continuities() const {
    std::vector<ContinuityAnalysis> results;

    for (size_t i = 0; i < surfaces_.size() - 1; ++i) {
        results.push_back(check_continuity(i, i + 1));
    }

    return results;
}

bool SewedSurface::is_valid() const {
    if (surfaces_.empty()) return false;

    for (const auto& surf : surfaces_) {
        if (!surf || !surf->is_valid()) return false;
    }

    return true;
}

} // namespace bendcore::surfaces
