#include "bendcore/geometry/curve_operations.hpp"

#include <cmath>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace bendcore::geometry {

// NurbsCurve implementation
NurbsCurve::NurbsCurve() = default;

NurbsCurve::NurbsCurve(const std::vector<Point3D>& control_points,
                       const std::vector<double>& knot_vector,
                       const std::vector<double>& weights,
                       int degree)
    : control_points_(control_points), knot_vector_(knot_vector), weights_(weights), degree_(degree) {}

Point3D NurbsCurve::evaluate(double u) const {
    if (control_points_.empty() || !is_valid()) {
        return Point3D(0, 0, 0);
    }

    int n = control_points_.size() - 1;
    double result_x = 0, result_y = 0, result_z = 0, result_w = 0;

    for (int i = 0; i <= n; ++i) {
        double R = rational_basis_function(i, degree_, u);
        result_x += R * control_points_[i].x;
        result_y += R * control_points_[i].y;
        result_z += R * control_points_[i].z;
        result_w += R;
    }

    if (result_w > 1e-10) {
        return Point3D(result_x / result_w, result_y / result_w, result_z / result_w);
    }
    return Point3D(0, 0, 0);
}

Vector3D NurbsCurve::tangent(double u) const {
    double delta = 1e-6;
    Point3D p1 = evaluate(u - delta);
    Point3D p2 = evaluate(u + delta);
    Vector3D t(p2.x - p1.x, p2.y - p1.y, p2.z - p1.z);
    return t.normalized();
}

