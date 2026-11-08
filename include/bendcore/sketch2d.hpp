// 2D-эскиз поверх PlaneGCS (LGPL-2.1-or-later): точки, отрезки, окружности, нативные дуги,
// эллипсы и дуги эллипсов, B-сплайны (полюса — точки эскиза), вспомогательная геометрия;
// геометрические ограничения и размеры (управляющие и справочные); диагностика определённости.
// Документация: docs/modeling/SKETCH.md.
//
// Эскиз не хранит решатель между вызовами: каждый solve() строит систему PlaneGCS заново по
// текущему состоянию. Номер ограничения (>= 1) — его tag в диагностике (конфликты,
// избыточность). Номера точек и кривых каждого вида начинаются с 0 и не переиспользуются:
// удалённые элементы помечаются и исключаются из решения.
//
// Ошибки программирования (номер вне диапазона, неподходящий вид элемента) — исключения
// std::out_of_range / std::invalid_argument.
#pragma once

#include <string>
#include <vector>

namespace bendcore::sketch2d {

using Id = int;   // номер точки, кривой (в пределах вида) или ограничения (≥ 1)

struct Point2 {
    double x = 0.0;
    double y = 0.0;
};

enum class GeoKind { Point, Line, Circle, Arc, Ellipse, EllipseArc, BSpline };

const char* to_string(GeoKind kind) noexcept;

// Ссылка на элемент эскиза: вид + номер в пределах вида.
struct Geo {
    GeoKind kind = GeoKind::Point;
    Id id = -1;
    friend bool operator==(const Geo& a, const Geo& b) { return a.kind == b.kind && a.id == b.id; }
};

inline Geo point_geo(Id p) { return {GeoKind::Point, p}; }
inline Geo line_geo(Id l) { return {GeoKind::Line, l}; }
inline Geo circle_geo(Id c) { return {GeoKind::Circle, c}; }
inline Geo arc_geo(Id a) { return {GeoKind::Arc, a}; }
inline Geo ellipse_geo(Id e) { return {GeoKind::Ellipse, e}; }
inline Geo ellipse_arc_geo(Id e) { return {GeoKind::EllipseArc, e}; }
inline Geo bspline_geo(Id s) { return {GeoKind::BSpline, s}; }

enum class ConstraintKind {
    Coincident,         // точки a, b
    Horizontal,         // точки a, b
    Vertical,           // точки a, b
    Distance,           // точки a, b; value мм
    DistanceX,          // точки a, b; b.x − a.x = value (со знаком)
    DistanceY,          // точки a, b; b.y − a.y = value
    PointCurveDistance, // точка a, отрезок/окружность/дуга b; расстояние до прямой/окружности
    Radius,             // окружность/дуга a
    Diameter,           // окружность/дуга a
    Angle,              // отрезки a, b; угол от a к b, градусы
    ArcLength,          // дуга a
    MajorRadius,        // эллипс/дуга эллипса a
    MinorRadius,        // эллипс/дуга эллипса a
    Parallel,           // отрезки a, b
    Perpendicular,      // отрезки a, b
    Tangent,            // кривые a, b (с общим концом — касание в этом конце)
    Equal,              // отрезки (длины), окружности/дуги (радиусы), эллипсы (полуоси)
    Symmetric,          // точки a, b относительно отрезка c
    SymmetricPoint,     // точки a, b относительно точки c
    Midpoint,           // точка a — середина отрезка/дуги b
    PointOn,            // точка a на кривой b (отрезок — на прямой)
    Concentric,         // центры кривых a, b совпадают
    Fix,                // точка a закреплена в (value, value2)
    Rotation,           // точка b = точка a, повёрнутая вокруг точки c на value градусов
};

const char* to_string(ConstraintKind kind) noexcept;
// Размер (имеет числовое значение и может быть справочным).
bool is_dimension(ConstraintKind kind) noexcept;

struct ConstraintInfo {
    ConstraintKind kind = ConstraintKind::Coincident;
    Geo a, b, c;
    double value = 0.0;    // значение размера (мм; углы — градусы)
    double value2 = 0.0;   // Fix: y
    bool driving = true;   // false — справочный размер: в решении не участвует, измеряется
    bool removed = false;
};

enum class Definition { Fully, Under, Over };

const char* to_string(Definition d) noexcept;

struct GeoStatus {
    Geo geo;
    bool fully_defined = false;   // все параметры элемента определены ограничениями
};

struct Report {
    std::string status;              // определено | недоопределено | избыточно | конфликт
    int dof = 0;                     // степени свободы (по диагностике PlaneGCS)
    std::vector<Id> conflicting;     // ограничения, входящие в конфликт
    std::vector<Id> redundant;       // избыточные ограничения
    std::vector<Id> partially_redundant;
    Definition definition = Definition::Under;   // Over: конфликт или избыточность
    bool converged = false;          // решатель сошёлся, координаты обновлены
    std::vector<GeoStatus> geometry; // все живые точки и кривые
    // Статус элемента; false для неизвестного элемента.
    bool fully_defined(Geo g) const;
};

struct LineData {
    Id start = -1;
    Id end = -1;
};
struct CircleData {
    Id center = -1;
    double radius = 0.0;
};
// Дуга от start к end вокруг center: против часовой стрелки при ccw, иначе по часовой.
struct ArcData {
    Id center = -1;
    Id start = -1;
    Id end = -1;
    bool ccw = true;
    double radius = 0.0;
};
// Эллипс: центр, фокус (точки эскиза, фокус задаёт направление большой оси) и малая полуось.
struct EllipseData {
    Id center = -1;
    Id focus = -1;
    double minor_radius = 0.0;
};
// Дуга эллипса от start к end (ccw — против часовой).
struct EllipseArcData {
    Id center = -1;
    Id focus = -1;
    double minor_radius = 0.0;
    Id start = -1;
    Id end = -1;
    bool ccw = true;
};
// B-сплайн: полюса — точки эскиза; узлы различны, с кратностями (соглашение OCCT); веса пусто
// — нерациональный. Непериодический сплайн должен быть «зажатым» (кратность крайних узлов
// degree + 1): тогда его концы — первый и последний полюса.
struct BSplineData {
    std::vector<Id> poles;
    int degree = 3;
    std::vector<double> knots;
    std::vector<int> multiplicities;
    std::vector<double> weights;
    bool periodic = false;
};

class Sketch {
public:
    // ---- геометрия
    Id add_point(double x, double y);
    void fix_point(Id p);            // закрепить текущие координаты точки (служебно, без номера)
    void set_point(Id p, double x, double y);   // новое положение (у закреплённой — и закрепление)
    Id add_line(Id a, Id b);         // отрезок между точками
    Id add_circle(Id center, double radius);
    Id add_arc(Id center, Id start, Id end, bool ccw = true);
    Id add_ellipse(Id center, Id focus, double minor_radius);
    Id add_ellipse_arc(Id center, Id focus, double minor_radius, Id start, Id end, bool ccw = true);
    Id add_bspline(const BSplineData& data);

