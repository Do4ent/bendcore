#include <bendcore/kernel/shape.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <map>

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_NurbsConvert.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <gp_Ax2.hxx>
#include <gp_Elips.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>

using namespace bendcore::kernel;

namespace {

constexpr double kPi = 3.14159265358979323846;

std::map<SurfaceType, int> surfaceHistogram(const TopologyIndex& index) {
    std::map<SurfaceType, int> histogram;
    for (const FaceInfo& face : index.faces()) ++histogram[face.surface];
    return histogram;
}

std::map<CurveType, int> curveHistogram(const TopologyIndex& index) {
    std::map<CurveType, int> histogram;
    for (const EdgeInfo& edge : index.edges()) ++histogram[edge.curve];
    return histogram;
}

void expectRelNear(double actual, double expected, double relTol) {
    EXPECT_NEAR(actual, expected, std::abs(expected) * relTol) << "expected " << expected;
}

}  // namespace

TEST(KernelShape, BoxPropertiesMatchAnalytic) {
    const Shape box(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape());
    ASSERT_FALSE(box.isNull());
    EXPECT_TRUE(box.checkValidity().valid);

    auto volume = box.volumeProperties();
    ASSERT_TRUE(volume.ok()) << volume.error().message;
    expectRelNear(volume.value().volume, 10.0 * 20.0 * 30.0, 1e-9);
    EXPECT_NEAR(volume.value().centerOfMass[0], 5.0, 1e-9);
    EXPECT_NEAR(volume.value().centerOfMass[1], 10.0, 1e-9);
    EXPECT_NEAR(volume.value().centerOfMass[2], 15.0, 1e-9);

    auto surface = box.surfaceProperties();
    ASSERT_TRUE(surface.ok());
    expectRelNear(surface.value().area, 2.0 * (10 * 20 + 10 * 30 + 20 * 30), 1e-9);

    auto bbox = box.boundingBox();
    ASSERT_TRUE(bbox.ok());
    for (int k = 0; k < 3; ++k) EXPECT_NEAR(bbox.value().min[k], 0.0, 1e-7);
    EXPECT_NEAR(bbox.value().max[0], 10.0, 1e-7);
    EXPECT_NEAR(bbox.value().max[1], 20.0, 1e-7);
    EXPECT_NEAR(bbox.value().max[2], 30.0, 1e-7);

    const TopologyCounts counts = box.counts();
    EXPECT_EQ(counts.solids, 1u);
    EXPECT_EQ(counts.shells, 1u);
    EXPECT_EQ(counts.faces, 6u);
    EXPECT_EQ(counts.edges, 12u);
    EXPECT_EQ(counts.vertices, 8u);

    auto index = TopologyIndex::build(box);
    ASSERT_TRUE(index.ok());
    EXPECT_EQ(surfaceHistogram(index.value()), (std::map<SurfaceType, int>{{SurfaceType::Plane, 6}}));
    EXPECT_EQ(curveHistogram(index.value()), (std::map<CurveType, int>{{CurveType::Line, 12}}));
    ASSERT_EQ(index.value().vertices().size(), 8u);
    for (const VertexInfo& v : index.value().vertices()) {
        for (int k = 0; k < 3; ++k) {
            const double extent = k == 0 ? 10.0 : (k == 1 ? 20.0 : 30.0);
            EXPECT_TRUE(std::abs(v.point[k]) < 1e-9 || std::abs(v.point[k] - extent) < 1e-9);
        }
    }
}

TEST(KernelShape, CylinderPropertiesAndTopology) {
    const double r = 5.0, h = 20.0;
    const Shape cylinder(BRepPrimAPI_MakeCylinder(r, h).Shape());
    auto volume = cylinder.volumeProperties();
    ASSERT_TRUE(volume.ok());
    expectRelNear(volume.value().volume, kPi * r * r * h, 1e-9);
    EXPECT_NEAR(volume.value().centerOfMass[2], h / 2.0, 1e-9);
    expectRelNear(cylinder.surfaceProperties().value().area, 2 * kPi * r * r + 2 * kPi * r * h, 1e-9);

    auto bbox = cylinder.boundingBox();
    ASSERT_TRUE(bbox.ok());
    EXPECT_NEAR(bbox.value().min[0], -r, 1e-6);
    EXPECT_NEAR(bbox.value().max[1], r, 1e-6);
    EXPECT_NEAR(bbox.value().max[2], h, 1e-6);

    auto index = TopologyIndex::build(cylinder);
    ASSERT_TRUE(index.ok());
    EXPECT_EQ(surfaceHistogram(index.value()),
              (std::map<SurfaceType, int>{{SurfaceType::Plane, 2}, {SurfaceType::Cylinder, 1}}));
    // Two rim circles and the seam line.
    EXPECT_EQ(curveHistogram(index.value()),
              (std::map<CurveType, int>{{CurveType::Line, 1}, {CurveType::Circle, 2}}));
    EXPECT_EQ(index.value().vertices().size(), 2u);
}

