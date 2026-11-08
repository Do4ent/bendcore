// Sketch solver (sketch2d over PlaneGCS): every constraint and dimension against analytic
// geometry, degrees of freedom and definition status of reference sketches, conflicting and
// redundant constraint ids, driven (reference) dimensions. docs/modeling/SKETCH.md.

#include <bendcore/sketch2d.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace bendcore::sketch2d;

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kTol = 1e-8;

bool has(const std::vector<Id>& v, Id x) { return std::find(v.begin(), v.end(), x) != v.end(); }

void expectPoint(const Sketch& s, Id p, double x, double y, double tol = kTol) {
    EXPECT_NEAR(s.point(p).x, x, tol) << "point " << p;
    EXPECT_NEAR(s.point(p).y, y, tol) << "point " << p;
}

Id fixed(Sketch& s, double x, double y) {
    const Id p = s.add_point(x, y);
    s.fix_point(p);
    return p;
}

void expectAllSatisfied(const Sketch& s) {
    for (Id c = 1; c <= static_cast<Id>(s.constraint_count()); ++c) {
        EXPECT_LT(s.residual(c), 1e-8) << "constraint " << c << " (" << to_string(s.constraint(c).kind) << ")";
    }
}

}  // namespace

TEST(SketchSolver, CoincidentHorizontalVerticalDistance) {
    Sketch s;
    const Id o = fixed(s, 0, 0);
    const Id a = s.add_point(9, 1), b = s.add_point(8, 7), c = s.add_point(0.5, 0.2);
    s.constrain_horizontal(o, a);
    s.constrain_vertical(a, b);
    s.constrain_distance(o, a, 12);
    s.constrain_distance(a, b, 5);
    s.constrain_coincident(c, o);
    const Report r = s.solve();
    EXPECT_TRUE(r.converged);
    EXPECT_EQ(r.status, "определено");
    EXPECT_EQ(r.definition, Definition::Fully);
    expectPoint(s, a, 12, 0);
    expectPoint(s, b, 12, 5);
    expectPoint(s, c, 0, 0);
    expectAllSatisfied(s);
}

TEST(SketchSolver, DistanceXYSigned) {
    Sketch s;
    const Id a = fixed(s, 2, 3);
    const Id b = s.add_point(10, 10);
    s.constrain_distance_x(a, b, 30);
    s.constrain_distance_y(a, b, -12);
    EXPECT_EQ(s.solve().dof, 0);
    expectPoint(s, b, 32, -9);
}

TEST(SketchSolver, PointCurveDistance) {
    Sketch s;
    const Id l0 = fixed(s, 0, 0), l1 = fixed(s, 10, 0);
    const Id line = s.add_line(l0, l1);
    const Id p = s.add_point(3.5, 4);
    s.constrain_point_curve_distance(p, line_geo(line), 5);
    s.constrain_distance_x(l0, p, 3);
    EXPECT_EQ(s.solve().dof, 0);
    expectPoint(s, p, 3, 5);

    // Distance to a circle: |PC| − r.
    const Id c = fixed(s, 50, 0);
    const Id circle = s.add_circle(c, 9);
    s.constrain_radius(circle, 10);
    const Id q = s.add_point(65, 1);
    s.constrain_horizontal(c, q);
    s.constrain_point_curve_distance(q, circle_geo(circle), 4);
    EXPECT_EQ(s.solve().dof, 0);
    expectPoint(s, q, 64, 0);
}

TEST(SketchSolver, RadiusDiameterOfCircleAndArc) {
    Sketch s;
    const Id c = fixed(s, 0, 0);
    const Id circle = s.add_circle(c, 3);
    s.constrain_diameter(circle_geo(circle), 30);
    const Id ac = fixed(s, 100, 0);
    const Id as = s.add_point(108, 0), ae = s.add_point(100, 9);
    const Id arc = s.add_arc(ac, as, ae, true);
    s.constrain_radius_of(arc_geo(arc), 7);
    s.constrain_horizontal(ac, as);
    s.constrain_vertical(ac, ae);
    const Report r = s.solve();
    EXPECT_EQ(r.dof, 0);
    EXPECT_NEAR(s.circle_radius(circle), 15, kTol);
    EXPECT_NEAR(s.arc(arc).radius, 7, kTol);
    expectPoint(s, as, 107, 0);
    expectPoint(s, ae, 100, 7);
    EXPECT_NEAR(s.arc_sweep(arc), kPi / 2, 1e-9);
}

