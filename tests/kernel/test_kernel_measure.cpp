// Measurements against analytic geometry (docs/kernel/ANALYSIS.md).

#include <bendcore/kernel/measure.hpp>

#include <gtest/gtest.h>

#include <cmath>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>

using namespace bendcore::kernel;

namespace {

constexpr double kPi = 3.14159265358979323846;

void expectPoint(const Point3& p, double x, double y, double z, double tol = 1e-7) {
    EXPECT_NEAR(p[0], x, tol);
    EXPECT_NEAR(p[1], y, tol);
    EXPECT_NEAR(p[2], z, tol);
}

// Face of `shape` whose surface is of `type` and whose centroid satisfies `pred`.
TopoDS_Face findFace(const TopoDS_Shape& shape, GeomAbs_SurfaceType type, auto pred) {
    for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
        const TopoDS_Face& f = TopoDS::Face(it.Current());
        if (BRepAdaptor_Surface(f).GetType() != type) continue;
        auto a = faceArea(f);
        if (a.ok() && pred(a.value().centroid)) return f;
    }
    return {};
}

TopoDS_Edge findEdge(const TopoDS_Shape& shape, GeomAbs_CurveType type) {
    for (TopExp_Explorer it(shape, TopAbs_EDGE); it.More(); it.Next()) {
        const TopoDS_Edge& e = TopoDS::Edge(it.Current());
        if (!BRep_Tool::Degenerated(e) && BRepAdaptor_Curve(e).GetType() == type) return e;
    }
    return {};
}

}  // namespace

TEST(Measure, DistanceBetweenSolidsFacesEdgesAndVertices) {
    const TopoDS_Shape a = BRepPrimAPI_MakeBox(10, 10, 10).Shape();
    const TopoDS_Shape b = BRepPrimAPI_MakeBox(gp_Pnt(20, 2, 3), 5, 5, 5).Shape();
    auto d = minimumDistance(a, b);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_NEAR(d.value().distance, 10.0, 1e-9);
    EXPECT_NEAR(d.value().pointOnFirst[0], 10.0, 1e-9);
    EXPECT_NEAR(d.value().pointOnSecond[0], 20.0, 1e-9);

    // Two spheres: |c1 c2| - r1 - r2, closest points on the centre line.
    const TopoDS_Shape s1 = BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, 0), 10).Shape();
    const TopoDS_Shape s2 = BRepPrimAPI_MakeSphere(gp_Pnt(30, 40, 0), 15).Shape();
    auto ds = minimumDistance(s1, s2);
    ASSERT_TRUE(ds.ok());
    EXPECT_NEAR(ds.value().distance, 25.0, 1e-6);
    // Near the minimum the distance is quadratic in the point position, so BRepExtrema
    // fixes the points only to ~sqrt of its tolerance on curved surfaces: the points lie on
    // the spheres exactly, and within 0.01 mm of the analytic closest points.
    const Point3& q1 = ds.value().pointOnFirst;
    const Point3& q2 = ds.value().pointOnSecond;
    EXPECT_NEAR(std::hypot(q1[0], q1[1], q1[2]), 10.0, 1e-6);
    EXPECT_NEAR(std::hypot(q2[0] - 30, q2[1] - 40, q2[2]), 15.0, 1e-6);
    EXPECT_NEAR(std::hypot(q1[0] - q2[0], q1[1] - q2[1], q1[2] - q2[2]), ds.value().distance, 1e-9);
    expectPoint(q1, 6, 8, 0, 1e-2);
    expectPoint(q2, 21, 28, 0, 1e-2);

    // Vertex above the top face.
    const TopoDS_Vertex v = BRepBuilderAPI_MakeVertex(gp_Pnt(5, 5, 20));
    const TopoDS_Face top = findFace(a, GeomAbs_Plane, [](const Point3& c) { return std::abs(c[2] - 10) < 1e-9; });
    auto dv = minimumDistance(v, top);
    ASSERT_TRUE(dv.ok());
    EXPECT_NEAR(dv.value().distance, 10.0, 1e-9);
    expectPoint(dv.value().pointOnSecond, 5, 5, 10);

    // Skew segments: X axis segment and a Y-direction segment at height 7.
    const TopoDS_Edge e1 = BRepBuilderAPI_MakeEdge(gp_Pnt(-10, 0, 0), gp_Pnt(10, 0, 0));
    const TopoDS_Edge e2 = BRepBuilderAPI_MakeEdge(gp_Pnt(3, -10, 7), gp_Pnt(3, 10, 7));
    auto de = minimumDistance(e1, e2);
    ASSERT_TRUE(de.ok());
    EXPECT_NEAR(de.value().distance, 7.0, 1e-9);
    expectPoint(de.value().pointOnFirst, 3, 0, 0);
    expectPoint(de.value().pointOnSecond, 3, 0, 7);

    // Touching solids: zero.
    auto touch = minimumDistance(a, BRepPrimAPI_MakeBox(gp_Pnt(10, 0, 0), 1, 1, 1).Shape());
    ASSERT_TRUE(touch.ok());
    EXPECT_NEAR(touch.value().distance, 0.0, 1e-9);
    EXPECT_EQ(minimumDistance(TopoDS_Shape(), a).error().code, ErrorCode::InvalidInput);
}

