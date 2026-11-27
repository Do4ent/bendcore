// Sketch editing tools and profiles of the new sketch geometry: trim, extend, offset, mirror,
// patterns, fillet and chamfer (coordinates against analytic values); profile with tangent arcs
// (area against the formula) and with a B-spline (area against a numerical integral).
// docs/modeling/SKETCH.md.

#include <bendcore/modeling/sketch_tools.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <functional>
#include <numbers>

using namespace bendcore::modeling;
using bendcore::sketch2d::Id;

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kTol = 1e-8;

SketchPlacement xy() { return SketchPlacement::fromPlane(gp_Pln()); }

void expectPoint(const BoundSketch& b, Id p, double x, double y, double tol = kTol) {
    EXPECT_NEAR(b.sketch().point(p).x, x, tol) << "point " << p;
    EXPECT_NEAR(b.sketch().point(p).y, y, tol) << "point " << p;
}

Id fixedPoint(BoundSketch& b, double x, double y) {
    const Id p = b.sketch().add_point(x, y);
    b.sketch().fix_point(p);
    return p;
}

double profileArea(const BoundSketch& b) {
    auto curves = b.curves();
    EXPECT_TRUE(curves.ok()) << (curves.ok() ? "" : curves.error().message);
    if (!curves.ok()) return -1;
    auto profile = SketchProfile::build(xy(), curves.value());
    EXPECT_TRUE(profile.ok()) << (profile.ok() ? "" : profile.error().message);
    if (!profile.ok()) return -1;
    double area = 0;
    for (std::size_t r : profile.value().materialRegions()) area += profile.value().regions()[r].area;
    return area;
}

void expectSatisfied(const BoundSketch& b) {
    const auto& s = b.sketch();
    for (Id c = 1; c <= static_cast<Id>(s.constraint_count()); ++c) {
        EXPECT_LT(s.residual(c), 1e-7) << "constraint " << c << " (" << bendcore::sketch2d::to_string(s.constraint(c).kind) << ")";
    }
}

// Rectangle 40 x 20 with a fixed corner at the origin; entities bottom, right, top, left.
struct Rect {
    BoundSketch b;
    Id p0, p1, p2, p3;
    SketchEntityId bottom, right, top, left;
    Id width;
    Rect() {
        auto& s = b.sketch();
        p0 = fixedPoint(b, 0, 0);
        p1 = s.add_point(41, 1);
        p2 = s.add_point(39, 21);
        p3 = s.add_point(1, 19);
        bottom = b.addLine(p0, p1);
        right = b.addLine(p1, p2);
        top = b.addLine(p2, p3);
        left = b.addLine(p3, p0);
        s.constrain_horizontal(p0, p1);
        s.constrain_vertical(p1, p2);
        s.constrain_horizontal(p2, p3);
        s.constrain_vertical(p3, p0);
        width = s.constrain_distance(p0, p1, 40);
        s.constrain_distance(p1, p2, 20);
    }
};

}  // namespace

TEST(SketchProfileGeometry, SlotWithTangentArcs) {
    // Obround: two lines tangent to two arcs of radius R, centres L apart. Area 2RL + πR².
    BoundSketch b;
    auto& s = b.sketch();
    const double R = 8, L = 30;
    const Id c1 = fixedPoint(b, 0, 0);
    const Id c2 = s.add_point(31, 0.5);
    const Id pa = s.add_point(0.4, -7.5), pb = s.add_point(29, -8.4), pc = s.add_point(31.5, 7.7), pd = s.add_point(-0.6, 8.2);
    const auto bottom = b.addLine(pa, pb);
    const auto right = b.addArc(c2, pb, pc, true);
    const auto top = b.addLine(pc, pd);
    const auto left = b.addArc(c1, pd, pa, true);
    s.constrain_horizontal(c1, c2);
    s.constrain_distance(c1, c2, L);
    s.constrain_radius_of(b.entity(left).geo, R);
    s.constrain_equal(b.entity(left).geo, b.entity(right).geo);
    for (auto [l, a] : {std::pair{bottom, right}, {bottom, left}, {top, right}, {top, left}}) {
        s.constrain_tangent(b.entity(l).geo, b.entity(a).geo);
    }
    const auto r = b.solve();
    EXPECT_EQ(r.dof, 0) << r.status;
    EXPECT_EQ(r.status, "определено");
    expectPoint(b, pa, 0, -R);
    expectPoint(b, pb, L, -R);
    expectPoint(b, pc, L, R);
    expectPoint(b, pd, 0, R);
    EXPECT_NEAR(profileArea(b), 2 * R * L + kPi * R * R, 1e-6);

    // The arcs are native: changing the radius dimension keeps tangency and the formula.
    s.set_constraint_value(3, 5);
    ASSERT_EQ(b.solve().status, "определено");
    EXPECT_NEAR(profileArea(b), 2 * 5 * L + kPi * 25, 1e-6);
}

