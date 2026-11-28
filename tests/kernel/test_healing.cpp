// Тесты лечения геометрии: эталоны — аналитические объёмы и известная топология
// примитивов (docs/kernel/GEOMETRY_HEALING.md, раздел «Как тестировать»).

#include <bendcore/kernel/healing.hpp>

#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_GTransform.hxx>
#include <BRepBuilderAPI_NurbsConvert.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Geom_ElementarySurface.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Trsf.hxx>

#include <gtest/gtest.h>

#include <cmath>

using namespace bendcore::kernel;

namespace {

constexpr double kPi = 3.14159265358979323846;

double volume(const TopoDS_Shape& s) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(s, p);
    return p.Mass();
}

int count(const TopoDS_Shape& s, TopAbs_ShapeEnum type) {
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> m;
    TopExp::MapShapes(s, type, m);
    return m.Extent();
}

int typeCount(const TypeCounts& c, const std::string& name) {
    auto it = c.find(name);
    return it == c.end() ? 0 : it->second;
}

TopoDS_Shape nurbs(const TopoDS_Shape& s) {
    BRepBuilderAPI_NurbsConvert conv(s, true);
    return conv.Shape();
}

HealingOptions onlyUnify() {
    HealingOptions o;
    o.directFaces = false;
    o.fixShape = false;
    o.removeSmallElements = false;
    o.convertToAnalytic = false;
    o.unifySameDomain = true;
    return o;
}

}  // namespace

TEST(KernelHealing, NurbsCylinderBecomesAnalyticAgain) {
    const double r = 5.0, h = 20.0;
    const TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(1, 2, 3), gp_Dir(0, 0, 1)), r, h).Shape();
    const TopoDS_Shape spline = nurbs(cyl);
    ASSERT_EQ(typeCount(countSurfaceTypes(spline), "BSplineSurface"), 3);
    ASSERT_EQ(typeCount(countSurfaceTypes(spline), "Cylinder"), 0);

    auto res = healShape(spline);
    ASSERT_TRUE(res.ok()) << res.error().message;
    const HealingReport& rep = res.value().report;
    EXPECT_EQ(typeCount(rep.surfacesBefore, "BSplineSurface"), 3);
    EXPECT_EQ(typeCount(rep.surfacesAfter, "Cylinder"), 1);
    EXPECT_EQ(typeCount(rep.surfacesAfter, "Plane"), 2);
    EXPECT_EQ(typeCount(rep.surfacesAfter, "BSplineSurface"), 0);
    EXPECT_EQ(typeCount(rep.curvesAfter, "Circle"), 2);
    EXPECT_EQ(typeCount(rep.curvesAfter, "Line"), 1);
    EXPECT_EQ(rep.surfacesConverted, 3);
    EXPECT_GE(rep.curvesConverted, 3);
    EXPECT_TRUE(rep.valid);
    EXPECT_TRUE(BRepCheck_Analyzer(res.value().shape).IsValid());
    const double exact = kPi * r * r * h;
    EXPECT_NEAR(volume(res.value().shape), exact, 1e-6 * exact);
    EXPECT_NEAR(rep.volumeAfter, exact, 1e-6 * exact);
    EXPECT_LE(rep.maxConversionDeviation, 1e-6);
}

TEST(KernelHealing, NurbsSphereBecomesAnalyticAgain) {
    const double r = 7.0;
    const TopoDS_Shape sph = BRepPrimAPI_MakeSphere(gp_Pnt(-3, 4, 1), r).Shape();
    const TopoDS_Shape spline = nurbs(sph);
    ASSERT_EQ(typeCount(countSurfaceTypes(spline), "BSplineSurface"), 1);

    auto res = healShape(spline);
    ASSERT_TRUE(res.ok()) << res.error().message;
    const HealingReport& rep = res.value().report;
    EXPECT_EQ(typeCount(rep.surfacesAfter, "Sphere"), 1);
    EXPECT_EQ(rep.facesAfter, 1);
    EXPECT_TRUE(rep.valid);
    const double exact = 4.0 / 3.0 * kPi * r * r * r;
    EXPECT_NEAR(volume(res.value().shape), exact, 1e-6 * exact);
}