TEST(KernelShape, SpherePropertiesAndTopology) {
    const double r = 7.0;
    const Shape sphere(BRepPrimAPI_MakeSphere(gp_Pnt(1, 2, 3), r).Shape());
    auto volume = sphere.volumeProperties();
    ASSERT_TRUE(volume.ok());
    expectRelNear(volume.value().volume, 4.0 / 3.0 * kPi * r * r * r, 1e-9);
    EXPECT_NEAR(volume.value().centerOfMass[0], 1.0, 1e-9);
    EXPECT_NEAR(volume.value().centerOfMass[1], 2.0, 1e-9);
    EXPECT_NEAR(volume.value().centerOfMass[2], 3.0, 1e-9);
    expectRelNear(sphere.surfaceProperties().value().area, 4 * kPi * r * r, 1e-9);

    auto index = TopologyIndex::build(sphere);
    ASSERT_TRUE(index.ok());
    EXPECT_EQ(surfaceHistogram(index.value()), (std::map<SurfaceType, int>{{SurfaceType::Sphere, 1}}));
    // Seam meridian plus the two pole edges collapsed to points.
    EXPECT_EQ(curveHistogram(index.value()),
              (std::map<CurveType, int>{{CurveType::Circle, 1}, {CurveType::Degenerated, 2}}));
}

TEST(KernelShape, ConeAndTorusMatchAnalytic) {
    const double r1 = 5.0, r2 = 2.0, h = 10.0;
    const Shape cone(BRepPrimAPI_MakeCone(r1, r2, h).Shape());
    expectRelNear(cone.volumeProperties().value().volume, kPi * h / 3.0 * (r1 * r1 + r1 * r2 + r2 * r2), 1e-9);
    const double slant = std::sqrt((r1 - r2) * (r1 - r2) + h * h);
    expectRelNear(cone.surfaceProperties().value().area, kPi * (r1 + r2) * slant + kPi * (r1 * r1 + r2 * r2),
                  1e-9);
    EXPECT_EQ(surfaceHistogram(TopologyIndex::build(cone).value()),
              (std::map<SurfaceType, int>{{SurfaceType::Plane, 2}, {SurfaceType::Cone, 1}}));

    const double major = 10.0, minor = 3.0;
    const Shape torus(BRepPrimAPI_MakeTorus(major, minor).Shape());
    expectRelNear(torus.volumeProperties().value().volume, 2 * kPi * kPi * major * minor * minor, 1e-7);
    expectRelNear(torus.surfaceProperties().value().area, 4 * kPi * kPi * major * minor, 1e-7);
    EXPECT_EQ(surfaceHistogram(TopologyIndex::build(torus).value()),
              (std::map<SurfaceType, int>{{SurfaceType::Torus, 1}}));
}

TEST(KernelShape, BSplineAndOtherSurfaceTypes) {
    // NURBS conversion keeps the geometry but changes every surface and curve to B-spline.
    BRepBuilderAPI_NurbsConvert convert(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape());
    const Shape nurbsBox(convert.Shape());
    expectRelNear(nurbsBox.volumeProperties().value().volume, 6000.0, 1e-7);
    auto nurbsIndex = TopologyIndex::build(nurbsBox);
    ASSERT_TRUE(nurbsIndex.ok());
    EXPECT_EQ(surfaceHistogram(nurbsIndex.value()), (std::map<SurfaceType, int>{{SurfaceType::BSpline, 6}}));
    EXPECT_EQ(curveHistogram(nurbsIndex.value()), (std::map<CurveType, int>{{CurveType::BSpline, 12}}));

    // Extruded ellipse: elliptic rims, a linear-extrusion side surface (reported as Other).
    const double a = 6.0, b = 3.0, h = 4.0;
    gp_Elips ellipse(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), a, b);
    TopoDS_Wire wire = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(ellipse).Edge()).Wire();
    TopoDS_Face base = BRepBuilderAPI_MakeFace(wire).Face();
    const Shape prism(BRepPrimAPI_MakePrism(base, gp_Vec(0, 0, h)).Shape());
    expectRelNear(prism.volumeProperties().value().volume, kPi * a * b * h, 1e-7);
    auto prismIndex = TopologyIndex::build(prism);
    ASSERT_TRUE(prismIndex.ok());
    EXPECT_EQ(surfaceHistogram(prismIndex.value()),
              (std::map<SurfaceType, int>{{SurfaceType::Plane, 2}, {SurfaceType::Other, 1}}));
    EXPECT_EQ(curveHistogram(prismIndex.value()),
              (std::map<CurveType, int>{{CurveType::Line, 1}, {CurveType::Ellipse, 2}}));
}

