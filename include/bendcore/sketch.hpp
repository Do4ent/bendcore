// Ограничения эскиза, решатель и привязки (1.2, 1.3, 5.5, 5.6, 9.3). Порт sketch.py.
// Трёхмерный решатель собственный (по решению 19.10.2026): PlaneGCS — только для 2D-эскизов.
// Решатель: Левенберг–Марквардт на Eigen, якобиан конечными разностями (как в sketch.py).
#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "bendcore/vec3.hpp"

namespace bendcore::sketch {

// Ограничение или размер эскиза (1.3). Виды: fix, coincide, along_axis, parallel, perp, coplanar,
// equal_len, symmetry; размеры: dist, straight_S, to_tangent, bend_angle, plane_dist, line_dist.
// Источник — приоритет при перетаскивании: user_dim > explicit > auto. reference — справочный размер.
struct Constraint {
    std::string kind;
    std::vector<int> idx;                    // индексы точек; −1 у концов пути (straight_S)
    double value = 0.0;
    std::optional<Vec3> target;
    std::optional<Vec3> normal;
    std::optional<Vec3> origin;
    std::string label;
    std::string source = "explicit";
    bool reference = false;
    double R = 0.0;                          // радиус гиба для straight_S и to_tangent
};

inline constexpr double kShapeWeight = 1e-3;
double weight_of(const std::string& source);   // user_dim 100, explicit 10, auto 1; прочее — 10

bool is_dimension(const std::string& kind);

// Текущее значение размера (для справочных и для невязки).
double measure(const std::vector<Vec3>& P, const Constraint& c);

struct SolveResult {
    std::string status;                      // определено | недоопределено | конфликт
    int dof = 0;
    std::vector<Vec3> points;
    double residual = 0.0;
    std::vector<std::string> conflicts;
    std::map<std::string, double> reference_values;
};

// Решатель (1.3). preserve_shape — режим перетаскивания: веса приоритета и слабое сохранение формы.
SolveResult solve(const std::vector<Vec3>& points, const std::vector<Constraint>& cons, bool preserve_shape = false);

// Диалог «Конфликт» (1.3).
std::vector<Constraint> delete_constraint(const std::vector<Constraint>& cons, const std::string& label);
std::vector<Constraint> make_reference(const std::vector<Constraint>& cons, const std::string& label);  // invalid_argument

// Привязки (1.2): порт → вершина → признак → направляющая → сетка → свободно.
struct Grid {
    Vec3 origin;
    Vec3 u;
    Vec3 v;
    double step;
};
struct Guide {
    Vec3 origin;
    Vec3 direction;
};
struct SnapResult {
    Vec3 point;
    std::string kind;
};
SnapResult snap_point(const Vec3& target, const std::vector<Vec3>& ports = {}, const std::vector<Vec3>& features = {},
                      double radius = 5.0, const std::vector<Vec3>& vertices = {},
                      const std::vector<Guide>& guides = {}, const std::optional<Grid>& grid = std::nullopt);

Vec3 ortho_step(const Vec3& prev, const Vec3& target);   // ортогональный режим: одна ось за шаг
double snap_angle(double angle_deg, const std::vector<double>& allowed);

inline constexpr const char* kScaleWarning = "Угол вне шкалы: точность ±2°";

struct BendSnap {
    double angle;
    std::optional<std::string> warning;
};
BendSnap snap_bend_angle(double angle_deg, bool manual, const std::vector<double>& preferred = {},
                         const std::vector<double>& ticks = {}, const std::vector<double>& stops = {},
                         std::optional<double> step = std::nullopt, bool typed = false,
                         double hard_radius = 7.5, double soft_radius = 2.0, double eps = 1e-6);

// 9.3: дуга на входе → вершина с радиусом профиля.
inline constexpr double kArcTolR = 0.1;   // мм
struct EntryArc {
    std::string status;                      // ok | conflict
    std::optional<double> R;
    std::string default_option;              // для конфликта: keep_body
    std::vector<std::string> options;
    std::string report;
};
EntryArc entry_arc_to_vertex(double R_arc, double R_profile, double tol_R = kArcTolR);

}  // namespace bendcore::sketch