TEST(KernelHealing, NurbsConeAndTorusBecomeAnalytic) {
    const double r1 = 6.0, r2 = 2.0, h = 9.0;
    const TopoDS_Shape cone = BRepPrimAPI_MakeCone(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 1, 0)), r1, r2, h).Shape();
    auto c = convertToAnalytic(nurbs(cone), 1e-6);
    ASSERT_TRUE(c.ok()) << c.error().message;
    EXPECT_EQ(typeCount(c.value().report.surfacesAfter, "Cone"), 1);
    EXPECT_EQ(typeCount(c.value().report.surfacesAfter, "Plane"), 2);
    EXPECT_TRUE(c.value().report.valid);
    const double coneExact = kPi * h / 3.0 * (r1 * r1 + r1 * r2 + r2 * r2);
    EXPECT_NEAR(volume(c.value().shape), coneExact, 1e-6 * coneExact);

    const double R = 10.0, rr = 3.0;
    const TopoDS_Shape torus = BRepPrimAPI_MakeTorus(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0)), R, rr).Shape();
    auto t = convertToAnalytic(nurbs(torus), 1e-6);
    ASSERT_TRUE(t.ok()) << t.error().message;
    EXPECT_EQ(typeCount(t.value().report.surfacesAfter, "Torus"), 1);
    EXPECT_TRUE(t.value().report.valid);
    const double torusExact = 2.0 * kPi * kPi * R * rr * rr;
    EXPECT_NEAR(volume(t.value().shape), torusExact, 1e-6 * torusExact);
}

TEST(KernelHealing, TightToleranceKeepsFreeFormSurface) {
    // A genuine free-form surface (perturbed B-spline) must not be replaced by a cylinder.
    const TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(5.0, 10.0).Shape();
    gp_GTrsf stretch;
    stretch.SetValue(1, 1, 1.02);  // elliptic cross-section: not a cylinder any more
    const TopoDS_Shape spline = nurbs(cyl);
    BRepBuilderAPI_GTransform gt(spline, stretch, true);
    auto res = convertToAnalytic(gt.Shape(), 1e-6);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_EQ(typeCount(res.value().report.surfacesAfter, "Cylinder"), 0);
    EXPECT_EQ(typeCount(res.value().report.surfacesAfter, "BSplineSurface"), 1);
    EXPECT_EQ(typeCount(res.value().report.surfacesAfter, "Plane"), 2);
}

TEST(KernelHealing, UnifySameDomainMergesCoplanarBoxes) {
    const TopoDS_Shape a = BRepPrimAPI_MakeBox(10, 10, 10).Shape();
    const TopoDS_Shape b = BRepPrimAPI_MakeBox(gp_Pnt(10, 0, 0), 10, 10, 10).Shape();
    BRepAlgoAPI_Fuse fuse(a, b);
    ASSERT_TRUE(fuse.IsDone());
    const TopoDS_Shape fused = fuse.Shape();
    ASSERT_EQ(count(fused, TopAbs_FACE), 10);  // 4 split side faces + 2 ends

    auto res = healShape(fused, onlyUnify());
    ASSERT_TRUE(res.ok()) << res.error().message;
    const HealingReport& rep = res.value().report;
    EXPECT_EQ(rep.facesBefore, 10);
    EXPECT_EQ(rep.facesAfter, 6);
    EXPECT_EQ(rep.facesMerged, 4);
    EXPECT_EQ(count(res.value().shape, TopAbs_EDGE), 12);
    EXPECT_TRUE(rep.valid);
    EXPECT_NEAR(volume(res.value().shape), 2000.0, 1e-9 * 2000.0);
}

