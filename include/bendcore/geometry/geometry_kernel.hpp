#pragma once

#include <cmath>
#include <optional>
#include <string>
#include <vector>
#include <array>

namespace bendcore::geometry {

/**
 * CAD-CORE-013: GeometryKernel
 * Базовые геометрические операции, примитивы и утилиты для CAD ядра
 */

// Глобальная точность для вычислений
constexpr double DEFAULT_TOLERANCE = 1e-10;
constexpr double DEFAULT_ANGULAR_TOLERANCE = 1e-9;

/**
 * Point3D — точка в 3D пространстве
 */
struct Point3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Point3D() = default;
    Point3D(double x, double y, double z) : x(x), y(y), z(z) {}

    [[nodiscard]] double distance_to(const Point3D& other) const;
    [[nodiscard]] bool equals(const Point3D& other, double tolerance = DEFAULT_TOLERANCE) const;
    [[nodiscard]] std::string to_string() const;
};

/**
 * Vector3D — вектор в 3D пространстве
 */
struct Vector3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vector3D() = default;
    Vector3D(double x, double y, double z) : x(x), y(y), z(z) {}

    [[nodiscard]] double magnitude() const;
    [[nodiscard]] Vector3D normalized() const;
    [[nodiscard]] double dot(const Vector3D& other) const;
    [[nodiscard]] Vector3D cross(const Vector3D& other) const;
    [[nodiscard]] double angle_to(const Vector3D& other) const;
    [[nodiscard]] bool equals(const Vector3D& other, double tolerance = DEFAULT_TOLERANCE) const;
    [[nodiscard]] bool is_parallel_to(const Vector3D& other, double tolerance = DEFAULT_ANGULAR_TOLERANCE) const;
    [[nodiscard]] bool is_perpendicular_to(const Vector3D& other, double tolerance = DEFAULT_ANGULAR_TOLERANCE) const;
    [[nodiscard]] std::string to_string() const;
};

/**
 * Plane — плоскость в 3D пространстве (определяется точкой и нормалью)
 */
struct Plane {
    Point3D origin;
    Vector3D normal;
    Vector3D u_axis;  // Первая ось плоскости
    Vector3D v_axis;  // Вторая ось плоскости (вычисляется)

    Plane() = default;
    Plane(const Point3D& origin, const Vector3D& normal);

    [[nodiscard]] Vector3D get_v_axis() const { return v_axis; }
    [[nodiscard]] double distance_to_point(const Point3D& p) const;
    [[nodiscard]] Point3D project_point(const Point3D& p) const;
    [[nodiscard]] bool contains_point(const Point3D& p, double tolerance = DEFAULT_TOLERANCE) const;
    [[nodiscard]] std::string to_string() const;
};

/**
 * Transform — преобразование (матрица 4x4)
 */
class Transform {
public:
    Transform();
    explicit Transform(const std::array<std::array<double, 4>, 4>& matrix);

    [[nodiscard]] static Transform identity();
    [[nodiscard]] static Transform translation(double dx, double dy, double dz);
    [[nodiscard]] static Transform rotation_x(double angle);
    [[nodiscard]] static Transform rotation_y(double angle);
    [[nodiscard]] static Transform rotation_z(double angle);
    [[nodiscard]] static Transform rotation_axis(const Vector3D& axis, double angle);
    [[nodiscard]] static Transform scaling(double sx, double sy, double sz);

    [[nodiscard]] Point3D apply_to_point(const Point3D& p) const;
    [[nodiscard]] Vector3D apply_to_vector(const Vector3D& v) const;
    [[nodiscard]] Transform inverse() const;
    [[nodiscard]] Transform compose(const Transform& other) const;
    [[nodiscard]] std::string to_string() const;

private:
    std::array<std::array<double, 4>, 4> matrix_;
};

/**
 * Line — бесконечная линия в 3D пространстве
 */
struct Line {
    Point3D point;
    Vector3D direction;

    Line() = default;
    Line(const Point3D& point, const Vector3D& direction);

    [[nodiscard]] Point3D evaluate(double t) const;
    [[nodiscard]] double closest_parameter_to_point(const Point3D& p) const;
    [[nodiscard]] Point3D closest_point_to(const Point3D& p) const;
    [[nodiscard]] double distance_to_point(const Point3D& p) const;
    [[nodiscard]] std::string to_string() const;
};

