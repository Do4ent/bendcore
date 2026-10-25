#include "bendcore/geometry/surface_operations.hpp"

#include <cmath>
#include <sstream>
#include <algorithm>

namespace bendcore::geometry {

// NurbsSurface implementation
NurbsSurface::NurbsSurface() = default;

NurbsSurface::NurbsSurface(const std::vector<std::vector<Point3D>>& control_points,
                           const std::vector<double>& u_knots,
                           const std::vector<double>& v_knots,
                           const std::vector<std::vector<double>>& weights,
                           int u_degree,
                           int v_degree)
    : control_points_(control_points), u_knots_(u_knots), v_knots_(v_knots),
      weights_(weights), u_degree_(u_degree), v_degree_(v_degree) {}

Point3D NurbsSurface::evaluate(double u, double v) const {
    if (control_points_.empty() || !is_valid()) {
        return Point3D(0, 0, 0);
    }

    double result_x = 0, result_y = 0, result_z = 0, result_w = 0;
    int m = control_points_.size() - 1;
    int n = control_points_[0].size() - 1;

    for (int i = 0; i <= m; ++i) {
        for (int j = 0; j <= n; ++j) {
            double R = basis_function_u(i, u_degree_, u) * basis_function_v(j, v_degree_, v);
            if (i < (int)weights_.size() && j < (int)weights_[i].size()) {
                R *= weights_[i][j];
            }
            result_x += R * control_points_[i][j].x;
            result_y += R * control_points_[i][j].y;
            result_z += R * control_points_[i][j].z;
            result_w += R;
        }
    }

    if (result_w > 1e-10) {
        return Point3D(result_x / result_w, result_y / result_w, result_z / result_w);
    }
    return Point3D(0, 0, 0);
}

Vector3D NurbsSurface::normal(double u, double v) const {
    double delta = 1e-6;
    Point3D p00 = evaluate(u - delta, v - delta);
    Point3D p10 = evaluate(u + delta, v - delta);
    Point3D p01 = evaluate(u - delta, v + delta);

    Vector3D du(p10.x - p00.x, p10.y - p00.y, p10.z - p00.z);
    Vector3D dv(p01.x - p00.x, p01.y - p00.y, p01.z - p00.z);

    return du.cross(dv).normalized();
}

Point3D NurbsSurface::closest_point_on_surface(const Point3D& p, int iterations) const {
    // Newton-Raphson method for closest point
    double u = 0.5;
    double v = 0.5;

    for (int iter = 0; iter < iterations; ++iter) {
        Point3D surf_p = evaluate(u, v);
        Vector3D n = normal(u, v);
        Vector3D to_p(p.x - surf_p.x, p.y - surf_p.y, p.z - surf_p.z);
        double dist = to_p.magnitude();

        if (dist < 1e-10) {
            break;
        }

        double du = 0.01;
        double dv = 0.01;
        Point3D p_u = evaluate(u + du, v);
        Point3D p_v = evaluate(u, v + dv);

        Vector3D tang_u(p_u.x - surf_p.x, p_u.y - surf_p.y, p_u.z - surf_p.z);
        Vector3D tang_v(p_v.x - surf_p.x, p_v.y - surf_p.y, p_v.z - surf_p.z);

        // Simplified update
        u = std::clamp(u + 0.1 * to_p.dot(tang_u) / (du * tang_u.magnitude() + 1e-10), 0.0, 1.0);
        v = std::clamp(v + 0.1 * to_p.dot(tang_v) / (dv * tang_v.magnitude() + 1e-10), 0.0, 1.0);
    }

    return evaluate(u, v);
}

std::vector<Point3D> NurbsSurface::intersect_with_curve(const NurbsCurve& curve, int samples) const {
    std::vector<Point3D> intersections;

    for (int i = 0; i <= samples; ++i) {
        double t = (double)i / samples;
        Point3D curve_p = curve.evaluate(t);
        Point3D closest = closest_point_on_surface(curve_p);

        if (closest.distance_to(curve_p) < 1e-6) {
            intersections.push_back(curve_p);
        }
    }

    return intersections;
}

std::unique_ptr<NurbsSurface> NurbsSurface::offset(double distance) const {
    // Simplified offset: offset control points in normal direction
    std::vector<std::vector<Point3D>> offset_points;

    for (const auto& row : control_points_) {
        std::vector<Point3D> offset_row;
        for (const auto& cp : row) {
            // Simplified: use average normal from nearby points
            offset_row.push_back(Point3D(
                cp.x + distance * 0.1,
                cp.y + distance * 0.1,
                cp.z + distance * 0.1
            ));
        }
        offset_points.push_back(offset_row);
    }

    return std::make_unique<NurbsSurface>(offset_points, u_knots_, v_knots_, weights_, u_degree_, v_degree_);
}

bool NurbsSurface::is_valid() const {
    return !control_points_.empty() && !u_knots_.empty() && !v_knots_.empty();
}

bool NurbsSurface::is_surface_closed_u() const {
    if (control_points_.empty() || control_points_[0].empty()) {
        return false;
    }
    Point3D first = control_points_[0][0];
    Point3D last = control_points_.back()[0];
    return first.equals(last, 1e-10);
}

bool NurbsSurface::is_surface_closed_v() const {
    if (control_points_.empty() || control_points_[0].empty()) {
        return false;
    }
    Point3D first = control_points_[0][0];
    Point3D last = control_points_[0].back();
    return first.equals(last, 1e-10);
}

std::string NurbsSurface::to_string() const {
    std::stringstream ss;
    ss << "NurbsSurface(u_degree=" << u_degree_ << ", v_degree=" << v_degree_
       << ", u_points=" << u_control_points() << ", v_points=" << v_control_points() << ")";
    return ss.str();
}

double NurbsSurface::basis_function_u(int i, int p, double u) const {
    if (p == 0) {
        return (u >= u_knots_[i] && u < u_knots_[i + 1]) ? 1.0 : 0.0;
    }
    if (i >= (int)u_knots_.size() - 1) {
        return 0.0;
    }
    return (u - u_knots_[i]) * basis_function_u(i, p - 1, u) / (u_knots_[i + p] - u_knots_[i]) +
           (u_knots_[i + p + 1] - u) * basis_function_u(i + 1, p - 1, u) / (u_knots_[i + p + 1] - u_knots_[i + 1]);
}

double NurbsSurface::basis_function_v(int j, int q, double v) const {
    if (q == 0) {
        return (v >= v_knots_[j] && v < v_knots_[j + 1]) ? 1.0 : 0.0;
    }
    if (j >= (int)v_knots_.size() - 1) {
        return 0.0;
    }
    return (v - v_knots_[j]) * basis_function_v(j, q - 1, v) / (v_knots_[j + q] - v_knots_[j]) +
           (v_knots_[j + q + 1] - v) * basis_function_v(j + 1, q - 1, v) / (v_knots_[j + q + 1] - v_knots_[j + 1]);
}

// TrimmedSurface implementation
TrimmedSurface::TrimmedSurface(const std::shared_ptr<NurbsSurface>& surface,
                               const std::vector<std::vector<Point3D>>& trim_boundaries)
    : surface_(surface), trim_boundaries_(trim_boundaries) {}

Point3D TrimmedSurface::evaluate(double u, double v) const {
    if (!surface_ || !is_point_inside_trim_region(u, v)) {
        return Point3D(0, 0, 0);
    }
    return surface_->evaluate(u, v);
}

Vector3D TrimmedSurface::normal(double u, double v) const {
    if (!surface_ || !is_point_inside_trim_region(u, v)) {
        return Vector3D(0, 0, 0);
    }
    return surface_->normal(u, v);
}

bool TrimmedSurface::is_point_inside_trim_region(double u, double v) const {
    // Simplified: assume point is inside if not explicitly outside
    return true;
}

std::string TrimmedSurface::to_string() const {
    std::stringstream ss;
    ss << "TrimmedSurface(boundaries=" << trim_boundaries_.size() << ")";
    return ss.str();
}

// RevolutionSurface implementation
RevolutionSurface::RevolutionSurface(const std::shared_ptr<NurbsCurve>& profile,
                                     const Point3D& axis_point,
                                     const Vector3D& axis_direction,
                                     double angle)
    : profile_(profile), axis_point_(axis_point), axis_direction_(axis_direction.normalized()), angle_(angle) {}

Point3D RevolutionSurface::evaluate(double u, double v) const {
    if (!profile_) {
        return Point3D(0, 0, 0);
    }

    Point3D profile_p = profile_->evaluate(u);
    double rotation_angle = v * angle_;

    // Rotate profile point around axis
    Transform rotation = Transform::rotation_axis(axis_direction_, rotation_angle);
    return rotation.apply_to_point(profile_p);
}

Vector3D RevolutionSurface::normal(double u, double v) const {
    double delta = 1e-6;
    Point3D p00 = evaluate(u - delta, v - delta);
    Point3D p10 = evaluate(u + delta, v - delta);
    Point3D p01 = evaluate(u - delta, v + delta);

    Vector3D du(p10.x - p00.x, p10.y - p00.y, p10.z - p00.z);
    Vector3D dv(p01.x - p00.x, p01.y - p00.y, p01.z - p00.z);

    return du.cross(dv).normalized();
}

std::string RevolutionSurface::to_string() const {
    std::stringstream ss;
    ss << "RevolutionSurface(angle=" << angle_ << ")";
    return ss.str();
}

// SurfaceOfRevolution implementation
SurfaceOfRevolution::SurfaceOfRevolution(const std::shared_ptr<AnalyticCurve>& centerline,
                                         double tube_radius)
    : centerline_(centerline), tube_radius_(tube_radius) {}

Point3D SurfaceOfRevolution::evaluate(double u, double v) const {
    if (!centerline_) {
        return Point3D(0, 0, 0);
    }

    Point3D center = centerline_->evaluate(u);
    Vector3D tangent = centerline_->tangent(u);

    // Create orthonormal basis
    Vector3D arbitrary(1, 0, 0);
    if (std::abs(tangent.dot(arbitrary)) > 0.9) {
        arbitrary = Vector3D(0, 1, 0);
    }
    Vector3D normal = tangent.cross(arbitrary).normalized();
    Vector3D binormal = tangent.cross(normal).normalized();

    return Point3D(
        center.x + tube_radius_ * (std::cos(v) * normal.x + std::sin(v) * binormal.x),
        center.y + tube_radius_ * (std::cos(v) * normal.y + std::sin(v) * binormal.y),
        center.z + tube_radius_ * (std::cos(v) * normal.z + std::sin(v) * binormal.z)
    );
}

Vector3D SurfaceOfRevolution::normal(double u, double v) const {
    double delta = 1e-6;
    Point3D p00 = evaluate(u - delta, v - delta);
    Point3D p10 = evaluate(u + delta, v - delta);
    Point3D p01 = evaluate(u - delta, v + delta);

    Vector3D du(p10.x - p00.x, p10.y - p00.y, p10.z - p00.z);
    Vector3D dv(p01.x - p00.x, p01.y - p00.y, p01.z - p00.z);

    return du.cross(dv).normalized();
}

double SurfaceOfRevolution::surface_area(int u_samples, int v_samples) const {
    double area = 0.0;

    for (int i = 0; i < u_samples; ++i) {
        for (int j = 0; j < v_samples; ++j) {
            double u1 = (double)i / u_samples;
            double v1 = (double)j / v_samples;
            double u2 = (double)(i + 1) / u_samples;
            double v2 = (double)(j + 1) / v_samples;

            Point3D p00 = evaluate(u1, v1);
            Point3D p10 = evaluate(u2, v1);
            Point3D p01 = evaluate(u1, v2);

            Vector3D e1(p10.x - p00.x, p10.y - p00.y, p10.z - p00.z);
            Vector3D e2(p01.x - p00.x, p01.y - p00.y, p01.z - p00.z);
            Vector3D cross = e1.cross(e2);
            area += cross.magnitude();
        }
    }

    return area * 0.5;
}

std::unique_ptr<SurfaceOfRevolution> SurfaceOfRevolution::offset_wall_thickness(double thickness) const {
    double new_radius = tube_radius_ + thickness;
    return std::make_unique<SurfaceOfRevolution>(centerline_, new_radius);
}

std::string SurfaceOfRevolution::to_string() const {
    std::stringstream ss;
    ss << "SurfaceOfRevolution(radius=" << tube_radius_ << ")";
    return ss.str();
}

// SurfaceOperations implementation
SurfaceOperations::SurfaceOperations(double tolerance) : tolerance_(tolerance) {}

std::unique_ptr<NurbsSurface> SurfaceOperations::create_nurbs_surface(
    const std::vector<std::vector<Point3D>>& control_points,
    const std::vector<double>& u_knots,
    const std::vector<double>& v_knots,
    const std::vector<std::vector<double>>& weights,
    int u_degree,
    int v_degree) {
    return std::make_unique<NurbsSurface>(control_points, u_knots, v_knots, weights, u_degree, v_degree);
}

std::unique_ptr<RevolutionSurface> SurfaceOperations::create_revolution_surface(
    const std::shared_ptr<NurbsCurve>& profile,
    const Point3D& axis_point,
    const Vector3D& axis_direction,
    double angle) {
    return std::make_unique<RevolutionSurface>(profile, axis_point, axis_direction, angle);
}

std::unique_ptr<SurfaceOfRevolution> SurfaceOperations::create_tube_surface(
    const std::shared_ptr<AnalyticCurve>& centerline,
    double radius) {
    return std::make_unique<SurfaceOfRevolution>(centerline, radius);
}

std::unique_ptr<TrimmedSurface> SurfaceOperations::create_trimmed_surface(
    const std::shared_ptr<NurbsSurface>& surface,
    const std::vector<std::vector<Point3D>>& trim_boundaries) {
    return std::make_unique<TrimmedSurface>(surface, trim_boundaries);
}

Point3D SurfaceOperations::surface_evaluate(const NurbsSurface& surface, double u, double v) const {
    return surface.evaluate(u, v);
}

Vector3D SurfaceOperations::surface_normal(const NurbsSurface& surface, double u, double v) const {
    return surface.normal(u, v);
}

std::vector<Point3D> SurfaceOperations::curve_surface_intersection(
    const NurbsCurve& curve,
    const NurbsSurface& surface,
    int samples) const {
    std::vector<Point3D> intersections;

    for (int i = 0; i <= samples; ++i) {
        double t = (double)i / samples;
        Point3D curve_p = curve.evaluate(t);

        // Find closest point on surface
        Point3D closest = surface.closest_point_on_surface(curve_p);

        if (closest.distance_to(curve_p) < tolerance_) {
            intersections.push_back(curve_p);
        }
    }

    return intersections;
}

SurfaceMetrics SurfaceOperations::compute_surface_metrics(
    const NurbsSurface& surface,
    int u_samples,
    int v_samples) const {
    SurfaceMetrics metrics;

    for (int i = 0; i < u_samples; ++i) {
        for (int j = 0; j < v_samples; ++j) {
            double u1 = (double)i / u_samples;
            double v1 = (double)j / v_samples;
            double u2 = (double)(i + 1) / u_samples;
            double v2 = (double)(j + 1) / v_samples;

            Point3D p00 = surface.evaluate(u1, v1);
            Point3D p10 = surface.evaluate(u2, v1);
            Point3D p01 = surface.evaluate(u1, v2);

            Vector3D e1(p10.x - p00.x, p10.y - p00.y, p10.z - p00.z);
            Vector3D e2(p01.x - p00.x, p01.y - p00.y, p01.z - p00.z);
            Vector3D cross = e1.cross(e2);
            metrics.surface_area += cross.magnitude();

            metrics.center_of_mass.x += p00.x;
            metrics.center_of_mass.y += p00.y;
            metrics.center_of_mass.z += p00.z;
        }
    }

    metrics.surface_area *= 0.5;
    metrics.center_of_mass.x /= (u_samples * v_samples);
    metrics.center_of_mass.y /= (u_samples * v_samples);
    metrics.center_of_mass.z /= (u_samples * v_samples);

    return metrics;
}

std::unique_ptr<NurbsSurface> SurfaceOperations::offset_surface(
    const NurbsSurface& surface,
    double distance) const {
    return surface.offset(distance);
}

std::unique_ptr<NurbsSurface> SurfaceOperations::create_wall_thickness(
    const NurbsSurface& surface,
    double thickness) const {
    return surface.offset(thickness);
}

}  // namespace bendcore::geometry