TEST(KernelHealing, UnifySameDomainMergesHalfCylinders) {
    const double r = 4.0, h = 12.0;
    const gp_Ax2 ax(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    const TopoDS_Shape half1 = BRepPrimAPI_MakeCylinder(ax, r, h, kPi).Shape();
    gp_Trsf rot;
    rot.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), kPi);
    const TopoDS_Shape half2 = BRepBuilderAPI_Transform(half1, rot, true).Shape();
    BRepAlgoAPI_Fuse fuse(half1, half2);
    ASSERT_TRUE(fuse.IsDone());
    const TopoDS_Shape fused = fuse.Shape();
    // Two half-cylindrical faces and two half-discs at each end.
    ASSERT_EQ(count(fused, TopAbs_FACE), 6);

    auto res = healShape(fused, onlyUnify());
    ASSERT_TRUE(res.ok()) << res.error().message;
    const HealingReport& rep = res.value().report;
    EXPECT_EQ(rep.facesAfter, 3);
    EXPECT_EQ(rep.facesMerged, 3);
    EXPECT_EQ(typeCount(rep.surfacesAfter, "Cylinder"), 1);
    EXPECT_EQ(typeCount(rep.surfacesAfter, "Plane"), 2);
    EXPECT_TRUE(rep.valid);
    const double exact = kPi * r * r * h;
    EXPECT_NEAR(volume(res.value().shape), exact, 1e-9 * exact);
}

TEST(KernelHealing, SmallEdgeIsRemoved) {
    // Prism over a pentagon whose edge (10,10)-(10-d,10+d) is far below smallElementSize.
    const double d = 2e-6, h = 5.0;
    BRepBuilderAPI_MakePolygon poly;
    poly.Add(gp_Pnt(0, 0, 0));
    poly.Add(gp_Pnt(10, 0, 0));
    poly.Add(gp_Pnt(10, 10, 0));
    poly.Add(gp_Pnt(10 - d, 10 + d, 0));
    poly.Add(gp_Pnt(0, 10, 0));
    poly.Close();
    const TopoDS_Face base = BRepBuilderAPI_MakeFace(poly.Wire(), true).Face();
    const TopoDS_Shape prism = BRepPrimAPI_MakePrism(base, gp_Vec(0, 0, h)).Shape();
    ASSERT_EQ(count(prism, TopAbs_FACE), 7);
    ASSERT_EQ(count(prism, TopAbs_EDGE), 15);

    HealingOptions o;
    o.unifySameDomain = false;  // isolate the small-element step
    o.smallElementSize = 1e-5;
    auto res = healShape(prism, o);
    ASSERT_TRUE(res.ok()) << res.error().message;
    const HealingReport& rep = res.value().report;
    EXPECT_EQ(rep.facesAfter, 6);
    EXPECT_EQ(rep.edgesAfter, 12);
    EXPECT_EQ(rep.smallFacesRemoved, 1);
    EXPECT_EQ(rep.smallEdgesRemoved, 3);
    EXPECT_TRUE(rep.valid);
    // Removing the edge changes the area by at most the sliver triangle d*d/2 (times h).
    EXPECT_NEAR(volume(res.value().shape), 500.0, 1e-4);
}

TEST(KernelHealing, DirectFacesMakesMirroredSurfacesDirect) {
    const double r = 3.0, h = 8.0;
    const TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(r, h).Shape();
    gp_Trsf mirror;
    mirror.SetMirror(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0)));
    const TopoDS_Shape mirrored = BRepBuilderAPI_Transform(cyl, mirror, true).Shape();
    int indirect = 0;
    for (TopExp_Explorer ex(mirrored, TopAbs_FACE); ex.More(); ex.Next()) {
        auto s = occ::down_cast<Geom_ElementarySurface>(BRep_Tool::Surface(TopoDS::Face(ex.Current())));
        if (!s.IsNull() && !s->Position().Direct()) ++indirect;
    }
    ASSERT_EQ(indirect, 3);

    HealingOptions o;
    o.unifySameDomain = false;
    auto res = healShape(mirrored, o);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_EQ(res.value().report.directedFaces, 3);
    int indirectAfter = 0;
    for (TopExp_Explorer ex(res.value().shape, TopAbs_FACE); ex.More(); ex.Next()) {
        auto s = occ::down_cast<Geom_ElementarySurface>(BRep_Tool::Surface(TopoDS::Face(ex.Current())));
        if (!s.IsNull() && !s->Position().Direct()) ++indirectAfter;
    }
    EXPECT_EQ(indirectAfter, 0);
    EXPECT_TRUE(res.value().report.valid);
    const double exact = kPi * r * r * h;
    EXPECT_NEAR(volume(res.value().shape), exact, 1e-9 * exact);
}