TEST(SketchProfileGeometry, BSplineInProfileAgainstNumericalIntegral) {
    // Clamped cubic B-spline (Bézier) from (0,0) to (40,0) closed by a line.
    BoundSketch b;
    auto& s = b.sketch();
    const double poles[4][2] = {{0, 0}, {10, 30}, {30, 30}, {40, 0}};
    bendcore::sketch2d::BSplineData d;
    for (const auto& p : poles) d.poles.push_back(fixedPoint(b, p[0], p[1]));
    d.degree = 3;
    d.knots = {0, 1};
    d.multiplicities = {4, 4};
    b.addBSpline(d);
    b.addLine(d.poles.back(), d.poles.front());
    // Area under the curve by Simpson's rule on ∫ y dx = ∫ y(t) x'(t) dt over the Bernstein form.
    auto bern = [&](double t, int axis) {
        const double u = 1 - t;
        return u * u * u * poles[0][axis] + 3 * u * u * t * poles[1][axis] + 3 * u * t * t * poles[2][axis] +
               t * t * t * poles[3][axis];
    };
    auto dx = [&](double t) {
        const double u = 1 - t;
        return 3 * (u * u * (poles[1][0] - poles[0][0]) + 2 * u * t * (poles[2][0] - poles[1][0]) + t * t * (poles[3][0] - poles[2][0]));
    };
    const int n = 2000;
    double integral = 0;
    for (int i = 0; i <= n; ++i) {
        const double t = static_cast<double>(i) / n;
        const double w = (i == 0 || i == n) ? 1 : (i % 2 ? 4 : 2);
        integral += w * bern(t, 1) * dx(t);
    }
    integral /= 3.0 * n;
    // Exact value of the polynomial integral (rational arithmetic on the power-basis form): 630.
    EXPECT_NEAR(integral, 630, 1e-6);
    EXPECT_NEAR(profileArea(b), integral, 1e-6);
}

TEST(SketchProfileGeometry, ConstructionGeometryIsNotInTheProfile) {
    Rect r;
    auto& s = r.b.sketch();
    const Id c = s.add_point(20, 10);
    const auto circle = r.b.addCircle(c, 30, true);  // crosses every side, but construction
    ASSERT_EQ(r.b.solve().dof, 3);
    EXPECT_NEAR(profileArea(r.b), 800, 1e-9);
    EXPECT_TRUE(s.is_construction(r.b.entity(circle).geo));
}

TEST(SketchTools, TrimLineAtCrossingAndSplit) {
    BoundSketch b;
    auto& s = b.sketch();
    const auto l1 = b.addLine(fixedPoint(b, 0, 0), fixedPoint(b, 20, 0));
    const auto l2 = b.addLine(fixedPoint(b, 10, -5), fixedPoint(b, 10, 5));
    auto t = trimEntity(b, l1, {15, 0});
    ASSERT_TRUE(t.ok()) << t.error().message;
    EXPECT_EQ(t.value().modified, (std::vector<SketchEntityId>{l1}));
    const auto& e1 = b.entity(l1);
    expectPoint(b, e1.start, 0, 0);
    expectPoint(b, e1.end, 10, 0);
    ASSERT_EQ(t.value().constraints.size(), 1u);
    EXPECT_EQ(s.constraint(t.value().constraints[0]).kind, bendcore::sketch2d::ConstraintKind::PointOn);

    // Split: a horizontal line crossed by two verticals loses its middle piece.
    const auto l3 = b.addLine(fixedPoint(b, 0, 10), fixedPoint(b, 30, 10));
    b.addLine(fixedPoint(b, 12, 5), fixedPoint(b, 12, 15));
    b.addLine(fixedPoint(b, 20, 5), fixedPoint(b, 20, 15));
    auto split = trimEntity(b, l3, {15, 10});
    ASSERT_TRUE(split.ok()) << split.error().message;
    ASSERT_EQ(split.value().created.size(), 1u);
    const auto& piece = b.entity(split.value().created[0]);
    expectPoint(b, b.entity(l3).end, 12, 10);
    expectPoint(b, piece.start, 20, 10);
    expectPoint(b, piece.end, 30, 10);
    b.solve();
    expectSatisfied(b);

    // A piece without intersections removes the entity.
    const auto lone = b.addLine(fixedPoint(b, 100, 100), fixedPoint(b, 110, 100));
    auto gone = trimEntity(b, lone, {105, 100});
    ASSERT_TRUE(gone.ok());
    EXPECT_TRUE(b.entity(lone).removed);
    (void)l2;
}

TEST(SketchTools, TrimCircleKeepsTheOtherArc) {
    BoundSketch b;
    const Id c = fixedPoint(b, 0, 0);
    const auto circle = b.addCircle(c, 10);
    b.sketch().constrain_radius(b.entity(circle).circle, 10);
    b.addLine(fixedPoint(b, -20, 0), fixedPoint(b, 20, 0));
    auto t = trimEntity(b, circle, {0, 10});
    ASSERT_TRUE(t.ok()) << t.error().message;
    ASSERT_EQ(t.value().created.size(), 1u);
    const auto& arc = b.entity(t.value().created[0]);
    EXPECT_EQ(arc.kind, BoundSketch::EntityKind::Arc);
    EXPECT_TRUE(b.entity(circle).removed);
    expectPoint(b, arc.start, -10, 0);
    expectPoint(b, arc.end, 10, 0);
    EXPECT_TRUE(arc.counterClockwise);  // through (0, -10)
    const auto r = b.solve();
    EXPECT_EQ(r.dof, 0) << r.status;    // radius dimension moved to the arc
    EXPECT_NEAR(b.sketch().arc(arc.arc).radius, 10, kTol);
}

TEST(SketchTools, ExtendLineToCircle) {
    BoundSketch b;
    const auto line = b.addLine(fixedPoint(b, 0, 0), b.sketch().add_point(5, 0));
    b.sketch().constrain_horizontal(b.entity(line).start, b.entity(line).end);
    const auto circle = b.addCircle(fixedPoint(b, 20, 0), 5);
    b.sketch().constrain_radius(b.entity(circle).circle, 5);
    auto e = extendEntity(b, line, {4, 0});
    ASSERT_TRUE(e.ok()) << e.error().message;
    expectPoint(b, b.entity(line).end, 15, 0);
    const auto r = b.solve();
    EXPECT_EQ(r.dof, 0) << r.status;
    expectPoint(b, b.entity(line).end, 15, 0);
    EXPECT_FALSE(extendEntity(b, line, {0, 0}).ok()) << "nothing beyond the start";
}

