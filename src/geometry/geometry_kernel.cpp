#include "bendcore/geometry/geometry_kernel.hpp"

#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace bendcore::geometry {

// Point3D implementation
double Point3D::distance_to(const Point3D& other) const {
    double dx = x - other.x;
    double dy = y - other.y;
    double dz = z - other.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool Point3D::equals(const Point3D& other, double tolerance) const {
    return distance_to(other) <= tolerance;
}

std::string Point3D::to_string() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(10);
    ss << "Point3D(" << x << ", " << y << ", " << z << ")";
    return ss.str();
}

// Vector3D implementation
double Vector3D::magnitude() const {
    return std::sqrt(x * x + y * y + z * z);
}

Vector3D Vector3D::normalized() const {
    double mag = magnitude();
    if (mag < DEFAULT_TOLERANCE) {
        return Vector3D(0, 0, 0);
    }
    return Vector3D(x / mag, y / mag, z / mag);
}

double Vector3D::dot(const Vector3D& other) const {
    return x * other.x + y * other.y + z * other.z;
}

Vector3D Vector3D::cross(const Vector3D& other) const {
    return Vector3D(
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x
    );
}

double Vector3D::angle_to(const Vector3D& other) const {
    double dot_product = dot(other);
    double magnitude_product = magnitude() * other.magnitude();
    if (magnitude_product < DEFAULT_TOLERANCE) {
        return 0.0;
    }
    double cos_angle = dot_product / magnitude_product;
    cos_angle = std::clamp(cos_angle, -1.0, 1.0);
    return std::acos(cos_angle);
}

bool Vector3D::equals(const Vector3D& other, double tolerance) const {
    Vector3D diff(x - other.x, y - other.y, z - other.z);
    return diff.magnitude() <= tolerance;
}

bool Vector3D::is_parallel_to(const Vector3D& other, double tolerance) const {
    Vector3D cross_product = cross(other);
    return cross_product.magnitude() <= tolerance;
}

bool Vector3D::is_perpendicular_to(const Vector3D& other, double tolerance) const {
    double dot_product = std::abs(dot(other));
    return dot_product <= tolerance;
}

std::string Vector3D::to_string() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(10);
    ss << "Vector3D(" << x << ", " << y << ", " << z << ")";
    return ss.str();
}

// Plane implementation
Plane::Plane(const Point3D& origin, const Vector3D& normal)
    : origin(origin), normal(normal.normalized()) {
    // Create orthonormal basis
    Vector3D n = this->normal;
    Vector3D arbitrary(1, 0, 0);
    if (std::abs(n.dot(arbitrary)) > 0.9) {
        arbitrary = Vector3D(0, 1, 0);
    }
    u_axis = n.cross(arbitrary).normalized();
    v_axis = n.cross(u_axis).normalized();
}

double Plane::distance_to_point(const Point3D& p) const {
    Vector3D v(p.x - origin.x, p.y - origin.y, p.z - origin.z);
    return std::abs(v.dot(normal));
}

Point3D Plane::project_point(const Point3D& p) const {
    Vector3D v(p.x - origin.x, p.y - origin.y, p.z - origin.z);
    double dist = v.dot(normal);
    return Point3D(
        p.x - dist * normal.x,
        p.y - dist * normal.y,
        p.z - dist * normal.z
    );
}

bool Plane::contains_point(const Point3D& p, double tolerance) const {
    return distance_to_point(p) <= tolerance;
}

std::string Plane::to_string() const {
    std::stringstream ss;
    ss << "Plane(origin: " << origin.to_string() << ", normal: " << normal.to_string() << ")";
    return ss.str();
}

// Transform implementation
Transform::Transform() {
    matrix_ = {{
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1}
    }};
}

Transform::Transform(const std::array<std::array<double, 4>, 4>& matrix) : matrix_(matrix) {}

Transform Transform::identity() {
    Transform t;
    return t;
}

Transform Transform::translation(double dx, double dy, double dz) {
    Transform t = identity();
    t.matrix_[0][3] = dx;
    t.matrix_[1][3] = dy;
    t.matrix_[2][3] = dz;
    return t;
}

Transform Transform::rotation_x(double angle) {
    double c = std::cos(angle);
    double s = std::sin(angle);
    Transform t = identity();
    t.matrix_[1][1] = c;
    t.matrix_[1][2] = -s;
    t.matrix_[2][1] = s;
    t.matrix_[2][2] = c;
    return t;
}