TEST(Measure, AngleBetweenPlanarFaces) {
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10, 10, 10).Shape();
    const TopoDS_Face top = findFace(box, GeomAbs_Plane, [](const Point3& c) { return std::abs(c[2] - 10) < 1e-9; });
    const TopoDS_Face bottom = findFace(box, GeomAbs_Plane, [](const Point3& c) { return std::abs(c[2]) < 1e-9; });
    const TopoDS_Face side = findFace(box, GeomAbs_Plane, [](const Point3& c) { return std::abs(c[0] - 10) < 1e-9; });
    auto adj = angleBetweenFaces(top, side);
    ASSERT_TRUE(adj.ok()) << adj.error().message;
    EXPECT_NEAR(adj.value().degrees, 90.0, 1e-9);
    // Arc anchor at the middle of the shared edge (x = 10, z = 10).
    expectPoint(adj.value().vertex, 10, 5, 10);
    auto opp = angleBetweenFaces(top, bottom);
    ASSERT_TRUE(opp.ok());
    EXPECT_NEAR(opp.value().degrees, 180.0, 1e-9);

    // Box rotated by 30 degrees about Y: its top face makes 30 degrees with the original top.
    gp_Trsf rot;
    rot.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp::DY()), kPi / 6);
    const TopoDS_Shape turned = BRepBuilderAPI_Transform(box, rot, true).Shape();
    const TopoDS_Face turnedTop = findFace(turned, GeomAbs_Plane, [](const Point3& c) {
        // Rotated top centroid: R_y(30) * (5, 5, 10).
        return std::abs(c[0] - (5 * std::cos(kPi / 6) + 10 * std::sin(kPi / 6))) < 1e-7;
    });
    ASSERT_FALSE(turnedTop.IsNull());
    auto a30 = angleBetweenFaces(top, turnedTop);
    ASSERT_TRUE(a30.ok());
    EXPECT_NEAR(a30.value().degrees, 30.0, 1e-7);
    EXPECT_NEAR(a30.value().radians, kPi / 6, 1e-9);

    const TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(5, 10).Shape();
    const TopoDS_Face lateral = findFace(cyl, GeomAbs_Cylinder, [](const Point3&) { return true; });
    EXPECT_EQ(angleBetweenFaces(top, lateral).error().code, ErrorCode::Unsupported);
}

TEST(Measure, AngleBetweenEdges) {
    const TopoDS_Vertex apex = BRepBuilderAPI_MakeVertex(gp_Pnt(1, 1, 0));
    const TopoDS_Vertex p1 = BRepBuilderAPI_MakeVertex(gp_Pnt(11, 1, 0));
    const TopoDS_Vertex p2 = BRepBuilderAPI_MakeVertex(gp_Pnt(1 + 5, 1 + 5 * std::sqrt(3.0), 0));
    // Edges share the apex vertex: corner angle 60 degrees, regardless of edge direction.
    const TopoDS_Edge e1 = BRepBuilderAPI_MakeEdge(p1, apex);
    const TopoDS_Edge e2 = BRepBuilderAPI_MakeEdge(apex, p2);
    auto a = angleBetweenEdges(e1, e2);
    ASSERT_TRUE(a.ok()) << a.error().message;
    EXPECT_NEAR(a.value().degrees, 60.0, 1e-9);
    expectPoint(a.value().vertex, 1, 1, 0);

    // Disjoint edges: angle of the oriented directions, 45 and 135 degrees.
    const TopoDS_Edge x = BRepBuilderAPI_MakeEdge(gp_Pnt(0, 0, 0), gp_Pnt(10, 0, 0));
    const TopoDS_Edge diag = BRepBuilderAPI_MakeEdge(gp_Pnt(0, 5, 3), gp_Pnt(4, 9, 3));
    auto a45 = angleBetweenEdges(x, diag);
    ASSERT_TRUE(a45.ok());
    EXPECT_NEAR(a45.value().degrees, 45.0, 1e-9);
    auto a135 = angleBetweenEdges(TopoDS::Edge(x.Reversed()), diag);
    ASSERT_TRUE(a135.ok());
    EXPECT_NEAR(a135.value().degrees, 135.0, 1e-9);

    const TopoDS_Edge circle = BRepBuilderAPI_MakeEdge(gp_Circ(gp::XOY(), 5));
    EXPECT_EQ(angleBetweenEdges(x, circle).error().code, ErrorCode::Unsupported);
}

