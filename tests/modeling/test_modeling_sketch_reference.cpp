// Reference geometry: planes, axes, points and coordinate systems against analytic values.

#include <bendcore/modeling/reference_geometry.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <optional>

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>
#include <gp_Lin.hxx>

using namespace bendcore::modeling;
namespace kernel = bendcore::kernel;

namespace {

constexpr double kPi = std::numbers::pi;

void expectDir(const gp_Dir& a, const gp_Dir& b, double tol = 1e-12) {
    EXPECT_NEAR(a.X(), b.X(), tol);
    EXPECT_NEAR(a.Y(), b.Y(), tol);
    EXPECT_NEAR(a.Z(), b.Z(), tol);
}

void expectPnt(const gp_Pnt& a, const gp_Pnt& b, double tol = 1e-12) {
    EXPECT_NEAR(a.X(), b.X(), tol);
    EXPECT_NEAR(a.Y(), b.Y(), tol);
    EXPECT_NEAR(a.Z(), b.Z(), tol);
}

struct Indexed {
    kernel::Shape shape;
    kernel::TopologyIndex topology;
};

Indexed index(const TopoDS_Shape& s) {
    kernel::Shape shape(s);
    auto t = kernel::TopologyIndex::build(shape);
    EXPECT_TRUE(t.ok());
    return Indexed{shape, std::move(t).value()};
}

std::optional<std::uint32_t> faceWithCentroid(const kernel::TopologyIndex& t, const gp_Pnt& c) {
    for (std::uint32_t i = 0; i < t.faces().size(); ++i) {
        GProp_GProps props;
        BRepGProp::SurfaceProperties(t.faces()[i].face, props);
        if (props.CentreOfMass().Distance(c) < 1e-9) return i;
    }
    return std::nullopt;
}

std::optional<std::uint32_t> faceOfType(const kernel::TopologyIndex& t, kernel::SurfaceType type) {
    for (std::uint32_t i = 0; i < t.faces().size(); ++i) {
        if (t.faces()[i].surface == type) return i;
    }
    return std::nullopt;
}

}  // namespace

TEST(ModelingReferencePlane, OffsetFromPlaneAndFace) {
    auto p = planeOffset(gp_Pln(gp_Pnt(0, 0, 2), gp_Dir(0, 0, 1)), 5);
    ASSERT_TRUE(p.ok());
    EXPECT_NEAR(p.value().Distance(gp_Pnt(0, 0, 7)), 0, 1e-12);
    expectDir(p.value().Axis().Direction(), gp_Dir(0, 0, 1));

    const Indexed box = index(BRepPrimAPI_MakeBox(10, 20, 30).Shape());
    const auto bottom = faceWithCentroid(box.topology, gp_Pnt(5, 10, 0));
    ASSERT_TRUE(bottom);
    // Outward normal of the bottom face is -Z: offset 4 goes to z = -4.
    auto q = planeOffsetFromFace(box.topology, *bottom, 4);
    ASSERT_TRUE(q.ok()) << q.error().message;
    expectDir(q.value().Axis().Direction(), gp_Dir(0, 0, -1));
    EXPECT_NEAR(q.value().Distance(gp_Pnt(3, 3, -4)), 0, 1e-12);

    const Indexed cyl = index(BRepPrimAPI_MakeCylinder(5, 10).Shape());
    const auto side = faceOfType(cyl.topology, kernel::SurfaceType::Cylinder);
    ASSERT_TRUE(side);
    EXPECT_FALSE(planeOffsetFromFace(cyl.topology, *side, 1).ok());
    EXPECT_FALSE(planeOffsetFromFace(box.topology, 99, 1).ok());
}

TEST(ModelingReferencePlane, AtAngleThroughAxis) {
    const gp_Pln base;  // XY
    const gp_Ax1 axis(gp_Pnt(0, 3, 0), gp_Dir(1, 0, 0));
    const double angle = kPi / 6;
    auto p = planeAtAngle(base, axis, angle);
    ASSERT_TRUE(p.ok()) << p.error().message;
    // Z rotated about +X by angle: (0, -sin, cos).
    expectDir(p.value().Axis().Direction(), gp_Dir(0, -std::sin(angle), std::cos(angle)));
    EXPECT_NEAR(p.value().Distance(gp_Pnt(7, 3, 0)), 0, 1e-12);  // contains the axis
    EXPECT_NEAR(p.value().Distance(gp_Pnt(0, 3 + std::cos(angle), std::sin(angle))), 0, 1e-12);

    EXPECT_FALSE(planeAtAngle(base, gp_Ax1(gp_Pnt(), gp_Dir(0, 0, 1)), angle).ok());
    EXPECT_FALSE(planeAtAngle(base, gp_Ax1(gp_Pnt(0, 0, 1), gp_Dir(1, 0, 0)), angle).ok());
}