Transform Transform::rotation_y(double angle) {
    double c = std::cos(angle);
    double s = std::sin(angle);
    Transform t = identity();
    t.matrix_[0][0] = c;
    t.matrix_[0][2] = s;
    t.matrix_[2][0] = -s;
    t.matrix_[2][2] = c;
    return t;
}

Transform Transform::rotation_z(double angle) {
    double c = std::cos(angle);
    double s = std::sin(angle);
    Transform t = identity();
    t.matrix_[0][0] = c;
    t.matrix_[0][1] = -s;
    t.matrix_[1][0] = s;
    t.matrix_[1][1] = c;
    return t;
}

Transform Transform::rotation_axis(const Vector3D& axis, double angle) {
    Vector3D a = axis.normalized();
    double c = std::cos(angle);
    double s = std::sin(angle);
    double t_val = 1.0 - c;

    Transform t = identity();
    t.matrix_[0][0] = t_val * a.x * a.x + c;
    t.matrix_[0][1] = t_val * a.x * a.y - s * a.z;
    t.matrix_[0][2] = t_val * a.x * a.z + s * a.y;

    t.matrix_[1][0] = t_val * a.x * a.y + s * a.z;
    t.matrix_[1][1] = t_val * a.y * a.y + c;
    t.matrix_[1][2] = t_val * a.y * a.z - s * a.x;

    t.matrix_[2][0] = t_val * a.x * a.z - s * a.y;
    t.matrix_[2][1] = t_val * a.y * a.z + s * a.x;
    t.matrix_[2][2] = t_val * a.z * a.z + c;

    return t;
}

Transform Transform::scaling(double sx, double sy, double sz) {
    Transform t = identity();
    t.matrix_[0][0] = sx;
    t.matrix_[1][1] = sy;
    t.matrix_[2][2] = sz;
    return t;
}

Point3D Transform::apply_to_point(const Point3D& p) const {
    double x = matrix_[0][0] * p.x + matrix_[0][1] * p.y + matrix_[0][2] * p.z + matrix_[0][3];
    double y = matrix_[1][0] * p.x + matrix_[1][1] * p.y + matrix_[1][2] * p.z + matrix_[1][3];
    double z = matrix_[2][0] * p.x + matrix_[2][1] * p.y + matrix_[2][2] * p.z + matrix_[2][3];
    double w = matrix_[3][0] * p.x + matrix_[3][1] * p.y + matrix_[3][2] * p.z + matrix_[3][3];
    if (std::abs(w) > 1e-10) {
        return Point3D(x / w, y / w, z / w);
    }
    return Point3D(x, y, z);
}

Vector3D Transform::apply_to_vector(const Vector3D& v) const {
    double x = matrix_[0][0] * v.x + matrix_[0][1] * v.y + matrix_[0][2] * v.z;
    double y = matrix_[1][0] * v.x + matrix_[1][1] * v.y + matrix_[1][2] * v.z;
    double z = matrix_[2][0] * v.x + matrix_[2][1] * v.y + matrix_[2][2] * v.z;
    return Vector3D(x, y, z);
}

Transform Transform::inverse() const {
    // Simplified inverse for homogeneous transformations
    Transform inv = identity();
    // For rotation/scale/translation only, transpose the 3x3 part
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            inv.matrix_[i][j] = matrix_[j][i];
        }
    }
    // Handle translation
    inv.matrix_[0][3] = -(matrix_[0][0] * matrix_[0][3] + matrix_[1][0] * matrix_[1][3] + matrix_[2][0] * matrix_[2][3]);
    inv.matrix_[1][3] = -(matrix_[0][1] * matrix_[0][3] + matrix_[1][1] * matrix_[1][3] + matrix_[2][1] * matrix_[2][3]);
    inv.matrix_[2][3] = -(matrix_[0][2] * matrix_[0][3] + matrix_[1][2] * matrix_[1][3] + matrix_[2][2] * matrix_[2][3]);
    return inv;
}

Transform Transform::compose(const Transform& other) const {
    Transform result = identity();
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.matrix_[i][j] = 0;
            for (int k = 0; k < 4; k++) {
                result.matrix_[i][j] += matrix_[i][k] * other.matrix_[k][j];
            }
        }
    }
    return result;
}

std::string Transform::to_string() const {
    std::stringstream ss;
    ss << "Transform(4x4 matrix)";
    return ss.str();
}

// Line implementation
Line::Line(const Point3D& point, const Vector3D& direction)
    : point(point), direction(direction.normalized()) {}

Point3D Line::evaluate(double t) const {
    return Point3D(
        point.x + t * direction.x,
        point.y + t * direction.y,
        point.z + t * direction.z
    );
}

