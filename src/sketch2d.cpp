// 2D-эскиз поверх PlaneGCS (docs/modeling/SKETCH.md).
#include "bendcore/sketch2d.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <numbers>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <unordered_set>

#include "GCS.h"

namespace bendcore::sketch2d {
namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kDeg = kPi / 180.0;

int tag_of(std::size_t index) {
    return static_cast<int>(index) + 1;
}

double wrap_pi(double a) {
    a = std::remainder(a, 2 * kPi);
    return a;
}

// Угол в [0, 2π).
double wrap_2pi(double a) {
    a = std::fmod(a, 2 * kPi);
    if (a < 0) a += 2 * kPi;
    return a;
}

double dist(Point2 a, Point2 b) {
    return std::hypot(b.x - a.x, b.y - a.y);
}

std::size_t kind_index(GeoKind k) {
    return static_cast<std::size_t>(k);
}

bool is_curve(GeoKind k) {
    return k != GeoKind::Point;
}

bool is_circular(GeoKind k) {
    return k == GeoKind::Circle || k == GeoKind::Arc;
}

bool is_elliptic(GeoKind k) {
    return k == GeoKind::Ellipse || k == GeoKind::EllipseArc;
}

std::string geo_text(Geo g) {
    return std::string(to_string(g.kind)) + " " + std::to_string(g.id);
}

// Поворот точки: q = c + R(θ)(p − c), по строке X (row = 0) или Y (row = 1).
// pvec = {px, py, qx, qy, cx, cy}.
class ConstraintRotation final : public GCS::Constraint {
public:
    ConstraintRotation(GCS::Point& p, GCS::Point& q, GCS::Point& c, double angle, int row)
        : co_(std::cos(angle)), si_(std::sin(angle)), row_(row) {
        pvec = {p.x, p.y, q.x, q.y, c.x, c.y};
        origpvec = pvec;
        rescale();
    }
    void errorgrad(double* err, double* grad, double* param) override {
        const double px = *pvec[0], py = *pvec[1], qx = *pvec[2], qy = *pvec[3], cx = *pvec[4], cy = *pvec[5];
        // Частные производные по px, py, qx, qy, cx, cy.
        double d[6];
        if (row_ == 0) {
            if (err) *err = qx - cx - co_ * (px - cx) + si_ * (py - cy);
            const double dd[6] = {-co_, si_, 1.0, 0.0, -1.0 + co_, -si_};
            std::copy(dd, dd + 6, d);
        } else {
            if (err) *err = qy - cy - si_ * (px - cx) - co_ * (py - cy);
            const double dd[6] = {-si_, -co_, 0.0, 1.0, si_, -1.0 + co_};
            std::copy(dd, dd + 6, d);
        }
        if (grad) {
            *grad = 0.0;
            for (int i = 0; i < 6; ++i) {
                if (pvec[i] == param) *grad += d[i];
            }
        }
    }

private:
    double co_, si_;
    int row_;
};

// Большая полуось эллипса: sqrt(|F − C|² + b²) = a. pvec = {cx, cy, fx, fy, b}.
class ConstraintMajorRadius final : public GCS::Constraint {
public:
    ConstraintMajorRadius(GCS::Ellipse& e, double* a) : a_(a) {
        pvec = {e.center.x, e.center.y, e.focus1.x, e.focus1.y, e.radmin};
        origpvec = pvec;
        rescale();
    }
    void errorgrad(double* err, double* grad, double* param) override {
        const double dx = *pvec[2] - *pvec[0], dy = *pvec[3] - *pvec[1], b = *pvec[4];
        const double s = std::sqrt(dx * dx + dy * dy + b * b);
        if (err) *err = s - *a_;
        if (grad) {
            const double d[5] = {-dx / s, -dy / s, dx / s, dy / s, b / s};
            *grad = 0.0;
            for (int i = 0; i < 5; ++i) {
                if (pvec[i] == param) *grad += d[i];
            }
        }
    }

private:
    double* a_;
};

}  // namespace

const char* to_string(GeoKind kind) noexcept {
    switch (kind) {
        case GeoKind::Point: return "point";
        case GeoKind::Line: return "line";
        case GeoKind::Circle: return "circle";
        case GeoKind::Arc: return "arc";
        case GeoKind::Ellipse: return "ellipse";
        case GeoKind::EllipseArc: return "ellipse-arc";
        case GeoKind::BSpline: return "bspline";
    }
    return "unknown";
}

const char* to_string(ConstraintKind kind) noexcept {
    switch (kind) {
        case ConstraintKind::Coincident: return "coincident";
        case ConstraintKind::Horizontal: return "horizontal";
        case ConstraintKind::Vertical: return "vertical";
        case ConstraintKind::Distance: return "distance";
        case ConstraintKind::DistanceX: return "distance-x";
        case ConstraintKind::DistanceY: return "distance-y";
        case ConstraintKind::PointCurveDistance: return "point-curve-distance";
        case ConstraintKind::Radius: return "radius";
        case ConstraintKind::Diameter: return "diameter";
        case ConstraintKind::Angle: return "angle";
        case ConstraintKind::ArcLength: return "arc-length";
        case ConstraintKind::MajorRadius: return "major-radius";
        case ConstraintKind::MinorRadius: return "minor-radius";
        case ConstraintKind::Parallel: return "parallel";
        case ConstraintKind::Perpendicular: return "perpendicular";
        case ConstraintKind::Tangent: return "tangent";
        case ConstraintKind::Equal: return "equal";
        case ConstraintKind::Symmetric: return "symmetric";
        case ConstraintKind::SymmetricPoint: return "symmetric-point";
        case ConstraintKind::Midpoint: return "midpoint";
        case ConstraintKind::PointOn: return "point-on";
        case ConstraintKind::Concentric: return "concentric";
        case ConstraintKind::Fix: return "fix";
        case ConstraintKind::Rotation: return "rotation";
    }
    return "unknown";
}

bool is_dimension(ConstraintKind kind) noexcept {
    switch (kind) {
        case ConstraintKind::Distance:
        case ConstraintKind::DistanceX:
        case ConstraintKind::DistanceY:
        case ConstraintKind::PointCurveDistance:
        case ConstraintKind::Radius:
        case ConstraintKind::Diameter:
        case ConstraintKind::Angle:
        case ConstraintKind::ArcLength:
        case ConstraintKind::MajorRadius:
        case ConstraintKind::MinorRadius: return true;
        default: return false;
    }
}

const char* to_string(Definition d) noexcept {
    switch (d) {
        case Definition::Fully: return "fully-defined";
        case Definition::Under: return "under-defined";
        case Definition::Over: return "over-defined";
    }
    return "unknown";
}