    void set_construction(Geo g, bool construction);
    bool is_construction(Geo g) const;
    void set_line_points(Id line, Id start, Id end);
    void set_arc_points(Id arc, Id start, Id end);
    // Удаление кривой или точки вместе с ограничениями, которые на неё ссылаются. Точки кривой
    // остаются. Точка, занятая живой кривой, не удаляется (std::invalid_argument).
    void remove(Geo g);
    bool is_removed(Geo g) const;
    // Ограничения (живые), ссылающиеся на элемент.
    std::vector<Id> constraints_of(Geo g) const;
    // Живые кривые, использующие точку (как конец, центр, фокус или полюс).
    std::vector<Geo> curves_using(Id point) const;

    std::size_t point_count() const noexcept { return points_.size(); }
    std::size_t count(GeoKind kind) const noexcept;

    // ---- ограничения (прежний API)
    Id constrain_coincident(Id a, Id b);
    Id constrain_horizontal(Id a, Id b);
    Id constrain_vertical(Id a, Id b);
    Id constrain_distance(Id a, Id b, double d);
    Id constrain_radius(Id circle, double r);
    Id constrain_parallel(Id l1, Id l2);             // отрезки ∥
    Id constrain_perpendicular(Id l1, Id l2);        // отрезки ⊥
    Id constrain_equal_length(Id l1, Id l2);         // равенство длин отрезков
    Id constrain_angle(Id l1, Id l2, double deg);    // угол между отрезками, градусы
    Id constrain_point_on_line(Id p, Id line);
    Id constrain_point_on_circle(Id p, Id circle);
    Id constrain_equal_radius(Id c1, Id c2);
    Id constrain_symmetric(Id p1, Id p2, Id axis);   // p1 и p2 симметричны относительно отрезка axis
    void remove_constraint(Id c);