TEST(SketchTools, OffsetClosedRectangleAndSlot) {
    Rect r;
    ASSERT_EQ(r.b.solve().status, "определено");
    // The rectangle runs counter-clockwise: offset to the right (negative) is outward.
    auto o = offsetChain(r.b, {r.bottom, r.right, r.top, r.left}, -5);
    ASSERT_TRUE(o.ok()) << o.error().message;
    ASSERT_EQ(o.value().created.size(), 4u);
    const auto& nb = r.b.entity(o.value().created[0]);
    const auto& nt = r.b.entity(o.value().created[2]);
    expectPoint(r.b, nb.start, -5, -5);
    expectPoint(r.b, nb.end, 45, -5);
    expectPoint(r.b, nt.start, 45, 25);
    expectPoint(r.b, nt.end, -5, 25);
    auto rep = r.b.solve();
    EXPECT_EQ(rep.dof, 0) << rep.status;
    expectSatisfied(r.b);
    // Width 40 -> 60: the offset follows.
    r.b.sketch().set_constraint_value(r.width, 60);
    ASSERT_TRUE(r.b.solve().converged);
    expectPoint(r.b, nb.end, 65, -5, 1e-7);

    // Slot (lines tangent to arcs): offset outward by 2 keeps the tangent joints.
    BoundSketch b;
    auto& s = b.sketch();
    const Id c1 = fixedPoint(b, 0, 0), c2 = fixedPoint(b, 30, 0);
    const Id pa = fixedPoint(b, 0, -8), pb = fixedPoint(b, 30, -8), pc = fixedPoint(b, 30, 8), pd = fixedPoint(b, 0, 8);
    const auto l1 = b.addLine(pa, pb);
    const auto a1 = b.addArc(c2, pb, pc, true);
    const auto l2 = b.addLine(pc, pd);
    const auto a2 = b.addArc(c1, pd, pa, true);
    auto so = offsetChain(b, {l1, a1, l2, a2}, -2);
    ASSERT_TRUE(so.ok()) << so.error().message;
    EXPECT_NEAR(profileArea(b), 2 * 10 * 30 + kPi * 100 - (2 * 8 * 30 + kPi * 64), 1e-6);
    const auto& oa = b.entity(so.value().created[1]);
    EXPECT_NEAR(s.arc(oa.arc).radius, 10, kTol);
    EXPECT_EQ(oa.center, c2);
}

TEST(SketchTools, FilletCornerFollowsDimensions) {
    Rect r;
    ASSERT_EQ(r.b.solve().status, "определено");
    auto f = filletCorner(r.b, r.right, r.top, 5);
    ASSERT_TRUE(f.ok()) << f.error().message;
    ASSERT_EQ(f.value().created.size(), 1u);
    const auto& arc = r.b.entity(f.value().created[0]);
    expectPoint(r.b, arc.center, 35, 15);
    expectPoint(r.b, r.b.entity(r.right).end, 40, 15);
    expectPoint(r.b, r.b.entity(r.top).start, 35, 20);
    const auto rep = r.b.solve();
    EXPECT_EQ(rep.status, "определено") << rep.dof;
    EXPECT_NEAR(profileArea(r.b), 800 - 25 + 25 * kPi / 4, 1e-6);
    // The corner stays as a virtual sharp: the width dimension still drives the sketch.
    r.b.sketch().set_constraint_value(r.width, 60);
    ASSERT_EQ(r.b.solve().status, "определено");
    expectPoint(r.b, arc.center, 55, 15, 1e-7);
    EXPECT_NEAR(profileArea(r.b), 1200 - 25 + 25 * kPi / 4, 1e-6);
    EXPECT_FALSE(filletCorner(r.b, r.bottom, r.left, 30).ok()) << "radius does not fit";
}

TEST(SketchTools, ChamferCorner) {
    Rect r;
    ASSERT_EQ(r.b.solve().status, "определено");
    auto c = chamferCorner(r.b, r.right, r.top, 4, 6);
    ASSERT_TRUE(c.ok()) << c.error().message;
    const auto& line = r.b.entity(c.value().created[0]);
    expectPoint(r.b, line.start, 40, 16);
    expectPoint(r.b, line.end, 34, 20);
    EXPECT_EQ(r.b.solve().status, "определено");
    EXPECT_NEAR(profileArea(r.b), 800 - 12, 1e-9);
}

TEST(SketchTools, MirrorReusesAxisPointsAndFollows) {
    BoundSketch b;
    auto& s = b.sketch();
    const Id a0 = fixedPoint(b, 0, -10), a1 = fixedPoint(b, 0, 10);
    const auto axis = b.addLine(a0, a1, true);
    const Id p = s.add_point(8, 3);
    const auto l1 = b.addLine(a0, p);
    const auto l2 = b.addLine(p, a1);
    const auto circle = b.addCircle(s.add_point(4, 0), 1);
    auto m = mirrorEntities(b, {l1, l2, circle, axis}, axis);
    ASSERT_TRUE(m.ok()) << m.error().message;
    ASSERT_EQ(m.value().created.size(), 3u);
    const auto& m1 = b.entity(m.value().created[0]);
    EXPECT_EQ(m1.start, a0);  // on the axis: reused
    expectPoint(b, m1.end, -8, 3);
    // Moving the original by constraints moves the image.
    s.constrain_distance_x(a0, p, 12);
    s.constrain_distance_y(a0, p, 15);
    s.constrain_fix(b.entity(circle).center);
    s.constrain_radius(b.entity(circle).circle, 2);
    const auto r = b.solve();
    EXPECT_EQ(r.dof, 0) << r.status;
    expectPoint(b, m1.end, -12, 5);
    EXPECT_NEAR(s.circle_radius(b.entity(m.value().created[2]).circle), 2, kTol);
    expectPoint(b, b.entity(m.value().created[2]).center, -4, 0);
}