TEST(Measure, LengthAreaAndCoordinates) {
    const TopoDS_Edge line = BRepBuilderAPI_MakeEdge(gp_Pnt(1, 2, 3), gp_Pnt(4, 6, 3));
    auto l = edgeLength(line);
    ASSERT_TRUE(l.ok());
    EXPECT_NEAR(l.value().length, 5.0, 1e-12);
    expectPoint(l.value().start, 1, 2, 3);
    expectPoint(l.value().end, 4, 6, 3);
    expectPoint(l.value().middle, 2.5, 4, 3);

    // Half circle of radius 5: length 5 pi, middle at the top.
    const TopoDS_Edge arc = BRepBuilderAPI_MakeEdge(gp_Circ(gp::XOY(), 5), 0.0, kPi);
    auto la = edgeLength(arc);
    ASSERT_TRUE(la.ok());
    EXPECT_NEAR(la.value().length, 5 * kPi, 1e-9);
    expectPoint(la.value().middle, 0, 5, 0, 1e-7);

    const TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(4, 10).Shape();
    const TopoDS_Face lateral = findFace(cyl, GeomAbs_Cylinder, [](const Point3&) { return true; });
    auto area = faceArea(lateral);
    ASSERT_TRUE(area.ok());
    EXPECT_NEAR(area.value().area, 2 * kPi * 4 * 10, 1e-7);
    EXPECT_NEAR(area.value().centroid[2], 5.0, 1e-7);

    const TopoDS_Shape box = BRepPrimAPI_MakeBox(3, 4, 5).Shape();
    const TopoDS_Face side = findFace(box, GeomAbs_Plane, [](const Point3& c) { return std::abs(c[0] - 3) < 1e-9; });
    auto as = faceArea(side);
    ASSERT_TRUE(as.ok());
    EXPECT_NEAR(as.value().area, 20.0, 1e-9);
    expectPoint(as.value().centroid, 3, 2, 2.5);

    auto p = vertexCoordinates(BRepBuilderAPI_MakeVertex(gp_Pnt(-1.5, 2.25, 7)));
    ASSERT_TRUE(p.ok());
    expectPoint(p.value(), -1.5, 2.25, 7);
    EXPECT_EQ(edgeLength(TopoDS_Edge()).error().code, ErrorCode::InvalidInput);
}

TEST(Measure, RadiusAndDiameter) {
    const TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(10, 0, 0), gp::DZ()), 6, 20).Shape();
    const TopoDS_Face lateral = findFace(cyl, GeomAbs_Cylinder, [](const Point3&) { return true; });
    auto rc = radiusOf(lateral);
    ASSERT_TRUE(rc.ok()) << rc.error().message;
    EXPECT_NEAR(rc.value().radius, 6.0, 1e-12);
    EXPECT_NEAR(rc.value().diameter, 12.0, 1e-12);
    EXPECT_NEAR(rc.value().center[0], 10.0, 1e-9);
    EXPECT_NEAR(rc.value().center[1], 0.0, 1e-9);
    EXPECT_NEAR(std::abs(rc.value().axis[2]), 1.0, 1e-12);
    // Leader point lies on the cylinder.
    EXPECT_NEAR(std::hypot(rc.value().pointOnElement[0] - 10, rc.value().pointOnElement[1]), 6.0, 1e-9);

    auto re = radiusOf(findEdge(cyl, GeomAbs_Circle));
    ASSERT_TRUE(re.ok());
    EXPECT_NEAR(re.value().radius, 6.0, 1e-12);
    EXPECT_NEAR(re.value().center[0], 10.0, 1e-9);

    const TopoDS_Shape sph = BRepPrimAPI_MakeSphere(gp_Pnt(1, 2, 3), 7.5).Shape();
    auto rs = radiusOf(findFace(sph, GeomAbs_Sphere, [](const Point3&) { return true; }));
    ASSERT_TRUE(rs.ok());
    EXPECT_NEAR(rs.value().diameter, 15.0, 1e-12);
    expectPoint(rs.value().center, 1, 2, 3);

    const TopoDS_Shape torus = BRepPrimAPI_MakeTorus(20, 3).Shape();
    auto rt = radiusOf(findFace(torus, GeomAbs_Torus, [](const Point3&) { return true; }));
    ASSERT_TRUE(rt.ok());
    EXPECT_NEAR(rt.value().radius, 3.0, 1e-12);
    // Centre of the tube cross-section lies on the major circle (radius 20, z = 0).
    EXPECT_NEAR(std::hypot(rt.value().center[0], rt.value().center[1]), 20.0, 1e-9);
    EXPECT_NEAR(rt.value().center[2], 0.0, 1e-9);

    // Cone from radius 10 at z = 0 to 5 at z = 8: reference radius at the base.
    const TopoDS_Shape cone = BRepPrimAPI_MakeCone(10, 5, 8).Shape();
    auto rk = radiusOf(findFace(cone, GeomAbs_Cone, [](const Point3&) { return true; }));
    ASSERT_TRUE(rk.ok());
    EXPECT_NEAR(rk.value().radius, 10.0, 1e-9);
    expectPoint(rk.value().center, 0, 0, 0);

    const TopoDS_Shape box = BRepPrimAPI_MakeBox(1, 1, 1).Shape();
    EXPECT_EQ(radiusOf(findFace(box, GeomAbs_Plane, [](const Point3&) { return true; })).error().code,
              ErrorCode::Unsupported);
    EXPECT_EQ(radiusOf(findEdge(box, GeomAbs_Line)).error().code, ErrorCode::Unsupported);
    EXPECT_EQ(radiusOf(box).error().code, ErrorCode::InvalidInput);
}
