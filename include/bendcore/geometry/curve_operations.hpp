#pragma once

#include <vector>
#include <optional>
#include <memory>

#include "bendcore/geometry/geometry_kernel.hpp"

namespace bendcore::geometry {

/**
 * CAD-CORE-015: CurveOperations
 * NURBS и аналитические кривые для труб и гибочных операций
 */

/**
 * NurbsCurve — NURBS кривая (Non-Uniform Rational B-Spline)
 */
class NurbsCurve {
public:
    NurbsCurve();
    NurbsCurve(const std::vector<Point3D>& control_points,
               const std::vector<double>& knot_vector,
               const std::vector<double>& weights,
               int degree);

    // Параметры
    [[nodiscard]] int degree() const { return degree_; }
    [[nodiscard]] int control_point_count() const { return control_points_.size(); }
    [[nodiscard]] const std::vector<Point3D>& control_points() const { return control_points_; }
    [[nodiscard]] const std::vector<double>& weights() const { return weights_; }
    [[nodiscard]] const std::vector<double>& knot_vector() const { return knot_vector_; }

    // Вычисление кривой
    [[nodiscard]] Point3D evaluate(double u) const;
    [[nodiscard]] Vector3D tangent(double u) const;
    [[nodiscard]] Vector3D curvature(double u) const;
    [[nodiscard]] double parameter_range_start() const;
    [[nodiscard]] double parameter_range_end() const;

    // Arc-length параметризация
    [[nodiscard]] double arc_length(double u1, double u2, int samples = 100) const;
    [[nodiscard]] double total_arc_length(int samples = 100) const;
    [[nodiscard]] std::optional<double> arc_length_parameter(double arc_length, int samples = 100) const;

    // Offset (параллельная кривая)
    [[nodiscard]] std::unique_ptr<NurbsCurve> offset(double distance, const Vector3D& normal) const;

    // Проверка валидности
    [[nodiscard]] bool is_valid() const;

    [[nodiscard]] std::string to_string() const;

private:
    std::vector<Point3D> control_points_;
    std::vector<double> knot_vector_;
    std::vector<double> weights_;
    int degree_ = 0;

    // Кэширование
    mutable std::vector<double> arc_length_cache_;
    mutable bool arc_length_computed_ = false;

    [[nodiscard]] double basis_function(int i, int p, double u) const;
    [[nodiscard]] double rational_basis_function(int i, int p, double u) const;
};

/**
 * BezierCurve — кубическая кривая Безье (частный случай NURBS)
 */
class BezierCurve {
public:
    BezierCurve();
    explicit BezierCurve(const std::vector<Point3D>& control_points);

    [[nodiscard]] int control_point_count() const { return control_points_.size(); }
    [[nodiscard]] const std::vector<Point3D>& control_points() const { return control_points_; }

    [[nodiscard]] Point3D evaluate(double t) const;  // t в [0, 1]
    [[nodiscard]] Vector3D tangent(double t) const;
    [[nodiscard]] double arc_length(int samples = 100) const;

    [[nodiscard]] std::string to_string() const;

private:
    std::vector<Point3D> control_points_;

    [[nodiscard]] double binomial_coefficient(int n, int k) const;
};

/**
 * AnalyticCurve — аналитические кривые (линия, окружность, эллипс)
 */
class AnalyticCurve {
public:
    enum class Type {
        Line,
        Circle,
        Arc,
        Ellipse,
        Spiral
    };

    AnalyticCurve() = default;

    // Создание аналитических кривых
    [[nodiscard]] static AnalyticCurve create_line(const Point3D& start, const Point3D& end);
    [[nodiscard]] static AnalyticCurve create_circle(const Point3D& center, double radius, const Vector3D& normal);
    [[nodiscard]] static AnalyticCurve create_arc(const Point3D& center, double radius, const Vector3D& normal,
                                                   double start_angle, double end_angle);
    [[nodiscard]] static AnalyticCurve create_ellipse(const Point3D& center, double semi_major, double semi_minor,
                                                       const Vector3D& normal);
    [[nodiscard]] static AnalyticCurve create_spiral(const Point3D& start, const Vector3D& axis,
                                                      double radius, double pitch, int turns);

