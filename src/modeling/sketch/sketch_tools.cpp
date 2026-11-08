// Sketch editing tools (docs/modeling/SKETCH.md, section "Инструменты редактирования").

#include <bendcore/modeling/sketch_tools.hpp>

#include "sketch_internal.hpp"

#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Geom2d_Circle.hxx>
#include <Geom2d_Line.hxx>
#include <Geom2d_TrimmedCurve.hxx>
#include <gp_Ax22d.hxx>
#include <gp_Ax2d.hxx>

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <optional>

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

// Line or circle that carries a line, arc or circle entity, parameterised as below.
struct Carrier {
    K kind = K::Line;
    Point2 a, b;          // line: start, end (t = 0, 1)
    Point2 c;             // circle/arc: centre
    double r = 0.0;
    double startAngle = 0.0;  // arc: angle of the start point
    double sweep = 2 * kPi;   // arc: sweep from start in the arc direction
    bool ccw = true;
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
        default: return err<Carrier>("entity " + std::to_string(id) + ": only lines, arcs and circles can be trimmed or extended");
    }
}

// Parameter of a point on the carrier: line — t (0 start, 1 end); circle — angle in [0, 2π);
// arc — angle from the start in the arc direction, [0, 2π).
double paramOf(const Carrier& c, Point2 p) {
    if (c.kind == K::Line) {
        const Point2 d = sub(c.b, c.a);
        return dot(sub(p, c.a), d) / dot(d, d);
    }
    const double ang = std::atan2(p.y - c.c.y, p.x - c.c.x);
    if (c.kind == K::Circle) return wrap2pi(ang);
    return wrap2pi(c.ccw ? ang - c.startAngle : c.startAngle - ang);
}

Point2 pointAt(const Carrier& c, double u) {
    if (c.kind == K::Line) return add(c.a, mul(sub(c.b, c.a), u));
    const double ang = c.kind == K::Circle ? u : (c.ccw ? c.startAngle + u : c.startAngle - u);
    return {c.c.x + c.r * std::cos(ang), c.c.y + c.r * std::sin(ang)};
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
            default: break;
        }
        if (added) {
            if (!info.driving) sk.set_driving(added, false);
            r.constraints.push_back(added);
        }
    }
}

}  // namespace

// ---------------------------------------------------------------------------------------------
// Trim / extend
// ---------------------------------------------------------------------------------------------