TEST(ModelingReferencePlane, ThroughThreePoints) {
    auto p = planeThroughPoints(gp_Pnt(1, 0, 0), gp_Pnt(0, 1, 0), gp_Pnt(0, 0, 1));
    ASSERT_TRUE(p.ok());
    const double k = 1 / std::sqrt(3.0);
    expectDir(p.value().Axis().Direction(), gp_Dir(k, k, k));
    EXPECT_NEAR(p.value().Distance(gp_Pnt(0, 0, 0)), k, 1e-12);
    expectDir(p.value().XAxis().Direction(), gp_Dir(-1, 1, 0));

    EXPECT_FALSE(planeThroughPoints(gp_Pnt(0, 0, 0), gp_Pnt(1, 1, 1), gp_Pnt(2, 2, 2)).ok());
    EXPECT_FALSE(planeThroughPoints(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0), gp_Pnt(2, 0, 0)).ok());
}

TEST(ModelingReferencePlane, TangentToCylinder) {
    const Indexed cyl = index(BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(1, 2, 0), gp_Dir(0, 0, 1)), 5, 10).Shape());
    const auto side = faceOfType(cyl.topology, kernel::SurfaceType::Cylinder);
    ASSERT_TRUE(side);
    const double a = kPi / 3;
    const gp_Pnt near(1 + 20 * std::cos(a), 2 + 20 * std::sin(a), 4);
    auto p = planeTangentToCylinder(cyl.topology, *side, near);
    ASSERT_TRUE(p.ok()) << p.error().message;
    expectDir(p.value().Axis().Direction(), gp_Dir(std::cos(a), std::sin(a), 0));
    expectPnt(p.value().Location(), gp_Pnt(1 + 5 * std::cos(a), 2 + 5 * std::sin(a), 4));
    // Tangent: the axis is at distance r from the plane, parallel to it.
    EXPECT_NEAR(p.value().Distance(gp_Pnt(1, 2, 0)), 5, 1e-12);
    EXPECT_NEAR(p.value().Distance(gp_Pnt(1, 2, 9)), 5, 1e-12);

    // A hole: the cylindrical face is reversed, its outward (material) normal points at the axis.
    const TopoDS_Shape block = BRepAlgoAPI_Cut(
        BRepPrimAPI_MakeBox(gp_Pnt(-9, -8, 0), 20, 20, 10).Shape(), cyl.shape.occt()).Shape();
    const Indexed holed = index(block);
    const auto hole = faceOfType(holed.topology, kernel::SurfaceType::Cylinder);
    ASSERT_TRUE(hole);
    auto h = planeTangentToCylinder(holed.topology, *hole, near);
    ASSERT_TRUE(h.ok()) << h.error().message;
    expectDir(h.value().Axis().Direction(), gp_Dir(-std::cos(a), -std::sin(a), 0));

    EXPECT_FALSE(planeTangentToCylinder(cyl.topology, *side, gp_Pnt(1, 2, 7)).ok());
    const auto top = faceOfType(cyl.topology, kernel::SurfaceType::Plane);
    EXPECT_FALSE(planeTangentToCylinder(cyl.topology, *top, near).ok());
}

TEST(ModelingReferencePlane, MidwayBetweenParallelFaces) {
    const Indexed box = index(BRepPrimAPI_MakeBox(10, 20, 30).Shape());
    const auto bottom = faceWithCentroid(box.topology, gp_Pnt(5, 10, 0));
    const auto top = faceWithCentroid(box.topology, gp_Pnt(5, 10, 30));
    const auto side = faceWithCentroid(box.topology, gp_Pnt(10, 10, 15));
    ASSERT_TRUE(bottom && top && side);
    auto mid = planeMidway(box.topology, *top, *bottom);
    ASSERT_TRUE(mid.ok()) << mid.error().message;
    EXPECT_NEAR(mid.value().Distance(gp_Pnt(-3, 40, 15)), 0, 1e-12);
    expectDir(mid.value().Axis().Direction(), gp_Dir(0, 0, 1));  // normal of the first face

    EXPECT_FALSE(planeMidway(box.topology, *top, *side).ok());
    EXPECT_FALSE(planeMidway(box.topology, *top, *top).ok());
}