bool Report::fully_defined(Geo g) const {
    for (const auto& s : geometry) {
        if (s.geo == g) return s.fully_defined;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------
// Геометрия
// ---------------------------------------------------------------------------------------------

const Sketch::PointRec& Sketch::prec(Id p) const {
    if (p < 0 || static_cast<std::size_t>(p) >= points_.size()) {
        throw std::out_of_range("нет точки с номером " + std::to_string(p));
    }
    return points_[static_cast<std::size_t>(p)];
}

const Sketch::CurveRec& Sketch::crec(Geo g) const {
    if (!is_curve(g.kind)) throw std::invalid_argument("элемент " + geo_text(g) + " не кривая");
    const auto& v = curves_[kind_index(g.kind)];
    if (g.id < 0 || static_cast<std::size_t>(g.id) >= v.size()) {
        throw std::out_of_range("нет элемента " + geo_text(g));
    }
    return v[static_cast<std::size_t>(g.id)];
}

Sketch::CurveRec& Sketch::crec_mut(Geo g) {
    crec(g);
    return curves_[kind_index(g.kind)][static_cast<std::size_t>(g.id)];
}

void Sketch::require(Geo g, std::initializer_list<GeoKind> kinds, const char* what) const {
    if (std::find(kinds.begin(), kinds.end(), g.kind) == kinds.end()) {
        throw std::invalid_argument(std::string(what) + ": недопустимый элемент " + geo_text(g));
    }
    if (g.kind == GeoKind::Point) {
        if (prec(g.id).removed) throw std::invalid_argument(std::string(what) + ": точка " + std::to_string(g.id) + " удалена");
    } else if (crec(g).removed) {
        throw std::invalid_argument(std::string(what) + ": элемент " + geo_text(g) + " удалён");
    }
}

Id Sketch::add_point(double x, double y) {
    PointRec r;
    r.pos = {x, y};
    points_.push_back(r);
    return static_cast<Id>(points_.size() - 1);
}

void Sketch::fix_point(Id p) {
    prec(p);
    points_[p].fixed = true;
    points_[p].pin = points_[p].pos;
}

void Sketch::set_point(Id p, double x, double y) {
    prec(p);
    points_[p].pos = {x, y};
    if (points_[p].fixed) points_[p].pin = {x, y};
}

Id Sketch::add_curve(CurveRec rec) {
    for (Id p : curve_points(rec)) require(point_geo(p), {GeoKind::Point}, "кривая");
    auto& v = curves_[kind_index(rec.kind)];
    v.push_back(std::move(rec));
    return static_cast<Id>(v.size() - 1);
}

Id Sketch::add_line(Id a, Id b) {
    CurveRec r;
    r.kind = GeoKind::Line;
    r.p0 = a;
    r.p1 = b;
    return add_curve(r);
}

Id Sketch::add_circle(Id center, double radius) {
    CurveRec r;
    r.kind = GeoKind::Circle;
    r.p0 = center;
    r.radius = radius;
    return add_curve(r);
}

Id Sketch::add_arc(Id center, Id start, Id end, bool ccw) {
    CurveRec r;
    r.kind = GeoKind::Arc;
    r.p0 = center;
    r.p1 = start;
    r.p2 = end;
    r.ccw = ccw;
    r.radius = dist(prec(center).pos, prec(start).pos);
    return add_curve(r);
}

Id Sketch::add_ellipse(Id center, Id focus, double minor_radius) {
    if (!(minor_radius > 0)) throw std::invalid_argument("эллипс: малая полуось должна быть > 0");
    CurveRec r;
    r.kind = GeoKind::Ellipse;
    r.p0 = center;
    r.p3 = focus;
    r.radius = minor_radius;
    return add_curve(r);
}

Id Sketch::add_ellipse_arc(Id center, Id focus, double minor_radius, Id start, Id end, bool ccw) {
    if (!(minor_radius > 0)) throw std::invalid_argument("дуга эллипса: малая полуось должна быть > 0");
    CurveRec r;
    r.kind = GeoKind::EllipseArc;
    r.p0 = center;
    r.p3 = focus;
    r.p1 = start;
    r.p2 = end;
    r.ccw = ccw;
    r.radius = minor_radius;
    return add_curve(r);
}

Id Sketch::add_bspline(const BSplineData& data) {
    const int p = data.degree;
    if (p < 1) throw std::invalid_argument("B-сплайн: степень должна быть >= 1");
    if (data.knots.size() < 2 || data.knots.size() != data.multiplicities.size()) {
        throw std::invalid_argument("B-сплайн: узлы и кратности не согласованы");
    }
    for (std::size_t i = 1; i < data.knots.size(); ++i) {
        if (!(data.knots[i] > data.knots[i - 1])) throw std::invalid_argument("B-сплайн: узлы должны возрастать");
    }
    const int sum = std::accumulate(data.multiplicities.begin(), data.multiplicities.end(), 0);
    const int n = static_cast<int>(data.poles.size());
    if (data.periodic) {
        if (sum - data.multiplicities.back() != n) {
            throw std::invalid_argument("B-сплайн (периодический): число полюсов должно равняться сумме кратностей без последнего узла");
        }
    } else {
        if (sum != n + p + 1) throw std::invalid_argument("B-сплайн: сумма кратностей должна быть = полюса + степень + 1");
        if (data.multiplicities.front() != p + 1 || data.multiplicities.back() != p + 1) {
            throw std::invalid_argument("B-сплайн: непериодический сплайн должен быть зажатым (кратность крайних узлов = степень + 1)");
        }
    }
    if (!data.weights.empty()) {
        if (data.weights.size() != data.poles.size()) throw std::invalid_argument("B-сплайн: число весов не равно числу полюсов");
        for (double w : data.weights) {
            if (!(w > 0)) throw std::invalid_argument("B-сплайн: веса должны быть > 0");
        }
    }
    CurveRec r;
    r.kind = GeoKind::BSpline;
    r.spline = data;
    return add_curve(r);
}

std::vector<Id> Sketch::curve_points(const CurveRec& c) const {
    std::vector<Id> out;
    switch (c.kind) {
        case GeoKind::Line: out = {c.p0, c.p1}; break;
        case GeoKind::Circle: out = {c.p0}; break;
        case GeoKind::Arc: out = {c.p0, c.p1, c.p2}; break;
        case GeoKind::Ellipse: out = {c.p0, c.p3}; break;
        case GeoKind::EllipseArc: out = {c.p0, c.p3, c.p1, c.p2}; break;
        case GeoKind::BSpline: out = c.spline.poles; break;
        case GeoKind::Point: break;
    }
    return out;
}

std::vector<Id> Sketch::curve_ends(Geo g) const {
    const CurveRec& c = crec(g);
    switch (c.kind) {
        case GeoKind::Line: return {c.p0, c.p1};
        case GeoKind::Arc:
        case GeoKind::EllipseArc: return {c.p1, c.p2};
        case GeoKind::BSpline:
            if (c.spline.periodic) return {};
            return {c.spline.poles.front(), c.spline.poles.back()};
        default: return {};
    }
}

void Sketch::set_construction(Geo g, bool construction) {
    crec_mut(g).construction = construction;
}

bool Sketch::is_construction(Geo g) const {
    return crec(g).construction;
}

void Sketch::set_line_points(Id line, Id start, Id end) {
    require(point_geo(start), {GeoKind::Point}, "отрезок");
    require(point_geo(end), {GeoKind::Point}, "отрезок");
    CurveRec& c = crec_mut(line_geo(line));
    c.p0 = start;
    c.p1 = end;
}

void Sketch::set_arc_points(Id arc, Id start, Id end) {
    require(point_geo(start), {GeoKind::Point}, "дуга");
    require(point_geo(end), {GeoKind::Point}, "дуга");
    CurveRec& c = crec_mut(arc_geo(arc));
    c.p1 = start;
    c.p2 = end;
    c.radius = dist(prec(c.p0).pos, prec(start).pos);
}

std::vector<Geo> Sketch::curves_using(Id point) const {
    std::vector<Geo> out;
    for (std::size_t k = 1; k < curves_.size(); ++k) {
        for (std::size_t i = 0; i < curves_[k].size(); ++i) {
            const CurveRec& c = curves_[k][i];
            if (c.removed) continue;
            const auto pts = curve_points(c);
            if (std::find(pts.begin(), pts.end(), point) != pts.end()) {
                out.push_back({static_cast<GeoKind>(k), static_cast<Id>(i)});
            }
        }
    }
    return out;
}

std::vector<Id> Sketch::constraints_of(Geo g) const {
    std::vector<Id> out;
    for (std::size_t j = 0; j < constraints_.size(); ++j) {
        const auto& c = constraints_[j];
        if (c.removed) continue;
        if (c.a == g || c.b == g || c.c == g) out.push_back(tag_of(j));
    }
    return out;
}

void Sketch::remove(Geo g) {
    if (g.kind == GeoKind::Point) {
        prec(g.id);
        if (!curves_using(g.id).empty()) {
            throw std::invalid_argument("точка " + std::to_string(g.id) + " занята кривой");
        }
        points_[g.id].removed = true;
    } else {
        crec_mut(g).removed = true;
    }
    for (Id c : constraints_of(g)) constraints_[c - 1].removed = true;
}

bool Sketch::is_removed(Geo g) const {
    if (g.kind == GeoKind::Point) return prec(g.id).removed;
    return crec(g).removed;
}

std::size_t Sketch::count(GeoKind kind) const noexcept {
    if (kind == GeoKind::Point) return points_.size();
    return curves_[kind_index(kind)].size();
}

// ---------------------------------------------------------------------------------------------
// Ограничения
// ---------------------------------------------------------------------------------------------

Id Sketch::add_constraint(ConstraintInfo info) {
    constraints_.push_back(info);
    return tag_of(constraints_.size() - 1);
}

namespace {
ConstraintInfo make(ConstraintKind k, Geo a, Geo b = {}, Geo c = {}, double v = 0.0) {
    ConstraintInfo i;
    i.kind = k;
    i.a = a;
    i.b = b;
    i.c = c;
    i.value = v;
    return i;
}
constexpr std::initializer_list<GeoKind> kPointOnly = {GeoKind::Point};
}  // namespace

Id Sketch::constrain_coincident(Id a, Id b) {
    require(point_geo(a), kPointOnly, "совпадение");
    require(point_geo(b), kPointOnly, "совпадение");
    return add_constraint(make(ConstraintKind::Coincident, point_geo(a), point_geo(b)));
}

Id Sketch::constrain_horizontal(Id a, Id b) {
    require(point_geo(a), kPointOnly, "горизонтальность");
    require(point_geo(b), kPointOnly, "горизонтальность");
    return add_constraint(make(ConstraintKind::Horizontal, point_geo(a), point_geo(b)));
}

Id Sketch::constrain_vertical(Id a, Id b) {
    require(point_geo(a), kPointOnly, "вертикальность");
    require(point_geo(b), kPointOnly, "вертикальность");
    return add_constraint(make(ConstraintKind::Vertical, point_geo(a), point_geo(b)));
}

Id Sketch::constrain_distance(Id a, Id b, double d) {
    require(point_geo(a), kPointOnly, "расстояние");
    require(point_geo(b), kPointOnly, "расстояние");
    return add_constraint(make(ConstraintKind::Distance, point_geo(a), point_geo(b), {}, d));
}

Id Sketch::constrain_distance_x(Id a, Id b, double d) {
    require(point_geo(a), kPointOnly, "расстояние по X");
    require(point_geo(b), kPointOnly, "расстояние по X");
    return add_constraint(make(ConstraintKind::DistanceX, point_geo(a), point_geo(b), {}, d));
}

Id Sketch::constrain_distance_y(Id a, Id b, double d) {
    require(point_geo(a), kPointOnly, "расстояние по Y");
    require(point_geo(b), kPointOnly, "расстояние по Y");
    return add_constraint(make(ConstraintKind::DistanceY, point_geo(a), point_geo(b), {}, d));
}

Id Sketch::constrain_point_curve_distance(Id p, Geo curve, double d) {
    require(point_geo(p), kPointOnly, "расстояние до кривой");
    require(curve, {GeoKind::Line, GeoKind::Circle, GeoKind::Arc}, "расстояние до кривой");
    return add_constraint(make(ConstraintKind::PointCurveDistance, point_geo(p), curve, {}, d));
}

Id Sketch::constrain_length(Id line, double d) {
    const CurveRec& c = crec(line_geo(line));
    return constrain_distance(c.p0, c.p1, d);
}

Id Sketch::constrain_radius(Id circle, double r) {
    return constrain_radius_of(circle_geo(circle), r);
}

Id Sketch::constrain_radius_of(Geo g, double r) {
    require(g, {GeoKind::Circle, GeoKind::Arc}, "радиус");
    return add_constraint(make(ConstraintKind::Radius, g, {}, {}, r));
}

Id Sketch::constrain_diameter(Geo g, double d) {
    require(g, {GeoKind::Circle, GeoKind::Arc}, "диаметр");
    return add_constraint(make(ConstraintKind::Diameter, g, {}, {}, d));
}

Id Sketch::constrain_arc_length(Id arc, double length) {
    require(arc_geo(arc), {GeoKind::Arc}, "длина дуги");
    return add_constraint(make(ConstraintKind::ArcLength, arc_geo(arc), {}, {}, length));
}

Id Sketch::constrain_major_radius(Geo e, double a) {
    require(e, {GeoKind::Ellipse, GeoKind::EllipseArc}, "большая полуось");
    return add_constraint(make(ConstraintKind::MajorRadius, e, {}, {}, a));
}

Id Sketch::constrain_minor_radius(Geo e, double b) {
    require(e, {GeoKind::Ellipse, GeoKind::EllipseArc}, "малая полуось");
    return add_constraint(make(ConstraintKind::MinorRadius, e, {}, {}, b));
}

Id Sketch::constrain_parallel(Id l1, Id l2) {
    require(line_geo(l1), {GeoKind::Line}, "параллельность");
    require(line_geo(l2), {GeoKind::Line}, "параллельность");
    return add_constraint(make(ConstraintKind::Parallel, line_geo(l1), line_geo(l2)));
}

Id Sketch::constrain_perpendicular(Id l1, Id l2) {
    require(line_geo(l1), {GeoKind::Line}, "перпендикулярность");
    require(line_geo(l2), {GeoKind::Line}, "перпендикулярность");
    return add_constraint(make(ConstraintKind::Perpendicular, line_geo(l1), line_geo(l2)));
}

Id Sketch::constrain_equal_length(Id l1, Id l2) {
    require(line_geo(l1), {GeoKind::Line}, "равенство длин");
    return constrain_equal(line_geo(l1), line_geo(l2));
}

Id Sketch::constrain_equal_radius(Id c1, Id c2) {
    return constrain_equal(circle_geo(c1), circle_geo(c2));
}

Id Sketch::constrain_equal(Geo a, Geo b) {
    const std::initializer_list<GeoKind> all = {GeoKind::Line, GeoKind::Circle, GeoKind::Arc, GeoKind::Ellipse,
                                                GeoKind::EllipseArc};
    require(a, all, "равенство");
    require(b, all, "равенство");
    const bool ok = (a.kind == GeoKind::Line && b.kind == GeoKind::Line) || (is_circular(a.kind) && is_circular(b.kind)) ||
                    (is_elliptic(a.kind) && is_elliptic(b.kind));
    if (!ok) throw std::invalid_argument("равенство: элементы разных видов " + geo_text(a) + ", " + geo_text(b));
    return add_constraint(make(ConstraintKind::Equal, a, b));
}

Id Sketch::constrain_angle(Id l1, Id l2, double deg) {
    require(line_geo(l1), {GeoKind::Line}, "угол");
    require(line_geo(l2), {GeoKind::Line}, "угол");
    return add_constraint(make(ConstraintKind::Angle, line_geo(l1), line_geo(l2), {}, deg));
}

Id Sketch::constrain_point_on_line(Id p, Id line) {
    return constrain_point_on(p, line_geo(line));
}

Id Sketch::constrain_point_on_circle(Id p, Id circle) {
    return constrain_point_on(p, circle_geo(circle));
}

Id Sketch::constrain_point_on(Id p, Geo curve) {
    require(point_geo(p), kPointOnly, "точка на объекте");
    require(curve, {GeoKind::Line, GeoKind::Circle, GeoKind::Arc, GeoKind::Ellipse, GeoKind::EllipseArc},
            "точка на объекте");
    return add_constraint(make(ConstraintKind::PointOn, point_geo(p), curve));
}

Id Sketch::constrain_symmetric(Id p1, Id p2, Id axis) {
    require(point_geo(p1), kPointOnly, "симметрия");
    require(point_geo(p2), kPointOnly, "симметрия");
    require(line_geo(axis), {GeoKind::Line}, "симметрия");
    return add_constraint(make(ConstraintKind::Symmetric, point_geo(p1), point_geo(p2), line_geo(axis)));
}

Id Sketch::constrain_symmetric_point(Id p1, Id p2, Id center) {
    require(point_geo(p1), kPointOnly, "симметрия");
    require(point_geo(p2), kPointOnly, "симметрия");
    require(point_geo(center), kPointOnly, "симметрия");
    return add_constraint(make(ConstraintKind::SymmetricPoint, point_geo(p1), point_geo(p2), point_geo(center)));
}

Id Sketch::constrain_midpoint(Id p, Geo g) {
    require(point_geo(p), kPointOnly, "середина");
    require(g, {GeoKind::Line, GeoKind::Arc}, "середина");
    return add_constraint(make(ConstraintKind::Midpoint, point_geo(p), g));
}

Id Sketch::constrain_concentric(Geo a, Geo b) {
    const std::initializer_list<GeoKind> centred = {GeoKind::Circle, GeoKind::Arc, GeoKind::Ellipse, GeoKind::EllipseArc};
    require(a, centred, "концентричность");
    require(b, centred, "концентричность");
    return add_constraint(make(ConstraintKind::Concentric, a, b));
}

Id Sketch::constrain_fix(Id p) {
    require(point_geo(p), kPointOnly, "фиксация");
    ConstraintInfo i = make(ConstraintKind::Fix, point_geo(p));
    i.value = points_[p].pos.x;
    i.value2 = points_[p].pos.y;
    return add_constraint(i);
}

Id Sketch::constrain_rotation(Id p, Id q, Id center, double deg) {
    require(point_geo(p), kPointOnly, "поворот");
    require(point_geo(q), kPointOnly, "поворот");
    require(point_geo(center), kPointOnly, "поворот");
    return add_constraint(make(ConstraintKind::Rotation, point_geo(p), point_geo(q), point_geo(center), deg));
}

Id Sketch::constrain_tangent(Geo a, Geo b) {
    const std::initializer_list<GeoKind> all = {GeoKind::Line, GeoKind::Circle, GeoKind::Arc, GeoKind::Ellipse,
                                                GeoKind::EllipseArc, GeoKind::BSpline};
    require(a, all, "касание");
    require(b, all, "касание");
    if (a.kind == GeoKind::Line && b.kind == GeoKind::Line) {
        throw std::invalid_argument("касание: два отрезка (используйте параллельность)");
    }
    if (is_elliptic(a.kind) && is_elliptic(b.kind)) throw std::invalid_argument("касание двух эллипсов не поддерживается");
    if ((is_elliptic(a.kind) && is_circular(b.kind)) || (is_circular(a.kind) && is_elliptic(b.kind))) {
        throw std::invalid_argument("касание эллипса и окружности не поддерживается");
    }
    if (a.kind == GeoKind::BSpline || b.kind == GeoKind::BSpline) {
        const Geo other = a.kind == GeoKind::BSpline ? b : a;
        if (is_elliptic(other.kind) || other.kind == GeoKind::Circle) {
            throw std::invalid_argument("касание сплайна: только с отрезком, дугой или сплайном в общем конце");
        }
    }
    return add_constraint(make(ConstraintKind::Tangent, a, b));
}

void Sketch::remove_constraint(Id c) {
    if (c < 1 || static_cast<std::size_t>(c) > constraints_.size()) {
        throw std::out_of_range("нет ограничения с таким номером");
    }
    constraints_[c - 1].removed = true;
}

const ConstraintInfo& Sketch::constraint(Id c) const {
    if (c < 1 || static_cast<std::size_t>(c) > constraints_.size()) {
        throw std::out_of_range("нет ограничения с таким номером");
    }
    return constraints_[c - 1];
}

void Sketch::set_driving(Id c, bool driving) {
    constraint(c);
    if (!is_dimension(constraints_[c - 1].kind)) {
        throw std::invalid_argument("справочным может быть только размер");
    }
    constraints_[c - 1].driving = driving;
}

void Sketch::set_constraint_value(Id c, double value) {
    constraint(c);
    constraints_[c - 1].value = value;
}

double Sketch::constraint_value(Id c) const {
    const ConstraintInfo& i = constraint(c);
    return i.driving ? i.value : measure(c);
}

// ---------------------------------------------------------------------------------------------
// Чтение
// ---------------------------------------------------------------------------------------------

Point2 Sketch::point(Id p) const {
    return prec(p).pos;
}

bool Sketch::is_point_fixed(Id p) const {
    return prec(p).fixed;
}

double Sketch::circle_radius(Id c) const {
    return crec(circle_geo(c)).radius;
}

LineData Sketch::line(Id l) const {
    const CurveRec& c = crec(line_geo(l));
    return {c.p0, c.p1};
}

CircleData Sketch::circle(Id id) const {
    const CurveRec& c = crec(circle_geo(id));
    return {c.p0, c.radius};
}

ArcData Sketch::arc(Id a) const {
    const CurveRec& c = crec(arc_geo(a));
    return {c.p0, c.p1, c.p2, c.ccw, c.radius};
}

EllipseData Sketch::ellipse(Id e) const {
    const CurveRec& c = crec(ellipse_geo(e));
    return {c.p0, c.p3, c.radius};
}

EllipseArcData Sketch::ellipse_arc(Id e) const {
    const CurveRec& c = crec(ellipse_arc_geo(e));
    return {c.p0, c.p3, c.radius, c.p1, c.p2, c.ccw};
}

const BSplineData& Sketch::bspline(Id s) const {
    return crec(bspline_geo(s)).spline;
}

double Sketch::arc_start_angle(Id a) const {
    const CurveRec& c = crec(arc_geo(a));
    const Point2 o = prec(c.p0).pos, s = prec(c.p1).pos;
    return std::atan2(s.y - o.y, s.x - o.x);
}

double Sketch::arc_sweep(Id a) const {
    const CurveRec& c = crec(arc_geo(a));
    const Point2 o = prec(c.p0).pos, s = prec(c.p1).pos, e = prec(c.p2).pos;
    const double a0 = std::atan2(s.y - o.y, s.x - o.x), a1 = std::atan2(e.y - o.y, e.x - o.x);
    double sweep = wrap_2pi(c.ccw ? a1 - a0 : a0 - a1);
    if (sweep < 1e-12) sweep = 2 * kPi;
    return sweep;
}

double Sketch::ellipse_major_radius(Geo e) const {
    require(e, {GeoKind::Ellipse, GeoKind::EllipseArc}, "эллипс");
    const CurveRec& c = crec(e);
    const double f = dist(prec(c.p0).pos, prec(c.p3).pos);
    return std::sqrt(f * f + c.radius * c.radius);
}

Point2 Sketch::ellipse_major_direction(Geo e) const {
    require(e, {GeoKind::Ellipse, GeoKind::EllipseArc}, "эллипс");
    const CurveRec& c = crec(e);
    const Point2 o = prec(c.p0).pos, f = prec(c.p3).pos;
    const double l = dist(o, f);
    if (l < 1e-300) return {1.0, 0.0};
    return {(f.x - o.x) / l, (f.y - o.y) / l};
}

double Sketch::ellipse_parameter(Geo e, Point2 p) const {
    const CurveRec& c = crec(e);
    const Point2 o = prec(c.p0).pos;
    const Point2 u = ellipse_major_direction(e);
    const double a = ellipse_major_radius(e), b = c.radius;
    const double dx = p.x - o.x, dy = p.y - o.y;
    const double x = dx * u.x + dy * u.y, y = -dx * u.y + dy * u.x;
    return std::atan2(y / b, x / a);
}

// ---------------------------------------------------------------------------------------------
// Измерение и невязки
// ---------------------------------------------------------------------------------------------

namespace {

double line_distance(Point2 p, Point2 a, Point2 b) {
    const double dx = b.x - a.x, dy = b.y - a.y, l = std::hypot(dx, dy);
    if (l < 1e-300) return dist(p, a);
    return std::abs((p.x - a.x) * dy - (p.y - a.y) * dx) / l;
}

}  // namespace

double Sketch::measure(Id id) const {
    const ConstraintInfo& c = constraint(id);
    auto P = [&](Geo g) { return prec(g.id).pos; };
    switch (c.kind) {
        case ConstraintKind::Distance: return dist(P(c.a), P(c.b));
        case ConstraintKind::DistanceX: return P(c.b).x - P(c.a).x;
        case ConstraintKind::DistanceY: return P(c.b).y - P(c.a).y;
        case ConstraintKind::PointCurveDistance: {
            const CurveRec& r = crec(c.b);
            if (r.kind == GeoKind::Line) return line_distance(P(c.a), prec(r.p0).pos, prec(r.p1).pos);
            return std::abs(dist(P(c.a), prec(r.p0).pos) - r.radius);
        }
        case ConstraintKind::Radius: return crec(c.a).radius;
        case ConstraintKind::Diameter: return 2 * crec(c.a).radius;
        case ConstraintKind::Angle: {
            const CurveRec& l1 = crec(c.a);
            const CurveRec& l2 = crec(c.b);
            const Point2 a0 = prec(l1.p0).pos, a1 = prec(l1.p1).pos, b0 = prec(l2.p0).pos, b1 = prec(l2.p1).pos;
            return wrap_pi(std::atan2(b1.y - b0.y, b1.x - b0.x) - std::atan2(a1.y - a0.y, a1.x - a0.x)) / kDeg;
        }
        case ConstraintKind::ArcLength: return crec(c.a).radius * arc_sweep(c.a.id);
        case ConstraintKind::MajorRadius: return ellipse_major_radius(c.a);
        case ConstraintKind::MinorRadius: return crec(c.a).radius;
        default: throw std::invalid_argument(std::string("ограничение ") + to_string(c.kind) + " не размер");
    }
}

double Sketch::residual(Id id) const {
    const ConstraintInfo& c = constraint(id);
    if (c.removed) return 0.0;
    auto P = [&](Geo g) { return prec(g.id).pos; };
    auto dir = [&](Geo l) {
        const CurveRec& r = crec(l);
        const Point2 a = prec(r.p0).pos, b = prec(r.p1).pos;
        return Point2{b.x - a.x, b.y - a.y};
    };
    auto center = [&](Geo g) { return prec(crec(g).p0).pos; };
    if (is_dimension(c.kind)) {
        if (!c.driving) return 0.0;
        if (c.kind == ConstraintKind::Angle) return std::abs(wrap_pi((measure(id) - c.value) * kDeg));
        if (c.kind == ConstraintKind::PointCurveDistance || c.kind == ConstraintKind::Distance) {
            return std::abs(measure(id) - std::abs(c.value));
        }
        return std::abs(measure(id) - c.value);
    }
    switch (c.kind) {
        case ConstraintKind::Coincident: return dist(P(c.a), P(c.b));
        case ConstraintKind::Horizontal: return std::abs(P(c.a).y - P(c.b).y);
        case ConstraintKind::Vertical: return std::abs(P(c.a).x - P(c.b).x);
        case ConstraintKind::Fix: return std::hypot(P(c.a).x - c.value, P(c.a).y - c.value2);
        case ConstraintKind::Parallel:
        case ConstraintKind::Perpendicular: {
            const Point2 u = dir(c.a), v = dir(c.b);
            const double lu = std::hypot(u.x, u.y), lv = std::hypot(v.x, v.y);
            if (lu == 0 || lv == 0) return 1.0;
            const double x = c.kind == ConstraintKind::Parallel ? u.x * v.y - u.y * v.x : u.x * v.x + u.y * v.y;
            return std::abs(x) / (lu * lv);
        }
        case ConstraintKind::Equal: {
            if (c.a.kind == GeoKind::Line) {
                const Point2 u = dir(c.a), v = dir(c.b);
                return std::abs(std::hypot(u.x, u.y) - std::hypot(v.x, v.y));
            }
            const double db = std::abs(crec(c.a).radius - crec(c.b).radius);
            if (is_circular(c.a.kind)) return db;
            return std::max(db, std::abs(ellipse_major_radius(c.a) - ellipse_major_radius(c.b)));
        }
        case ConstraintKind::Symmetric: {
            const CurveRec& ax = crec(c.c);
            const Point2 a = prec(ax.p0).pos, b = prec(ax.p1).pos;
            const Point2 mid{(P(c.a).x + P(c.b).x) / 2, (P(c.a).y + P(c.b).y) / 2};
            const Point2 d = dir(c.c);
            const double l = std::hypot(d.x, d.y);
            const double along = l > 0 ? std::abs((P(c.b).x - P(c.a).x) * d.x + (P(c.b).y - P(c.a).y) * d.y) / l : 0;
            return std::max(line_distance(mid, a, b), along);
        }
        case ConstraintKind::SymmetricPoint: {
            const Point2 mid{(P(c.a).x + P(c.b).x) / 2, (P(c.a).y + P(c.b).y) / 2};
            return dist(mid, P(c.c));
        }
        case ConstraintKind::Midpoint: {
            const CurveRec& r = crec(c.b);
            if (r.kind == GeoKind::Line) {
                const Point2 a = prec(r.p0).pos, b = prec(r.p1).pos;
                return dist(P(c.a), {(a.x + b.x) / 2, (a.y + b.y) / 2});
            }
            const Point2 o = prec(r.p0).pos, s = prec(r.p1).pos, e = prec(r.p2).pos;
            const double on = std::abs(dist(P(c.a), o) - r.radius);
            return std::max(on, std::abs(dist(P(c.a), s) - dist(P(c.a), e)));
        }
        case ConstraintKind::PointOn: {
            const CurveRec& r = crec(c.b);
            const Point2 p = P(c.a);
            if (r.kind == GeoKind::Line) return line_distance(p, prec(r.p0).pos, prec(r.p1).pos);
            if (is_circular(r.kind)) return std::abs(dist(p, prec(r.p0).pos) - r.radius);
            const Point2 o = prec(r.p0).pos, f = prec(r.p3).pos;
            const Point2 f2{2 * o.x - f.x, 2 * o.y - f.y};
            return std::abs(dist(p, f) + dist(p, f2) - 2 * ellipse_major_radius(c.b));
        }
        case ConstraintKind::Concentric: return dist(center(c.a), center(c.b));
        case ConstraintKind::Rotation: {
            const Point2 p = P(c.a), q = P(c.b), o = P(c.c);
            const double co = std::cos(c.value * kDeg), si = std::sin(c.value * kDeg);
            const Point2 r{o.x + co * (p.x - o.x) - si * (p.y - o.y), o.y + si * (p.x - o.x) + co * (p.y - o.y)};
            return dist(q, r);
        }
        case ConstraintKind::Tangent: {
            // Общий конец: касание в нём; иначе — касание кривых в целом.
            Geo a = c.a, b = c.b;
            if (b.kind == GeoKind::Line || (b.kind == GeoKind::BSpline && a.kind != GeoKind::Line)) std::swap(a, b);
            const auto ea = curve_ends(a), eb = curve_ends(b);
            std::optional<std::pair<Id, Id>> shared;
            for (Id x : ea) {
                for (Id y : eb) {
                    if (!shared && dist(prec(x).pos, prec(y).pos) < 1e-7 * (1 + std::abs(prec(x).pos.x) + std::abs(prec(x).pos.y))) {
                        shared = std::pair{x, y};
                    }
                }
            }
            // Направление касательной кривой в конце (единичный вектор).
            auto tangent_at = [&](Geo g, Id end) -> Point2 {
                const CurveRec& r = crec(g);
                Point2 t{0, 0};
                if (r.kind == GeoKind::Line) {
                    t = dir(g);
                } else if (r.kind == GeoKind::Arc) {
                    const Point2 o = prec(r.p0).pos, p = prec(end).pos;
                    t = {-(p.y - o.y), p.x - o.x};
                } else if (r.kind == GeoKind::BSpline) {
                    const auto& poles = r.spline.poles;
                    const Id q = end == poles.front() ? poles[1] : poles[poles.size() - 2];
                    t = {prec(q).pos.x - prec(end).pos.x, prec(q).pos.y - prec(end).pos.y};
                } else if (r.kind == GeoKind::EllipseArc) {
                    const double u = ellipse_parameter(g, prec(end).pos);
                    const Point2 m = ellipse_major_direction(g);
                    const double A = ellipse_major_radius(g), B = r.radius;
                    t = {-A * std::sin(u) * m.x - B * std::cos(u) * m.y, -A * std::sin(u) * m.y + B * std::cos(u) * m.x};
                }
                const double l = std::hypot(t.x, t.y);
                return l > 0 ? Point2{t.x / l, t.y / l} : t;
            };
            if (shared) {
                const Point2 ta = tangent_at(a, shared->first), tb = tangent_at(b, shared->second);
                return std::abs(ta.x * tb.y - ta.y * tb.x);
            }
            const CurveRec& ra = crec(a);
            const CurveRec& rb = crec(b);
            if (a.kind == GeoKind::Line && is_circular(b.kind)) {
                return std::abs(line_distance(prec(rb.p0).pos, prec(ra.p0).pos, prec(ra.p1).pos) - rb.radius);
            }
            if (is_circular(a.kind) && is_circular(b.kind)) {
                const double d = dist(prec(ra.p0).pos, prec(rb.p0).pos);
                return std::min(std::abs(d - (ra.radius + rb.radius)), std::abs(d - std::abs(ra.radius - rb.radius)));
            }
            if (a.kind == GeoKind::Line && is_elliptic(b.kind)) {
                // В системе эллипса, сжатой до единичной окружности, касательная — на расстоянии 1.
                const Point2 o = prec(rb.p0).pos, m = ellipse_major_direction(b);
                const double A = ellipse_major_radius(b), B = rb.radius;
                auto local = [&](Point2 p) {
                    const double dx = p.x - o.x, dy = p.y - o.y;
                    return Point2{(dx * m.x + dy * m.y) / A, (-dx * m.y + dy * m.x) / B};
                };
                return std::abs(line_distance({0, 0}, local(prec(ra.p0).pos), local(prec(ra.p1).pos)) - 1.0) * B;
            }
            return 1.0;
        }
        default: return 0.0;
    }
}

// ---------------------------------------------------------------------------------------------
// Решение
// ---------------------------------------------------------------------------------------------

// Система PlaneGCS, построенная по текущему состоянию эскиза.
struct SolverModel {
    const Sketch& s;
    std::deque<double> store;
    std::vector<GCS::Point> gp;
    std::vector<GCS::Line> gl;
    std::vector<GCS::Circle> gc;
    std::vector<GCS::Arc> ga;
    std::vector<GCS::Ellipse> ge;
    std::vector<GCS::ArcOfEllipse> gea;
    GCS::System sys;
    GCS::VEC_pD unknowns;
    std::vector<int> parent;   // классы совпадающих точек

    explicit SolverModel(const Sketch& sketch) : s(sketch) {}

    double* param(double v, bool unknown) {
        store.push_back(v);
        if (unknown) unknowns.push_back(&store.back());
        return &store.back();
    }

    int find(int p) {
        while (parent[p] != p) p = parent[p] = parent[parent[p]];
        return p;
    }

    using CurveRec = Sketch::CurveRec;
    const CurveRec& rec(Geo g) const { return s.crec(g); }

    GCS::Circle& circular(Geo g) {
        if (g.kind == GeoKind::Circle) return gc[g.id];
        return ga[g.id];
    }
    GCS::Ellipse& elliptic(Geo g) {
        if (g.kind == GeoKind::Ellipse) return ge[g.id];
        return gea[g.id];
    }

    void build() {
        const std::size_t n = s.points_.size();
        gp.resize(n);
        parent.resize(n);
        std::iota(parent.begin(), parent.end(), 0);
        for (std::size_t i = 0; i < n; ++i) {
            const auto& p = s.points_[i];
            const bool live = !p.removed;
            gp[i].x = param(p.pos.x, live);
            gp[i].y = param(p.pos.y, live);
        }
        const auto& lines = s.curves_[kind_index(GeoKind::Line)];
        gl.resize(lines.size());
        for (std::size_t k = 0; k < lines.size(); ++k) {
            gl[k].p1 = gp[lines[k].p0];
            gl[k].p2 = gp[lines[k].p1];
        }
        const auto& circles = s.curves_[kind_index(GeoKind::Circle)];
        gc.resize(circles.size());
        for (std::size_t k = 0; k < circles.size(); ++k) {
            gc[k].center = gp[circles[k].p0];
            gc[k].rad = param(circles[k].radius, !circles[k].removed);
        }
        const auto& arcs = s.curves_[kind_index(GeoKind::Arc)];
        ga.resize(arcs.size());
        for (std::size_t k = 0; k < arcs.size(); ++k) {
            const auto& r = arcs[k];
            const bool live = !r.removed;
            const Id first = r.ccw ? r.p1 : r.p2, second = r.ccw ? r.p2 : r.p1;
            const Point2 o = s.points_[r.p0].pos, a = s.points_[first].pos, b = s.points_[second].pos;
            const double a0 = std::atan2(a.y - o.y, a.x - o.x);
            double a1 = std::atan2(b.y - o.y, b.x - o.x);
            while (a1 <= a0) a1 += 2 * kPi;
            ga[k].center = gp[r.p0];
            ga[k].rad = param(r.radius > 0 ? r.radius : dist(o, a), live);
            ga[k].startAngle = param(a0, live);
            ga[k].endAngle = param(a1, live);
            ga[k].start = gp[first];
            ga[k].end = gp[second];
            if (live) sys.addConstraintArcRules(ga[k], 0, true);
        }
        const auto& ellipses = s.curves_[kind_index(GeoKind::Ellipse)];
        ge.resize(ellipses.size());
        for (std::size_t k = 0; k < ellipses.size(); ++k) {
            ge[k].center = gp[ellipses[k].p0];
            ge[k].focus1 = gp[ellipses[k].p3];
            ge[k].radmin = param(ellipses[k].radius, !ellipses[k].removed);
        }
        const auto& earcs = s.curves_[kind_index(GeoKind::EllipseArc)];
        gea.resize(earcs.size());
        for (std::size_t k = 0; k < earcs.size(); ++k) {
            const auto& r = earcs[k];
            const bool live = !r.removed;
            const Geo g = ellipse_arc_geo(static_cast<Id>(k));
            const Id first = r.ccw ? r.p1 : r.p2, second = r.ccw ? r.p2 : r.p1;
            const double u0 = s.ellipse_parameter(g, s.points_[first].pos);
            double u1 = s.ellipse_parameter(g, s.points_[second].pos);
            while (u1 <= u0) u1 += 2 * kPi;
            gea[k].center = gp[r.p0];
            gea[k].focus1 = gp[r.p3];
            gea[k].radmin = param(r.radius, live);
            gea[k].startAngle = param(u0, live);
            gea[k].endAngle = param(u1, live);
            gea[k].start = gp[first];
            gea[k].end = gp[second];
            if (live) sys.addConstraintArcOfEllipseRules(gea[k], 0, true);
        }
        sys.declareUnknowns(unknowns);

        for (std::size_t i = 0; i < n; ++i) {
            const auto& p = s.points_[i];
            if (p.removed || !p.fixed) continue;
            sys.addConstraintCoordinateX(gp[i], param(p.pin.x, false), 0, true);
            sys.addConstraintCoordinateY(gp[i], param(p.pin.y, false), 0, true);
        }
        for (const auto& c : s.constraints_) {
            if (!c.removed && c.driving && c.kind == ConstraintKind::Coincident) {
                parent[find(c.a.id)] = find(c.b.id);
            }
        }
        for (std::size_t j = 0; j < s.constraints_.size(); ++j) {
            const auto& c = s.constraints_[j];
            if (c.removed || !c.driving) continue;
            add(c, tag_of(j));
        }
    }

    // Общий конец двух кривых (по номерам точек или ограничениям совпадения).
    std::optional<std::pair<Id, Id>> shared_end(Geo a, Geo b) {
        for (Id x : s.curve_ends(a)) {
            for (Id y : s.curve_ends(b)) {
                if (find(x) == find(y)) return std::pair{x, y};
            }
        }
        return std::nullopt;
    }

    // Соседний с концом полюс сплайна (касательная в конце зажатого сплайна).
    Id spline_neighbour(Geo g, Id end) const {
        const auto& poles = rec(g).spline.poles;
        return end == poles.front() ? poles[1] : poles[poles.size() - 2];
    }

    void add_tangent(Geo a, Geo b, int tag) {
        // Порядок: отрезок первым, затем дуга/окружность, затем сплайн.
        auto rank = [](GeoKind k) {
            switch (k) {
                case GeoKind::Line: return 0;
                case GeoKind::Circle:
                case GeoKind::Arc: return 1;
                case GeoKind::Ellipse:
                case GeoKind::EllipseArc: return 2;
                default: return 3;
            }
        };
        if (rank(b.kind) < rank(a.kind)) std::swap(a, b);
        const auto shared = shared_end(a, b);
        if (shared) {
            const Id pa = shared->first, pb = shared->second;
            if (a.kind == GeoKind::Line && b.kind == GeoKind::Arc) {
                sys.addConstraintPerpendicular(gl[a.id].p1, gl[a.id].p2, ga[b.id].center, gp[pb], tag, true);
                return;
            }
            if (a.kind == GeoKind::Arc && b.kind == GeoKind::Arc) {
                sys.addConstraintPointOnLine(gp[pa], ga[a.id].center, ga[b.id].center, tag, true);
                return;
            }
            if (a.kind == GeoKind::Line && b.kind == GeoKind::BSpline) {
                sys.addConstraintPointOnLine(gp[spline_neighbour(b, pb)], gl[a.id].p1, gl[a.id].p2, tag, true);
                return;
            }
            if (a.kind == GeoKind::Arc && b.kind == GeoKind::BSpline) {
                sys.addConstraintPerpendicular(ga[a.id].center, gp[pa], gp[pa], gp[spline_neighbour(b, pb)], tag, true);
                return;
            }
            if (a.kind == GeoKind::BSpline && b.kind == GeoKind::BSpline) {
                sys.addConstraintPointOnLine(gp[spline_neighbour(b, pb)], gp[spline_neighbour(a, pa)], gp[pa], tag, true);
                return;
            }
        }
        if (a.kind == GeoKind::Line && is_circular(b.kind)) {
            GCS::Circle& c = circular(b);
            const double x0 = *c.center.x, y0 = *c.center.y;
            const double x1 = *gl[a.id].p1.x, y1 = *gl[a.id].p1.y, x2 = *gl[a.id].p2.x, y2 = *gl[a.id].p2.y;
            const double area = -x0 * (y2 - y1) + y0 * (x2 - x1) + x1 * y2 - x2 * y1;
            sys.addConstraintP2LDistance(c.center, gl[a.id], c.rad, area >= 0, tag, true);
            return;
        }
        if (is_circular(a.kind) && is_circular(b.kind)) {
            GCS::Circle& c1 = circular(a);
            GCS::Circle& c2 = circular(b);
            const double d = std::hypot(*c2.center.x - *c1.center.x, *c2.center.y - *c1.center.y);
            sys.addConstraintTangentCircumf(c1.center, c2.center, c1.rad, c2.rad, d < *c1.rad || d < *c2.rad, tag, true);
            return;
        }
        if (a.kind == GeoKind::Line && is_elliptic(b.kind)) {
            sys.addConstraintTangent(gl[a.id], elliptic(b), tag, true);
            return;
        }
        throw std::logic_error("касание " + geo_text(a) + " и " + geo_text(b) +
                               " возможно только в общем конце (ограничение " + std::to_string(tag) + ")");
    }

    void add(const ConstraintInfo& c, int tag) {
        auto P = [&](Geo g) -> GCS::Point& { return gp[g.id]; };
        switch (c.kind) {
            case ConstraintKind::Coincident: sys.addConstraintP2PCoincident(P(c.a), P(c.b), tag, true); break;
            case ConstraintKind::Horizontal: sys.addConstraintHorizontal(P(c.a), P(c.b), tag, true); break;
            case ConstraintKind::Vertical: sys.addConstraintVertical(P(c.a), P(c.b), tag, true); break;
            case ConstraintKind::Distance:
                sys.addConstraintP2PDistance(P(c.a), P(c.b), param(c.value, false), tag, true);
                break;
            case ConstraintKind::DistanceX:
                sys.addConstraintDifference(P(c.a).x, P(c.b).x, param(c.value, false), tag, true);
                break;
            case ConstraintKind::DistanceY:
                sys.addConstraintDifference(P(c.a).y, P(c.b).y, param(c.value, false), tag, true);
                break;
            case ConstraintKind::PointCurveDistance:
                if (c.b.kind == GeoKind::Line) {
                    const GCS::Line& l = gl[c.b.id];
                    const double x0 = *P(c.a).x, y0 = *P(c.a).y;
                    const double area = -x0 * (*l.p2.y - *l.p1.y) + y0 * (*l.p2.x - *l.p1.x) + *l.p1.x * *l.p2.y - *l.p2.x * *l.p1.y;
                    sys.addConstraintP2LDistance(P(c.a), gl[c.b.id], param(std::abs(c.value), false), area >= 0, tag, true);
                } else {
                    sys.addConstraintP2CDistance(P(c.a), circular(c.b), param(std::abs(c.value), false), tag, true);
                }
                break;
            case ConstraintKind::Radius:
                if (c.a.kind == GeoKind::Circle) sys.addConstraintCircleRadius(gc[c.a.id], param(c.value, false), tag, true);
                else sys.addConstraintArcRadius(ga[c.a.id], param(c.value, false), tag, true);
                break;
            case ConstraintKind::Diameter:
                if (c.a.kind == GeoKind::Circle) sys.addConstraintCircleDiameter(gc[c.a.id], param(c.value, false), tag, true);
                else sys.addConstraintArcDiameter(ga[c.a.id], param(c.value, false), tag, true);
                break;
            case ConstraintKind::Angle:
                sys.addConstraintL2LAngle(gl[c.a.id], gl[c.b.id], param(c.value * kDeg, false), tag, true);
                break;
            case ConstraintKind::ArcLength: sys.addConstraintArcLength(ga[c.a.id], param(c.value, false), tag, true); break;
            case ConstraintKind::MajorRadius: {
                auto* con = new ConstraintMajorRadius(elliptic(c.a), param(c.value, false));
                con->setTag(tag);
                sys.addConstraint(con);
                break;
            }
            case ConstraintKind::MinorRadius:
                sys.addConstraintEqual(elliptic(c.a).radmin, param(c.value, false), tag, true);
                break;
            case ConstraintKind::Parallel: sys.addConstraintParallel(gl[c.a.id], gl[c.b.id], tag, true); break;
            case ConstraintKind::Perpendicular: sys.addConstraintPerpendicular(gl[c.a.id], gl[c.b.id], tag, true); break;
            case ConstraintKind::Tangent: add_tangent(c.a, c.b, tag); break;
            case ConstraintKind::Equal:
                if (c.a.kind == GeoKind::Line) {
                    sys.addConstraintEqualLength(gl[c.a.id], gl[c.b.id], tag, true);
                } else if (is_circular(c.a.kind)) {
                    sys.addConstraintEqual(circular(c.a).rad, circular(c.b).rad, tag, true);
                } else {
                    sys.addConstraintEqualRadii(elliptic(c.a), elliptic(c.b), tag, true);
                }
                break;
            case ConstraintKind::Symmetric: sys.addConstraintP2PSymmetric(P(c.a), P(c.b), gl[c.c.id], tag, true); break;
            case ConstraintKind::SymmetricPoint: sys.addConstraintP2PSymmetric(P(c.a), P(c.b), P(c.c), tag, true); break;
            case ConstraintKind::Midpoint:
                if (c.b.kind == GeoKind::Line) {
                    sys.addConstraintP2PSymmetric(gl[c.b.id].p1, gl[c.b.id].p2, P(c.a), tag, true);
                } else {
                    GCS::Arc& a = ga[c.b.id];
                    sys.addConstraintPointOnCircle(P(c.a), a, tag, true);
                    sys.addConstraintPointOnPerpBisector(P(c.a), a.start, a.end, tag, true);
                }
                break;
            case ConstraintKind::PointOn:
                if (c.b.kind == GeoKind::Line) sys.addConstraintPointOnLine(P(c.a), gl[c.b.id], tag, true);
                else if (is_circular(c.b.kind)) sys.addConstraintPointOnCircle(P(c.a), circular(c.b), tag, true);
                else sys.addConstraintPointOnEllipse(P(c.a), elliptic(c.b), tag, true);
                break;
            case ConstraintKind::Concentric: {
                GCS::Point& c1 = is_circular(c.a.kind) ? circular(c.a).center : elliptic(c.a).center;
                GCS::Point& c2 = is_circular(c.b.kind) ? circular(c.b).center : elliptic(c.b).center;
                sys.addConstraintP2PCoincident(c1, c2, tag, true);
                break;
            }
            case ConstraintKind::Fix:
                sys.addConstraintCoordinateX(P(c.a), param(c.value, false), tag, true);
                sys.addConstraintCoordinateY(P(c.a), param(c.value2, false), tag, true);
                break;
            case ConstraintKind::Rotation:
                for (int row = 0; row < 2; ++row) {
                    auto* con = new ConstraintRotation(P(c.a), P(c.b), P(c.c), c.value * kDeg, row);
                    con->setTag(tag);
                    sys.addConstraint(con);
                }
                break;
        }
    }

    // Определённость элементов по зависимым параметрам диагностики.
    std::vector<GeoStatus> statuses() {
        GCS::VEC_pD dep;
        sys.getDependentParams(dep);
        const std::unordered_set<double*> free(dep.begin(), dep.end());
        std::vector<GeoStatus> out;
        std::vector<bool> point_ok(s.points_.size());
        for (std::size_t i = 0; i < s.points_.size(); ++i) {
            point_ok[i] = !free.count(gp[i].x) && !free.count(gp[i].y);
            if (!s.points_[i].removed) out.push_back({point_geo(static_cast<Id>(i)), point_ok[i]});
        }
        for (std::size_t k = 1; k < s.curves_.size(); ++k) {
            const GeoKind kind = static_cast<GeoKind>(k);
            for (std::size_t i = 0; i < s.curves_[k].size(); ++i) {
                const auto& r = s.curves_[k][i];
                if (r.removed) continue;
                bool ok = true;
                for (Id p : s.curve_points(r)) ok = ok && point_ok[p];
                std::vector<double*> own;
                if (kind == GeoKind::Circle) own = {gc[i].rad};
                if (kind == GeoKind::Arc) own = {ga[i].rad, ga[i].startAngle, ga[i].endAngle};
                if (kind == GeoKind::Ellipse) own = {ge[i].radmin};
                if (kind == GeoKind::EllipseArc) own = {gea[i].radmin, gea[i].startAngle, gea[i].endAngle};
                for (double* p : own) ok = ok && !free.count(p);
                out.push_back({{kind, static_cast<Id>(i)}, ok});
            }
        }
        return out;
    }
};

Report Sketch::solve() {
    Report report;
    bool solved = false;
    {
        SolverModel m(*this);
        m.build();
        m.sys.initSolution();
        const int dofs = m.sys.diagnose();
        GCS::VEC_I conflictTags, redundantTags, partialTags;
        m.sys.getConflicting(conflictTags);
        m.sys.getRedundant(redundantTags);
        m.sys.getPartiallyRedundant(partialTags);
        report.dof = std::max(dofs, 0);
        for (int t : conflictTags) {
            if (t > 0) report.conflicting.push_back(t);
        }
        for (int t : redundantTags) {
            if (t > 0) report.redundant.push_back(t);
        }
        for (int t : partialTags) {
            if (t > 0) report.partially_redundant.push_back(t);
        }
        report.geometry = m.statuses();

        const GCS::SolveStatus st = m.sys.solve();
        if (st == GCS::SolveStatus::Success || st == GCS::SolveStatus::Converged) {
            m.sys.applySolution();
            solved = true;
            for (std::size_t i = 0; i < points_.size(); ++i) points_[i].pos = {*m.gp[i].x, *m.gp[i].y};
            for (std::size_t k = 0; k < m.gc.size(); ++k) curves_[kind_index(GeoKind::Circle)][k].radius = *m.gc[k].rad;
            for (std::size_t k = 0; k < m.ga.size(); ++k) curves_[kind_index(GeoKind::Arc)][k].radius = *m.ga[k].rad;
            for (std::size_t k = 0; k < m.ge.size(); ++k) curves_[kind_index(GeoKind::Ellipse)][k].radius = *m.ge[k].radmin;
            for (std::size_t k = 0; k < m.gea.size(); ++k) {
                curves_[kind_index(GeoKind::EllipseArc)][k].radius = *m.gea[k].radmin;
            }
        }
    }
    report.converged = solved;

    // Диагностика в решённом положении: якобиан в начальном приближении может быть вырожден
    // случайно (например, совпадающие начальные точки), в решении он отражает итоговую геометрию.
    if (solved && report.conflicting.empty()) {
        SolverModel m(*this);
        m.build();
        const int dofs = m.sys.diagnose();
        GCS::VEC_I redundantTags, partialTags;
        m.sys.getRedundant(redundantTags);
        m.sys.getPartiallyRedundant(partialTags);
        report.dof = std::max(dofs, 0);
        report.redundant.clear();
        report.partially_redundant.clear();
        for (int t : redundantTags) {
            if (t > 0) report.redundant.push_back(t);
        }
        for (int t : partialTags) {
            if (t > 0) report.partially_redundant.push_back(t);
        }
        report.geometry = m.statuses();
    }

    if (!report.conflicting.empty()) {
        report.status = "конфликт";
        report.definition = Definition::Over;
    } else if (!report.redundant.empty()) {
        report.status = "избыточно";
        report.definition = Definition::Over;
    } else if (report.dof == 0) {
        report.status = "определено";
        report.definition = Definition::Fully;
    } else {
        report.status = "недоопределено";
        report.definition = Definition::Under;
    }
    return report;
}

}  // namespace bendcore::sketch2d