    [[nodiscard]] Type get_type() const { return type_; }
    [[nodiscard]] Point3D evaluate(double t) const;
    [[nodiscard]] Vector3D tangent(double t) const;
    [[nodiscard]] double arc_length(int samples = 100) const;
    [[nodiscard]] bool is_closed() const;

    [[nodiscard]] std::string to_string() const;

private:
    Type type_ = Type::Line;
    Point3D start_;
    Point3D center_;
    Vector3D normal_;
    Vector3D axis_;
    double radius_ = 0.0;
    double semi_major_ = 0.0;
    double semi_minor_ = 0.0;
    double pitch_ = 0.0;
    int turns_ = 0;
    Point3D end_;

    AnalyticCurve(Type type) : type_(type) {}
};

/**
 * TubeCurve — специализированная кривая для моделирования труб
 */
class TubeCurve {
public:
    TubeCurve();
    explicit TubeCurve(const std::shared_ptr<NurbsCurve>& centerline, double tube_radius);
    explicit TubeCurve(const std::shared_ptr<AnalyticCurve>& centerline, double tube_radius);

    // Параметры трубы
    [[nodiscard]] double tube_radius() const { return tube_radius_; }
    [[nodiscard]] double centerline_length(int samples = 100) const;

    // Вычисление поверхности трубы
    [[nodiscard]] Point3D surface_point(double u, double v) const;  // u — параметр кривой, v — угол [0, 2π]
    [[nodiscard]] Vector3D surface_normal(double u, double v) const;

    // Offset трубы
    [[nodiscard]] std::unique_ptr<TubeCurve> offset(double wall_thickness) const;

    // Проверка пересечений
    [[nodiscard]] bool intersects_with(const TubeCurve& other, double tolerance = DEFAULT_TOLERANCE) const;

    [[nodiscard]] std::string to_string() const;

private:
    std::shared_ptr<NurbsCurve> nurbs_centerline_;
    std::shared_ptr<AnalyticCurve> analytic_centerline_;
    double tube_radius_ = 0.0;

    [[nodiscard]] Vector3D get_binormal(double u) const;
    [[nodiscard]] Vector3D get_normal(double u) const;
};

/**
 * CurveOperations — основной класс для работы с кривыми
 */
class CurveOperations {
public:
    CurveOperations(double tolerance = DEFAULT_TOLERANCE);

    // Создание NURBS кривых
    [[nodiscard]] std::unique_ptr<NurbsCurve> create_nurbs_curve(
        const std::vector<Point3D>& control_points,
        const std::vector<double>& knot_vector,
        const std::vector<double>& weights,
        int degree);

    // Создание аналитических кривых
    [[nodiscard]] std::unique_ptr<AnalyticCurve> create_line(const Point3D& start, const Point3D& end);
    [[nodiscard]] std::unique_ptr<AnalyticCurve> create_circle(const Point3D& center, double radius, const Vector3D& normal);
    [[nodiscard]] std::unique_ptr<AnalyticCurve> create_arc(const Point3D& center, double radius, const Vector3D& normal,
                                                             double start_angle, double end_angle);

    // Создание tube curves
    [[nodiscard]] std::unique_ptr<TubeCurve> create_tube_from_nurbs(
        const std::shared_ptr<NurbsCurve>& centerline,
        double radius);
    [[nodiscard]] std::unique_ptr<TubeCurve> create_tube_from_analytic(
        const std::shared_ptr<AnalyticCurve>& centerline,
        double radius);

    // Interpolation
    [[nodiscard]] std::unique_ptr<NurbsCurve> interpolate_points(
        const std::vector<Point3D>& points,
        int degree = 3);

    // Curve operations
    [[nodiscard]] double curve_length(const NurbsCurve& curve, int samples = 100) const;
    [[nodiscard]] double curve_length(const AnalyticCurve& curve, int samples = 100) const;
    [[nodiscard]] Point3D curve_evaluate(const NurbsCurve& curve, double u) const;
    [[nodiscard]] Vector3D curve_tangent(const NurbsCurve& curve, double u) const;

private:
    double tolerance_;
};

}  // namespace bendcore::geometry