TEST(SketchTools, LinearAndCircularPatterns) {
    BoundSketch b;
    auto& s = b.sketch();
    const auto circle = b.addCircle(fixedPoint(b, 0, 0), 2);
    s.constrain_radius(b.entity(circle).circle, 2);
    auto lp = linearPattern(b, {circle}, {10, 0}, 3);
    ASSERT_TRUE(lp.ok()) << lp.error().message;
    ASSERT_EQ(lp.value().created.size(), 2u);
    expectPoint(b, b.entity(lp.value().created[1]).center, 20, 0);
    EXPECT_EQ(b.solve().dof, 0);

    const Id o = fixedPoint(b, 100, 0);
    const auto spoke = b.addLine(fixedPoint(b, 110, 0), fixedPoint(b, 115, 0));
    auto cp = circularPattern(b, {spoke}, o, 4, 90);
    ASSERT_TRUE(cp.ok()) << cp.error().message;
    ASSERT_EQ(cp.value().created.size(), 3u);
    const auto& second = b.entity(cp.value().created[0]);
    const auto& fourth = b.entity(cp.value().created[2]);
    expectPoint(b, second.start, 100, 10);
    expectPoint(b, second.end, 100, 15);
    expectPoint(b, fourth.start, 100, -10);
    const auto r = b.solve();
    EXPECT_EQ(r.dof, 0) << r.status;
    expectSatisfied(b);
    EXPECT_FALSE(circularPattern(b, {spoke}, o, 2, 360).ok());
}

// ---- Trim / extend of ellipses, elliptical arcs and splines ----------------------------------

namespace {

// Point of a solved sketch curve at a parameter fraction, through the profile curve kinds.
double polylineLength(const std::function<Point2(double)>& f, int n = 20000) {
    double len = 0;
    Point2 prev = f(0);
    for (int i = 1; i <= n; ++i) {
        const Point2 q = f(static_cast<double>(i) / n);
        len += std::hypot(q.x - prev.x, q.y - prev.y);
        prev = q;
    }
    return len;
}

// Cubic clamped Bezier through sketch points (degree 3, knots 0, 1).
Point2 bezier(const std::array<Point2, 4>& p, double t) {
    const double u = 1 - t;
    const double b0 = u * u * u, b1 = 3 * u * u * t, b2 = 3 * u * t * t, b3 = t * t * t;
    return {b0 * p[0].x + b1 * p[1].x + b2 * p[2].x + b3 * p[3].x, b0 * p[0].y + b1 * p[1].y + b2 * p[2].y + b3 * p[3].y};
}

// De Boor evaluation of a clamped non-rational B-spline of the sketch at parameter u.
Point2 splinePoint(const BoundSketch& b, SketchEntityId id, double u) {
    const auto& d = b.sketch().bspline(b.entity(id).geo.id);
    std::vector<double> flat;
    for (std::size_t k = 0; k < d.knots.size(); ++k) {
        for (int m = 0; m < d.multiplicities[k]; ++m) flat.push_back(d.knots[k]);
    }
    const int p = d.degree;
    const int n = static_cast<int>(d.poles.size());
    int span = p;
    while (span < n - 1 && u >= flat[static_cast<std::size_t>(span + 1)]) ++span;
    std::vector<Point2> dd;
    for (int j = 0; j <= p; ++j) {
        const auto q = b.sketch().point(d.poles[static_cast<std::size_t>(span - p + j)]);
        dd.push_back({q.x, q.y});
    }
    for (int r = 1; r <= p; ++r) {
        for (int j = p; j >= r; --j) {
            const double lo = flat[static_cast<std::size_t>(j + span - p)];
            const double hi = flat[static_cast<std::size_t>(j + 1 + span - r)];
            const double a = hi > lo ? (u - lo) / (hi - lo) : 0;
            dd[static_cast<std::size_t>(j)] = {(1 - a) * dd[static_cast<std::size_t>(j - 1)].x + a * dd[static_cast<std::size_t>(j)].x,
                                               (1 - a) * dd[static_cast<std::size_t>(j - 1)].y + a * dd[static_cast<std::size_t>(j)].y};
        }
    }
    return dd[static_cast<std::size_t>(p)];
}

std::pair<double, double> splineRange(const BoundSketch& b, SketchEntityId id) {
    const auto& d = b.sketch().bspline(b.entity(id).geo.id);
    return {d.knots.front(), d.knots.back()};
}

double splineLength(const BoundSketch& b, SketchEntityId id) {
    const auto [u0, u1] = splineRange(b, id);
    return polylineLength([&](double t) { return splinePoint(b, id, u0 + (u1 - u0) * t); });
}

// Distance from q to the Bezier curve (dense sampling plus local refinement).
double distanceToBezier(const std::array<Point2, 4>& p, Point2 q) {
    double best = 1e300, bt = 0;
    for (int i = 0; i <= 4000; ++i) {
        const double t = i / 4000.0;
        const Point2 x = bezier(p, t);
        const double d = std::hypot(x.x - q.x, x.y - q.y);
        if (d < best) best = d, bt = t;
    }
    for (double h = 1.0 / 4000; h > 1e-14; h *= 0.5) {
        for (double t : {bt - h, bt + h}) {
            if (t < 0 || t > 1) continue;
            const Point2 x = bezier(p, t);
            const double d = std::hypot(x.x - q.x, x.y - q.y);
            if (d < best) best = d, bt = t;
        }
    }
    return best;
}

SketchEntityId addBezier(BoundSketch& b, const std::array<Point2, 4>& p, bool fixed = true) {
    bendcore::sketch2d::BSplineData d;
    for (const Point2& q : p) d.poles.push_back(fixed ? fixedPoint(b, q.x, q.y) : b.sketch().add_point(q.x, q.y));
    d.degree = 3;
    d.knots = {0, 1};
    d.multiplicities = {4, 4};
    return b.addBSpline(d);
}

}  // namespace