    // ---- ограничения общего вида
    Id constrain_fix(Id p);                          // закрепление с номером (в диагностике)
    Id constrain_tangent(Geo a, Geo b);
    Id constrain_equal(Geo a, Geo b);
    Id constrain_concentric(Geo a, Geo b);
    Id constrain_midpoint(Id p, Geo line_or_arc);
    Id constrain_point_on(Id p, Geo curve);
    Id constrain_symmetric_point(Id p1, Id p2, Id center);
    Id constrain_rotation(Id p, Id q, Id center, double deg);

    // ---- размеры
    Id constrain_distance_x(Id a, Id b, double d);
    Id constrain_distance_y(Id a, Id b, double d);
    Id constrain_point_curve_distance(Id p, Geo curve, double d);
    Id constrain_length(Id line, double d);          // Distance между концами отрезка
    Id constrain_radius_of(Geo circle_or_arc, double r);
    Id constrain_diameter(Geo circle_or_arc, double d);
    Id constrain_arc_length(Id arc, double length);
    Id constrain_major_radius(Geo ellipse, double a);
    Id constrain_minor_radius(Geo ellipse, double b);

    void set_driving(Id c, bool driving);           // только для размеров
    void set_constraint_value(Id c, double value);
    const ConstraintInfo& constraint(Id c) const;
    std::size_t constraint_count() const noexcept { return constraints_.size(); }
    // Значение размера: у управляющего — заданное, у справочного — измеренное по геометрии.
    double constraint_value(Id c) const;
    // Измеренное значение размера по текущей геометрии.
    double measure(Id c) const;
    // Невязка ограничения по текущей геометрии (мм или радианы); 0 — выполнено.
    double residual(Id c) const;

    Report solve();                  // при успехе координаты обновляются

    // ---- чтение
    Point2 point(Id p) const;
    bool is_point_fixed(Id p) const;
    double circle_radius(Id c) const;
    LineData line(Id l) const;
    CircleData circle(Id c) const;
    ArcData arc(Id a) const;
    EllipseData ellipse(Id e) const;
    EllipseArcData ellipse_arc(Id e) const;
    const BSplineData& bspline(Id s) const;
    double arc_start_angle(Id a) const;   // угол начала дуги, радианы
    double arc_sweep(Id a) const;         // размах дуги (0, 2π], радианы
    double ellipse_major_radius(Geo e) const;
    Point2 ellipse_major_direction(Geo e) const;   // единичный вектор от центра к фокусу
    // Параметр точки на эллипсе (угол в системе большой/малой осей).
    double ellipse_parameter(Geo e, Point2 p) const;

private:
    struct PointRec {
        Point2 pos;
        bool fixed = false;
        Point2 pin;
        bool removed = false;
    };
    struct CurveRec {
        GeoKind kind = GeoKind::Line;
        Id p0 = -1;          // Line: start; Circle/Arc/Ellipse/EllipseArc: center
        Id p1 = -1;          // Line: end; Arc/EllipseArc: start; Ellipse*: focus в p3
        Id p2 = -1;          // Arc/EllipseArc: end
        Id p3 = -1;          // Ellipse/EllipseArc: focus
        double radius = 0.0; // Circle/Arc: радиус; Ellipse*: малая полуось
        bool ccw = true;
        bool construction = false;
        bool removed = false;
        BSplineData spline;
    };
    friend struct SolverModel;

    std::vector<PointRec> points_;
    std::vector<std::vector<CurveRec>> curves_ = std::vector<std::vector<CurveRec>>(7);
    std::vector<ConstraintInfo> constraints_;   // номер ограничения = индекс + 1

    const PointRec& prec(Id p) const;
    const CurveRec& crec(Geo g) const;
    CurveRec& crec_mut(Geo g);
    Id add_curve(CurveRec rec);
    Id add_constraint(ConstraintInfo info);
    void require(Geo g, std::initializer_list<GeoKind> kinds, const char* what) const;
    std::vector<Id> curve_points(const CurveRec& c) const;
    std::vector<Id> curve_ends(Geo g) const;   // концы кривой (пусто у замкнутых)
    void check_arc_ends(const CurveRec& c) const;
};

}  // namespace bendcore::sketch2d
