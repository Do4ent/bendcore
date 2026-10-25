#include "bendcore/sketch/primitives.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

namespace bendcore::sketch {

// ============= Point =============
double Point::distance(const Point& other) const {
    double dx = x_ - other.x_;
    double dy = y_ - other.y_;
    return std::sqrt(dx * dx + dy * dy);
}

// ============= Line =============
Point Line::getMidpoint() const {
    return Point((start_.getX() + end_.getX()) / 2.0,
                 (start_.getY() + end_.getY()) / 2.0);
}

double Line::getSlope() const {
    double dx = end_.getX() - start_.getX();
    if (std::abs(dx) < 1e-10) {
        return std::numeric_limits<double>::infinity();
    }
    return (end_.getY() - start_.getY()) / dx;
}

bool Line::isHorizontal(double tolerance) const {
    double dy = std::abs(end_.getY() - start_.getY());
    return dy < tolerance;
}

bool Line::isVertical(double tolerance) const {
    double dx = std::abs(end_.getX() - start_.getX());
    return dx < tolerance;
}

// ============= Circle =============
Point Circle::getPoint(double angle_deg) const {
    double rad = angle_deg * M_PI / 180.0;
    return Point(center_.getX() + radius_ * std::cos(rad),
                 center_.getY() + radius_ * std::sin(rad));
}

// ============= Arc =============
Point Arc::getStartPoint() const {
    double rad = start_angle_ * M_PI / 180.0;
    return Point(center_.getX() + radius_ * std::cos(rad),
                 center_.getY() + radius_ * std::sin(rad));
}

Point Arc::getEndPoint() const {
    double rad = end_angle_ * M_PI / 180.0;
    return Point(center_.getX() + radius_ * std::cos(rad),
                 center_.getY() + radius_ * std::sin(rad));
}

Point Arc::getMidPoint() const {
    double mid_angle = (start_angle_ + end_angle_) / 2.0;
    double rad = mid_angle * M_PI / 180.0;
    return Point(center_.getX() + radius_ * std::cos(rad),
                 center_.getY() + radius_ * std::sin(rad));
}

double Arc::getArcLength() const {
    double angle_diff = std::abs(end_angle_ - start_angle_);
    if (angle_diff > 360.0) angle_diff = 360.0;
    double rad = angle_diff * M_PI / 180.0;
    return radius_ * rad;
}

// ============= Ellipse =============
double Ellipse::getEccentricity() const {
    if (major_axis_ < 1e-10) return 0.0;
    double ratio = minor_axis_ / major_axis_;
    return std::sqrt(1.0 - ratio * ratio);
}

double Ellipse::getPerimeter() const {
    // Ramanujan's approximation
    double a = major_axis_;
    double b = minor_axis_;
    double h = (a - b) * (a - b) / ((a + b) * (a + b));
    return M_PI * (a + b) * (1.0 + 3.0 * h / (10.0 + std::sqrt(4.0 - 3.0 * h)));
}

Point Ellipse::getPoint(double angle_deg) const {
    double rad = angle_deg * M_PI / 180.0;
    double cos_rot = std::cos(rotation_ * M_PI / 180.0);
    double sin_rot = std::sin(rotation_ * M_PI / 180.0);

    double x = major_axis_ * std::cos(rad);
    double y = minor_axis_ * std::sin(rad);

    double rotated_x = x * cos_rot - y * sin_rot;
    double rotated_y = x * sin_rot + y * cos_rot;

    return Point(center_.getX() + rotated_x, center_.getY() + rotated_y);
}

// ============= Spline =============
Point Spline::evaluate(double t) const {
    if (control_points_.empty()) {
        return Point(0, 0);
    }

    switch (type_) {
        case SplineType::BEZIER:
            return evaluateBezier(t);
        case SplineType::BSPLINE:
            return evaluateBSpline(t);
        case SplineType::INTERPOLATED:
            return evaluateBezier(t);  // Default to Bezier
    }
    return Point(0, 0);
}

Point Spline::getTangent(double t) const {
    const double dt = 1e-6;
    Point p1 = evaluate(std::max(0.0, t - dt));
    Point p2 = evaluate(std::min(1.0, t + dt));

    return Point((p2.getX() - p1.getX()) / (2.0 * dt),
                 (p2.getY() - p1.getY()) / (2.0 * dt));
}

double Spline::getLength() const {
    switch (type_) {
        case SplineType::BEZIER:
            return calculateBezierLength();
        default:
            return 0.0;
    }
}

Point Spline::evaluateBezier(double t) const {
    if (control_points_.size() < 2) {
        return control_points_.empty() ? Point(0, 0) : control_points_[0];
    }

    // De Casteljau algorithm for Bezier curve
    std::vector<Point> points = control_points_;

    for (size_t i = 0; i < control_points_.size() - 1; i++) {
        for (size_t j = 0; j < control_points_.size() - 1 - i; j++) {
            Point p0 = points[j];
            Point p1 = points[j + 1];

            points[j] = Point((1.0 - t) * p0.getX() + t * p1.getX(),
                            (1.0 - t) * p0.getY() + t * p1.getY());
        }
    }

    return points[0];
}

Point Spline::evaluateBSpline(double t) const {
    // Simplified B-spline evaluation
    if (control_points_.size() < 2) {
        return control_points_.empty() ? Point(0, 0) : control_points_[0];
    }

    // Clamp t to [0, 1]
    t = std::max(0.0, std::min(1.0, t));

    // Find the segment
    int n = control_points_.size();
    int segment = std::min(static_cast<int>(t * (n - 1)), n - 2);
    double u = t * (n - 1) - segment;

    // Cubic B-spline basis functions
    double b0 = (1.0 - u) * (1.0 - u) * (1.0 - u) / 6.0;
    double b1 = (3.0 * u * u * u - 6.0 * u * u + 4.0) / 6.0;
    double b2 = (-3.0 * u * u * u + 3.0 * u * u + 3.0 * u + 1.0) / 6.0;
    double b3 = u * u * u / 6.0;

    Point result(0, 0);
    if (segment >= 0 && segment < n) result.setPosition(result.getX() + b0 * control_points_[segment].getX(),
                                                         result.getY() + b0 * control_points_[segment].getY());
    if (segment + 1 < n) {
        double new_x = result.getX() + b1 * control_points_[segment + 1].getX();
        double new_y = result.getY() + b1 * control_points_[segment + 1].getY();
        result.setPosition(new_x, new_y);
    }
    if (segment + 2 < n) {
        double new_x = result.getX() + b2 * control_points_[segment + 2].getX();
        double new_y = result.getY() + b2 * control_points_[segment + 2].getY();
        result.setPosition(new_x, new_y);
    }
    if (segment + 3 < n) {
        double new_x = result.getX() + b3 * control_points_[segment + 3].getX();
        double new_y = result.getY() + b3 * control_points_[segment + 3].getY();
        result.setPosition(new_x, new_y);
    }

    return result;
}

double Spline::calculateBezierLength() const {
    const int segments = 100;
    double length = 0.0;
    Point prev = evaluate(0.0);

    for (int i = 1; i <= segments; i++) {
        double t = static_cast<double>(i) / segments;
        Point curr = evaluate(t);
        length += prev.distance(curr);
        prev = curr;
    }

    return length;
}

// ============= Rectangle =============
Point Rectangle::getCenter() const {
    double cos_rot = std::cos(rotation_ * M_PI / 180.0);
    double sin_rot = std::sin(rotation_ * M_PI / 180.0);

    double cx = width_ / 2.0;
    double cy = height_ / 2.0;

    double rotated_cx = cx * cos_rot - cy * sin_rot;
    double rotated_cy = cx * sin_rot + cy * cos_rot;

    return Point(origin_.getX() + rotated_cx, origin_.getY() + rotated_cy);
}

std::vector<Point> Rectangle::getCorners() const {
    std::vector<Point> corners;
    double cos_rot = std::cos(rotation_ * M_PI / 180.0);
    double sin_rot = std::sin(rotation_ * M_PI / 180.0);

    std::vector<std::pair<double, double>> local_corners = {
        {0, 0}, {width_, 0}, {width_, height_}, {0, height_}
    };

    for (const auto& corner : local_corners) {
        double rotated_x = corner.first * cos_rot - corner.second * sin_rot;
        double rotated_y = corner.first * sin_rot + corner.second * cos_rot;

        corners.push_back(Point(origin_.getX() + rotated_x, origin_.getY() + rotated_y));
    }

    return corners;
}

// ============= Polygon =============
void Polygon::regenerateVertices() {
    vertices_.clear();

    if (num_sides_ < 3) return;

    double angle_step = 360.0 / num_sides_;
    double cos_rot = std::cos(rotation_ * M_PI / 180.0);
    double sin_rot = std::sin(rotation_ * M_PI / 180.0);

    for (int i = 0; i < num_sides_; i++) {
        double angle = i * angle_step + rotation_;
        double rad = angle * M_PI / 180.0;

        double x = radius_ * std::cos(rad);
        double y = radius_ * std::sin(rad);

        double rotated_x = x * cos_rot - y * sin_rot;
        double rotated_y = x * sin_rot + y * cos_rot;

        vertices_.push_back(Point(center_.getX() + rotated_x, center_.getY() + rotated_y));
    }
}

double Polygon::getPerimeter() const {
    double perimeter = 0.0;

    for (size_t i = 0; i < vertices_.size(); i++) {
        size_t next = (i + 1) % vertices_.size();
        perimeter += vertices_[i].distance(vertices_[next]);
    }

    return perimeter;
}

double Polygon::getArea() const {
    if (vertices_.size() < 3) return 0.0;

    // Shoelace formula
    double area = 0.0;
    for (size_t i = 0; i < vertices_.size(); i++) {
        size_t next = (i + 1) % vertices_.size();
        area += vertices_[i].getX() * vertices_[next].getY();
        area -= vertices_[next].getX() * vertices_[i].getY();
    }

    return std::abs(area) / 2.0;
}

} // namespace bendcore::sketch