Vector3D NurbsCurve::curvature(double u) const {
    double delta = 1e-6;
    Point3D p0 = evaluate(u - delta);
    Point3D p1 = evaluate(u);
    Point3D p2 = evaluate(u + delta);

    Vector3D t1(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
    Vector3D t2(p2.x - p1.x, p2.y - p1.y, p2.z - p1.z);
    Vector3D ct = Vector3D(t2.x - t1.x, t2.y - t1.y, t2.z - t1.z);
    return ct.normalized();
}

double NurbsCurve::parameter_range_start() const {
    if (knot_vector_.empty()) {
        return 0.0;
    }
    return knot_vector_.front();
}

double NurbsCurve::parameter_range_end() const {
    if (knot_vector_.empty()) {
        return 1.0;
    }
    return knot_vector_.back();
}

double NurbsCurve::arc_length(double u1, double u2, int samples) const {
    double length = 0.0;
    Point3D prev = evaluate(u1);

    for (int i = 1; i <= samples; ++i) {
        double u = u1 + (u2 - u1) * i / samples;
        Point3D curr = evaluate(u);
        length += prev.distance_to(curr);
        prev = curr;
    }

    return length;
}

double NurbsCurve::total_arc_length(int samples) const {
    return arc_length(parameter_range_start(), parameter_range_end(), samples);
}

std::optional<double> NurbsCurve::arc_length_parameter(double arc_len, int samples) const {
    double total_len = total_arc_length(samples);
    if (arc_len > total_len) {
        return std::nullopt;
    }

    double u1 = parameter_range_start();
    double u2 = parameter_range_end();

    // Binary search for the parameter
    for (int iter = 0; iter < 20; ++iter) {
        double mid = (u1 + u2) / 2.0;
        double len = arc_length(parameter_range_start(), mid, samples);

        if (std::abs(len - arc_len) < 1e-6) {
            return mid;
        }

        if (len < arc_len) {
            u1 = mid;
        } else {
            u2 = mid;
        }
    }

    return (u1 + u2) / 2.0;
}

std::unique_ptr<NurbsCurve> NurbsCurve::offset(double distance, const Vector3D& normal) const {
    // Simplified offset: offset each control point
    std::vector<Point3D> offset_points;
    for (const auto& cp : control_points_) {
        offset_points.push_back(Point3D(
            cp.x + distance * normal.x,
            cp.y + distance * normal.y,
            cp.z + distance * normal.z
        ));
    }

    return std::make_unique<NurbsCurve>(offset_points, knot_vector_, weights_, degree_);
}

bool NurbsCurve::is_valid() const {
    return !control_points_.empty() && !knot_vector_.empty() &&
           control_points_.size() == weights_.size();
}

std::string NurbsCurve::to_string() const {
    std::stringstream ss;
    ss << "NurbsCurve(degree=" << degree_ << ", points=" << control_points_.size() << ")";
    return ss.str();
}

double NurbsCurve::basis_function(int i, int p, double u) const {
    if (p == 0) {
        return (u >= knot_vector_[i] && u < knot_vector_[i + 1]) ? 1.0 : 0.0;
    }

    double left = (u - knot_vector_[i]) * basis_function(i, p - 1, u) / (knot_vector_[i + p] - knot_vector_[i]);
    double right = (knot_vector_[i + p + 1] - u) * basis_function(i + 1, p - 1, u) / (knot_vector_[i + p + 1] - knot_vector_[i + 1]);

    return left + right;
}

double NurbsCurve::rational_basis_function(int i, int p, double u) const {
    if (i < 0 || i >= (int)weights_.size()) {
        return 0.0;
    }

    double numerator = weights_[i] * basis_function(i, p, u);
    double denominator = 0.0;

    for (int j = 0; j < (int)weights_.size(); ++j) {
        denominator += weights_[j] * basis_function(j, p, u);
    }

    if (denominator < 1e-10) {
        return 0.0;
    }

    return numerator / denominator;
}

// BezierCurve implementation
BezierCurve::BezierCurve() = default;

BezierCurve::BezierCurve(const std::vector<Point3D>& control_points)
    : control_points_(control_points) {}

Point3D BezierCurve::evaluate(double t) const {
    if (control_points_.empty()) {
        return Point3D(0, 0, 0);
    }

    int n = control_points_.size() - 1;
    double result_x = 0, result_y = 0, result_z = 0;

    for (int i = 0; i <= n; ++i) {
        double B = binomial_coefficient(n, i) * std::pow(1 - t, n - i) * std::pow(t, i);
        result_x += B * control_points_[i].x;
        result_y += B * control_points_[i].y;
        result_z += B * control_points_[i].z;
    }

    return Point3D(result_x, result_y, result_z);
}

Vector3D BezierCurve::tangent(double t) const {
    if (control_points_.empty()) {
        return Vector3D(0, 0, 0);
    }

    int n = control_points_.size() - 1;
    double result_x = 0, result_y = 0, result_z = 0;

    for (int i = 0; i < n; ++i) {
        double B = binomial_coefficient(n - 1, i) * std::pow(1 - t, n - 1 - i) * std::pow(t, i);
        double dx = control_points_[i + 1].x - control_points_[i].x;
        double dy = control_points_[i + 1].y - control_points_[i].y;
        double dz = control_points_[i + 1].z - control_points_[i].z;
        result_x += n * B * dx;
        result_y += n * B * dy;
        result_z += n * B * dz;
    }

    Vector3D v(result_x, result_y, result_z);
    return v.normalized();
}

double BezierCurve::arc_length(int samples) const {
    double length = 0.0;
    Point3D prev = evaluate(0.0);

    for (int i = 1; i <= samples; ++i) {
        double t = (double)i / samples;
        Point3D curr = evaluate(t);
        length += prev.distance_to(curr);
        prev = curr;
    }

    return length;
}

std::string BezierCurve::to_string() const {
    std::stringstream ss;
    ss << "BezierCurve(points=" << control_points_.size() << ")";
    return ss.str();
}

double BezierCurve::binomial_coefficient(int n, int k) const {
    if (k > n) return 0.0;
    if (k == 0 || k == n) return 1.0;

    double result = 1.0;
    for (int i = 0; i < k; ++i) {
        result *= (double)(n - i) / (i + 1);
    }
    return result;
}

// AnalyticCurve implementation
AnalyticCurve AnalyticCurve::create_line(const Point3D& start, const Point3D& end) {
    AnalyticCurve curve(Type::Line);
    curve.start_ = start;
    curve.end_ = end;
    return curve;
}

AnalyticCurve AnalyticCurve::create_circle(const Point3D& center, double radius, const Vector3D& normal) {
    AnalyticCurve curve(Type::Circle);
    curve.center_ = center;
    curve.radius_ = radius;
    curve.normal_ = normal.normalized();
    return curve;
}

AnalyticCurve AnalyticCurve::create_arc(const Point3D& center, double radius, const Vector3D& normal,
                                        double start_angle, double end_angle) {
    AnalyticCurve curve(Type::Arc);
    curve.center_ = center;
    curve.radius_ = radius;
    curve.normal_ = normal.normalized();
    curve.start_ = Point3D(start_angle, end_angle, 0);  // Store angles as placeholder
    return curve;
}

AnalyticCurve AnalyticCurve::create_ellipse(const Point3D& center, double semi_major, double semi_minor,
                                            const Vector3D& normal) {
    AnalyticCurve curve(Type::Ellipse);
    curve.center_ = center;
    curve.semi_major_ = semi_major;
    curve.semi_minor_ = semi_minor;
    curve.normal_ = normal.normalized();
    return curve;
}

AnalyticCurve AnalyticCurve::create_spiral(const Point3D& start, const Vector3D& axis,
                                           double radius, double pitch, int turns) {
    AnalyticCurve curve(Type::Spiral);
    curve.start_ = start;
    curve.axis_ = axis.normalized();
    curve.radius_ = radius;
    curve.pitch_ = pitch;
    curve.turns_ = turns;
    return curve;
}

Point3D AnalyticCurve::evaluate(double t) const {
    switch (type_) {
    case Type::Line: {
        return Point3D(
            start_.x + (end_.x - start_.x) * t,
            start_.y + (end_.y - start_.y) * t,
            start_.z + (end_.z - start_.z) * t
        );
    }
    case Type::Circle: {
        Vector3D n = normal_;
        Vector3D arbitrary(1, 0, 0);
        if (std::abs(n.dot(arbitrary)) > 0.9) {
            arbitrary = Vector3D(0, 1, 0);
        }
        Vector3D u = n.cross(arbitrary).normalized();
        Vector3D v = n.cross(u).normalized();

        return Point3D(
            center_.x + radius_ * (std::cos(t) * u.x + std::sin(t) * v.x),
            center_.y + radius_ * (std::cos(t) * u.y + std::sin(t) * v.y),
            center_.z + radius_ * (std::cos(t) * u.z + std::sin(t) * v.z)
        );
    }
    case Type::Arc: {
        // Similar to circle but limited to angle range
        double angle = start_.x + (end_.x - start_.x) * t;
        Circle circle(center_, radius_, normal_);
        return circle.evaluate(angle);
    }
    case Type::Ellipse: {
        Vector3D n = normal_;
        Vector3D arbitrary(1, 0, 0);
        if (std::abs(n.dot(arbitrary)) > 0.9) {
            arbitrary = Vector3D(0, 1, 0);
        }
        Vector3D u = n.cross(arbitrary).normalized();
        Vector3D v = n.cross(u).normalized();

        return Point3D(
            center_.x + semi_major_ * (std::cos(t) * u.x + std::sin(t) * v.x),
            center_.y + semi_major_ * (std::cos(t) * u.y + std::sin(t) * v.y),
            center_.z + semi_major_ * (std::cos(t) * u.z + std::sin(t) * v.z)
        );
    }
    case Type::Spiral: {
        double angle = t * 2.0 * M_PI * turns_;
        double height = t * pitch_ * turns_;

        Vector3D arb(1, 0, 0);
        if (std::abs(axis_.dot(arb)) > 0.9) {
            arb = Vector3D(0, 1, 0);
        }
        Vector3D u = axis_.cross(arb).normalized();
        Vector3D v = axis_.cross(u).normalized();

        return Point3D(
            start_.x + radius_ * (std::cos(angle) * u.x + std::sin(angle) * v.x) + height * axis_.x,
            start_.y + radius_ * (std::cos(angle) * u.y + std::sin(angle) * v.y) + height * axis_.y,
            start_.z + radius_ * (std::cos(angle) * u.z + std::sin(angle) * v.z) + height * axis_.z
        );
    }
    }
    return Point3D(0, 0, 0);
}

Vector3D AnalyticCurve::tangent(double t) const {
    double delta = 1e-6;
    Point3D p1 = evaluate(t - delta);
    Point3D p2 = evaluate(t + delta);
    Vector3D tan(p2.x - p1.x, p2.y - p1.y, p2.z - p1.z);
    return tan.normalized();
}

double AnalyticCurve::arc_length(int samples) const {
    double length = 0.0;
    Point3D prev = evaluate(0.0);

    for (int i = 1; i <= samples; ++i) {
        double t = (double)i / samples;
        Point3D curr = evaluate(t);
        length += prev.distance_to(curr);
        prev = curr;
    }

    return length;
}

bool AnalyticCurve::is_closed() const {
    return type_ == Type::Circle || type_ == Type::Ellipse;
}

std::string AnalyticCurve::to_string() const {
    std::stringstream ss;
    ss << "AnalyticCurve(type=" << (int)type_ << ")";
    return ss.str();
}

// TubeCurve implementation
TubeCurve::TubeCurve() = default;

TubeCurve::TubeCurve(const std::shared_ptr<NurbsCurve>& centerline, double tube_radius)
    : nurbs_centerline_(centerline), tube_radius_(tube_radius) {}

TubeCurve::TubeCurve(const std::shared_ptr<AnalyticCurve>& centerline, double tube_radius)
    : analytic_centerline_(centerline), tube_radius_(tube_radius) {}

double TubeCurve::centerline_length(int samples) const {
    if (nurbs_centerline_) {
        return nurbs_centerline_->total_arc_length(samples);
    } else if (analytic_centerline_) {
        return analytic_centerline_->arc_length(samples);
    }
    return 0.0;
}

Point3D TubeCurve::surface_point(double u, double v) const {
    Point3D center;
    if (nurbs_centerline_) {
        center = nurbs_centerline_->evaluate(u);
    } else if (analytic_centerline_) {
        center = analytic_centerline_->evaluate(u);
    } else {
        return Point3D(0, 0, 0);
    }

    Vector3D normal = get_normal(u);
    return Point3D(
        center.x + tube_radius_ * std::cos(v) * normal.x,
        center.y + tube_radius_ * std::cos(v) * normal.y,
        center.z + tube_radius_ * std::cos(v) * normal.z
    );
}

Vector3D TubeCurve::surface_normal(double u, double v) const {
    Vector3D tangent = get_binormal(u);
    Vector3D normal = get_normal(u);
    Vector3D cn(
        std::cos(v) * normal.x - std::sin(v) * tangent.x,
        std::cos(v) * normal.y - std::sin(v) * tangent.y,
        std::cos(v) * normal.z - std::sin(v) * tangent.z
    );
    return cn.normalized();
}

std::unique_ptr<TubeCurve> TubeCurve::offset(double wall_thickness) const {
    double new_radius = tube_radius_ + wall_thickness;
    if (nurbs_centerline_) {
        return std::make_unique<TubeCurve>(nurbs_centerline_, new_radius);
    } else if (analytic_centerline_) {
        return std::make_unique<TubeCurve>(analytic_centerline_, new_radius);
    }
    return nullptr;
}

bool TubeCurve::intersects_with(const TubeCurve& other, double tolerance) const {
    // Simplified intersection check
    return false;
}

std::string TubeCurve::to_string() const {
    std::stringstream ss;
    ss << "TubeCurve(radius=" << tube_radius_ << ")";
    return ss.str();
}

Vector3D TubeCurve::get_binormal(double u) const {
    Vector3D tangent;
    if (nurbs_centerline_) {
        tangent = nurbs_centerline_->tangent(u);
    } else if (analytic_centerline_) {
        tangent = analytic_centerline_->tangent(u);
    }
    return tangent;
}

Vector3D TubeCurve::get_normal(double u) const {
    Vector3D tangent = get_binormal(u);
    Vector3D arbitrary(1, 0, 0);
    if (std::abs(tangent.dot(arbitrary)) > 0.9) {
        arbitrary = Vector3D(0, 1, 0);
    }
    Vector3D normal = tangent.cross(arbitrary).normalized();
    return normal;
}

// CurveOperations implementation
CurveOperations::CurveOperations(double tolerance) : tolerance_(tolerance) {}

std::unique_ptr<NurbsCurve> CurveOperations::create_nurbs_curve(
    const std::vector<Point3D>& control_points,
    const std::vector<double>& knot_vector,
    const std::vector<double>& weights,
    int degree) {
    return std::make_unique<NurbsCurve>(control_points, knot_vector, weights, degree);
}

std::unique_ptr<AnalyticCurve> CurveOperations::create_line(const Point3D& start, const Point3D& end) {
    return std::make_unique<AnalyticCurve>(AnalyticCurve::create_line(start, end));
}

std::unique_ptr<AnalyticCurve> CurveOperations::create_circle(const Point3D& center, double radius, const Vector3D& normal) {
    return std::make_unique<AnalyticCurve>(AnalyticCurve::create_circle(center, radius, normal));
}

std::unique_ptr<AnalyticCurve> CurveOperations::create_arc(const Point3D& center, double radius, const Vector3D& normal,
                                                            double start_angle, double end_angle) {
    return std::make_unique<AnalyticCurve>(AnalyticCurve::create_arc(center, radius, normal, start_angle, end_angle));
}

std::unique_ptr<TubeCurve> CurveOperations::create_tube_from_nurbs(
    const std::shared_ptr<NurbsCurve>& centerline,
    double radius) {
    return std::make_unique<TubeCurve>(centerline, radius);
}

std::unique_ptr<TubeCurve> CurveOperations::create_tube_from_analytic(
    const std::shared_ptr<AnalyticCurve>& centerline,
    double radius) {
    return std::make_unique<TubeCurve>(centerline, radius);
}

std::unique_ptr<NurbsCurve> CurveOperations::interpolate_points(
    const std::vector<Point3D>& points,
    int degree) {
    // Simplified interpolation: create a NURBS with uniform knots
    std::vector<double> knots;
    int n = points.size() - 1;
    for (int i = 0; i <= degree; ++i) {
        knots.push_back(0.0);
    }
    for (int i = 1; i < n - degree; ++i) {
        knots.push_back((double)i / (n - degree + 1));
    }
    for (int i = 0; i <= degree; ++i) {
        knots.push_back(1.0);
    }

    std::vector<double> weights(points.size(), 1.0);
    return std::make_unique<NurbsCurve>(points, knots, weights, degree);
}

double CurveOperations::curve_length(const NurbsCurve& curve, int samples) const {
    return curve.total_arc_length(samples);
}

double CurveOperations::curve_length(const AnalyticCurve& curve, int samples) const {
    return curve.arc_length(samples);
}

Point3D CurveOperations::curve_evaluate(const NurbsCurve& curve, double u) const {
    return curve.evaluate(u);
}

Vector3D CurveOperations::curve_tangent(const NurbsCurve& curve, double u) const {
    return curve.tangent(u);
}

}  // namespace bendcore::geometry