TEST(SketchTools, TrimEllipseToArcAndSplitEllipticalArc) {
    BoundSketch b;
    auto& s = b.sketch();
    // Ellipse a = 10 (along X), b = 5: focus at (sqrt(75), 0).
    const Id c = fixedPoint(b, 0, 0), f = fixedPoint(b, std::sqrt(75.0), 0);
    const auto el = b.addEllipse(c, f, 5);
    s.constrain_minor_radius(b.entity(el).geo, 5);
    b.addLine(fixedPoint(b, -20, 0), fixedPoint(b, 20, 0));
    auto t = trimEntity(b, el, {0, 5});
    ASSERT_TRUE(t.ok()) << t.error().message;
    ASSERT_EQ(t.value().created.size(), 1u);
    EXPECT_TRUE(b.entity(el).removed);
    const auto& arc = b.entity(t.value().created[0]);
    EXPECT_EQ(arc.kind, BoundSketch::EntityKind::EllipticalArc);
    EXPECT_EQ(arc.center, c);
    EXPECT_EQ(arc.focus, f);
    expectPoint(b, arc.start, -10, 0);  // the lower half is kept: (-10, 0) -> (0, -5) -> (10, 0)
    expectPoint(b, arc.end, 10, 0);
    auto curve = b.curve(arc.id);
    ASSERT_TRUE(curve.ok());
    const auto& ea = std::get<EllipticalArc2d>(curve.value().geometry);
    EXPECT_NEAR(ea.endParameter - ea.startParameter, kPi, 1e-9);
    EXPECT_NEAR(std::fmod(ea.startParameter + 4 * kPi, 2 * kPi), kPi, 1e-9);
    const auto rep = b.solve();
    EXPECT_EQ(rep.dof, 0) << rep.status;  // minor radius moved to the arc, ends on the line
    EXPECT_TRUE(rep.redundant.empty());
    expectSatisfied(b);

    // Split the half ellipse by two verticals x = -6 and x = 6: the middle piece goes.
    b.addLine(fixedPoint(b, -6, -10), fixedPoint(b, -6, 10));
    b.addLine(fixedPoint(b, 6, -10), fixedPoint(b, 6, 10));
    auto split = trimEntity(b, arc.id, {0, -5});
    ASSERT_TRUE(split.ok()) << split.error().message;
    ASSERT_EQ(split.value().created.size(), 1u);
    const auto& left = b.entity(arc.id);
    const auto& right = b.entity(split.value().created[0]);
    expectPoint(b, left.start, -10, 0);
    expectPoint(b, left.end, -6, -4);    // x = -6 on the ellipse: y = -5 * sqrt(1 - 0.36)
    expectPoint(b, right.start, 6, -4);
    expectPoint(b, right.end, 10, 0);
    EXPECT_EQ(right.center, c);
    EXPECT_EQ(right.focus, f);
    const auto rep2 = b.solve();
    EXPECT_TRUE(rep2.redundant.empty()) << rep2.status;  // equal minor radii only (shared centre and focus)
    EXPECT_EQ(rep2.dof, 0) << rep2.status;
    expectSatisfied(b);
    // The minor radius dimension (moved from the ellipse to the arc) drives both pieces.
    Id minor = 0;
    for (Id k = 1; k <= static_cast<Id>(s.constraint_count()); ++k) {
        if (!s.constraint(k).removed && s.constraint(k).kind == bendcore::sketch2d::ConstraintKind::MinorRadius) minor = k;
    }
    ASSERT_NE(minor, 0);
    s.set_constraint_value(minor, 4);
    ASSERT_TRUE(b.solve().converged);
    EXPECT_NEAR(s.ellipse_arc(right.geo.id).minor_radius, 4, 1e-9);
}

TEST(SketchTools, ExtendEllipticalArcToLine) {
    BoundSketch b;
    const Id c = fixedPoint(b, 0, 0), f = fixedPoint(b, std::sqrt(75.0), 0);
    const Id p0 = fixedPoint(b, 10, 0), p1 = b.sketch().add_point(0, 5);
    const auto arc = b.addEllipticalArc(c, f, 5, p0, p1, true);
    b.sketch().constrain_minor_radius(b.entity(arc).geo, 5);
    b.addLine(fixedPoint(b, -6, -10), fixedPoint(b, -6, 10));
    auto e = extendEntity(b, arc, {0, 5});
    ASSERT_TRUE(e.ok()) << e.error().message;
    expectPoint(b, b.entity(arc).end, -6, 4);  // cos t = -0.6, sin t = 0.8
    const auto rep = b.solve();
    EXPECT_EQ(rep.dof, 0) << rep.status;
    expectPoint(b, b.entity(arc).end, -6, 4);
    // Backwards from the start the ellipse meets the line again at (-6, -4).
    auto back = extendEntity(b, arc, {10, 0});
    ASSERT_TRUE(back.ok()) << back.error().message;
    expectPoint(b, b.entity(arc).start, -6, -4);
}