Result<SketchEditResult> trimEntity(BoundSketch& s, SketchEntityId id, const Point2& pick) {
    using R = SketchEditResult;
    if (id == 0 || id > s.entities().size() || s.entity(id).removed) return err<R>("no live sketch entity " + std::to_string(id));
    auto car = carrierOf(s, id);
    if (!car) return car.error();
    const Carrier c = car.value();
    auto hits = intersections(s, id, c);
    if (!hits) return hits.error();
    const double eps = 1e-9;
    const double tp = paramOf(c, pick);
    const BoundSketch::Entity e = s.entity(id);
    SketchEditResult r;

    if (c.kind == K::Circle) {
        std::vector<Hit> hs;
        for (const Hit& h : hits.value()) {
            if (std::none_of(hs.begin(), hs.end(), [&](const Hit& o) { return std::abs(o.u - h.u) < eps; })) hs.push_back(h);
        }
        if (hs.size() < 2) return err<R>("trim: circle " + std::to_string(id) + " needs at least two intersections");
        std::sort(hs.begin(), hs.end(), [](const Hit& a, const Hit& b) { return a.u < b.u; });
        // Neighbours of the pick angle (cyclic): the kept arc runs ccw from `next` to `prev`.
        std::size_t next = 0;
        while (next < hs.size() && hs[next].u <= tp) ++next;
        const Hit& hi = hs[next % hs.size()];
        const Hit& lo = hs[(next + hs.size() - 1) % hs.size()];
        const auto ci = s.sketch().circle(e.circle);
        const Id ps = newPoint(s, r, hi.at), pe = newPoint(s, r, lo.at);
        const SketchEntityId arc = s.addArc(ci.center, ps, pe, true, e.construction);
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
    } else {
        piece = s.addArc(e.center, q2, finish, e.counterClockwise, e.construction);
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
    if (e.kind != K::Line && e.kind != K::Arc) return err<R>("extend: entity " + std::to_string(id) + " is not a line or an arc");
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

struct OffsetPiece {
    SketchEntityId source = 0;
    bool isLine = true;
    bool reversed = false;  // traversed from the entity end to its start
    Point2 a, b;            // offset line: traversal start, end
    Point2 c;               // offset arc: centre
    double r = 0.0;
    Id center = -1;
};

// Intersections of two carriers (line through a,b / circle c,r).
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
    if (p.isLine && q.isLine) {
        const Point2 d1 = sub(p.b, p.a), d2 = sub(q.b, q.a);
        const double den = cross(d1, d2);
        if (std::abs(den) < 1e-14 * dot(d1, d1)) return out;
        const double t = cross(sub(q.a, p.a), d2) / den;
        out.push_back(add(p.a, mul(d1, t)));
    } else if (p.isLine) {
        lineCircle(p.a, p.b, q.c, q.r);
    } else if (q.isLine) {
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

}  // namespace

Result<SketchEditResult> offsetChain(BoundSketch& s, const std::vector<SketchEntityId>& chain, double distance,
                                     bool constrain) {
    using R = SketchEditResult;
    if (chain.empty()) return err<R>("offset: empty chain");
    if (!(std::abs(distance) > 0)) return err<R>("offset: distance must be non-zero");
    const double tol = 1e-7 * sketchScale(s);
    for (SketchEntityId id : chain) {
        if (id == 0 || id > s.entities().size() || s.entity(id).removed) return err<R>("no live sketch entity " + std::to_string(id));
        const K k = s.entity(id).kind;
        if (k != K::Line && k != K::Arc) return err<R>("offset: entity " + std::to_string(id) + " is not a line or an arc");
    }
    // Orientation of each entity along the chain.
    const std::size_t n = chain.size();
    std::vector<bool> rev(n, false);
    auto ends = [&](std::size_t i) {
        const auto& e = s.entity(chain[i]);
        Point2 a = P(s, e.start), b = P(s, e.end);
        if (rev[i]) std::swap(a, b);
        return std::pair{a, b};
    };
    if (n > 1) {
        const auto& e0 = s.entity(chain[0]);
        const auto& e1 = s.entity(chain[1]);
        const Point2 s0 = P(s, e0.start);
        const bool startTouches = dist(s0, P(s, e1.start)) < tol || dist(s0, P(s, e1.end)) < tol;
        rev[0] = startTouches;
        for (std::size_t i = 1; i < n; ++i) {
            const Point2 prevEnd = ends(i - 1).second;
            const auto& e = s.entity(chain[i]);
            if (dist(prevEnd, P(s, e.start)) < tol) rev[i] = false;
            else if (dist(prevEnd, P(s, e.end)) < tol) rev[i] = true;
            else return err<R>("offset: entities " + std::to_string(chain[i - 1]) + " and " + std::to_string(chain[i]) + " are not connected");
        }
    }
    const bool closed = n > 1 && dist(ends(n - 1).second, ends(0).first) < tol;

    std::vector<OffsetPiece> pieces(n);
    for (std::size_t i = 0; i < n; ++i) {
        const auto& e = s.entity(chain[i]);
        OffsetPiece& p = pieces[i];
        p.source = e.id;
        p.reversed = rev[i];
        const auto [a, b] = ends(i);
        if (e.kind == K::Line) {
            const Point2 d = unit(sub(b, a)), left{-d.y, d.x};
            p.a = add(a, mul(left, distance));
            p.b = add(b, mul(left, distance));
        } else {
            p.isLine = false;
            const auto arc = s.sketch().arc(e.arc);
            p.center = arc.center;
            p.c = P(s, arc.center);
            // Traversal counter-clockwise: the left side is towards the centre.
            const bool travCcw = arc.ccw != rev[i];
            p.r = dist(p.c, a) + (travCcw ? -distance : distance);
            if (!(p.r > tol)) return err<R>("offset: arc " + std::to_string(e.id) + " collapses (radius " + std::to_string(p.r) + ")");
            p.a = add(p.c, mul(unit(sub(a, p.c)), p.r));
            p.b = add(p.c, mul(unit(sub(b, p.c)), p.r));
        }
    }
    // Joint points: joint j is between piece j-1 and piece j (j = 0 is the chain start).
    std::vector<Point2> joint(n + 1);
    joint[0] = pieces[0].a;
    joint[n] = pieces[n - 1].b;
    auto corner = [&](std::size_t i, std::size_t j, Point2 original) -> std::optional<Point2> {
        if (dist(pieces[i].b, pieces[j].a) < tol) return pieces[i].b;
        std::optional<Point2> best;
        for (Point2 q : meet(pieces[i], pieces[j])) {
            if (!best || dist(q, original) < dist(*best, original)) best = q;
        }
        return best;
    };
    for (std::size_t j = 1; j < n; ++j) {
        auto q = corner(j - 1, j, ends(j).first);
        if (!q) return err<R>("offset: the offsets of entities " + std::to_string(chain[j - 1]) + " and " + std::to_string(chain[j]) + " do not meet");
        joint[j] = *q;
    }
    if (closed) {
        auto q = corner(n - 1, 0, ends(0).first);
        if (!q) return err<R>("offset: the offsets of entities " + std::to_string(chain[n - 1]) + " and " + std::to_string(chain[0]) + " do not meet");
        joint[0] = joint[n] = *q;
    }
    SketchEditResult r;
    std::vector<Id> jp(n + 1);
    for (std::size_t j = 0; j <= n; ++j) {
        if (closed && j == n) {
            jp[n] = jp[0];
            continue;
        }
        jp[j] = newPoint(s, r, joint[j]);
    }
    auto& sk = s.sketch();
    for (std::size_t i = 0; i < n; ++i) {
        const OffsetPiece& p = pieces[i];
        const BoundSketch::Entity src = s.entity(p.source);  // copy: adding entities reallocates
        // Back to the entity's own orientation.
        const Id a = p.reversed ? jp[i + 1] : jp[i];
        const Id b = p.reversed ? jp[i] : jp[i + 1];
        SketchEntityId made = 0;
        if (p.isLine) {
            made = s.addLine(a, b, src.construction);
            if (constrain) {
                r.constraints.push_back(sk.constrain_parallel(s.entity(made).line, src.line));
                r.constraints.push_back(sk.constrain_point_curve_distance(a, src.geo, std::abs(distance)));
            }
        } else {
            made = s.addArc(p.center, a, b, src.counterClockwise, src.construction);
            if (constrain) r.constraints.push_back(sk.constrain_point_curve_distance(a, src.geo, std::abs(distance)));
        }
        r.created.push_back(made);
    }
    return r;
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