/**
 * Circle — окружность в 3D пространстве
 */
struct Circle {
    Point3D center;
    Vector3D normal;  // Нормаль к плоскости окружности
    double radius = 0.0;

    Circle() = default;
    Circle(const Point3D& center, double radius, const Vector3D& normal);

    [[nodiscard]] Point3D evaluate(double t) const;  // t в [0, 2*pi]
    [[nodiscard]] double circumference() const;
    [[nodiscard]] double area() const;
    [[nodiscard]] double closest_parameter_to_point(const Point3D& p) const;
    [[nodiscard]] Point3D closest_point_to(const Point3D& p) const;
    [[nodiscard]] std::string to_string() const;
};

/**
 * Arc — дуга окружности в 3D пространстве
 */
struct Arc {
    Point3D center;
    double radius = 0.0;
    Vector3D normal;  // Нормаль к плоскости дуги
    double start_angle = 0.0;
    double end_angle = 0.0;

    Arc() = default;
    Arc(const Point3D& center, double radius, const Vector3D& normal,
        double start_angle, double end_angle);

    [[nodiscard]] Point3D evaluate(double t) const;  // t в [start_angle, end_angle]
    [[nodiscard]] double arc_length() const;
    [[nodiscard]] Point3D start_point() const;
    [[nodiscard]] Point3D end_point() const;
    [[nodiscard]] std::string to_string() const;
};

/**
 * IntersectionResult — результат операции пересечения
 */
struct IntersectionResult {
    bool intersects = false;
    std::vector<Point3D> points;
    std::vector<double> parameters;  // Параметры пересечения (если применимо)

    [[nodiscard]] bool has_intersection() const { return intersects && !points.empty(); }
};

/**
 * GeometryKernel — основной класс для геометрических операций
 */
class GeometryKernel {
public:
    GeometryKernel(double tolerance = DEFAULT_TOLERANCE);

    // Операции с точками
    [[nodiscard]] double point_distance(const Point3D& p1, const Point3D& p2) const;
    [[nodiscard]] bool points_equal(const Point3D& p1, const Point3D& p2) const;

    // Операции с вектора...ми
    [[nodiscard]] double vector_magnitude(const Vector3D& v) const;
    [[nodiscard]] Vector3D vector_normalize(const Vector3D& v) const;
    [[nodiscard]] double vector_dot(const Vector3D& v1, const Vector3D& v2) const;
    [[nodiscard]] Vector3D vector_cross(const Vector3D& v1, const Vector3D& v2) const;

    // Операции с линиями
    [[nodiscard]] Point3D line_evaluate(const Line& line, double t) const;
    [[nodiscard]] IntersectionResult line_line_intersection(const Line& l1, const Line& l2) const;
    [[nodiscard]] IntersectionResult line_circle_intersection(const Line& line, const Circle& circle) const;
    [[nodiscard]] Point3D line_closest_point(const Line& line, const Point3D& p) const;

    // Операции с окружностями
    [[nodiscard]] Point3D circle_evaluate(const Circle& circle, double angle) const;
    [[nodiscard]] IntersectionResult circle_circle_intersection(const Circle& c1, const Circle& c2) const;
    [[nodiscard]] Point3D circle_closest_point(const Circle& circle, const Point3D& p) const;

    // Операции с плоскостями
    [[nodiscard]] double plane_distance_to_point(const Plane& plane, const Point3D& p) const;
    [[nodiscard]] Point3D plane_project_point(const Plane& plane, const Point3D& p) const;
    [[nodiscard]] IntersectionResult plane_line_intersection(const Plane& plane, const Line& line) const;

    // Proximity detection
    [[nodiscard]] double point_to_line_distance(const Point3D& p, const Line& line) const;
    [[nodiscard]] double point_to_circle_distance(const Point3D& p, const Circle& circle) const;

    // Tolerance management
    [[nodiscard]] double get_tolerance() const { return tolerance_; }
    void set_tolerance(double tolerance) { tolerance_ = tolerance; }

private:
    double tolerance_;
};

}  // namespace bendcore::geometry