TEST(SketchTools, TrimSplineAtIntersectionsKeepsTheCurve) {
    BoundSketch b;
    // Point-symmetric about (15, 0): crosses y = 0 at x = 15 for t = 1/2.
    const std::array<Point2, 4> poles{Point2{0, 0}, Point2{10, 20}, Point2{20, -20}, Point2{30, 0}};
    const auto sp = addBezier(b, poles);
    const Id first = b.sketch().bspline(b.entity(sp).geo.id).poles.front();
    const double fullLength = splineLength(b, sp);
    b.addLine(fixedPoint(b, 15, -10), fixedPoint(b, 15, 10));
    auto t = trimEntity(b, sp, bezier(poles, 0.8));
    ASSERT_TRUE(t.ok()) << t.error().message;
    EXPECT_EQ(t.value().modified, (std::vector<SketchEntityId>{sp}));
    const auto& d = b.sketch().bspline(b.entity(sp).geo.id);
    EXPECT_EQ(d.poles.front(), first);  // the kept end keeps its point
    expectPoint(b, d.poles.back(), 15, 0, 1e-9);
    EXPECT_NEAR(splineLength(b, sp), fullLength / 2, 1e-6);
    // The kept piece is the same curve: every point lies on the original.
    const auto [u0, u1] = splineRange(b, sp);
    for (int i = 0; i <= 10; ++i) EXPECT_LT(distanceToBezier(poles, splinePoint(b, sp, u0 + (u1 - u0) * i / 10.0)), 1e-9);
    // Inner poles keep their offsets from the kept end; the cut end slides along the cutter.
    EXPECT_EQ(b.solve().dof, 1);
    expectSatisfied(b);

    // Split: two verticals at x = 5 and x = 25 remove the middle piece of a second spline.
    BoundSketch b2;
    const auto sp2 = addBezier(b2, poles);
    b2.addLine(fixedPoint(b2, 5, -30), fixedPoint(b2, 5, 30));
    b2.addLine(fixedPoint(b2, 25, -30), fixedPoint(b2, 25, 30));
    auto split = trimEntity(b2, sp2, {15, 0});
    ASSERT_TRUE(split.ok()) << split.error().message;
    ASSERT_EQ(split.value().created.size(), 1u);
    const auto& d1 = b2.sketch().bspline(b2.entity(sp2).geo.id);
    const auto& d2 = b2.sketch().bspline(b2.entity(split.value().created[0]).geo.id);
    EXPECT_NEAR(b2.sketch().point(d1.poles.back()).x, 5, 1e-9);
    EXPECT_NEAR(b2.sketch().point(d2.poles.front()).x, 25, 1e-9);
    EXPECT_LT(distanceToBezier(poles, {b2.sketch().point(d1.poles.back()).x, b2.sketch().point(d1.poles.back()).y}), 1e-9);
    // Symmetric pieces: equal lengths.
    EXPECT_NEAR(splineLength(b2, sp2), splineLength(b2, split.value().created[0]), 1e-6);
    EXPECT_EQ(b2.solve().dof, 2);
    expectSatisfied(b2);
}

TEST(SketchTools, ExtendSplineToLine) {
    BoundSketch b;
    const std::array<Point2, 4> poles{Point2{0, 0}, Point2{5, 5}, Point2{10, 5}, Point2{15, 0}};
    const auto sp = addBezier(b, poles, false);
    const Id first = b.sketch().bspline(b.entity(sp).geo.id).poles.front();
    b.sketch().fix_point(first);
    b.addLine(fixedPoint(b, 25, -40), fixedPoint(b, 25, 40));
    auto e = extendEntity(b, sp, {15, 0});
    ASSERT_TRUE(e.ok()) << e.error().message;
    const auto& d = b.sketch().bspline(b.entity(sp).geo.id);
    EXPECT_EQ(d.poles.front(), first);
    EXPECT_NEAR(b.sketch().point(d.poles.back()).x, 25, 1e-9);  // ends on the line (point-on)
    // The original piece of the curve is kept: its points lie on the extended spline.
    const auto [u0, u1] = splineRange(b, sp);
    for (int i = 0; i <= 8; ++i) {
        const Point2 q = bezier(poles, i / 8.0);
        double best = 1e300;
        for (int k = 0; k <= 20000; ++k) {
            const Point2 x = splinePoint(b, sp, u0 + (u1 - u0) * k / 20000.0);
            best = std::min(best, std::hypot(x.x - q.x, x.y - q.y));
        }
        EXPECT_LT(best, 2e-3) << "t = " << i / 8.0;
    }
    expectSatisfied(b);
}

// ---- Offset ------------------------------------------------------------------------------------

namespace {

double entityLength(const BoundSketch& b, SketchEntityId id) {
    const auto& e = b.entity(id);
    const auto& s = b.sketch();
    switch (e.kind) {
        case BoundSketch::EntityKind::Line: {
            const auto p = s.point(e.start), q = s.point(e.end);
            return std::hypot(q.x - p.x, q.y - p.y);
        }
        case BoundSketch::EntityKind::Arc: return s.arc(e.arc).radius * s.arc_sweep(e.arc);
        case BoundSketch::EntityKind::Circle: return 2 * kPi * s.circle(e.circle).radius;
        default: return -1;
    }
}

double outerLoopArea(const BoundSketch& b) {
    auto curves = b.curves();
    if (!curves.ok()) return -1;
    auto profile = SketchProfile::build(xy(), curves.value());
    EXPECT_TRUE(profile.ok()) << (profile.ok() ? "" : profile.error().message);
    if (!profile.ok()) return -1;
    double area = 0;
    for (const auto& l : profile.value().loops()) area = std::max(area, l.area);
    return area;
}

int offsetDimensions(const BoundSketch& b) {
    int n = 0;
    for (Id c = 1; c <= static_cast<Id>(b.sketch().constraint_count()); ++c) {
        const auto& i = b.sketch().constraint(c);
        if (!i.removed && i.kind == bendcore::sketch2d::ConstraintKind::OffsetDistance && i.master == 0) ++n;
    }
    return n;
}

}  // namespace