TEST(SketchSolver, AngleParallelPerpendicular) {
    Sketch s;
    const Id o = fixed(s, 0, 0), x = fixed(s, 10, 0);
    const Id base = s.add_line(o, x);
    const Id p = s.add_point(8, 6);
    const Id l = s.add_line(o, p);
    s.constrain_angle(base, l, 30);
    s.constrain_distance(o, p, 10);
    // A parallel and a perpendicular line through fixed starts.
    const Id q0 = fixed(s, 0, 20), q1 = s.add_point(7, 26);
    const Id par = s.add_line(q0, q1);
    s.constrain_parallel(par, l);
    s.constrain_length(par, 4);
    const Id r0 = fixed(s, 0, -20), r1 = s.add_point(-3, -15);
    const Id perp = s.add_line(r0, r1);
    s.constrain_perpendicular(perp, l);
    s.constrain_length(perp, 2);
    EXPECT_EQ(s.solve().dof, 0);
    const double c30 = std::cos(kPi / 6), s30 = std::sin(kPi / 6);
    expectPoint(s, p, 10 * c30, 10 * s30);
    expectPoint(s, q1, 4 * c30, 20 + 4 * s30);
    expectPoint(s, r1, -2 * s30, -20 + 2 * c30);
    EXPECT_NEAR(s.measure(1), 30, 1e-9);
}

TEST(SketchSolver, TangentLineCircleWithoutSharedPoint) {
    Sketch s;
    const Id o = fixed(s, 0, 0);
    const Id circle = s.add_circle(o, 10);
    s.constrain_radius(circle, 10);
    const Id a = s.add_point(-20, 12), b = s.add_point(20, 11);
    const Id line = s.add_line(a, b);
    s.constrain_horizontal(a, b);
    s.constrain_distance_x(o, a, -20);
    s.constrain_distance_x(o, b, 20);
    const Id t = s.constrain_tangent(line_geo(line), circle_geo(circle));
    const Report r = s.solve();
    EXPECT_EQ(r.dof, 0);
    expectPoint(s, a, -20, 10);
    expectPoint(s, b, 20, 10);
    EXPECT_LT(s.residual(t), 1e-9);
}

TEST(SketchSolver, TangentLineArcAtSharedEnd) {
    // Line (0,0)-(10,0), arc starting at its end turning left: centre straight above the joint.
    Sketch s;
    const Id a = fixed(s, 0, 0), j = fixed(s, 10, 0);
    const Id line = s.add_line(a, j);
    const Id c = s.add_point(10.6, 4.4), e = s.add_point(16, 5.5);
    const Id arc = s.add_arc(c, j, e, true);
    s.constrain_tangent(line_geo(line), arc_geo(arc));
    s.constrain_radius_of(arc_geo(arc), 5);
    s.constrain_arc_length(arc, 5 * kPi / 2);
    const Report r = s.solve();
    EXPECT_EQ(r.dof, 0) << r.status;
    expectPoint(s, c, 10, 5);
    expectPoint(s, e, 15, 5);
    expectAllSatisfied(s);
}