double Line::closest_parameter_to_point(const Point3D& p) const {
    Vector3D v(p.x - point.x, p.y - point.y, p.z - point.z);
    return v.dot(direction);
}

Point3D Line::closest_point_to(const Point3D& p) const {
    double t = closest_parameter_to_point(p);
    return evaluate(t);
}

double Line::distance_to_point(const Point3D& p) const {
    return p.distance_to(closest_point_to(p));
}

std::string Line::to_string() const {
    std::stringstream ss;
    ss << "Line(point: " << point.to_string() << ", direction: " << direction.to_string() << ")";
    return ss.str();
}

// Circle implementation
Circle::Circle(const Point3D& center, double radius, const Vector3D& normal)
    : center(center), normal(normal.normalized()), radius(radius) {}

Point3D Circle::evaluate(double t) const {
    // Create orthonormal basis
    Vector3D n = normal;
    Vector3D arbitrary(1, 0, 0);
    if (std::abs(n.dot(arbitrary)) > 0.9) {
        arbitrary = Vector3D(0, 1, 0);
    }
    Vector3D u = n.cross(arbitrary).normalized();
    Vector3D v = n.cross(u).normalized();

    double x = center.x + radius * (std::cos(t) * u.x + std::sin(t) * v.x);
    double y = center.y + radius * (std::cos(t) * u.y + std::sin(t) * v.y);
    double z = center.z + radius * (std::cos(t) * u.z + std::sin(t) * v.z);
    return Point3D(x, y, z);
}

double Circle::circumference() const {
    return 2.0 * M_PI * radius;
}

double Circle::area() const {
    return M_PI * radius * radius;
}

double Circle::closest_parameter_to_point(const Point3D& p) const {
    Vector3D v(p.x - center.x, p.y - center.y, p.z - center.z);
    double dot_product = v.dot(normal);
    Vector3D proj(
        v.x - normal.x * dot_product,
        v.y - normal.y * dot_product,
        v.z - normal.z * dot_product
    );
    return std::atan2(proj.y, proj.x);
}

Point3D Circle::closest_point_to(const Point3D& p) const {
    double t = closest_parameter_to_point(p);
    return evaluate(t);
}

std::string Circle::to_string() const {
    std::stringstream ss;
    ss << "Circle(center: " << center.to_string() << ", radius: " << radius << ")";
    return ss.str();
}

// Arc implementation
Arc::Arc(const Point3D& center, double radius, const Vector3D& normal,
         double start_angle, double end_angle)
    : center(center), radius(radius), normal(normal.normalized()),
      start_angle(start_angle), end_angle(end_angle) {}

Point3D Arc::evaluate(double t) const {
    Circle circle(center, radius, normal);
    return circle.evaluate(t);
}

double Arc::arc_length() const {
    return radius * (end_angle - start_angle);
}

Point3D Arc::start_point() const {
    return evaluate(start_angle);
}

Point3D Arc::end_point() const {
    return evaluate(end_angle);
}

std::string Arc::to_string() const {
    std::stringstream ss;
    ss << "Arc(center: " << center.to_string() << ", radius: " << radius
       << ", start: " << start_angle << ", end: " << end_angle << ")";
    return ss.str();
}

// GeometryKernel implementation
GeometryKernel::GeometryKernel(double tolerance) : tolerance_(tolerance) {}

double GeometryKernel::point_distance(const Point3D& p1, const Point3D& p2) const {
    return p1.distance_to(p2);
}

bool GeometryKernel::points_equal(const Point3D& p1, const Point3D& p2) const {
    return p1.equals(p2, tolerance_);
}

double GeometryKernel::vector_magnitude(const Vector3D& v) const {
    return v.magnitude();
}

Vector3D GeometryKernel::vector_normalize(const Vector3D& v) const {
    return v.normalized();
}

double GeometryKernel::vector_dot(const Vector3D& v1, const Vector3D& v2) const {
    return v1.dot(v2);
}

Vector3D GeometryKernel::vector_cross(const Vector3D& v1, const Vector3D& v2) const {
    return v1.cross(v2);
}

Point3D GeometryKernel::line_evaluate(const Line& line, double t) const {
    return line.evaluate(t);
}

