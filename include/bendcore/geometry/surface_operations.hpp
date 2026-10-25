#pragma once

#include <vector>
#include <optional>
#include <memory>

#include "bendcore/geometry/geometry_kernel.hpp"
#include "bendcore/geometry/curve_operations.hpp"

namespace bendcore::geometry {

/**
 * CAD-CORE-016: SurfaceOperations
 * NURBS поверхности для моделирования оболочек труб
 */

/**
 * NurbsSurface — NURBS поверхность
 */
class NurbsSurface {
public:
    NurbsSurface();
    NurbsSurface(const std::vector<std::vector<Point3D>>& control_points,
                 const std::vector<double>& u_knots,
                 const std::vector<double>& v_knots,
                 const std::vector<std::vector<double>>& weights,
                 int u_degree,
                 int v_degree);

    // Параметры
    [[nodiscard]] int u_degree() const { return u_degree_; }
    [[nodiscard]] int v_degree() const { return v_degree_; }
    [[nodiscard]] int u_control_points() const { return control_points_.size(); }
    [[nodiscard]] int v_control_points() const {
        return control_points_.empty() ? 0 : control_points_[0].size();
    }

    // Вычисление поверхности
    [[nodiscard]] Point3D evaluate(double u, double v) const;
    [[nodiscard]] Vector3D normal(double u, double v) const;
    [[nodiscard]] Point3D closest_point_on_surface(const Point3D& p, int iterations = 10) const;

    // Кривая-поверхность пересечение
    [[nodiscard]] std::vector<Point3D> intersect_with_curve(const NurbsCurve& curve, int samples = 100) const;

    // Surface offset
    [[nodiscard]] std::unique_ptr<NurbsSurface> offset(double distance) const;

    // Проверка валидности
    [[nodiscard]] bool is_valid() const;
    [[nodiscard]] bool is_surface_closed_u() const;
    [[nodiscard]] bool is_surface_closed_v() const;

    [[nodiscard]] std::string to_string() const;

private:
    std::vector<std::vector<Point3D>> control_points_;
    std::vector<double> u_knots_;
    std::vector<double> v_knots_;
    std::vector<std::vector<double>> weights_;
    int u_degree_ = 0;
    int v_degree_ = 0;

    [[nodiscard]] double basis_function_u(int i, int p, double u) const;
    [[nodiscard]] double basis_function_v(int j, int q, double v) const;
};

/**
 * TrimmedSurface — обрезанная NURBS поверхность
 */
class TrimmedSurface {
public:
    TrimmedSurface() = default;
    TrimmedSurface(const std::shared_ptr<NurbsSurface>& surface,
                   const std::vector<std::vector<Point3D>>& trim_boundaries);

    [[nodiscard]] const std::shared_ptr<NurbsSurface>& get_surface() const { return surface_; }
    [[nodiscard]] const std::vector<std::vector<Point3D>>& get_trim_boundaries() const { return trim_boundaries_; }

    [[nodiscard]] Point3D evaluate(double u, double v) const;
    [[nodiscard]] Vector3D normal(double u, double v) const;
    [[nodiscard]] bool is_point_inside_trim_region(double u, double v) const;

    [[nodiscard]] std::string to_string() const;

private:
    std::shared_ptr<NurbsSurface> surface_;
    std::vector<std::vector<Point3D>> trim_boundaries_;
};

/**
 * RevolutionSurface — поверхность вращения (для создания тел вращения)
 */
class RevolutionSurface {
public:
    RevolutionSurface() = default;
    RevolutionSurface(const std::shared_ptr<NurbsCurve>& profile,
                      const Point3D& axis_point,
                      const Vector3D& axis_direction,
                      double angle = 2.0 * M_PI);

    [[nodiscard]] Point3D evaluate(double u, double v) const;  // u — параметр профиля, v — угол
    [[nodiscard]] Vector3D normal(double u, double v) const;

    [[nodiscard]] std::string to_string() const;

private:
    std::shared_ptr<NurbsCurve> profile_;
    Point3D axis_point_;
    Vector3D axis_direction_;
    double angle_ = 2.0 * M_PI;
};

/**
 * SurfaceOfRevolution — специализированная поверхность для труб (вращение окружности вокруг оси)
 */
class SurfaceOfRevolution {
public:
    SurfaceOfRevolution() = default;
    SurfaceOfRevolution(const std::shared_ptr<AnalyticCurve>& centerline,
                        double tube_radius);

    [[nodiscard]] Point3D evaluate(double u, double v) const;
    [[nodiscard]] Vector3D normal(double u, double v) const;
    [[nodiscard]] double surface_area(int u_samples = 50, int v_samples = 50) const;

    // Wall thickness offset (для создания толщины стенок)
    [[nodiscard]] std::unique_ptr<SurfaceOfRevolution> offset_wall_thickness(double thickness) const;

    [[nodiscard]] std::string to_string() const;

private:
    std::shared_ptr<AnalyticCurve> centerline_;
    double tube_radius_ = 0.0;
};

/**
 * Surface metrics
 */
struct SurfaceMetrics {
    double surface_area = 0.0;
    double perimeter = 0.0;
    Vector3D center_of_mass;
    double gaussian_curvature = 0.0;
    double mean_curvature = 0.0;
};

/**
 * SurfaceOperations — основной класс для работы с поверхностями
 */
class SurfaceOperations {
public:
    SurfaceOperations(double tolerance = DEFAULT_TOLERANCE);

    // Создание NURBS поверхностей
    [[nodiscard]] std::unique_ptr<NurbsSurface> create_nurbs_surface(
        const std::vector<std::vector<Point3D>>& control_points,
        const std::vector<double>& u_knots,
        const std::vector<double>& v_knots,
        const std::vector<std::vector<double>>& weights,
        int u_degree,
        int v_degree);

    // Создание поверхностей вращения
    [[nodiscard]] std::unique_ptr<RevolutionSurface> create_revolution_surface(
        const std::shared_ptr<NurbsCurve>& profile,
        const Point3D& axis_point,
        const Vector3D& axis_direction,
        double angle = 2.0 * M_PI);

    // Создание поверхностей труб
    [[nodiscard]] std::unique_ptr<SurfaceOfRevolution> create_tube_surface(
        const std::shared_ptr<AnalyticCurve>& centerline,
        double radius);

    // Создание обрезанных поверхностей
    [[nodiscard]] std::unique_ptr<TrimmedSurface> create_trimmed_surface(
        const std::shared_ptr<NurbsSurface>& surface,
        const std::vector<std::vector<Point3D>>& trim_boundaries);

    // Операции с поверхностями
    [[nodiscard]] Point3D surface_evaluate(const NurbsSurface& surface, double u, double v) const;
    [[nodiscard]] Vector3D surface_normal(const NurbsSurface& surface, double u, double v) const;

    // Пересечение кривой с поверхностью
    [[nodiscard]] std::vector<Point3D> curve_surface_intersection(
        const NurbsCurve& curve,
        const NurbsSurface& surface,
        int samples = 50) const;

    // Вычисление метрик поверхности
    [[nodiscard]] SurfaceMetrics compute_surface_metrics(
        const NurbsSurface& surface,
        int u_samples = 50,
        int v_samples = 50) const;

    // Offset поверхности
    [[nodiscard]] std::unique_ptr<NurbsSurface> offset_surface(
        const NurbsSurface& surface,
        double distance) const;

    // Толщина стенок (для труб)
    [[nodiscard]] std::unique_ptr<NurbsSurface> create_wall_thickness(
        const NurbsSurface& surface,
        double thickness) const;

private:
    double tolerance_;
};

}  // namespace bendcore::geometry