TEST(SketchSolver, TangentArcArcAtCoincidentEnds) {
    Sketch s;
    const Id o = fixed(s, 0, 0), s1 = fixed(s, 10, 0), e1 = fixed(s, 0, 10);
    const Id arc1 = s.add_arc(o, s1, e1, true);
    // The second arc has its own start point, coincident with the end of the first.
    const Id s2 = s.add_point(0.3, 10.2), c2 = s.add_point(0.5, 14.2), e2 = s.add_point(-5, 15);
    const Id arc2 = s.add_arc(c2, s2, e2, false);
    s.constrain_coincident(e1, s2);
    s.constrain_tangent(arc_geo(arc1), arc_geo(arc2));
    s.constrain_radius_of(arc_geo(arc2), 5);
    s.constrain_horizontal(c2, e2);
    const Report r = s.solve();
    EXPECT_EQ(r.dof, 0) << r.status;
    expectPoint(s, c2, 0, 15);
    expectPoint(s, e2, -5, 15);
    expectAllSatisfied(s);
}

TEST(SketchSolver, TangentCircles) {
    Sketch s;
    const Id o = fixed(s, 0, 0);
    const Id c1 = s.add_circle(o, 10);
    s.constrain_radius(c1, 10);
    const Id p = s.add_point(17, 0.5);
    const Id c2 = s.add_circle(p, 4);
    s.constrain_radius(c2, 5);
    s.constrain_horizontal(o, p);
    s.constrain_tangent(circle_geo(c1), circle_geo(c2));
    EXPECT_EQ(s.solve().dof, 0);
    expectPoint(s, p, 15, 0);
    // Internal tangency is kept when the start is inside.
    Sketch t;
    const Id o2 = fixed(t, 0, 0);
    const Id big = t.add_circle(o2, 10);
    t.constrain_radius(big, 10);
    const Id q = t.add_point(4, 0.3);
    const Id small = t.add_circle(q, 3);
    t.constrain_radius(small, 3);
    t.constrain_horizontal(o2, q);
    t.constrain_tangent(circle_geo(big), circle_geo(small));
    EXPECT_EQ(t.solve().dof, 0);
    expectPoint(t, q, 7, 0);
}

TEST(SketchSolver, EqualSymmetricMidpointConcentric) {
    Sketch s;
    const Id a = fixed(s, 0, 0), b = fixed(s, 10, 0);
    const Id l1 = s.add_line(a, b);
    const Id c = fixed(s, 0, 5), d = s.add_point(3, 8);
    const Id l2 = s.add_line(c, d);
    s.constrain_equal(line_geo(l1), line_geo(l2));
    s.constrain_vertical(c, d);
    // Symmetric about the line l1 (x axis), about a point, midpoint of a line.
    const Id p = fixed(s, 3, 4), q = s.add_point(2, -3);
    s.constrain_symmetric(p, q, l1);
    const Id m = fixed(s, 20, 20), u = s.add_point(25, 21), v = fixed(s, 23, 24);
    s.constrain_symmetric_point(u, v, m);
    const Id mid = s.add_point(4, 1);
    s.constrain_midpoint(mid, line_geo(l1));
    // Circles: equal radii and concentric.
    const Id k1 = fixed(s, 50, 0);
    const Id circle1 = s.add_circle(k1, 6);
    s.constrain_radius(circle1, 6);
    const Id k2 = s.add_point(52, 1);
    const Id circle2 = s.add_circle(k2, 2);
    s.constrain_concentric(circle_geo(circle1), circle_geo(circle2));
    s.constrain_equal(circle_geo(circle1), circle_geo(circle2));
    const Report r = s.solve();
    EXPECT_EQ(r.dof, 0) << r.status;
    expectPoint(s, d, 0, 15);
    expectPoint(s, q, 3, -4);
    expectPoint(s, u, 17, 16);
    expectPoint(s, mid, 5, 0);
    expectPoint(s, k2, 50, 0);
    EXPECT_NEAR(s.circle_radius(circle2), 6, kTol);
}