TEST(SketchTools, OffsetCircleLengthAndAreaFollowTheDimension) {
    BoundSketch b;
    auto& s = b.sketch();
    const auto circle = b.addCircle(fixedPoint(b, 3, 4), 10);
    const Id radius = s.constrain_radius(b.entity(circle).circle, 10);
    OffsetOptions o;
    o.distance = -3;  // a circle runs counter-clockwise: the right side is outside
    auto r = offsetChain(b, {circle}, o);
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ(r.value().created.size(), 1u);
    const auto oc = r.value().created[0];
    EXPECT_EQ(b.entity(oc).center, b.entity(circle).center);
    EXPECT_NEAR(entityLength(b, oc), 2 * kPi * 13, 1e-9);
    EXPECT_NEAR(outerLoopArea(b), kPi * 169, 1e-6);
    ASSERT_NE(r.value().dimension, 0);
    auto rep = b.solve();
    EXPECT_EQ(rep.dof, 0) << rep.status;
    // Dimension 3 -> 5 and radius 10 -> 12: offset radius 17.
    s.set_constraint_value(r.value().dimension, 5);
    s.set_constraint_value(radius, 12);
    ASSERT_TRUE(b.solve().converged);
    EXPECT_NEAR(entityLength(b, oc), 2 * kPi * 17, 1e-8);
    EXPECT_NEAR(outerLoopArea(b), kPi * 289, 1e-6);
    // Both sides: radii 12 ± 2 from one dimension.
    BoundSketch b2;
    const auto c2 = b2.addCircle(fixedPoint(b2, 0, 0), 12);
    b2.sketch().constrain_radius(b2.entity(c2).circle, 12);
    OffsetOptions both;
    both.distance = 2;
    both.bothSides = true;
    auto r2 = offsetChain(b2, {c2}, both);
    ASSERT_TRUE(r2.ok()) << r2.error().message;
    ASSERT_EQ(r2.value().created.size(), 2u);
    EXPECT_NEAR(b2.sketch().circle_radius(b2.entity(r2.value().created[0]).circle), 10, 1e-12);
    EXPECT_NEAR(b2.sketch().circle_radius(b2.entity(r2.value().created[1]).circle), 14, 1e-12);
    EXPECT_EQ(offsetDimensions(b2), 1);
    b2.sketch().set_constraint_value(r2.value().dimension, 4);
    const auto rep2 = b2.solve();
    EXPECT_EQ(rep2.dof, 0) << rep2.status;
    EXPECT_NEAR(b2.sketch().circle_radius(b2.entity(r2.value().created[0]).circle), 8, 1e-9);
    EXPECT_NEAR(b2.sketch().circle_radius(b2.entity(r2.value().created[1]).circle), 16, 1e-9);
}

TEST(SketchTools, OffsetRectangleWithRoundCornersAgainstFormulas) {
    Rect r;
    ASSERT_EQ(r.b.solve().status, "определено");
    OffsetOptions o;
    o.distance = -5;  // outward
    o.corners = OffsetCorners::Round;
    auto res = offsetChain(r.b, {r.bottom, r.right, r.top, r.left}, o);
    ASSERT_TRUE(res.ok()) << res.error().message;
    ASSERT_EQ(res.value().created.size(), 8u);  // 4 lines + 4 corner arcs
    auto check = [&](double w, double h, double d) {
        double length = 0;
        for (SketchEntityId id : res.value().created) length += entityLength(r.b, id);
        EXPECT_NEAR(length, 2 * (w + h) + 2 * kPi * d, 1e-7);
        EXPECT_NEAR(outerLoopArea(r.b), w * h + 2 * d * (w + h) + kPi * d * d, 1e-6);
    };
    check(40, 20, 5);
    const auto rep = r.b.solve();
    EXPECT_EQ(rep.dof, 0) << rep.status;
    EXPECT_TRUE(rep.redundant.empty());
    EXPECT_EQ(offsetDimensions(r.b), 1);
    expectSatisfied(r.b);
    // Offset 5 -> 8 and width 40 -> 60: the offset follows.
    r.b.sketch().set_constraint_value(res.value().dimension, 8);
    r.b.sketch().set_constraint_value(r.width, 60);
    ASSERT_TRUE(r.b.solve().converged);
    check(60, 20, 8);
    expectSatisfied(r.b);
}

TEST(SketchTools, OffsetBothSidesWithExtendedCorners) {
    Rect r;
    ASSERT_EQ(r.b.solve().status, "определено");
    OffsetOptions o;
    o.distance = 2;
    o.bothSides = true;
    auto res = offsetChain(r.b, {r.bottom, r.right, r.top, r.left}, o);
    ASSERT_TRUE(res.ok()) << res.error().message;
    ASSERT_EQ(res.value().created.size(), 8u);
    const auto& inner = r.b.entity(res.value().created[0]);
    const auto& outer = r.b.entity(res.value().created[4]);
    expectPoint(r.b, inner.start, 2, 2);
    expectPoint(r.b, inner.end, 38, 2);
    expectPoint(r.b, outer.start, -2, -2);
    expectPoint(r.b, outer.end, 42, -2);
    const auto rep = r.b.solve();
    EXPECT_EQ(rep.dof, 0) << rep.status;
    EXPECT_TRUE(rep.redundant.empty());
    EXPECT_EQ(offsetDimensions(r.b), 1);
    r.b.sketch().set_constraint_value(res.value().dimension, 3);
    ASSERT_TRUE(r.b.solve().converged);
    expectPoint(r.b, inner.start, 3, 3, 1e-7);
    expectPoint(r.b, outer.end, 43, -3, 1e-7);
}