TEST(KernelHealing, FullPipelineOnNurbsFusedBoxes) {
    // NURBS + split faces: conversion back to planes enables UnifySameDomain to merge them.
    const TopoDS_Shape a = BRepPrimAPI_MakeBox(10, 10, 10).Shape();
    const TopoDS_Shape b = BRepPrimAPI_MakeBox(gp_Pnt(10, 0, 0), 10, 10, 10).Shape();
    const TopoDS_Shape fused = BRepAlgoAPI_Fuse(a, b).Shape();
    auto res = healShape(nurbs(fused));
    ASSERT_TRUE(res.ok()) << res.error().message;
    const HealingReport& rep = res.value().report;
    EXPECT_EQ(typeCount(rep.surfacesBefore, "BSplineSurface"), 10);
    EXPECT_EQ(typeCount(rep.surfacesAfter, "Plane"), 6);
    EXPECT_EQ(rep.facesAfter, 6);
    EXPECT_TRUE(rep.valid);
    EXPECT_NEAR(rep.volumeAfter, 2000.0, 1e-6 * 2000.0);
    EXPECT_NEAR(rep.volumeChange, 0.0, 1e-6 * 2000.0);
}

TEST(KernelHealing, DisabledStepsLeaveShapeUntouched) {
    const TopoDS_Shape spline = nurbs(BRepPrimAPI_MakeCylinder(2.0, 3.0).Shape());
    HealingOptions o;
    o.directFaces = o.fixShape = o.removeSmallElements = o.convertToAnalytic = o.unifySameDomain = false;
    auto res = healShape(spline, o);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_TRUE(res.value().report.steps.empty());
    EXPECT_TRUE(res.value().shape.IsSame(spline));
    EXPECT_EQ(typeCount(res.value().report.surfacesAfter, "BSplineSurface"), 3);
}

TEST(KernelHealing, EmptyInputIsRejected) {
    auto nullRes = healShape(TopoDS_Shape());
    ASSERT_FALSE(nullRes.ok());
    EXPECT_EQ(nullRes.error().code, ErrorCode::InvalidInput);

    TopoDS_Compound empty;
    BRep_Builder().MakeCompound(empty);
    auto emptyRes = healShape(empty);
    ASSERT_FALSE(emptyRes.ok());
    EXPECT_EQ(emptyRes.error().code, ErrorCode::InvalidInput);

    auto convRes = convertToAnalytic(TopoDS_Shape());
    ASSERT_FALSE(convRes.ok());
    EXPECT_EQ(convRes.error().code, ErrorCode::InvalidInput);
}

TEST(KernelHealing, InvalidOptionsAreRejected) {
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(1, 1, 1).Shape();
    HealingOptions o;
    o.precision = -1.0;
    auto r1 = healShape(box, o);
    ASSERT_FALSE(r1.ok());
    EXPECT_EQ(r1.error().code, ErrorCode::InvalidInput);
    HealingOptions o2;
    o2.maxTolerance = o2.precision / 10.0;
    auto r2 = healShape(box, o2);
    ASSERT_FALSE(r2.ok());
    EXPECT_EQ(r2.error().code, ErrorCode::InvalidInput);
    auto r3 = convertToAnalytic(box, 0.0);
    ASSERT_FALSE(r3.ok());
    EXPECT_EQ(r3.error().code, ErrorCode::InvalidInput);
}