TEST(SketchSolver, MidpointOfArcAndPointOnObjects) {
    Sketch s;
    const Id o = fixed(s, 0, 0), a = fixed(s, 10, 0), b = fixed(s, -10, 0);
    const Id arc = s.add_arc(o, a, b, true);
    const Id m = s.add_point(1, 9);
    s.constrain_midpoint(m, arc_geo(arc));
    // Point on a line (extended) and on a circle: intersection.
    const Id l0 = fixed(s, 0, -30), l1 = fixed(s, 10, -30);
    const Id line = s.add_line(l0, l1);
    const Id k = fixed(s, 40, -30);
    const Id circle = s.add_circle(k, 5);
    s.constrain_radius(circle, 5);
    const Id p = s.add_point(46, -29);
    s.constrain_point_on(p, line_geo(line));
    s.constrain_point_on(p, circle_geo(circle));
    const Report r = s.solve();
    EXPECT_EQ(r.dof, 0);
    expectPoint(s, m, 0, 10);
    expectPoint(s, p, 45, -30);
}

TEST(SketchSolver, EllipseRadiiAndPointOnEllipse) {
    Sketch s;
    const Id c = fixed(s, 0, 0);
    const Id f = s.add_point(3, 0.2);
    const Id e = s.add_ellipse(c, f, 2);
    s.constrain_horizontal(c, f);
    s.constrain_major_radius(ellipse_geo(e), 5);
    s.constrain_minor_radius(ellipse_geo(e), 3);
    const Id p = s.add_point(3, 2);
    s.constrain_point_on(p, ellipse_geo(e));
    s.constrain_distance_x(c, p, 3);
    const Report r = s.solve();
    EXPECT_EQ(r.dof, 0) << r.status;
    expectPoint(s, f, 4, 0);   // c = sqrt(a² − b²)
    EXPECT_NEAR(s.ellipse_major_radius(ellipse_geo(e)), 5, kTol);
    expectPoint(s, p, 3, 2.4); // y = b sqrt(1 − x²/a²)
}

TEST(SketchSolver, EllipseArcAndLineTangentToEllipse) {
    Sketch s;
    const Id c = fixed(s, 0, 0), f = fixed(s, 4, 0);
    const Id st = fixed(s, 5, 0);
    const Id en = s.add_point(0.3, 2.8);
    const Id ea = s.add_ellipse_arc(c, f, 3, st, en, true);
    s.constrain_vertical(c, en);
    // Horizontal tangent line y = 3 to the arc's ellipse.
    const Id a = s.add_point(-6, 3.5), b = s.add_point(6, 3.4);
    const Id line = s.add_line(a, b);
    s.constrain_horizontal(a, b);
    s.constrain_distance_x(c, a, -6);
    s.constrain_distance_x(c, b, 6);
    s.constrain_tangent(line_geo(line), ellipse_arc_geo(ea));
    const Report r = s.solve();
    EXPECT_EQ(r.dof, 0) << r.status;
    expectPoint(s, en, 0, 3);
    expectPoint(s, a, -6, 3);
}

TEST(SketchSolver, ArcLengthAndRotation) {
    Sketch s;
    const Id o = fixed(s, 0, 0), a = fixed(s, 10, 0);
    const Id e = s.add_point(1, 9);
    const Id arc = s.add_arc(o, a, e, true);
    s.constrain_arc_length(arc, 10 * kPi / 2);
    const Id p = fixed(s, 20, 5), q = s.add_point(0, 0);
    s.constrain_rotation(p, q, o, 90);
    EXPECT_EQ(s.solve().dof, 0);
    expectPoint(s, e, 0, 10);
    expectPoint(s, q, -5, 20);
}

TEST(SketchSolver, FixConstraintHasAnIdInConflicts) {
    Sketch s;
    const Id a = s.add_point(0, 0), b = s.add_point(5, 0);
    const Id fa = s.constrain_fix(a);
    const Id fb = s.constrain_fix(b);
    const Id d = s.constrain_distance(a, b, 10);
    const Report r = s.solve();
    EXPECT_EQ(r.status, "конфликт");
    EXPECT_EQ(r.definition, Definition::Over);
    EXPECT_TRUE(has(r.conflicting, d));
    EXPECT_TRUE(has(r.conflicting, fa) || has(r.conflicting, fb));
}