IntersectionResult GeometryKernel::line_line_intersection(const Line& l1, const Line& l2) const {
    IntersectionResult result;

    // Solve l1(s) = l2(t) for s, t
    // l1.point + s * l1.direction = l2.point + t * l2.direction
    Vector3D w(l1.point.x - l2.point.x, l1.point.y - l2.point.y, l1.point.z - l2.point.z);

    double a = l1.direction.dot(l1.direction);
    double b = l1.direction.dot(l2.direction);
    double c = l2.direction.dot(l2.direction);
    double d = l1.direction.dot(w);
    double e = l2.direction.dot(w);

    double denom = a * c - b * b;
    if (std::abs(denom) < tolerance_) {
        // Lines are parallel
        return result;
    }

    double s = (b * e - c * d) / denom;
    double t = (a * e - b * d) / denom;

    Point3D p1 = l1.evaluate(s);
    Point3D p2 = l2.evaluate(t);

    if (p1.equals(p2, tolerance_)) {
        result.intersects = true;
        result.points.push_back(p1);
        result.parameters.push_back(s);
        result.parameters.push_back(t);
    }

    return result;
}

IntersectionResult GeometryKernel::line_circle_intersection(const Line& line, const Circle& circle) const {
    IntersectionResult result;

    // Solve |line(t) - circle.center| = circle.radius
    Vector3D v(line.point.x - circle.center.x, line.point.y - circle.center.y, line.point.z - circle.center.z);
    double a = line.direction.dot(line.direction);
    double b = 2.0 * v.dot(line.direction);
    double c = v.dot(v) - circle.radius * circle.radius;

    double discriminant = b * b - 4 * a * c;
    if (discriminant < 0) {
        return result;
    }

    double sqrt_disc = std::sqrt(discriminant);
    double t1 = (-b - sqrt_disc) / (2 * a);
    double t2 = (-b + sqrt_disc) / (2 * a);

    result.intersects = true;
    result.points.push_back(line.evaluate(t1));
    result.parameters.push_back(t1);

    if (std::abs(t1 - t2) > tolerance_) {
        result.points.push_back(line.evaluate(t2));
        result.parameters.push_back(t2);
    }

    return result;
}

Point3D GeometryKernel::line_closest_point(const Line& line, const Point3D& p) const {
    return line.closest_point_to(p);
}

Point3D GeometryKernel::circle_evaluate(const Circle& circle, double angle) const {
    return circle.evaluate(angle);
}

IntersectionResult GeometryKernel::circle_circle_intersection(const Circle& c1, const Circle& c2) const {
    IntersectionResult result;

    double d = c1.center.distance_to(c2.center);
    if (d > c1.radius + c2.radius + tolerance_ || d < std::abs(c1.radius - c2.radius) - tolerance_) {
        return result;
    }

    // Circles intersect
    double a = (d * d + c1.radius * c1.radius - c2.radius * c2.radius) / (2 * d);
    double h = std::sqrt(c1.radius * c1.radius - a * a);

    Point3D p0(
        c1.center.x + a * (c2.center.x - c1.center.x) / d,
        c1.center.y + a * (c2.center.y - c1.center.y) / d,
        c1.center.z + a * (c2.center.z - c1.center.z) / d
    );

    double px = -h * (c2.center.y - c1.center.y) / d;
    double py = h * (c2.center.x - c1.center.x) / d;

    result.intersects = true;
    result.points.push_back(Point3D(p0.x + px, p0.y + py, p0.z));
    if (h > tolerance_) {
        result.points.push_back(Point3D(p0.x - px, p0.y - py, p0.z));
    }

    return result;
}

Point3D GeometryKernel::circle_closest_point(const Circle& circle, const Point3D& p) const {
    return circle.closest_point_to(p);
}

double GeometryKernel::plane_distance_to_point(const Plane& plane, const Point3D& p) const {
    return plane.distance_to_point(p);
}

Point3D GeometryKernel::plane_project_point(const Plane& plane, const Point3D& p) const {
    return plane.project_point(p);
}

IntersectionResult GeometryKernel::plane_line_intersection(const Plane& plane, const Line& line) const {
    IntersectionResult result;

    double denom = line.direction.dot(plane.normal);
    if (std::abs(denom) < tolerance_) {
        return result;  // Line parallel to plane
    }

    Vector3D v(plane.origin.x - line.point.x, plane.origin.y - line.point.y, plane.origin.z - line.point.z);
    double t = v.dot(plane.normal) / denom;

    result.intersects = true;
    result.points.push_back(line.evaluate(t));
    result.parameters.push_back(t);

    return result;
}

double GeometryKernel::point_to_line_distance(const Point3D& p, const Line& line) const {
    return line.distance_to_point(p);
}

double GeometryKernel::point_to_circle_distance(const Point3D& p, const Circle& circle) const {
    double dist_to_center = p.distance_to(circle.center);
    return std::abs(dist_to_center - circle.radius);
}

}  // namespace bendcore::geometry