TEST(ModelingReferenceAxis, AllKinds) {
    auto a = axisThroughPoints(gp_Pnt(1, 1, 1), gp_Pnt(1, 1, 5));
    ASSERT_TRUE(a.ok());
    expectDir(a.value().Direction(), gp_Dir(0, 0, 1));
    EXPECT_FALSE(axisThroughPoints(gp_Pnt(1, 1, 1), gp_Pnt(1, 1, 1)).ok());

    const Indexed cyl = index(
        BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(3, 4, 0), gp_Dir(1, 0, 0)), 2, 10).Shape());
    auto ca = axisOfFace(cyl.topology, *faceOfType(cyl.topology, kernel::SurfaceType::Cylinder));
    ASSERT_TRUE(ca.ok());
    EXPECT_NEAR(std::abs(ca.value().Direction().X()), 1, 1e-12);
    EXPECT_NEAR(gp_Lin(ca.value()).Distance(gp_Pnt(3, 4, 0)), 0, 1e-12);

    const Indexed cone =
        index(BRepPrimAPI_MakeCone(gp_Ax2(gp_Pnt(0, 0, 5), gp_Dir(0, 1, 0)), 4, 1, 6).Shape());
    auto co = axisOfFace(cone.topology, *faceOfType(cone.topology, kernel::SurfaceType::Cone));
    ASSERT_TRUE(co.ok()) << co.error().message;
    EXPECT_NEAR(std::abs(co.value().Direction().Y()), 1, 1e-12);
    EXPECT_NEAR(gp_Lin(co.value()).Distance(gp_Pnt(0, 0, 5)), 0, 1e-12);
    EXPECT_FALSE(axisOfFace(cone.topology, *faceOfType(cone.topology, kernel::SurfaceType::Plane)).ok());

    const Indexed box = index(BRepPrimAPI_MakeBox(10, 20, 30).Shape());
    int straight = 0;
    for (std::uint32_t i = 0; i < box.topology.edges().size(); ++i) {
        auto e = axisOfEdge(box.topology, i);
        ASSERT_TRUE(e.ok());
        const gp_Dir d = e.value().Direction();
        EXPECT_NEAR(std::abs(d.X()) + std::abs(d.Y()) + std::abs(d.Z()), 1, 1e-12);
        ++straight;
    }
    EXPECT_EQ(straight, 12);
    for (std::uint32_t i = 0; i < cyl.topology.edges().size(); ++i) {
        if (cyl.topology.edges()[i].curve == kernel::CurveType::Circle) {
            EXPECT_FALSE(axisOfEdge(cyl.topology, i).ok());
        }
    }

    // x = 2 and y + z = 4 meet in the line (2, t, 4 - t).
    auto ix = axisOfPlaneIntersection(gp_Pln(gp_Pnt(2, 0, 0), gp_Dir(1, 0, 0)),
                                      gp_Pln(gp_Pnt(0, 0, 4), gp_Dir(0, 1, 1)));
    ASSERT_TRUE(ix.ok());
    const double k = 1 / std::sqrt(2.0);
    expectDir(ix.value().Direction(), gp_Dir(0, -k, k));
    EXPECT_NEAR(gp_Lin(ix.value()).Distance(gp_Pnt(2, 1, 3)), 0, 1e-12);
    EXPECT_NEAR(gp_Lin(ix.value()).Distance(gp_Pnt(2, -5, 9)), 0, 1e-12);
    EXPECT_FALSE(axisOfPlaneIntersection(gp_Pln(), gp_Pln(gp_Pnt(0, 0, 3), gp_Dir(0, 0, -1))).ok());
}