TEST(SketchSolver, DegreesOfFreedomOfFreeGeometry) {
    {
        Sketch s;
        s.add_line(s.add_point(0, 0), s.add_point(5, 1));
        EXPECT_EQ(s.solve().dof, 4) << "line";
    }
    {
        Sketch s;
        s.add_circle(s.add_point(0, 0), 3);
        EXPECT_EQ(s.solve().dof, 3) << "circle";
    }
    {
        Sketch s;
        s.add_arc(s.add_point(0, 0), s.add_point(3, 0), s.add_point(0, 3), true);
        EXPECT_EQ(s.solve().dof, 5) << "arc";
    }
    {
        Sketch s;
        s.add_ellipse(s.add_point(0, 0), s.add_point(3, 0), 2);
        EXPECT_EQ(s.solve().dof, 5) << "ellipse";
    }
    {
        Sketch s;
        const Id c = s.add_point(0, 0), f = s.add_point(4, 0);
        s.add_ellipse_arc(c, f, 3, s.add_point(5, 0), s.add_point(0, 3), true);
        EXPECT_EQ(s.solve().dof, 7) << "elliptical arc";
    }
    {
        Sketch s;
        BSplineData d;
        for (int i = 0; i < 4; ++i) d.poles.push_back(s.add_point(i, i % 2));
        d.degree = 3;
        d.knots = {0, 1};
        d.multiplicities = {4, 4};
        s.add_bspline(d);
        EXPECT_EQ(s.solve().dof, 8) << "cubic Bézier spline";
    }
}

TEST(SketchSolver, PerElementDefinitionStatus) {
    // Fixed corner, horizontal bottom with length; the right side is vertical but its length is
    // free: bottom line defined, the upper point and the right line are not.
    Sketch s;
    const Id p0 = fixed(s, 0, 0), p1 = s.add_point(9, 1), p2 = s.add_point(10, 7);
    const Id bottom = s.add_line(p0, p1), right = s.add_line(p1, p2);
    s.constrain_horizontal(p0, p1);
    s.constrain_distance(p0, p1, 10);
    s.constrain_vertical(p1, p2);
    Report r = s.solve();
    EXPECT_EQ(r.dof, 1);
    EXPECT_EQ(r.definition, Definition::Under);
    EXPECT_TRUE(r.fully_defined(point_geo(p0)));
    EXPECT_TRUE(r.fully_defined(point_geo(p1)));
    EXPECT_TRUE(r.fully_defined(line_geo(bottom)));
    EXPECT_FALSE(r.fully_defined(point_geo(p2)));
    EXPECT_FALSE(r.fully_defined(line_geo(right)));

    s.constrain_distance(p1, p2, 6);
    r = s.solve();
    EXPECT_EQ(r.dof, 0);
    for (const GeoStatus& g : r.geometry) EXPECT_TRUE(g.fully_defined) << to_string(g.geo.kind) << " " << g.geo.id;
}

TEST(SketchSolver, RedundantAndConflictingIdsOnRectangle) {
    Sketch s;
    const Id p0 = fixed(s, 0, 0), p1 = s.add_point(40, 1), p2 = s.add_point(41, 21), p3 = s.add_point(1, 19);
    const Id l0 = s.add_line(p0, p1), l1 = s.add_line(p1, p2), l2 = s.add_line(p2, p3), l3 = s.add_line(p3, p0);
    s.constrain_horizontal(p0, p1);
    s.constrain_vertical(p1, p2);
    s.constrain_horizontal(p2, p3);
    s.constrain_vertical(p3, p0);
    s.constrain_distance(p0, p1, 40);
    s.constrain_distance(p1, p2, 20);
    ASSERT_EQ(s.solve().status, "определено");
    // Parallel of opposite sides is implied by the horizontals: redundant.
    const Id par = s.constrain_parallel(l0, l2);
    Report r = s.solve();
    EXPECT_EQ(r.status, "избыточно");
    // The redundant group is the parallel and the two horizontals that imply it.
    ASSERT_FALSE(r.redundant.empty());
    for (Id c : r.redundant) EXPECT_TRUE(c == par || c == 1 || c == 3) << c;
    s.remove_constraint(par);
    // A second, different width conflicts with the first.
    const Id width2 = s.constrain_distance(p2, p3, 50);
    r = s.solve();
    EXPECT_EQ(r.status, "конфликт");
    EXPECT_TRUE(has(r.conflicting, width2));
    EXPECT_TRUE(has(r.conflicting, 5));
    s.remove_constraint(width2);
    // As a reference (driven) dimension it measures and does not over-constrain.
    const Id ref = s.constrain_distance(p2, p3, 50);
    s.set_driving(ref, false);
    r = s.solve();
    EXPECT_EQ(r.status, "определено");
    EXPECT_NEAR(s.constraint_value(ref), 40, kTol);
    (void)l1;
    (void)l3;
}