TEST(KernelShape, TransformedShapeMovesPropertiesAndBox) {
    gp_Trsf rotation;
    rotation.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), kPi / 4.0);
    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(100.0, -50.0, 7.0));
    const gp_Trsf trsf = translation * rotation;
    const Shape moved(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape().Moved(TopLoc_Location(trsf)));

    auto volume = moved.volumeProperties();
    ASSERT_TRUE(volume.ok());
    expectRelNear(volume.value().volume, 6000.0, 1e-9);
    const gp_Pnt expectedCenter = gp_Pnt(5.0, 10.0, 15.0).Transformed(trsf);
    EXPECT_NEAR(volume.value().centerOfMass[0], expectedCenter.X(), 1e-9);
    EXPECT_NEAR(volume.value().centerOfMass[1], expectedCenter.Y(), 1e-9);
    EXPECT_NEAR(volume.value().centerOfMass[2], expectedCenter.Z(), 1e-9);

    // A 10x20 rectangle turned by 45 degrees spans (10+20)/sqrt(2) in x and y.
    auto bbox = moved.boundingBox();
    ASSERT_TRUE(bbox.ok());
    EXPECT_NEAR(bbox.value().max[0] - bbox.value().min[0], 30.0 / std::sqrt(2.0), 1e-6);
    EXPECT_NEAR(bbox.value().max[1] - bbox.value().min[1], 30.0 / std::sqrt(2.0), 1e-6);
    EXPECT_NEAR(bbox.value().min[2], 7.0, 1e-6);
    EXPECT_NEAR(bbox.value().max[2], 37.0, 1e-6);
}

TEST(KernelShape, ReversedSolidHasNegativeVolume) {
    const Shape reversed(BRepPrimAPI_MakeBox(2.0, 3.0, 4.0).Shape().Reversed());
    expectRelNear(reversed.volumeProperties().value().volume, -24.0, 1e-9);
}

TEST(KernelShape, OpenShellSolidIsReportedInvalidWithReason) {
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
    BRep_Builder builder;
    TopoDS_Shell shell;
    builder.MakeShell(shell);
    int added = 0;
    for (TopExp_Explorer it(box, TopAbs_FACE); it.More() && added < 5; it.Next(), ++added) {
        builder.Add(shell, it.Current());
    }
    TopoDS_Solid solid;
    builder.MakeSolid(solid);
    builder.Add(solid, shell);

    const ValidityReport report = Shape(solid).checkValidity();
    EXPECT_FALSE(report.valid);
    ASSERT_FALSE(report.issues.empty());
    bool mentionsNotClosed = false;
    for (const std::string& issue : report.issues) {
        if (issue.find("NotClosed") != std::string::npos && issue.find("SHELL #0") != std::string::npos) {
            mentionsNotClosed = true;
        }
    }
    EXPECT_TRUE(mentionsNotClosed) << report.issues.front();
}

TEST(KernelShape, NullAndFaceOnlyShapesGiveExplicitErrors) {
    const Shape null;
    EXPECT_TRUE(null.isNull());
    const ValidityReport report = null.checkValidity();
    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.issues.size(), 1u);
    EXPECT_EQ(null.boundingBox().error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(null.volumeProperties().error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(null.surfaceProperties().error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(TopologyIndex::build(null).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(null.counts().faces, 0u);

    const Shape square(BRepBuilderAPI_MakeFace(gp_Pln(), 0.0, 4.0, 0.0, 5.0).Face());
    EXPECT_EQ(square.volumeProperties().error().code, ErrorCode::InvalidInput);
    expectRelNear(square.surfaceProperties().value().area, 20.0, 1e-12);
    EXPECT_TRUE(square.checkValidity().valid);
}

TEST(KernelShape, TopologyIndexIsDeterministicAndFindsSubShapes) {
    const Shape cylinder(BRepPrimAPI_MakeCylinder(3.0, 8.0).Shape());
    auto first = TopologyIndex::build(cylinder);
    auto second = TopologyIndex::build(cylinder);
    ASSERT_TRUE(first.ok());
    ASSERT_TRUE(second.ok());
    ASSERT_EQ(first.value().faces().size(), second.value().faces().size());
    for (std::size_t i = 0; i < first.value().faces().size(); ++i) {
        EXPECT_TRUE(first.value().faces()[i].face.IsSame(second.value().faces()[i].face));
        EXPECT_EQ(first.value().faceIndex(first.value().faces()[i].face), std::optional<std::uint32_t>(i));
        // Orientation does not matter for lookup.
        EXPECT_EQ(first.value().faceIndex(first.value().faces()[i].face.Reversed()),
                  std::optional<std::uint32_t>(i));
    }
    for (std::size_t i = 0; i < first.value().edges().size(); ++i) {
        EXPECT_TRUE(first.value().edges()[i].edge.IsSame(second.value().edges()[i].edge));
        EXPECT_EQ(first.value().edgeIndex(first.value().edges()[i].edge), std::optional<std::uint32_t>(i));
    }
    for (std::size_t i = 0; i < first.value().vertices().size(); ++i) {
        EXPECT_EQ(first.value().vertexIndex(first.value().vertices()[i].vertex), std::optional<std::uint32_t>(i));
    }

    // Faces of a different shape, and the same face at another location, are not part of it.
    const TopoDS_Shape other = BRepPrimAPI_MakeBox(1.0, 1.0, 1.0).Shape();
    EXPECT_FALSE(first.value().faceIndex(TopExp_Explorer(other, TopAbs_FACE).Current()).has_value());
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(1, 0, 0));
    EXPECT_FALSE(first.value().faceIndex(first.value().faces()[0].face.Moved(TopLoc_Location(shift))).has_value());
    EXPECT_FALSE(first.value().faceIndex(TopoDS_Shape()).has_value());
}
