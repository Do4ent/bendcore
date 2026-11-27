// Sketch editing tools (docs/modeling/SKETCH.md, section "Инструменты редактирования").

#include <bendcore/modeling/sketch_tools.hpp>

#include "sketch_internal.hpp"

#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2dAPI_ProjectPointOnCurve.hxx>
#include <Geom2dConvert_ApproxCurve.hxx>
#include <Geom2d_OffsetCurve.hxx>
#include <GeomAbs_Shape.hxx>
#include <Geom2d_BSplineCurve.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_Ellipse.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <GeomLib.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_BoundedCurve.hxx>
#include <gp_Ax22d.hxx>
#include <gp_Ax2d.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <optional>
#include <tuple>

namespace bendcore::modeling {

namespace {

using K = BoundSketch::EntityKind;
using sketch2d::Id;
constexpr double kPi = std::numbers::pi;

template <typename T>
Result<T> err(const std::string& message) {
    return Result<T>::failure(ErrorCode::InvalidInput, message);
}

Point2 P(const BoundSketch& s, Id p) {
    const auto q = s.sketch().point(p);
    return {q.x, q.y};
}

double dist(Point2 a, Point2 b) { return std::hypot(b.x - a.x, b.y - a.y); }
Point2 sub(Point2 a, Point2 b) { return {a.x - b.x, a.y - b.y}; }
Point2 add(Point2 a, Point2 b) { return {a.x + b.x, a.y + b.y}; }
Point2 mul(Point2 a, double k) { return {a.x * k, a.y * k}; }
double dot(Point2 a, Point2 b) { return a.x * b.x + a.y * b.y; }
double cross(Point2 a, Point2 b) { return a.x * b.y - a.y * b.x; }
Point2 unit(Point2 a) {
    const double l = std::hypot(a.x, a.y);
    return {a.x / l, a.y / l};
}

double wrap2pi(double a) {
    a = std::fmod(a, 2 * kPi);
    return a < 0 ? a + 2 * kPi : a;
}

// Size of the sketch: tolerance scale and length of the "infinite" line carriers.
double sketchScale(const BoundSketch& s) {
    double m = 1.0;
    for (std::size_t i = 0; i < s.sketch().point_count(); ++i) {
        if (s.sketch().is_removed(sketch2d::point_geo(static_cast<Id>(i)))) continue;
        const auto p = s.sketch().point(static_cast<Id>(i));
        m = std::max({m, std::abs(p.x), std::abs(p.y)});
    }
    return m;
}

// Line, circle or ellipse that carries a line, arc, circle, ellipse or elliptical arc entity,
// parameterised as below. Ellipse angles are parameters in the (U, V) frame of the ellipse
// (U towards the focus, V = U turned by +90°), so they grow counter-clockwise like circle angles.
struct Carrier {
    K kind = K::Line;
    Point2 a, b;          // line: start, end (t = 0, 1)
    Point2 c;             // circle/arc/ellipse: centre
    double r = 0.0;       // circle: radius; ellipse: major radius
    double rb = 0.0;      // ellipse: minor radius
    Point2 u{1, 0};       // ellipse: major direction
    double startAngle = 0.0;  // arc: angle of the start point
    double sweep = 2 * kPi;   // arc: sweep from start in the arc direction
    bool ccw = true;
    bool elliptic() const { return kind == K::Ellipse || kind == K::EllipticalArc; }
    bool closed() const { return kind == K::Circle || kind == K::Ellipse; }
    double angleOf(Point2 p) const {
        if (!elliptic()) return std::atan2(p.y - c.y, p.x - c.x);
        const Point2 d{p.x - c.x, p.y - c.y};
        const Point2 v{-u.y, u.x};
        return std::atan2((d.x * v.x + d.y * v.y) / rb, (d.x * u.x + d.y * u.y) / r);
    }
    Point2 at(double ang) const {
        if (!elliptic()) return {c.x + r * std::cos(ang), c.y + r * std::sin(ang)};
        const double x = r * std::cos(ang), y = rb * std::sin(ang);
        return {c.x + x * u.x - y * u.y, c.y + x * u.y + y * u.x};
    }
};

Result<Carrier> carrierOf(const BoundSketch& s, SketchEntityId id) {
    const auto& e = s.entity(id);
    Carrier c;
    c.kind = e.kind;
    switch (e.kind) {
        case K::Line: {
            const auto l = s.sketch().line(e.line);
            c.a = P(s, l.start);
            c.b = P(s, l.end);
            if (dist(c.a, c.b) < 1e-12) return err<Carrier>("entity " + std::to_string(id) + " has zero length");
            return c;
        }
        case K::Circle: {
            const auto ci = s.sketch().circle(e.circle);
            c.c = P(s, ci.center);
            c.r = ci.radius;
            return c;
        }
        case K::Arc: {
            const auto a = s.sketch().arc(e.arc);
            c.c = P(s, a.center);
            c.r = dist(c.c, P(s, a.start));
            c.ccw = a.ccw;
            c.startAngle = s.sketch().arc_start_angle(e.arc);
            c.sweep = s.sketch().arc_sweep(e.arc);
            return c;
        }
        case K::Ellipse:
        case K::EllipticalArc: {
            const auto& sk = s.sketch();
            const bool full = e.kind == K::Ellipse;
            c.c = P(s, full ? sk.ellipse(e.geo.id).center : sk.ellipse_arc(e.geo.id).center);
            c.rb = full ? sk.ellipse(e.geo.id).minor_radius : sk.ellipse_arc(e.geo.id).minor_radius;
            c.r = sk.ellipse_major_radius(e.geo);
            const auto d = sk.ellipse_major_direction(e.geo);
            c.u = {d.x, d.y};
            if (!(c.rb > 0) || !(c.r > 0)) return err<Carrier>("entity " + std::to_string(id) + " is a degenerate ellipse");
            if (!full) {
                const auto a = sk.ellipse_arc(e.geo.id);
                c.ccw = a.ccw;
                c.startAngle = c.angleOf(P(s, a.start));
                const double endAngle = c.angleOf(P(s, a.end));
                c.sweep = wrap2pi(c.ccw ? endAngle - c.startAngle : c.startAngle - endAngle);
                if (c.sweep < 1e-12) c.sweep = 2 * kPi;
            }
            return c;
        }
        default: return err<Carrier>("entity " + std::to_string(id) + ": not a line, arc, circle or ellipse");
    }
}

// Parameter of a point on the carrier: line — t (0 start, 1 end); circle — angle in [0, 2π);
// arc — angle from the start in the arc direction, [0, 2π).
double paramOf(const Carrier& c, Point2 p) {
    if (c.kind == K::Line) {
        const Point2 d = sub(c.b, c.a);
        return dot(sub(p, c.a), d) / dot(d, d);
    }
    const double ang = c.angleOf(p);
    if (c.closed()) return wrap2pi(ang);
    return wrap2pi(c.ccw ? ang - c.startAngle : c.startAngle - ang);
}

struct Hit {
    double u = 0.0;
    Point2 at;
    SketchEntityId with = 0;
};

// Intersections of the (extended) carrier with every other live entity.
Result<std::vector<Hit>> intersections(const BoundSketch& s, SketchEntityId id, const Carrier& c) {
    const double scale = sketchScale(s);
    const double tol = 1e-9 * scale;
    occ::handle<Geom2d_Curve> carrier;
    if (c.kind == K::Line) {
        const Point2 d = unit(sub(c.b, c.a));
        const double L = 1e3 * scale;
        carrier = new Geom2d_TrimmedCurve(new Geom2d_Line(gp_Pnt2d(c.a.x, c.a.y), gp_Dir2d(d.x, d.y)), -L, L);
    } else if (c.elliptic()) {
        carrier = new Geom2d_Ellipse(gp_Ax22d(gp_Pnt2d(c.c.x, c.c.y), gp_Dir2d(c.u.x, c.u.y), gp_Dir2d(-c.u.y, c.u.x)), c.r,
                                     c.rb);
    } else {
        carrier = new Geom2d_Circle(gp_Ax22d(gp_Pnt2d(c.c.x, c.c.y), gp_Dir2d(1, 0), gp_Dir2d(0, 1)), c.r);
    }
    std::vector<Hit> hits;
    for (const auto& e : s.entities()) {
        if (e.removed || e.id == id) continue;
        auto curve = s.curve(e.id, 1e-6 * scale);
        if (!curve) return curve.error();
        auto seg = detail::toGeom2d(curve.value().geometry, tol);
        if (!seg) continue;  // degenerate curves cut nothing
        occ::handle<Geom2d_Curve> other = seg.value().basis;
        if (!seg.value().closed) other = new Geom2d_TrimmedCurve(other, seg.value().first, seg.value().last);
        try {
            Geom2dAPI_InterCurveCurve inter(carrier, other, tol);
            for (int i = 1; i <= inter.NbPoints(); ++i) {
                const gp_Pnt2d q = inter.Point(i);
                const Point2 p{q.X(), q.Y()};
                hits.push_back({paramOf(c, p), p, e.id});
            }
        } catch (const Standard_Failure& f) {
            return Result<std::vector<Hit>>::failure(ErrorCode::OperationFailed, detail::failureText(f));
        }
    }
    return hits;
}

void removeIfUnused(BoundSketch& s, Id p) {
    if (!s.sketch().is_removed(sketch2d::point_geo(p)) && s.sketch().curves_using(p).empty()) {
        s.sketch().remove(sketch2d::point_geo(p));
    }
}

// An end point replaced by `now`: when the old point is no longer used by any curve, its
// horizontal/vertical relations move to the new point (they still hold along the same line)
// and the old point is removed with its other constraints.
void replaceEnd(BoundSketch& s, SketchEditResult& r, Id old, Id now) {
    auto& sk = s.sketch();
    if (sk.is_removed(sketch2d::point_geo(old)) || !sk.curves_using(old).empty()) return;
    using CK = sketch2d::ConstraintKind;
    for (Id c : sk.constraints_of(sketch2d::point_geo(old))) {
        const auto info = sk.constraint(c);
        if (info.kind != CK::Horizontal && info.kind != CK::Vertical) continue;
        const Id other = info.a.id == old ? info.b.id : info.a.id;
        if (other == old) continue;
        r.constraints.push_back(info.kind == CK::Horizontal ? sk.constrain_horizontal(other, now)
                                                            : sk.constrain_vertical(other, now));
    }
    sk.remove(sketch2d::point_geo(old));
}

Id newPoint(BoundSketch& s, SketchEditResult& r, Point2 p) {
    const Id id = s.sketch().add_point(p.x, p.y);
    r.points.push_back(id);
    return id;
}

void pointOn(BoundSketch& s, SketchEditResult& r, Id p, SketchEntityId on) {
    const auto& e = s.entity(on);
    if (e.kind == K::BSpline) return;  // point-on-spline is not a sketch2d constraint
    r.constraints.push_back(s.sketch().constrain_point_on(p, e.geo));
}

// Copies the radius-like constraints of a circle onto the arc that replaces it.
void retarget(BoundSketch& s, SketchEditResult& r, sketch2d::Geo from, sketch2d::Geo to) {
    auto& sk = s.sketch();
    for (Id c : sk.constraints_of(from)) {
        const auto info = sk.constraint(c);
        auto pick = [&](sketch2d::Geo g) { return g == from ? to : g; };
        using CK = sketch2d::ConstraintKind;
        Id added = 0;
        switch (info.kind) {
            case CK::Radius: added = sk.constrain_radius_of(to, info.value); break;
            case CK::Diameter: added = sk.constrain_diameter(to, info.value); break;
            case CK::Equal: added = sk.constrain_equal(pick(info.a), pick(info.b)); break;
            case CK::Concentric: added = sk.constrain_concentric(pick(info.a), pick(info.b)); break;
            case CK::Tangent: added = sk.constrain_tangent(pick(info.a), pick(info.b)); break;
            case CK::PointOn: added = sk.constrain_point_on(info.a.id, to); break;
            case CK::MajorRadius: added = sk.constrain_major_radius(to, info.value); break;
            case CK::MinorRadius: added = sk.constrain_minor_radius(to, info.value); break;
            default: break;
        }
        if (added) {
            if (!info.driving) sk.set_driving(added, false);
            r.constraints.push_back(added);
        }
    }
}


// ---- B-splines --------------------------------------------------------------------------------

occ::handle<Geom2d_BSplineCurve> splineCurve(const BoundSketch& s, SketchEntityId id) {
    const auto& d = s.sketch().bspline(s.entity(id).geo.id);
    const int n = static_cast<int>(d.poles.size());
    NCollection_Array1<gp_Pnt2d> poles(1, n);
    for (int i = 0; i < n; ++i) {
        const Point2 q = P(s, d.poles[static_cast<std::size_t>(i)]);
        poles.SetValue(i + 1, gp_Pnt2d(q.x, q.y));
    }
    const int nk = static_cast<int>(d.knots.size());
    NCollection_Array1<double> knots(1, nk);
    NCollection_Array1<int> mults(1, nk);
    for (int i = 0; i < nk; ++i) {
        knots.SetValue(i + 1, d.knots[static_cast<std::size_t>(i)]);
        mults.SetValue(i + 1, d.multiplicities[static_cast<std::size_t>(i)]);
    }
    if (d.weights.empty()) return new Geom2d_BSplineCurve(poles, knots, mults, d.degree, d.periodic);
    NCollection_Array1<double> w(1, n);
    for (int i = 0; i < n; ++i) w.SetValue(i + 1, d.weights[static_cast<std::size_t>(i)]);
    return new Geom2d_BSplineCurve(poles, w, knots, mults, d.degree, d.periodic);
}

// Parameter of the point of `curve` nearest to p.
double nearestParameter(const occ::handle<Geom2d_Curve>& curve, Point2 p, double first, double last) {
    Geom2dAPI_ProjectPointOnCurve proj(gp_Pnt2d(p.x, p.y), curve, first, last);
    double best = proj.NbPoints() > 0 ? proj.LowerDistance() : 1e300;
    double u = proj.NbPoints() > 0 ? proj.LowerDistanceParameter() : first;
    // The nearest point may be an end.
    for (double e : {first, last}) {
        const double d = curve->Value(e).Distance(gp_Pnt2d(p.x, p.y));
        if (d < best) {
            best = d;
            u = e;
        }
    }
    return u;
}

// Intersections of `curve` (parameters on it) with every other live entity.
Result<std::vector<Hit>> curveHits(const BoundSketch& s, SketchEntityId id, const occ::handle<Geom2d_Curve>& curve,
                                   double first, double last) {
    const double scale = sketchScale(s);
    const double tol = 1e-9 * scale;
    std::vector<Hit> hits;
    for (const auto& e : s.entities()) {
        if (e.removed || e.id == id) continue;
        auto cv = s.curve(e.id, 1e-6 * scale);
        if (!cv) return cv.error();
        auto seg = detail::toGeom2d(cv.value().geometry, tol);
        if (!seg) continue;
        occ::handle<Geom2d_Curve> other = seg.value().basis;
        if (!seg.value().closed) other = new Geom2d_TrimmedCurve(other, seg.value().first, seg.value().last);
        try {
            Geom2dAPI_InterCurveCurve inter(curve, other, tol);
            for (int i = 1; i <= inter.NbPoints(); ++i) {
                const gp_Pnt2d q = inter.Point(i);
                const Point2 p{q.X(), q.Y()};
                hits.push_back({nearestParameter(curve, p, first, last), p, e.id});
            }
        } catch (const Standard_Failure& f) {
            return Result<std::vector<Hit>>::failure(ErrorCode::OperationFailed, detail::failureText(f));
        }
    }
    return hits;
}

// Sketch data of an OCCT spline. Poles at the position of an existing pole of `reuse` keep that
// point (with its constraints); the others become new points.
sketch2d::BSplineData splineData(BoundSketch& s, SketchEditResult& r, const occ::handle<Geom2d_BSplineCurve>& c,
                                 const std::vector<Id>& reuse, std::vector<Id>& taken) {
    const double tol = 1e-9 * sketchScale(s);
    sketch2d::BSplineData d;
    d.degree = c->Degree();
    d.periodic = c->IsPeriodic();
    for (int i = 1; i <= c->NbPoles(); ++i) {
        const gp_Pnt2d q = c->Pole(i);
        const Point2 p{q.X(), q.Y()};
        Id id = -1;
        for (Id o : reuse) {
            if (std::find(taken.begin(), taken.end(), o) == taken.end() && dist(P(s, o), p) < tol) {
                id = o;
                break;
            }
        }
        if (id < 0) id = newPoint(s, r, p);
        taken.push_back(id);
        d.poles.push_back(id);
    }
    for (int i = 1; i <= c->NbKnots(); ++i) {
        d.knots.push_back(c->Knot(i));
        d.multiplicities.push_back(c->Multiplicity(i));
    }
    if (c->IsRational()) {
        for (int i = 1; i <= c->NbPoles(); ++i) d.weights.push_back(c->Weight(i));
    }
    return d;
}

// Piece [u1, u2] of a spline as a clamped non-periodic spline.
Result<occ::handle<Geom2d_BSplineCurve>> splinePiece(const occ::handle<Geom2d_BSplineCurve>& c, double u1, double u2) {
    try {
        occ::handle<Geom2d_BSplineCurve> piece = occ::down_cast<Geom2d_BSplineCurve>(c->Copy());
        if (piece->IsPeriodic()) {
            const double period = piece->LastParameter() - piece->FirstParameter();
            while (u2 <= u1) u2 += period;
        }
        piece->Segment(u1, u2);
        if (piece->IsPeriodic()) piece->SetNotPeriodic();
        return piece;
    } catch (const Standard_Failure& f) {
        return Result<occ::handle<Geom2d_BSplineCurve>>::failure(ErrorCode::OperationFailed, detail::failureText(f));
    }
}

// New inner poles of a cut or extended spline keep their offsets from `anchor` (a kept end), so
// the piece keeps its shape and follows that end; poles taken over from the source keep their
// own constraints.
void tieInnerPoles(BoundSketch& s, SketchEditResult& r, const sketch2d::BSplineData& d, Id anchor,
                   const std::vector<Id>& old) {
    const Point2 o = P(s, anchor);
    for (std::size_t k = 1; k + 1 < d.poles.size(); ++k) {
        const Id q = d.poles[k];
        if (q == anchor || std::find(old.begin(), old.end(), q) != old.end()) continue;
        const Point2 pq = P(s, q);
        r.constraints.push_back(s.sketch().constrain_distance_x(anchor, q, pq.x - o.x));
        r.constraints.push_back(s.sketch().constrain_distance_y(anchor, q, pq.y - o.y));
    }
}

void removeUnusedPoles(BoundSketch& s, const std::vector<Id>& poles) {
    for (Id p : poles) removeIfUnused(s, p);
}

Result<SketchEditResult> trimSpline(BoundSketch& s, SketchEntityId id, const Point2& pick) {
    using R = SketchEditResult;
    const BoundSketch::Entity e = s.entity(id);
    const sketch2d::BSplineData old = s.sketch().bspline(e.geo.id);
    occ::handle<Geom2d_BSplineCurve> c = splineCurve(s, id);
    const double u0 = c->FirstParameter(), u1 = c->LastParameter();
    auto hits = curveHits(s, id, c, u0, u1);
    if (!hits) return hits.error();
    const double eps = 1e-9 * (u1 - u0);
    const double tp = nearestParameter(c, pick, u0, u1);
    SketchEditResult r;
    std::vector<Id> taken;
    if (c->IsPeriodic()) {
        std::vector<Hit> hs;
        for (const Hit& h : hits.value()) {
            if (std::none_of(hs.begin(), hs.end(), [&](const Hit& o) { return std::abs(o.u - h.u) < 1e-7 * (u1 - u0); })) {
                hs.push_back(h);
            }
        }
        if (hs.size() < 2) return err<R>("trim: closed spline " + std::to_string(id) + " needs at least two intersections");
        std::sort(hs.begin(), hs.end(), [](const Hit& a, const Hit& b) { return a.u < b.u; });
        std::size_t next = 0;
        while (next < hs.size() && hs[next].u <= tp) ++next;
        const Hit& hi = hs[next % hs.size()];
        const Hit& lo = hs[(next + hs.size() - 1) % hs.size()];
        auto piece = splinePiece(c, hi.u, lo.u);
        if (!piece) return piece.error();
        sketch2d::BSplineData d = splineData(s, r, piece.value(), old.poles, taken);
        s.setBSpline(id, d);
        r.modified.push_back(id);
        tieInnerPoles(s, r, d, d.poles.front(), old.poles);
        pointOn(s, r, d.poles.front(), hi.with);
        pointOn(s, r, d.poles.back(), lo.with);
        removeUnusedPoles(s, old.poles);
        return r;
    }
    std::optional<Hit> lo, hi;
    for (const Hit& h : hits.value()) {
        if (h.u <= u0 + eps || h.u >= u1 - eps) continue;
        if (h.u < tp && (!lo || h.u > lo->u)) lo = h;
        if (h.u > tp && (!hi || h.u < hi->u)) hi = h;
    }
    if (!lo && !hi) {
        s.removeEntity(id);
        r.removed.push_back(id);
        removeUnusedPoles(s, old.poles);
        return r;
    }
    // Kept pieces: [u0, lo] and [hi, u1]. The kept curve ends keep their points.
    std::optional<sketch2d::BSplineData> first, second;
    if (lo) {
        auto piece = splinePiece(c, u0, lo->u);
        if (!piece) return piece.error();
        first = splineData(s, r, piece.value(), old.poles, taken);
    }
    if (hi) {
        auto piece = splinePiece(c, hi->u, u1);
        if (!piece) return piece.error();
        second = splineData(s, r, piece.value(), old.poles, taken);
    }
    if (first) {
        s.setBSpline(id, *first);
        r.modified.push_back(id);
        tieInnerPoles(s, r, *first, first->poles.front(), old.poles);
        pointOn(s, r, first->poles.back(), lo->with);
    }
    if (second) {
        if (first) {
            r.created.push_back(s.addBSpline(*second, e.construction));
        } else {
            s.setBSpline(id, *second);
            r.modified.push_back(id);
        }
        tieInnerPoles(s, r, *second, second->poles.back(), old.poles);
        pointOn(s, r, second->poles.front(), hi->with);
    }
    removeUnusedPoles(s, old.poles);
    return r;
}

// GeomLib::ExtendCurveToPoint works on 3D curves: the spline is lifted to z = 0.
Result<occ::handle<Geom2d_BSplineCurve>> extendedSpline(const occ::handle<Geom2d_BSplineCurve>& c, Point2 to, bool after) {
    using RC = Result<occ::handle<Geom2d_BSplineCurve>>;
    try {
        const int n = c->NbPoles();
        NCollection_Array1<gp_Pnt> poles(1, n);
        for (int i = 1; i <= n; ++i) poles.SetValue(i, gp_Pnt(c->Pole(i).X(), c->Pole(i).Y(), 0));
        NCollection_Array1<double> knots(1, c->NbKnots());
        NCollection_Array1<int> mults(1, c->NbKnots());
        for (int i = 1; i <= c->NbKnots(); ++i) {
            knots.SetValue(i, c->Knot(i));
            mults.SetValue(i, c->Multiplicity(i));
        }
        occ::handle<Geom_BSplineCurve> c3;
        if (c->IsRational()) {
            NCollection_Array1<double> w(1, n);
            for (int i = 1; i <= n; ++i) w.SetValue(i, c->Weight(i));
            c3 = new Geom_BSplineCurve(poles, w, knots, mults, c->Degree());
        } else {
            c3 = new Geom_BSplineCurve(poles, knots, mults, c->Degree());
        }
        occ::handle<Geom_BoundedCurve> bounded = c3;
        GeomLib::ExtendCurveToPoint(bounded, gp_Pnt(to.x, to.y, 0), std::clamp(c->Degree() - 1, 1, 2), after);
        occ::handle<Geom_BSplineCurve> e3 = occ::down_cast<Geom_BSplineCurve>(bounded);
        if (e3.IsNull()) return RC::failure(ErrorCode::OperationFailed, "extend: the spline could not be extended");
        const int m = e3->NbPoles();
        NCollection_Array1<gp_Pnt2d> p2(1, m);
        for (int i = 1; i <= m; ++i) p2.SetValue(i, gp_Pnt2d(e3->Pole(i).X(), e3->Pole(i).Y()));
        NCollection_Array1<double> k2(1, e3->NbKnots());
        NCollection_Array1<int> m2(1, e3->NbKnots());
        for (int i = 1; i <= e3->NbKnots(); ++i) {
            k2.SetValue(i, e3->Knot(i));
            m2.SetValue(i, e3->Multiplicity(i));
        }
        if (e3->IsRational()) {
            NCollection_Array1<double> w(1, m);
            for (int i = 1; i <= m; ++i) w.SetValue(i, e3->Weight(i));
            return occ::handle<Geom2d_BSplineCurve>(new Geom2d_BSplineCurve(p2, w, k2, m2, e3->Degree()));
        }
        return occ::handle<Geom2d_BSplineCurve>(new Geom2d_BSplineCurve(p2, k2, m2, e3->Degree()));
    } catch (const Standard_Failure& f) {
        return RC::failure(ErrorCode::OperationFailed, detail::failureText(f));
    }
}

Result<SketchEditResult> extendSpline(BoundSketch& s, SketchEntityId id, const Point2& pick) {
    using R = SketchEditResult;
    const BoundSketch::Entity e = s.entity(id);
    const sketch2d::BSplineData old = s.sketch().bspline(e.geo.id);
    if (old.periodic) return err<R>("extend: spline " + std::to_string(id) + " is closed");
    occ::handle<Geom2d_BSplineCurve> c = splineCurve(s, id);
    const double u0 = c->FirstParameter(), u1 = c->LastParameter();
    const Point2 ps = P(s, old.poles.front()), pe = P(s, old.poles.back());
    const double scale = sketchScale(s);
    if (dist(ps, pe) < 1e-9 * scale) return err<R>("extend: spline " + std::to_string(id) + " is closed");
    const bool atEnd = dist(pick, pe) < dist(pick, ps);
    gp_Pnt2d endPnt;
    gp_Vec2d tan;
    c->D1(atEnd ? u1 : u0, endPnt, tan);
    if (tan.Magnitude() < 1e-12) return err<R>("extend: spline " + std::to_string(id) + " has no tangent at that end");
    const Point2 dir = unit({atEnd ? tan.X() : -tan.X(), atEnd ? tan.Y() : -tan.Y()});
    const Point2 endP{endPnt.X(), endPnt.Y()};
    // Probe extensions of growing length along the end tangent; the nearest intersection on the
    // first probe that meets another entity is the target, and the spline is then extended
    // exactly to it.
    double chord = 0.0;
    for (int i = 0; i <= 16; ++i) {
        const gp_Pnt2d q = c->Value(u0 + (u1 - u0) * i / 16.0);
        chord = std::max(chord, dist(endP, {q.X(), q.Y()}));
    }
    const double limit = 4 * scale + chord;
    std::optional<Hit> best;
    for (double len = std::max(chord, 1e-6 * scale); !best && len <= 2 * limit; len *= 2) {
        auto probe = extendedSpline(c, add(endP, mul(dir, len)), atEnd);
        if (!probe) continue;
        const occ::handle<Geom2d_BSplineCurve>& pc = probe.value();
        const double pf = pc->FirstParameter(), pl = pc->LastParameter();
        const double ue = nearestParameter(pc, endP, pf, pl);  // the original end on the probe
        auto hits = curveHits(s, id, pc, pf, pl);
        if (!hits) return hits.error();
        const double eps = 1e-9 * (pl - pf);
        for (const Hit& h : hits.value()) {
            const bool beyond = atEnd ? h.u > ue + eps : h.u < ue - eps;
            if (!beyond || dist(h.at, endP) < 1e-9 * scale) continue;
            if (!best || std::abs(h.u - ue) < std::abs(best->u - ue)) best = h;
        }
    }
    if (!best) return err<R>("extend: entity " + std::to_string(id) + " meets nothing beyond that end");
    auto ext = extendedSpline(c, best->at, atEnd);
    if (!ext) return ext.error();
    SketchEditResult r;
    std::vector<Id> taken;
    // Poles that keep their position keep their points; the extended end gets a new point.
    std::vector<Id> reuse = old.poles;
    const Id moved = atEnd ? old.poles.back() : old.poles.front();
    reuse.erase(std::remove(reuse.begin(), reuse.end(), moved), reuse.end());
    sketch2d::BSplineData d = splineData(s, r, ext.value(), reuse, taken);
    s.setBSpline(id, d);
    r.modified.push_back(id);
    tieInnerPoles(s, r, d, atEnd ? d.poles.front() : d.poles.back(), old.poles);
    pointOn(s, r, atEnd ? d.poles.back() : d.poles.front(), best->with);
    removeUnusedPoles(s, old.poles);
    return r;
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Trim / extend
// ---------------------------------------------------------------------------------------------

Result<SketchEditResult> trimEntity(BoundSketch& s, SketchEntityId id, const Point2& pick) {
    using R = SketchEditResult;
    if (id == 0 || id > s.entities().size() || s.entity(id).removed) return err<R>("no live sketch entity " + std::to_string(id));
    if (s.entity(id).kind == K::BSpline) return trimSpline(s, id, pick);
    auto car = carrierOf(s, id);
    if (!car) return car.error();
    const Carrier c = car.value();
    auto hits = intersections(s, id, c);
    if (!hits) return hits.error();
    const double eps = 1e-9;
    const double tp = paramOf(c, pick);
    const BoundSketch::Entity e = s.entity(id);
    SketchEditResult r;

    if (c.closed()) {
        std::vector<Hit> hs;
        for (const Hit& h : hits.value()) {
            if (std::none_of(hs.begin(), hs.end(), [&](const Hit& o) { return std::abs(o.u - h.u) < eps; })) hs.push_back(h);
        }
        if (hs.size() < 2) {
            return err<R>(std::string("trim: ") + (c.kind == K::Circle ? "circle " : "ellipse ") + std::to_string(id) +
                          " needs at least two intersections");
        }
        std::sort(hs.begin(), hs.end(), [](const Hit& a, const Hit& b) { return a.u < b.u; });
        // Neighbours of the pick angle (cyclic): the kept arc runs ccw from `next` to `prev`.
        std::size_t next = 0;
        while (next < hs.size() && hs[next].u <= tp) ++next;
        const Hit& hi = hs[next % hs.size()];
        const Hit& lo = hs[(next + hs.size() - 1) % hs.size()];
        const Id ps = newPoint(s, r, hi.at), pe = newPoint(s, r, lo.at);
        const SketchEntityId arc =
            c.kind == K::Circle
                ? s.addArc(e.center, ps, pe, true, e.construction)
                : s.addEllipticalArc(e.center, e.focus, s.sketch().ellipse(e.geo.id).minor_radius, ps, pe, true, e.construction);
        r.created.push_back(arc);
        retarget(s, r, e.geo, s.entity(arc).geo);
        pointOn(s, r, ps, hi.with);
        pointOn(s, r, pe, lo.with);
        s.removeEntity(id);
        r.removed.push_back(id);
        return r;
    }

    const double end = c.kind == K::Line ? 1.0 : c.sweep;
    if (tp < -eps || tp > end + eps) return err<R>("trim: the pick point is outside entity " + std::to_string(id));
    std::optional<Hit> lo, hi;
    for (const Hit& h : hits.value()) {
        if (h.u <= eps || h.u >= end - eps) continue;
        if (h.u < tp && (!lo || h.u > lo->u)) lo = h;
        if (h.u > tp && (!hi || h.u < hi->u)) hi = h;
    }
    const Id start = e.start, finish = e.end;
    // In the arc parameter, u grows from the arc start, so the entity's start/end are u = 0/sweep.
    if (!lo && !hi) {
        s.removeEntity(id);
        r.removed.push_back(id);
        removeIfUnused(s, start);
        removeIfUnused(s, finish);
        return r;
    }
    if (!lo) {
        const Id q = newPoint(s, r, hi->at);
        s.setEndPoints(id, q, finish);
        pointOn(s, r, q, hi->with);
        r.modified.push_back(id);
        replaceEnd(s, r, start, q);
        return r;
    }
    if (!hi) {
        const Id q = newPoint(s, r, lo->at);
        s.setEndPoints(id, start, q);
        pointOn(s, r, q, lo->with);
        r.modified.push_back(id);
        replaceEnd(s, r, finish, q);
        return r;
    }
    const Id q1 = newPoint(s, r, lo->at), q2 = newPoint(s, r, hi->at);
    s.setEndPoints(id, start, q1);
    r.modified.push_back(id);
    SketchEntityId piece = 0;
    if (c.kind == K::Line) {
        piece = s.addLine(q2, finish, e.construction);
        r.constraints.push_back(s.sketch().constrain_point_on(q2, e.geo));
        r.constraints.push_back(s.sketch().constrain_point_on(finish, e.geo));
    } else if (c.kind == K::Arc) {
        piece = s.addArc(e.center, q2, finish, e.counterClockwise, e.construction);
        r.constraints.push_back(s.sketch().constrain_equal(e.geo, s.entity(piece).geo));
    } else {
        // Same centre and focus points; equal minor radii (sketch2d then equates only them).
        piece = s.addEllipticalArc(e.center, e.focus, s.sketch().ellipse_arc(e.geo.id).minor_radius, q2, finish,
                                   e.counterClockwise, e.construction);
        r.constraints.push_back(s.sketch().constrain_equal(e.geo, s.entity(piece).geo));
    }
    r.created.push_back(piece);
    pointOn(s, r, q1, lo->with);
    pointOn(s, r, q2, hi->with);
    return r;
}

Result<SketchEditResult> extendEntity(BoundSketch& s, SketchEntityId id, const Point2& pick) {
    using R = SketchEditResult;
    if (id == 0 || id > s.entities().size() || s.entity(id).removed) return err<R>("no live sketch entity " + std::to_string(id));
    const BoundSketch::Entity e = s.entity(id);
    if (e.kind == K::BSpline) return extendSpline(s, id, pick);
    if (e.kind != K::Line && e.kind != K::Arc && e.kind != K::EllipticalArc) {
        return err<R>("extend: entity " + std::to_string(id) + " is closed (a circle or an ellipse)");
    }
    auto car = carrierOf(s, id);
    if (!car) return car.error();
    const Carrier c = car.value();
    auto hits = intersections(s, id, c);
    if (!hits) return hits.error();
    const bool atEnd = dist(pick, P(s, e.end)) < dist(pick, P(s, e.start));
    const double eps = 1e-9;
    const double end = c.kind == K::Line ? 1.0 : c.sweep;
    std::optional<Hit> best;
    for (const Hit& h : hits.value()) {
        if (c.kind == K::Line) {
            if (atEnd && h.u > 1 + eps && (!best || h.u < best->u)) best = h;
            if (!atEnd && h.u < -eps && (!best || h.u > best->u)) best = h;
        } else if (h.u > end + eps && h.u < 2 * kPi - eps) {
            // Beyond the end going forward (smallest u) or before the start going back (largest u).
            if (atEnd && (!best || h.u < best->u)) best = h;
            if (!atEnd && (!best || h.u > best->u)) best = h;
        }
    }
    if (!best) return err<R>("extend: entity " + std::to_string(id) + " meets nothing beyond that end");
    SketchEditResult r;
    const Id q = newPoint(s, r, best->at);
    const Id old = atEnd ? e.end : e.start;
    if (atEnd) s.setEndPoints(id, e.start, q);
    else s.setEndPoints(id, q, e.end);
    r.modified.push_back(id);
    pointOn(s, r, q, best->with);
    replaceEnd(s, r, old, q);
    return r;
}

// ---------------------------------------------------------------------------------------------
// Offset
// ---------------------------------------------------------------------------------------------

namespace {

// One entity of the chain in traversal order and its offset.
struct OffsetPiece {
    SketchEntityId source = 0;
    K kind = K::Line;
    bool reversed = false;  // traversed from the entity end to its start
    Id from = -1, to = -1;  // traversal start/end points of the source (-1 for closed entities)
    occ::handle<Geom2d_Curve> curve;  // source curve, oriented along the traversal
    double u0 = 0, u1 = 0;
    // Offset: lines and arcs analytic, everything else an approximating spline.
    Point2 a, b;            // offset start, end
    Point2 c;               // arc/circle: centre
    double r = 0.0;
    occ::handle<Geom2d_BSplineCurve> spline;
    bool analytic() const { return kind == K::Line || kind == K::Arc || kind == K::Circle; }
};

// Intersections of the carriers (line through a,b / circle c,r) of two analytic offsets.
std::vector<Point2> meet(const OffsetPiece& p, const OffsetPiece& q) {
    std::vector<Point2> out;
    auto lineCircle = [&](Point2 a, Point2 b, Point2 c, double r) {
        const Point2 d = unit(sub(b, a));
        const Point2 f = sub(a, c);
        const double B = dot(f, d), C = dot(f, f) - r * r, D = B * B - C;
        if (D < -1e-12 * r * r) return;
        const double sq = std::sqrt(std::max(D, 0.0));
        out.push_back(add(a, mul(d, -B - sq)));
        out.push_back(add(a, mul(d, -B + sq)));
    };
    const bool pl = p.kind == K::Line, ql = q.kind == K::Line;
    if (pl && ql) {
        const Point2 d1 = sub(p.b, p.a), d2 = sub(q.b, q.a);
        const double den = cross(d1, d2);
        if (std::abs(den) < 1e-14 * dot(d1, d1)) return out;
        const double t = cross(sub(q.a, p.a), d2) / den;
        out.push_back(add(p.a, mul(d1, t)));
    } else if (pl) {
        lineCircle(p.a, p.b, q.c, q.r);
    } else if (ql) {
        lineCircle(q.a, q.b, p.c, p.r);
    } else {
        const double d = dist(p.c, q.c);
        if (d < 1e-14 || d > p.r + q.r || d < std::abs(p.r - q.r)) return out;
        const double a = (p.r * p.r - q.r * q.r + d * d) / (2 * d);
        const double h = std::sqrt(std::max(p.r * p.r - a * a, 0.0));
        const Point2 u = unit(sub(q.c, p.c)), m = add(p.c, mul(u, a)), n{-u.y, u.x};
        out.push_back(add(m, mul(n, h)));
        out.push_back(add(m, mul(n, -h)));
    }
    return out;
}

// Offset curve as an OCCT curve (lines: a long segment through the offset, so that it can be
// intersected beyond its ends).
occ::handle<Geom2d_Curve> offsetCurve(const OffsetPiece& p, double scale) {
    if (p.kind == K::Line) {
        const Point2 d = unit(sub(p.b, p.a));
        const double L = 1e3 * scale;
        return new Geom2d_TrimmedCurve(new Geom2d_Line(gp_Pnt2d(p.a.x, p.a.y), gp_Dir2d(d.x, d.y)), -L, L);
    }
    if (p.kind == K::Arc || p.kind == K::Circle) {
        return new Geom2d_Circle(gp_Ax22d(gp_Pnt2d(p.c.x, p.c.y), gp_Dir2d(1, 0), gp_Dir2d(0, 1)), p.r);
    }
    return occ::handle<Geom2d_Curve>(p.spline);
}

Point2 tangentAt(const OffsetPiece& p, bool atEnd) {
    gp_Pnt2d q;
    gp_Vec2d v;
    p.curve->D1(atEnd ? p.u1 : p.u0, q, v);
    if (v.Magnitude() < 1e-300) return {0, 0};
    return unit({v.X(), v.Y()});
}

struct EntityEnds {
    Id start = -1, end = -1;
};

EntityEnds endsOf(const BoundSketch& s, const BoundSketch::Entity& e) {
    switch (e.kind) {
        case K::Line:
        case K::Arc:
        case K::EllipticalArc: return {e.start, e.end};
        case K::BSpline: {
            const auto& d = s.sketch().bspline(e.geo.id);
            if (d.periodic) return {};
            return {d.poles.front(), d.poles.back()};
        }
        default: return {};
    }
}

// Constraints added by an offset that the solver finds redundant (a closed chain links the
// distances twice; tangent joints imply what offset distances also say) are removed, newest
// first, never a dimension other constraints take their value from.
void pruneRedundant(BoundSketch& s, SketchEditResult& r) {
    for (std::size_t guard = 0; guard < r.constraints.size(); ++guard) {
        BoundSketch probe = s;
        const auto rep = probe.sketch().solve();
        std::optional<Id> drop;
        for (Id c : rep.redundant) {
            if (std::find(r.constraints.begin(), r.constraints.end(), c) == r.constraints.end()) continue;
            bool isMaster = false;
            for (Id o : r.constraints) {
                if (!s.sketch().constraint(o).removed && s.sketch().constraint(o).master == c) isMaster = true;
            }
            if (!isMaster && (!drop || c > *drop)) drop = c;
        }
        if (!drop) return;
        s.sketch().remove_constraint(*drop);
        r.constraints.erase(std::remove(r.constraints.begin(), r.constraints.end(), *drop), r.constraints.end());
    }
}

// One side of the offset. `master` (0: none yet) is the offset dimension the distances follow;
// the first constrained offset becomes it.
Result<SketchEditResult> offsetSide(BoundSketch& s, const std::vector<OffsetPiece>& chain, bool closed, double d,
                                    OffsetCorners corners, bool constrain, Id& master) {
    using R = SketchEditResult;
    const double scale = sketchScale(s);
    const double tol = 1e-7 * scale;
    const std::size_t n = chain.size();
    std::vector<OffsetPiece> pieces = chain;
    for (OffsetPiece& p : pieces) {
        const auto& e = s.entity(p.source);
        const gp_Pnt2d q0 = p.curve->Value(p.u0), q1 = p.curve->Value(p.u1);
        const Point2 a{q0.X(), q0.Y()}, b{q1.X(), q1.Y()};
        if (p.kind == K::Line) {
            const Point2 t = unit(sub(b, a)), left{-t.y, t.x};
            p.a = add(a, mul(left, d));
            p.b = add(b, mul(left, d));
        } else if (p.kind == K::Arc || p.kind == K::Circle) {
            p.c = P(s, e.center);
            const double r0 = p.kind == K::Circle ? s.sketch().circle(e.circle).radius : dist(p.c, a);
            // Counter-clockwise traversal: the left side is towards the centre.
            const Point2 t = tangentAt(p, false), radial = sub(a, p.c);
            const bool travCcw = p.kind == K::Circle ? !p.reversed : cross(radial, t) > 0;
            p.r = r0 + (travCcw ? -d : d);
            if (!(p.r > tol)) {
                return err<R>("offset: arc " + std::to_string(p.source) + " collapses (radius " + std::to_string(p.r) + ")");
            }
            p.a = add(p.c, mul(unit(sub(a, p.c)), p.r));
            p.b = add(p.c, mul(unit(sub(b, p.c)), p.r));
        } else {
            try {
                occ::handle<Geom2d_Curve> basis = new Geom2d_TrimmedCurve(p.curve, p.u0, p.u1);
                // Geom2d_OffsetCurve puts a positive offset on the right of the curve.
                occ::handle<Geom2d_OffsetCurve> off = new Geom2d_OffsetCurve(basis, -d);
                {
                    const double um = 0.5 * (p.u0 + p.u1);
                    gp_Pnt2d q;
                    gp_Vec2d t;
                    basis->D1(um, q, t);
                    const gp_Pnt2d qo = off->Value(um);
                    if (((qo.X() - q.X()) * -t.Y() + (qo.Y() - q.Y()) * t.X()) * d < 0) off = new Geom2d_OffsetCurve(basis, d);
                }
                Geom2dConvert_ApproxCurve approx(off, 1e-6 * scale, GeomAbs_C2, 64, 5);
                if (!approx.HasResult()) return err<R>("offset: entity " + std::to_string(p.source) + " cannot be offset");
                p.spline = approx.Curve();
            } catch (const Standard_Failure& f) {
                return Result<R>::failure(ErrorCode::OperationFailed, "offset: entity " + std::to_string(p.source) + ": " +
                                                                          detail::failureText(f));
            }
            const gp_Pnt2d s0 = p.spline->Value(p.spline->FirstParameter()), s1 = p.spline->Value(p.spline->LastParameter());
            p.a = {s0.X(), s0.Y()};
            p.b = {s1.X(), s1.Y()};
        }
    }

    // Joints. Joint j is between piece j-1 and piece j (j = 0: chain start, j = n: chain end).
    struct Joint {
        Point2 at;             // offset joint point (Round at a convex corner: unused)
        bool roundCorner = false;
        Point2 p1, p2;         // round corner: end of piece j-1, start of piece j
        Id vertex = -1;        // round corner: the original corner point (arc centre)
    };
    std::vector<Joint> joint(n + 1);
    joint[0].at = pieces[0].a;
    joint[n].at = pieces[n - 1].b;
    auto corner = [&](std::size_t i, std::size_t j, Joint& out) -> std::optional<std::string> {
        const OffsetPiece& p = pieces[i];
        const OffsetPiece& q = pieces[j];
        if (dist(p.b, q.a) < tol) {
            out.at = p.b;
            return std::nullopt;
        }
        const gp_Pnt2d vp = p.curve->Value(p.u1);
        const Point2 v{vp.X(), vp.Y()};
        const Point2 t1 = tangentAt(p, true), t2 = tangentAt(q, false);
        const bool convex = cross(t1, t2) * d < 0;
        const bool bothAnalytic = p.analytic() && q.analytic();
        if (convex && (corners == OffsetCorners::Round || !bothAnalytic)) {
            out.roundCorner = true;
            out.p1 = p.b;
            out.p2 = q.a;
            out.vertex = p.to;
            return std::nullopt;
        }
        std::optional<Point2> best;
        if (bothAnalytic) {
            for (Point2 x : meet(p, q)) {
                if (!best || dist(x, v) < dist(*best, v)) best = x;
            }
        } else {
            try {
                Geom2dAPI_InterCurveCurve inter(offsetCurve(p, scale), offsetCurve(q, scale), tol);
                for (int k = 1; k <= inter.NbPoints(); ++k) {
                    const Point2 x{inter.Point(k).X(), inter.Point(k).Y()};
                    if (!best || dist(x, v) < dist(*best, v)) best = x;
                }
            } catch (const Standard_Failure& f) {
                return detail::failureText(f);
            }
        }
        if (!best) {
            return "offset: the offsets of entities " + std::to_string(p.source) + " and " + std::to_string(q.source) +
                   " do not meet";
        }
        out.at = *best;
        return std::nullopt;
    };
    for (std::size_t j = 1; j < n; ++j) {
        if (auto e = corner(j - 1, j, joint[j])) return err<R>(*e);
    }
    if (closed && n > 1) {
        if (auto e = corner(n - 1, 0, joint[0])) return err<R>(*e);
        joint[n] = joint[0];
    }

    SketchEditResult r;
    auto& sk = s.sketch();
    // Points: start and end of every offset piece (shared at sharp and tangent joints).
    std::vector<Id> startPt(n), endPt(n);
    std::vector<Id> jp(n + 1, -1);
    for (std::size_t j = 0; j <= n; ++j) {
        if (closed && j == n) {
            jp[n] = jp[0];
            continue;
        }
        if (n == 1 && closed) break;  // a single closed entity (circle, ellipse, periodic spline)
        if (!joint[j].roundCorner) jp[j] = newPoint(s, r, joint[j].at);
    }
    for (std::size_t i = 0; i < n; ++i) {
        const Joint& js = joint[i];
        const Joint& je = joint[i + 1];
        startPt[i] = js.roundCorner ? newPoint(s, r, js.p2) : jp[i];
        endPt[i] = je.roundCorner ? newPoint(s, r, je.p1) : jp[i + 1];
    }
    // A distance constraint of a piece: the first one is the dimension, the others follow it.
    auto distance = [&](SketchEntityId made, const BoundSketch::Entity& src) {
        const Id c = sk.constrain_offset_distance(s.entity(made).geo, src.geo, std::abs(d), master);
        if (master == 0) {
            master = c;
            r.dimension = c;
        }
        r.constraints.push_back(c);
    };
    std::vector<SketchEntityId> made(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        OffsetPiece& p = pieces[i];
        const BoundSketch::Entity src = s.entity(p.source);  // copy: adding entities reallocates
        const Id a = p.reversed ? endPt[i] : startPt[i];
        const Id b = p.reversed ? startPt[i] : endPt[i];
        if (p.kind == K::Line) {
            made[i] = s.addLine(a, b, src.construction);
            if (constrain) {
                r.constraints.push_back(sk.constrain_parallel(s.entity(made[i]).line, src.line));
                distance(made[i], src);
            }
        } else if (p.kind == K::Arc) {
            made[i] = s.addArc(src.center, a, b, src.counterClockwise, src.construction);
            if (constrain) distance(made[i], src);
        } else if (p.kind == K::Circle) {
            made[i] = s.addCircle(src.center, p.r, src.construction);
            if (constrain) distance(made[i], src);
        } else {
            // Approximating spline: its end poles are the joint points; inner poles keep their
            // offsets from a point of the source (they follow it when it moves).
            occ::handle<Geom2d_BSplineCurve> sp = p.spline;
            if (p.reversed) sp = occ::down_cast<Geom2d_BSplineCurve>(sp->Reversed());
            sketch2d::BSplineData data;
            data.degree = sp->Degree();
            const bool loop = n == 1 && closed;
            const int np = sp->NbPoles();
            for (int k = 1; k <= np; ++k) {
                const Point2 q{sp->Pole(k).X(), sp->Pole(k).Y()};
                if (!loop && k == 1) data.poles.push_back(a);
                else if (!loop && k == np) data.poles.push_back(b);
                else if (loop && k == np && dist(q, P(s, data.poles.front())) < tol) data.poles.push_back(data.poles.front());
                else data.poles.push_back(newPoint(s, r, q));
            }
            for (int k = 1; k <= sp->NbKnots(); ++k) {
                data.knots.push_back(sp->Knot(k));
                data.multiplicities.push_back(sp->Multiplicity(k));
            }
            if (sp->IsRational()) {
                for (int k = 1; k <= np; ++k) data.weights.push_back(sp->Weight(k));
            }
            made[i] = s.addBSpline(data, src.construction);
            if (constrain) {
                const Id anchor = src.kind == K::BSpline ? sk.bspline(src.geo.id).poles.front() : src.center;
                const Point2 o = P(s, anchor);
                for (std::size_t k = 0; k < data.poles.size(); ++k) {
                    const Id q = data.poles[k];
                    if (q == a || q == b || (k > 0 && q == data.poles.front())) continue;
                    const Point2 pq = P(s, q);
                    r.constraints.push_back(sk.constrain_distance_x(anchor, q, pq.x - o.x));
                    r.constraints.push_back(sk.constrain_distance_y(anchor, q, pq.y - o.y));
                }
            }
        }
        r.created.push_back(made[i]);
    }
    // Corner arcs (round corners) and tangent constraints at the joints.
    auto joinTangent = [&](SketchEntityId x, SketchEntityId y) {
        const K kx = s.entity(x).kind, ky = s.entity(y).kind;
        if (kx == K::Line && ky == K::Line) return;
        if (kx == K::EllipticalArc || ky == K::EllipticalArc || kx == K::Ellipse || ky == K::Ellipse) return;
        r.constraints.push_back(sk.constrain_tangent(s.entity(x).geo, s.entity(y).geo));
    };
    for (std::size_t j = 0; j <= n; ++j) {
        const bool inner = j > 0 && j < n;
        const bool closing = closed && n > 1 && j == 0;
        if (!inner && !closing) continue;
        const std::size_t i = closing ? n - 1 : j - 1, k = closing ? 0 : j;
        if (joint[j].roundCorner) {
            const Id p1 = endPt[i], p2 = startPt[k];
            const Point2 v = P(s, joint[j].vertex);
            const bool ccw = cross(sub(joint[j].p1, v), sub(joint[j].p2, v)) > 0;
            const bool construction = s.entity(pieces[i].source).construction && s.entity(pieces[k].source).construction;
            const SketchEntityId arc = ccw ? s.addArc(joint[j].vertex, p1, p2, true, construction)
                                           : s.addArc(joint[j].vertex, p2, p1, false, construction);
            r.created.push_back(arc);
            if (constrain) {
                joinTangent(made[i], arc);
                joinTangent(arc, made[k]);
                if (!pieces[i].analytic() || !pieces[k].analytic()) {
                    r.constraints.push_back(sk.constrain_radius_of(s.entity(arc).geo, std::abs(d)));
                }
            }
        } else if (constrain) {
            // Tangent joints of the source stay tangent.
            const Point2 t1 = tangentAt(pieces[i], true), t2 = tangentAt(pieces[k], false);
            if (std::abs(cross(t1, t2)) < 1e-9 && dot(t1, t2) > 0) joinTangent(made[i], made[k]);
        }
    }
    return r;
}

}  // namespace

Result<SketchEditResult> offsetChain(BoundSketch& s, const std::vector<SketchEntityId>& chain, const OffsetOptions& o) {
    using R = SketchEditResult;
    if (chain.empty()) return err<R>("offset: empty chain");
    if (!(std::abs(o.distance) > 0)) return err<R>("offset: distance must be non-zero");
    const double scale = sketchScale(s);
    const double tol = 1e-7 * scale;
    const std::size_t n = chain.size();
    for (SketchEntityId id : chain) {
        if (id == 0 || id > s.entities().size() || s.entity(id).removed) return err<R>("no live sketch entity " + std::to_string(id));
        const auto& e = s.entity(id);
        if (n > 1 && endsOf(s, e).start < 0) {
            return err<R>("offset: closed entity " + std::to_string(id) + " can only be offset alone");
        }
    }
    std::vector<OffsetPiece> pieces(n);
    std::vector<bool> rev(n, false);
    auto ends = [&](std::size_t i) {
        const EntityEnds e = endsOf(s, s.entity(chain[i]));
        return rev[i] ? std::pair{e.end, e.start} : std::pair{e.start, e.end};
    };
    auto near = [&](Id a, Id b) { return a == b || dist(P(s, a), P(s, b)) < tol; };
    if (n > 1) {
        const EntityEnds e0 = endsOf(s, s.entity(chain[0])), e1 = endsOf(s, s.entity(chain[1]));
        rev[0] = near(e0.start, e1.start) || near(e0.start, e1.end);
        for (std::size_t i = 1; i < n; ++i) {
            const Id prevEnd = ends(i - 1).second;
            const EntityEnds e = endsOf(s, s.entity(chain[i]));
            if (near(prevEnd, e.start)) rev[i] = false;
            else if (near(prevEnd, e.end)) rev[i] = true;
            else return err<R>("offset: entities " + std::to_string(chain[i - 1]) + " and " + std::to_string(chain[i]) + " are not connected");
        }
    }
    const bool closed = n == 1 ? endsOf(s, s.entity(chain[0])).start < 0 : near(ends(n - 1).second, ends(0).first);
    for (std::size_t i = 0; i < n; ++i) {
        OffsetPiece& p = pieces[i];
        const auto& e = s.entity(chain[i]);
        p.source = e.id;
        p.kind = e.kind;
        p.reversed = rev[i];
        if (e.kind != K::Circle && e.kind != K::Ellipse && !(e.kind == K::BSpline && s.sketch().bspline(e.geo.id).periodic)) {
            std::tie(p.from, p.to) = ends(i);
        }
        auto cv = s.curve(e.id, 1e-6 * scale);
        if (!cv) return cv.error();
        auto seg = detail::toGeom2d(cv.value().geometry, 1e-9 * scale);
        if (!seg) return err<R>("offset: entity " + std::to_string(e.id) + ": " + seg.error().message);
        occ::handle<Geom2d_Curve> c = new Geom2d_TrimmedCurve(seg.value().basis, seg.value().first, seg.value().last);
        if (seg.value().closed && (e.kind == K::BSpline && s.sketch().bspline(e.geo.id).periodic)) c = seg.value().basis;
        if (p.from >= 0) {
            // Orient along the traversal.
            const gp_Pnt2d q0 = c->Value(c->FirstParameter());
            if (dist({q0.X(), q0.Y()}, P(s, p.from)) > dist({q0.X(), q0.Y()}, P(s, p.to))) c = c->Reversed();
        } else if (e.kind == K::Circle || e.kind == K::Ellipse) {
            p.reversed = false;  // counter-clockwise parameterisation
        }
        p.curve = c;
        p.u0 = c->FirstParameter();
        p.u1 = c->LastParameter();
    }
    Id master = 0;
    auto first = offsetSide(s, pieces, closed, o.distance, o.corners, o.constrain, master);
    if (!first) return first.error();
    SketchEditResult r = first.value();
    if (o.bothSides) {
        auto second = offsetSide(s, pieces, closed, -o.distance, o.corners, o.constrain, master);
        if (!second) return second.error();
        const SketchEditResult& b = second.value();
        r.created.insert(r.created.end(), b.created.begin(), b.created.end());
        r.points.insert(r.points.end(), b.points.begin(), b.points.end());
        r.constraints.insert(r.constraints.end(), b.constraints.begin(), b.constraints.end());
    }
    if (o.constrain) pruneRedundant(s, r);
    return r;
}

Result<SketchEditResult> offsetChain(BoundSketch& s, const std::vector<SketchEntityId>& chain, double distance,
                                     bool constrain) {
    OffsetOptions o;
    o.distance = distance;
    o.constrain = constrain;
    return offsetChain(s, chain, o);
}

// ---------------------------------------------------------------------------------------------
// Mirror and patterns
// ---------------------------------------------------------------------------------------------

namespace {

// Copies entities with a point map: `mapPoint` gives (or creates) the image of a point.
template <typename MapPoint, typename OnCopy>
Result<SketchEditResult> copyEntities(BoundSketch& s, const std::vector<SketchEntityId>& ids, bool flip,
                                      MapPoint&& mapPoint, OnCopy&& onCopy, SketchEditResult& r) {
    using R = SketchEditResult;
    for (SketchEntityId id : ids) {
        if (id == 0 || id > s.entities().size() || s.entity(id).removed) return err<R>("no live sketch entity " + std::to_string(id));
        const BoundSketch::Entity e = s.entity(id);
        auto& sk = s.sketch();
        SketchEntityId made = 0;
        switch (e.kind) {
            case K::Line: made = s.addLine(mapPoint(e.start), mapPoint(e.end), e.construction); break;
            case K::Circle: made = s.addCircle(mapPoint(e.center), sk.circle(e.circle).radius, e.construction); break;
            case K::Arc:
                made = s.addArc(mapPoint(e.center), mapPoint(e.start), mapPoint(e.end), e.counterClockwise != flip, e.construction);
                break;
            case K::Ellipse:
                made = s.addEllipse(mapPoint(e.center), mapPoint(e.focus), sk.ellipse(e.geo.id).minor_radius, e.construction);
                break;
            case K::EllipticalArc:
                made = s.addEllipticalArc(mapPoint(e.center), mapPoint(e.focus), sk.ellipse_arc(e.geo.id).minor_radius,
                                          mapPoint(e.start), mapPoint(e.end), e.counterClockwise != flip, e.construction);
                break;
            case K::BSpline: {
                sketch2d::BSplineData d = sk.bspline(e.geo.id);
                for (Id& p : d.poles) p = mapPoint(p);
                made = s.addBSpline(d, e.construction);
                break;
            }
        }
        r.created.push_back(made);
        onCopy(e, s.entity(made));
    }
    return r;
}

void equalRadius(BoundSketch& s, SketchEditResult& r, const BoundSketch::Entity& from, const BoundSketch::Entity& to) {
    if (from.kind == K::Circle) r.constraints.push_back(s.sketch().constrain_equal(from.geo, to.geo));
}

}  // namespace

Result<SketchEditResult> mirrorEntities(BoundSketch& s, const std::vector<SketchEntityId>& ids, SketchEntityId axis,
                                        bool constrain) {
    using R = SketchEditResult;
    if (axis == 0 || axis > s.entities().size() || s.entity(axis).removed || s.entity(axis).kind != K::Line) {
        return err<R>("mirror: entity " + std::to_string(axis) + " is not a live line");
    }
    const auto& ax = s.entity(axis);
    const Point2 a = P(s, ax.start), b = P(s, ax.end);
    if (dist(a, b) < 1e-12) return err<R>("mirror: the axis has zero length");
    const Point2 d = unit(sub(b, a));
    const double tol = 1e-9 * sketchScale(s);
    std::vector<SketchEntityId> list;
    for (SketchEntityId id : ids) {
        if (id != axis) list.push_back(id);
    }
    SketchEditResult r;
    std::map<Id, Id> image;
    auto mapPoint = [&](Id p) {
        auto it = image.find(p);
        if (it != image.end()) return it->second;
        const Point2 q = P(s, p), rel = sub(q, a);
        const Point2 foot = add(a, mul(d, dot(rel, d)));
        Id m = p;
        if (dist(q, foot) > tol) {
            m = newPoint(s, r, sub(mul(foot, 2), q));
            if (constrain) r.constraints.push_back(s.sketch().constrain_symmetric(p, m, ax.line));
        }
        image[p] = m;
        return m;
    };
    auto onCopy = [&](const BoundSketch::Entity& from, const BoundSketch::Entity& to) {
        if (constrain) equalRadius(s, r, from, to);
    };
    return copyEntities(s, list, true, mapPoint, onCopy, r);
}

Result<SketchEditResult> linearPattern(BoundSketch& s, const std::vector<SketchEntityId>& ids, const Point2& step, int count,
                                       bool constrain) {
    using R = SketchEditResult;
    if (count < 2) return err<R>("linear pattern: count must be at least 2");
    if (std::hypot(step.x, step.y) < 1e-12) return err<R>("linear pattern: zero step");
    SketchEditResult r;
    for (int k = 1; k < count; ++k) {
        std::map<Id, Id> image;
        auto mapPoint = [&](Id p) {
            auto it = image.find(p);
            if (it != image.end()) return it->second;
            const Id m = newPoint(s, r, add(P(s, p), mul(step, k)));
            if (constrain) {
                r.constraints.push_back(s.sketch().constrain_distance_x(p, m, k * step.x));
                r.constraints.push_back(s.sketch().constrain_distance_y(p, m, k * step.y));
            }
            image[p] = m;
            return m;
        };
        auto onCopy = [&](const BoundSketch::Entity& from, const BoundSketch::Entity& to) {
            if (constrain) equalRadius(s, r, from, to);
        };
        auto res = copyEntities(s, ids, false, mapPoint, onCopy, r);
        if (!res) return res.error();
    }
    return r;
}

Result<SketchEditResult> circularPattern(BoundSketch& s, const std::vector<SketchEntityId>& ids, Id center, int count,
                                         double stepDegrees, bool constrain) {
    using R = SketchEditResult;
    if (count < 2) return err<R>("circular pattern: count must be at least 2");
    if (std::abs(std::remainder(stepDegrees, 360.0)) < 1e-9) return err<R>("circular pattern: the step angle is a full turn");
    const Point2 o = P(s, center);
    SketchEditResult r;
    for (int k = 1; k < count; ++k) {
        const double ang = k * stepDegrees * kPi / 180.0, co = std::cos(ang), si = std::sin(ang);
        std::map<Id, Id> image;
        image[center] = center;
        auto mapPoint = [&](Id p) {
            auto it = image.find(p);
            if (it != image.end()) return it->second;
            const Point2 q = sub(P(s, p), o);
            const Id m = newPoint(s, r, add(o, {co * q.x - si * q.y, si * q.x + co * q.y}));
            if (constrain) r.constraints.push_back(s.sketch().constrain_rotation(p, m, center, k * stepDegrees));
            image[p] = m;
            return m;
        };
        auto onCopy = [&](const BoundSketch::Entity& from, const BoundSketch::Entity& to) {
            if (constrain) equalRadius(s, r, from, to);
        };
        auto res = copyEntities(s, ids, false, mapPoint, onCopy, r);
        if (!res) return res.error();
    }
    return r;
}

// ---------------------------------------------------------------------------------------------
// Fillet and chamfer
// ---------------------------------------------------------------------------------------------

namespace {

struct Corner {
    Id va = -1, vb = -1;  // corner end points of a and b (equal when shared)
    Id fa = -1, fb = -1;  // far end points
    bool aAtEnd = false, bAtEnd = false;  // the corner is the end (not the start) of the line
    Point2 v, u, w;       // corner, unit directions towards the far ends
    double la = 0, lb = 0;
};

Result<Corner> cornerOf(const BoundSketch& s, SketchEntityId a, SketchEntityId b) {
    for (SketchEntityId id : {a, b}) {
        if (id == 0 || id > s.entities().size() || s.entity(id).removed || s.entity(id).kind != K::Line) {
            return err<Corner>("corner: entity " + std::to_string(id) + " is not a live line");
        }
    }
    const auto& ea = s.entity(a);
    const auto& eb = s.entity(b);
    const double tol = 1e-7 * sketchScale(s);
    Corner c;
    bool found = false;
    for (bool aEnd : {false, true}) {
        for (bool bEnd : {false, true}) {
            const Id pa = aEnd ? ea.end : ea.start, pb = bEnd ? eb.end : eb.start;
            if (!found && (pa == pb || dist(P(s, pa), P(s, pb)) < tol)) {
                found = true;
                c.va = pa;
                c.vb = pb;
                c.aAtEnd = aEnd;
                c.bAtEnd = bEnd;
                c.fa = aEnd ? ea.start : ea.end;
                c.fb = bEnd ? eb.start : eb.end;
            }
        }
    }
    if (!found) return err<Corner>("corner: lines " + std::to_string(a) + " and " + std::to_string(b) + " do not share an end point");
    c.v = P(s, c.va);
    c.la = dist(c.v, P(s, c.fa));
    c.lb = dist(c.v, P(s, c.fb));
    if (c.la < tol || c.lb < tol) return err<Corner>("corner: a line has zero length");
    c.u = unit(sub(P(s, c.fa), c.v));
    c.w = unit(sub(P(s, c.fb), c.v));
    if (std::abs(cross(c.u, c.w)) < 1e-9) return err<Corner>("corner: the lines are collinear");
    return c;
}

// Shortens both lines to t1, t2 and keeps the corner points as virtual sharps.
void cutCorner(BoundSketch& s, SketchEditResult& r, SketchEntityId a, SketchEntityId b, const Corner& c, Id t1, Id t2) {
    const auto& ea = s.entity(a);
    const auto& eb = s.entity(b);
    s.setEndPoints(a, c.aAtEnd ? ea.start : t1, c.aAtEnd ? t1 : ea.end);
    s.setEndPoints(b, c.bAtEnd ? eb.start : t2, c.bAtEnd ? t2 : eb.end);
    r.modified = {a, b};
    r.constraints.push_back(s.sketch().constrain_point_on(c.va, s.entity(a).geo));
    r.constraints.push_back(s.sketch().constrain_point_on(c.vb, s.entity(b).geo));
}

}  // namespace

Result<SketchEditResult> filletCorner(BoundSketch& s, SketchEntityId a, SketchEntityId b, double radius) {
    using R = SketchEditResult;
    if (!(radius > 0)) return err<R>("fillet: radius must be positive");
    auto cor = cornerOf(s, a, b);
    if (!cor) return cor.error();
    const Corner c = cor.value();
    const double theta = std::acos(std::clamp(dot(c.u, c.w), -1.0, 1.0));
    const double t = radius / std::tan(theta / 2);
    if (t >= c.la || t >= c.lb) return err<R>("fillet: radius " + std::to_string(radius) + " does not fit the corner");
    const Point2 p1 = add(c.v, mul(c.u, t)), p2 = add(c.v, mul(c.w, t));
    const Point2 center = add(c.v, mul(unit(add(c.u, c.w)), radius / std::sin(theta / 2)));
    SketchEditResult r;
    const Id t1 = newPoint(s, r, p1), t2 = newPoint(s, r, p2), pc = newPoint(s, r, center);
    const bool ccw = cross(sub(p1, center), sub(p2, center)) > 0;
    const bool construction = s.entity(a).construction && s.entity(b).construction;
    cutCorner(s, r, a, b, c, t1, t2);
    const SketchEntityId arc = s.addArc(pc, t1, t2, ccw, construction);
    r.created.push_back(arc);
    auto& sk = s.sketch();
    r.constraints.push_back(sk.constrain_tangent(s.entity(a).geo, s.entity(arc).geo));
    r.constraints.push_back(sk.constrain_tangent(s.entity(b).geo, s.entity(arc).geo));
    r.constraints.push_back(sk.constrain_radius_of(s.entity(arc).geo, radius));
    return r;
}

Result<SketchEditResult> chamferCorner(BoundSketch& s, SketchEntityId a, SketchEntityId b, double d1, double d2) {
    using R = SketchEditResult;
    if (!(d1 > 0) || !(d2 > 0)) return err<R>("chamfer: distances must be positive");
    auto cor = cornerOf(s, a, b);
    if (!cor) return cor.error();
    const Corner c = cor.value();
    if (d1 >= c.la || d2 >= c.lb) return err<R>("chamfer: the distances do not fit the corner");
    SketchEditResult r;
    const Id t1 = newPoint(s, r, add(c.v, mul(c.u, d1))), t2 = newPoint(s, r, add(c.v, mul(c.w, d2)));
    const bool construction = s.entity(a).construction && s.entity(b).construction;
    cutCorner(s, r, a, b, c, t1, t2);
    r.created.push_back(s.addLine(t1, t2, construction));
    r.constraints.push_back(s.sketch().constrain_distance(c.va, t1, d1));
    r.constraints.push_back(s.sketch().constrain_distance(c.vb, t2, d2));
    return r;
}

}  // namespace bendcore::modeling