TEST(SketchTools, OffsetChainWithArcAndSpline) {
    BoundSketch b;
    // Line (0,0)-(10,0), then a spline from (10,0) tangent to the line.
    const Id p0 = fixedPoint(b, 0, 0), p1 = fixedPoint(b, 10, 0);
    const auto line = b.addLine(p0, p1);
    bendcore::sketch2d::BSplineData d;
    d.poles = {p1, fixedPoint(b, 15, 0), fixedPoint(b, 20, 5), fixedPoint(b, 25, 10)};
    d.degree = 3;
    d.knots = {0, 1};
    d.multiplicities = {4, 4};
    const auto sp = b.addBSpline(d);
    const std::array<Point2, 4> poles{Point2{10, 0}, Point2{15, 0}, Point2{20, 5}, Point2{25, 10}};
    OffsetOptions o;
    o.distance = 2;  // left: above the line
    auto res = offsetChain(b, {line, sp}, o);
    ASSERT_TRUE(res.ok()) << res.error().message;
    ASSERT_EQ(res.value().created.size(), 2u);
    const auto& ol = b.entity(res.value().created[0]);
    expectPoint(b, ol.start, 0, 2);
    expectPoint(b, ol.end, 10, 2);
    const auto os = res.value().created[1];
    EXPECT_EQ(b.sketch().bspline(b.entity(os).geo.id).poles.front(), ol.end);  // tangent joint shared
    const auto [u0, u1] = splineRange(b, os);
    for (int i = 0; i <= 20; ++i) {
        EXPECT_NEAR(distanceToBezier(poles, splinePoint(b, os, u0 + (u1 - u0) * i / 20.0)), 2, 1e-5) << i;
    }
    const auto rep = b.solve();
    EXPECT_TRUE(rep.converged);
    EXPECT_TRUE(rep.redundant.empty()) << rep.status;
    (void)line;
}

// ---- Mirror and patterns of ellipses and splines; circular pattern against the formula -------

TEST(SketchTools, MirrorAndPatternOfEllipsesFollowTheMinorRadius) {
    BoundSketch b;
    auto& s = b.sketch();
    const Id a0 = fixedPoint(b, 0, -10), a1 = fixedPoint(b, 0, 10);
    const auto axis = b.addLine(a0, a1, true);
    const auto el = b.addEllipse(fixedPoint(b, 10, 0), fixedPoint(b, 14, 3), 2);
    const Id minor = s.constrain_minor_radius(b.entity(el).geo, 2);
    auto m = mirrorEntities(b, {el}, axis);
    ASSERT_TRUE(m.ok()) << m.error().message;
    auto lp = linearPattern(b, {el}, {0, 20}, 2);
    ASSERT_TRUE(lp.ok()) << lp.error().message;
    auto rep = b.solve();
    EXPECT_EQ(rep.dof, 0) << rep.status;
    EXPECT_TRUE(rep.redundant.empty()) << rep.status;
    s.set_constraint_value(minor, 3);
    rep = b.solve();
    ASSERT_TRUE(rep.converged);
    for (SketchEntityId id : {m.value().created[0], lp.value().created[0]}) {
        EXPECT_NEAR(s.ellipse(b.entity(id).geo.id).minor_radius, 3, 1e-9);
        EXPECT_NEAR(s.ellipse_major_radius(b.entity(id).geo), std::sqrt(25.0 + 9.0), 1e-9);
    }
    expectPoint(b, b.entity(m.value().created[0]).focus, -14, 3);
    expectPoint(b, b.entity(lp.value().created[0]).focus, 14, 23);
}

TEST(SketchTools, CircularPatternCoordinatesByFormula) {
    BoundSketch b;
    auto& s = b.sketch();
    const Id o = fixedPoint(b, 5, -3);
    const Id p = s.add_point(25, -3);  // moved below by dimensions: polar (18, 30°) around o
    const auto c = b.addCircle(p, 2);
    s.constrain_radius(b.entity(c).circle, 2);
    const int n = 7;
    const double step = 360.0 / n;
    auto cp = circularPattern(b, {c}, o, n, step);
    ASSERT_TRUE(cp.ok()) << cp.error().message;
    ASSERT_EQ(cp.value().created.size(), static_cast<std::size_t>(n - 1));
    const auto guide = b.addLine(o, p, true);
    s.constrain_length(b.entity(guide).line, 18);
    const auto ref = b.addLine(o, fixedPoint(b, 15, -3), true);
    s.constrain_angle(b.entity(ref).line, b.entity(guide).line, 30);
    const auto rep = b.solve();
    ASSERT_TRUE(rep.converged);
    EXPECT_EQ(rep.dof, 0) << rep.status;
    for (int k = 1; k < n; ++k) {
        const double ang = (30 + k * step) * kPi / 180;
        const auto& e = b.entity(cp.value().created[static_cast<std::size_t>(k - 1)]);
        expectPoint(b, e.center, 5 + 18 * std::cos(ang), -3 + 18 * std::sin(ang), 1e-9);
        EXPECT_NEAR(s.circle_radius(e.circle), 2, 1e-12);
    }
}