TEST(ModelingReferencePoint, AllKinds) {
    const Indexed cyl = index(BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(1, 2, 3), gp_Dir(0, 0, 1)), 5, 10).Shape());
    bool sawCircle = false;
    for (std::uint32_t i = 0; i < cyl.topology.edges().size(); ++i) {
        const auto& e = cyl.topology.edges()[i];
        if (e.curve == kernel::CurveType::Circle) {
            auto c = pointCenterOfEdge(cyl.topology, i);
            ASSERT_TRUE(c.ok());
            EXPECT_NEAR(c.value().X(), 1, 1e-12);
            EXPECT_NEAR(c.value().Y(), 2, 1e-12);
            EXPECT_TRUE(std::abs(c.value().Z() - 3) < 1e-12 || std::abs(c.value().Z() - 13) < 1e-12);
            // Half the circumference away from the start: diametrically opposite point.
            auto start = pointOnEdge(cyl.topology, i, 0.0);
            auto half = pointOnEdge(cyl.topology, i, 0.5);
            ASSERT_TRUE(start.ok() && half.ok());
            EXPECT_NEAR(start.value().Distance(half.value()), 10, 1e-9);
            sawCircle = true;
        } else if (e.curve == kernel::CurveType::Line) {
            EXPECT_FALSE(pointCenterOfEdge(cyl.topology, i).ok());
            auto mid = pointOnEdge(cyl.topology, i, 0.5);
            ASSERT_TRUE(mid.ok());
            EXPECT_NEAR(mid.value().Z(), 8, 1e-9);
        }
    }
    EXPECT_TRUE(sawCircle);
    EXPECT_FALSE(pointOnEdge(cyl.topology, 0, 1.5).ok());

    const Indexed box = index(BRepPrimAPI_MakeBox(10, 20, 30).Shape());
    auto v = pointOfVertex(box.topology, 0);
    ASSERT_TRUE(v.ok());
    EXPECT_FALSE(pointOfVertex(box.topology, 8).ok());
    auto c = pointCentroidOfFace(box.topology.faces()[*faceWithCentroid(box.topology, gp_Pnt(5, 10, 30))].face);
    ASSERT_TRUE(c.ok());
    expectPnt(c.value(), gp_Pnt(5, 10, 30), 1e-9);

    auto ip = pointAxisPlaneIntersection(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(1, 1, 1)),
                                         gp_Pln(gp_Pnt(0, 0, 6), gp_Dir(0, 0, 1)));
    ASSERT_TRUE(ip.ok());
    expectPnt(ip.value(), gp_Pnt(6, 6, 6));
    EXPECT_FALSE(pointAxisPlaneIntersection(gp_Ax1(gp_Pnt(), gp_Dir(1, 0, 0)), gp_Pln()).ok());
    expectPnt(pointProjectedOnPlane(gp_Pnt(3, 4, 5), gp_Pln(gp_Pnt(0, 0, 1), gp_Dir(0, 0, 1))),
              gp_Pnt(3, 4, 1));
}

TEST(ModelingReferenceCoordinateSystem, AllKinds) {
    auto cs = coordinateSystem(gp_Pnt(1, 2, 3), gp_Dir(0, 0, 1), gp_Dir(1, 1, 1));
    ASSERT_TRUE(cs.ok());
    const double k = 1 / std::sqrt(2.0);
    expectDir(cs.value().XDirection(), gp_Dir(k, k, 0));
    expectDir(cs.value().YDirection(), gp_Dir(-k, k, 0));
    EXPECT_TRUE(cs.value().Direct());
    EXPECT_FALSE(coordinateSystem(gp_Pnt(), gp_Dir(0, 0, 1), gp_Dir(0, 0, -1)).ok());

    auto fp = coordinateSystemFromPoints(gp_Pnt(0, 0, 0), gp_Pnt(0, 5, 0), gp_Pnt(-1, 3, 0));
    ASSERT_TRUE(fp.ok());
    expectDir(fp.value().XDirection(), gp_Dir(0, 1, 0));
    expectDir(fp.value().YDirection(), gp_Dir(-1, 0, 0));
    expectDir(fp.value().Direction(), gp_Dir(0, 0, 1));
    EXPECT_FALSE(coordinateSystemFromPoints(gp_Pnt(), gp_Pnt(1, 0, 0), gp_Pnt(3, 0, 0)).ok());

    gp_Ax3 left(gp_Pnt(), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    left.YReverse();
    const gp_Ax3 onPlane = coordinateSystemOnPlane(gp_Pln(left), gp_Pnt(4, 5, 6));
    EXPECT_TRUE(onPlane.Direct());
    expectPnt(onPlane.Location(), gp_Pnt(4, 5, 0));
}