TEST(SketchSolver, DrivenDimensionsMeasureEveryKind) {
    Sketch s;
    const Id o = fixed(s, 0, 0), a = fixed(s, 10, 0), b = fixed(s, 0, 10);
    const Id arc = s.add_arc(o, a, b, true);
    const Id line = s.add_line(o, a), line2 = s.add_line(o, b);
    auto driven = [&](Id c) {
        s.set_driving(c, false);
        return c;
    };
    const Id len = driven(s.constrain_arc_length(arc, 1));
    const Id rad = driven(s.constrain_radius_of(arc_geo(arc), 1));
    const Id dia = driven(s.constrain_diameter(arc_geo(arc), 1));
    const Id ang = driven(s.constrain_angle(line, line2, 1));
    const Id dx = driven(s.constrain_distance_x(a, b, 1));
    const Id dy = driven(s.constrain_distance_y(a, b, 1));
    const Id pc = driven(s.constrain_point_curve_distance(b, line_geo(line), 1));
    const Report r = s.solve();
    EXPECT_EQ(r.dof, 0);
    EXPECT_NEAR(s.constraint_value(len), 5 * kPi, 1e-9);
    EXPECT_NEAR(s.constraint_value(rad), 10, 1e-9);
    EXPECT_NEAR(s.constraint_value(dia), 20, 1e-9);
    EXPECT_NEAR(s.constraint_value(ang), 90, 1e-9);
    EXPECT_NEAR(s.constraint_value(dx), -10, 1e-9);
    EXPECT_NEAR(s.constraint_value(dy), 10, 1e-9);
    EXPECT_NEAR(s.constraint_value(pc), 10, 1e-9);
    EXPECT_THROW(s.set_driving(s.constrain_horizontal(o, a), false), std::invalid_argument);
}

TEST(SketchSolver, InvalidOperandsThrow) {
    Sketch s;
    const Id a = s.add_point(0, 0), b = s.add_point(1, 0);
    const Id l = s.add_line(a, b);
    const Id l2 = s.add_line(b, s.add_point(1, 1));
    EXPECT_THROW(s.constrain_tangent(line_geo(l), line_geo(l2)), std::invalid_argument);
    EXPECT_THROW(s.constrain_radius_of(line_geo(l), 3), std::invalid_argument);
    EXPECT_THROW(s.constrain_point_on(99, line_geo(l)), std::out_of_range);
    BSplineData bad;
    bad.poles = {a, b};
    bad.degree = 3;
    bad.knots = {0, 1};
    bad.multiplicities = {4, 4};
    EXPECT_THROW(s.add_bspline(bad), std::invalid_argument);
    // Removing a line removes its constraints; its points stay.
    const Id h = s.constrain_parallel(l, l2);
    s.remove(line_geo(l));
    EXPECT_TRUE(s.constraint(h).removed);
    EXPECT_FALSE(s.is_removed(point_geo(a)));
    EXPECT_THROW(s.remove(point_geo(b)), std::invalid_argument);  // used by l2
}
