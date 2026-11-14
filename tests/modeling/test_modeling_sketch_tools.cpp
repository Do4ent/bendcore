// Sketch editing tools and profiles of the new sketch geometry: trim, extend, offset, mirror,
// patterns, fillet and chamfer (coordinates against analytic values); profile with tangent arcs
// (area against the formula) and with a B-spline (area against a numerical integral).
// docs/modeling/SKETCH.md.

#include <bendcore/modeling/sketch_tools.hpp>

#include <gtest/gtest.h>

#include <cmath>
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
