// Projection of model edges onto the sketch plane, checked against analytic projections.

#include <bendcore/modeling/sketch_profile.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_BSplineCurve.hxx>
#include <NCollection_Array1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>

using namespace bendcore::modeling;
namespace kernel = bendcore::kernel;

namespace {

constexpr double kPi = std::numbers::pi;

bool near01(double v, double a, double b) {
    return std::abs(v - a) < 1e-9 || std::abs(v - b) < 1e-9;
}

TopoDS_Edge topCircle(double radius, double height) {
    gp_Circ circ(gp_Ax2(gp_Pnt(0, 0, height), gp_Dir(0, 0, 1)), radius);
    return BRepBuilderAPI_MakeEdge(circ).Edge();
}

}  // namespace

TEST(ModelingSketchProjection, BoxEdgesOntoParallelPlane) {
    const kernel::Shape box(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape());
    auto topology = kernel::TopologyIndex::build(box);
    ASSERT_TRUE(topology.ok());
    auto placement = SketchPlacement::fromAxes(gp_Pnt(0, 0, 50), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    ASSERT_TRUE(placement.ok());
    std::vector<std::uint32_t> all;
    for (std::uint32_t i = 0; i < topology.value().edges().size(); ++i) all.push_back(i);
    auto projected = projectEdges(placement.value(), topology.value(), all);
    ASSERT_TRUE(projected.ok()) << projected.error().message;
    int points = 0, segments = 0;
    for (const ProjectedEdge& e : projected.value()) {
        ASSERT_TRUE(e.edgeIndex.has_value());
        if (e.point) {
            ++points;
            EXPECT_TRUE(near01(e.point->x, 0, 10));
            EXPECT_TRUE(near01(e.point->y, 0, 20));
            continue;
        }
        ASSERT_TRUE(e.curve.has_value());
        const auto* line = std::get_if<LineSegment2d>(&*e.curve);
        ASSERT_NE(line, nullptr);
        ++segments;
        for (const Point2& p : {line->start, line->end}) {
            EXPECT_TRUE(near01(p.x, 0, 10));
            EXPECT_TRUE(near01(p.y, 0, 20));
        }
        const double length = std::hypot(line->end.x - line->start.x, line->end.y - line->start.y);
        EXPECT_TRUE(std::abs(length - 10) < 1e-9 || std::abs(length - 20) < 1e-9);
    }
    EXPECT_EQ(points, 4);    // the vertical edges
    EXPECT_EQ(segments, 8);  // top and bottom rectangles

    auto vertex = projectVertex(placement.value(), topology.value(), 0);
    ASSERT_TRUE(vertex.ok());
    EXPECT_FALSE(projectEdges(placement.value(), topology.value(), {99}).ok());
}

TEST(ModelingSketchProjection, CircleBecomesCircleOrEllipse) {
    const TopoDS_Edge edge = topCircle(5.0, 10.0);

    auto parallel = SketchPlacement::fromAxes(gp_Pnt(1, 2, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    ASSERT_TRUE(parallel.ok());
    auto a = projectEdge(parallel.value(), edge);
    ASSERT_TRUE(a.ok()) << a.error().message;
    const auto* circle = std::get_if<Circle2d>(&*a.value().curve);
    ASSERT_NE(circle, nullptr);
    EXPECT_NEAR(circle->radius, 5.0, 1e-9);
    EXPECT_NEAR(circle->center.x, -1.0, 1e-9);
    EXPECT_NEAR(circle->center.y, -2.0, 1e-9);

    // Plane tilted by 30 degrees about X: radius along X stays 5, along Y shrinks to 5*cos30.
    const double angle = kPi / 6;
    auto tilted = SketchPlacement::fromAxes(gp_Pnt(0, 0, 0), gp_Dir(0, -std::sin(angle), std::cos(angle)),
                                            gp_Dir(1, 0, 0));
    ASSERT_TRUE(tilted.ok());
    auto b = projectEdge(tilted.value(), edge);
    ASSERT_TRUE(b.ok()) << b.error().message;
    const auto* ellipse = std::get_if<Ellipse2d>(&*b.value().curve);
    ASSERT_NE(ellipse, nullptr);
    EXPECT_NEAR(ellipse->majorRadius, 5.0, 1e-7);
    EXPECT_NEAR(ellipse->minorRadius, 5.0 * std::cos(angle), 1e-7);
    // Centre (0,0,10) in sketch coordinates: y = 10 * sin(angle).
    EXPECT_NEAR(ellipse->center.x, 0.0, 1e-7);
    EXPECT_NEAR(ellipse->center.y, 10.0 * std::sin(angle), 1e-7);
    EXPECT_NEAR(std::abs(ellipse->majorDirection.x), 1.0, 1e-7);
}

TEST(ModelingSketchProjection, ArcKeepsEndPointsAndSense) {
    // Half circle of radius 4 around (0,0,7) from (4,0,7) through (0,4,7) to (-4,0,7).
    occ::handle<Geom_TrimmedCurve> arc =
        GC_MakeArcOfCircle(gp_Pnt(4, 0, 7), gp_Pnt(0, 4, 7), gp_Pnt(-4, 0, 7)).Value();
    const TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(arc).Edge();
    const auto placement = SketchPlacement::fromPlane(gp_Pln());
    auto projected = projectEdge(placement, edge);
    ASSERT_TRUE(projected.ok()) << projected.error().message;
    const auto* a = std::get_if<CircularArc2d>(&*projected.value().curve);
    ASSERT_NE(a, nullptr);
    EXPECT_NEAR(a->start.x, 4, 1e-9);
    EXPECT_NEAR(a->start.y, 0, 1e-9);
    EXPECT_NEAR(a->end.x, -4, 1e-9);
    EXPECT_NEAR(a->end.y, 0, 1e-9);
    EXPECT_TRUE(a->counterClockwise);

    // Seen from below (normal -Z, X kept) the same arc runs clockwise.
    auto below = SketchPlacement::fromAxes(gp_Pnt(), gp_Dir(0, 0, -1), gp_Dir(1, 0, 0));
    ASSERT_TRUE(below.ok());
    auto flipped = projectEdge(below.value(), edge);
    ASSERT_TRUE(flipped.ok());
    const auto* f = std::get_if<CircularArc2d>(&*flipped.value().curve);
    ASSERT_NE(f, nullptr);
    EXPECT_FALSE(f->counterClockwise);
    EXPECT_NEAR(f->start.x, 4, 1e-9);

    // The projected arc is usable as profile input: closing chord gives a half disc.
    auto profile = SketchProfile::build(
        placement, {{1, *projected.value().curve, false}, {2, LineSegment2d{{-4, 0}, {4, 0}}, false}});
    ASSERT_TRUE(profile.ok()) << profile.error().message;
    EXPECT_NEAR(profile.value().regions()[0].area, 8 * kPi, 1e-7);
}

TEST(ModelingSketchProjection, BSplinePolesProjectAffinely) {
    NCollection_Array1<gp_Pnt> poles(1, 4);
    poles.SetValue(1, gp_Pnt(0, 0, 0));
    poles.SetValue(2, gp_Pnt(3, 5, 2));
    poles.SetValue(3, gp_Pnt(7, -2, 9));
    poles.SetValue(4, gp_Pnt(10, 1, 4));
    NCollection_Array1<double> knots(1, 2);
    knots.SetValue(1, 0.0);
    knots.SetValue(2, 1.0);
    NCollection_Array1<int> mults(1, 2);
    mults.SetValue(1, 4);
    mults.SetValue(2, 4);
    occ::handle<Geom_BSplineCurve> curve = new Geom_BSplineCurve(poles, knots, mults, 3);
    const TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(curve).Edge();
    auto placement = SketchPlacement::fromAxes(gp_Pnt(1, 1, 1), gp_Dir(1, 1, 1), gp_Dir(1, -1, 0));
    ASSERT_TRUE(placement.ok());
    auto projected = projectEdge(placement.value(), edge);
    ASSERT_TRUE(projected.ok()) << projected.error().message;
    const auto* spline = std::get_if<BSpline2d>(&*projected.value().curve);
    ASSERT_NE(spline, nullptr);
    ASSERT_EQ(spline->poles.size(), 4u);
    EXPECT_EQ(spline->degree, 3);
    for (int i = 0; i < 4; ++i) {
        const Point2 expected = placement.value().toLocal(poles.Value(i + 1));
        EXPECT_NEAR(spline->poles[i].x, expected.x, 1e-9);
        EXPECT_NEAR(spline->poles[i].y, expected.y, 1e-9);
    }
}

TEST(ModelingSketchProjection, CylinderEdgesOnSideFace) {
    const kernel::Shape cylinder(BRepPrimAPI_MakeCylinder(5.0, 10.0).Shape());
    auto topology = kernel::TopologyIndex::build(cylinder);
    ASSERT_TRUE(topology.ok());
    // Sketch on the plane x = 0 (sketch x = model Y, sketch y = model Z) sees both circles
    // edge-on: diameters from y = -5 to 5 at heights 0 and 10.
    auto placement = SketchPlacement::fromAxes(gp_Pnt(), gp_Dir(1, 0, 0), gp_Dir(0, 1, 0));
    ASSERT_TRUE(placement.ok());
    int circles = 0;
    for (std::uint32_t i = 0; i < topology.value().edges().size(); ++i) {
        if (topology.value().edges()[i].curve != kernel::CurveType::Circle) continue;
        ++circles;
        auto e = projectEdges(placement.value(), topology.value(), {i});
        ASSERT_TRUE(e.ok()) << e.error().message;
        ASSERT_TRUE(e.value()[0].curve.has_value());
        const auto* line = std::get_if<LineSegment2d>(&*e.value()[0].curve);
        ASSERT_NE(line, nullptr);
        EXPECT_NEAR(std::min(line->start.x, line->end.x), -5.0, 1e-9);
        EXPECT_NEAR(std::max(line->start.x, line->end.x), 5.0, 1e-9);
        EXPECT_NEAR(line->start.y, line->end.y, 1e-12);
        EXPECT_TRUE(near01(line->start.y, 0, 10));
    }
    EXPECT_EQ(circles, 2);
}
