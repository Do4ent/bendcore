// Sketch profiles: placement, contours, nesting, regions, diagnostics, sketch2d binding.
// Reference values are closed-form areas and analytic frame vectors, not values of the code.

#include <bendcore/modeling/sketch_profile.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>

#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepLProp_SLProps.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

using namespace bendcore::modeling;
namespace kernel = bendcore::kernel;

namespace {

constexpr double kPi = std::numbers::pi;

void addRect(std::vector<SketchCurve>& out, SketchEntityId firstId, double x0, double y0, double w,
             double h) {
    const Point2 a{x0, y0}, b{x0 + w, y0}, c{x0 + w, y0 + h}, d{x0, y0 + h};
    out.push_back({firstId, LineSegment2d{a, b}, false});
    out.push_back({firstId + 1, LineSegment2d{b, c}, false});
    out.push_back({firstId + 2, LineSegment2d{c, d}, false});
    out.push_back({firstId + 3, LineSegment2d{d, a}, false});
}

double area(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(shape, props);
    return props.Mass();
}

gp_Dir faceNormal(const TopoDS_Face& face) {
    BRepAdaptor_Surface s(face);
    const double u = 0.5 * (s.FirstUParameter() + s.LastUParameter());
    const double v = 0.5 * (s.FirstVParameter() + s.LastVParameter());
    BRepLProp_SLProps props(s, u, v, 1, 1e-9);
    gp_Dir n = props.Normal();
    if (face.Orientation() == TopAbs_REVERSED) n.Reverse();
    return n;
}

void expectDir(const gp_Dir& actual, const gp_Dir& expected, double tol = 1e-12) {
    EXPECT_NEAR(actual.X(), expected.X(), tol);
    EXPECT_NEAR(actual.Y(), expected.Y(), tol);
    EXPECT_NEAR(actual.Z(), expected.Z(), tol);
}

SketchPlacement xy() { return SketchPlacement::fromPlane(gp_Pln()); }

bool hasIssue(const std::vector<ProfileIssue>& issues, ProfileIssueKind kind,
              const std::vector<SketchEntityId>& ids) {
    return std::any_of(issues.begin(), issues.end(), [&](const ProfileIssue& i) {
        return i.kind == kind && i.entities == ids;
    });
}

}  // namespace

TEST(ModelingSketchPlacement, FromAxesIsRightHandedAndRoundTrips) {
    const double s = std::sin(kPi / 6), c = std::cos(kPi / 6);
    auto placement = SketchPlacement::fromAxes(gp_Pnt(5, 5, 5), gp_Dir(0, -s, c), gp_Dir(1, 0, 0));
    ASSERT_TRUE(placement.ok()) << placement.error().message;
    const auto& p = placement.value();
    expectDir(p.yDirection(), gp_Dir(0, c, s));
    const gp_Pnt w = p.toWorld({30, 20});
    EXPECT_NEAR(w.X(), 35, 1e-12);
    EXPECT_NEAR(w.Y(), 5 + 20 * c, 1e-12);
    EXPECT_NEAR(w.Z(), 5 + 20 * s, 1e-12);
    const Point2 back = p.toLocal(w.Translated(gp_Vec(p.normal()) * 7.0));
    EXPECT_NEAR(back.x, 30, 1e-12);
    EXPECT_NEAR(back.y, 20, 1e-12);
    EXPECT_NEAR(p.signedDistance(w.Translated(gp_Vec(p.normal()) * 7.0)), 7.0, 1e-12);
    const gp_Pnt viaTrsf = gp_Pnt(30, 20, 0).Transformed(p.localToWorld());
    EXPECT_LT(viaTrsf.Distance(w), 1e-12);

    // X is projected onto the plane.
    auto skew = SketchPlacement::fromAxes(gp_Pnt(), gp_Dir(0, 0, 1), gp_Dir(1, 0, 1));
    ASSERT_TRUE(skew.ok());
    expectDir(skew.value().xDirection(), gp_Dir(1, 0, 0));

    EXPECT_FALSE(SketchPlacement::fromAxes(gp_Pnt(), gp_Dir(0, 0, 1), gp_Dir(0, 0, -1)).ok());
    gp_Ax3 left(gp_Pnt(), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    left.YReverse();
    EXPECT_FALSE(SketchPlacement::fromFrame(left).ok());
    expectDir(SketchPlacement::fromPlane(gp_Pln(left)).yDirection(), gp_Dir(0, 1, 0));
}

TEST(ModelingSketchProfile, RectangleWithCircularHole) {
    std::vector<SketchCurve> curves;
    addRect(curves, 1, 0, 0, 100, 60);
    curves.push_back({5, Circle2d{{50, 30}, 10}, false});
    auto built = SketchProfile::build(xy(), curves);
    ASSERT_TRUE(built.ok()) << built.error().message;
    const SketchProfile& profile = built.value();

    ASSERT_EQ(profile.loops().size(), 2u);
    EXPECT_EQ(profile.loops()[0].entities, (std::vector<SketchEntityId>{1, 2, 3, 4}));
    EXPECT_EQ(profile.loops()[1].entities, (std::vector<SketchEntityId>{5}));
    EXPECT_EQ(profile.loops()[1].parent, 0);
    EXPECT_EQ(profile.loops()[1].depth, 1);
    EXPECT_NEAR(profile.loops()[0].area, 6000.0, 1e-9);
    EXPECT_NEAR(profile.loops()[1].area, 100 * kPi, 1e-9);

    EXPECT_EQ(profile.materialRegions(), (std::vector<std::size_t>{0}));
    auto face = profile.makeMaterialFaces();
    ASSERT_TRUE(face.ok()) << face.error().message;
    ASSERT_EQ(face.value().ShapeType(), TopAbs_FACE);
    EXPECT_NEAR(area(face.value()), 6000.0 - 100 * kPi, 1e-7);
    expectDir(faceNormal(TopoDS::Face(face.value())), gp_Dir(0, 0, 1));
    int wires = 0;
    for (TopExp_Explorer e(face.value(), TopAbs_WIRE); e.More(); e.Next()) ++wires;
    EXPECT_EQ(wires, 2);

    EXPECT_EQ(profile.regionAt({5, 5}), std::optional<std::size_t>(0));
    EXPECT_EQ(profile.regionAt({50, 30}), std::optional<std::size_t>(1));
    EXPECT_FALSE(profile.regionAt({200, 0}).has_value());
    EXPECT_FALSE(profile.regionAt({50, 0}).has_value());  // on the boundary

    ASSERT_EQ(profile.edges().size(), 5u);
    for (const ProfileEdge& e : profile.edges()) EXPECT_EQ(profile.entityOf(e.edge), e.entity);
}

TEST(ModelingSketchProfile, IslandInsideHole) {
    std::vector<SketchCurve> curves;
    addRect(curves, 1, 0, 0, 100, 60);
    curves.push_back({5, Circle2d{{50, 30}, 20}, false});
    curves.push_back({6, Circle2d{{50, 30}, 8}, false});
    auto built = SketchProfile::build(xy(), curves);
    ASSERT_TRUE(built.ok()) << built.error().message;
    const auto& profile = built.value();
    ASSERT_EQ(profile.regions().size(), 3u);
    EXPECT_EQ(profile.loops()[2].parent, 1);
    EXPECT_EQ(profile.loops()[2].depth, 2);
    EXPECT_EQ(profile.materialRegions(), (std::vector<std::size_t>{0, 2}));
    EXPECT_NEAR(profile.regions()[0].area, 6000 - 400 * kPi, 1e-7);
    EXPECT_NEAR(profile.regions()[1].area, 400 * kPi - 64 * kPi, 1e-7);
    EXPECT_NEAR(profile.regions()[2].area, 64 * kPi, 1e-7);

    auto material = profile.makeMaterialFaces();
    ASSERT_TRUE(material.ok());
    EXPECT_EQ(material.value().ShapeType(), TopAbs_COMPOUND);
    EXPECT_NEAR(area(material.value()), 6000 - 400 * kPi + 64 * kPi, 1e-7);

    // Selecting the ring (region 1) by a point.
    auto ring = profile.makeFacesAt({{50, 45}});
    ASSERT_TRUE(ring.ok());
    EXPECT_NEAR(area(ring.value()), 336 * kPi, 1e-7);

    EXPECT_FALSE(profile.makeFaces({}).ok());
    EXPECT_FALSE(profile.makeFaces({3}).ok());
    EXPECT_FALSE(profile.makeFaces({0, 0}).ok());
    EXPECT_FALSE(profile.makeFacesAt({{-5, -5}}).ok());
}

TEST(ModelingSketchProfile, ThreeLevelNesting) {
    std::vector<SketchCurve> curves;
    // Deliberately out of nesting order.
    addRect(curves, 9, 35, 35, 30, 30);    // level 2
    addRect(curves, 1, 0, 0, 100, 100);    // level 0
    curves.push_back({20, Circle2d{{50, 50}, 5}, false});  // level 3
    addRect(curves, 5, 20, 20, 60, 60);    // level 1
    auto built = SketchProfile::build(xy(), curves);
    ASSERT_TRUE(built.ok()) << built.error().message;
    const auto& loops = built.value().loops();
    ASSERT_EQ(loops.size(), 4u);
    // Loops are ordered by their smallest entity id: 1, 5, 9, 20.
    EXPECT_EQ(loops[0].entities.front(), 1u);
    EXPECT_EQ(loops[1].entities.front(), 5u);
    EXPECT_EQ(loops[2].entities.front(), 9u);
    EXPECT_EQ(loops[3].entities.front(), 20u);
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(loops[i].depth, i);
        EXPECT_EQ(loops[i].parent, i - 1);
    }
    const auto& regions = built.value().regions();
    EXPECT_NEAR(regions[0].area, 10000 - 3600, 1e-7);
    EXPECT_NEAR(regions[1].area, 3600 - 900, 1e-7);
    EXPECT_NEAR(regions[2].area, 900 - 25 * kPi, 1e-7);
    EXPECT_NEAR(regions[3].area, 25 * kPi, 1e-7);
    EXPECT_EQ(built.value().materialRegions(), (std::vector<std::size_t>{0, 2}));
    auto material = built.value().makeMaterialFaces();
    ASSERT_TRUE(material.ok());
    EXPECT_NEAR(area(material.value()), 6400 + 900 - 25 * kPi, 1e-7);
}

TEST(ModelingSketchProfile, ArcsInBothDirections) {
    // Obround 40 x 20 with semicircular ends: area = 40*20 + pi*10^2.
    std::vector<SketchCurve> ccw{
        {1, LineSegment2d{{0, 0}, {40, 0}}, false},
        {2, CircularArc2d{{40, 10}, {40, 0}, {40, 20}, true}, false},
        {3, LineSegment2d{{40, 20}, {0, 20}}, false},
        {4, CircularArc2d{{0, 10}, {0, 20}, {0, 0}, true}, false},
    };
    auto a = SketchProfile::build(xy(), ccw);
    ASSERT_TRUE(a.ok()) << a.error().message;
    EXPECT_NEAR(a.value().regions()[0].area, 800 + 100 * kPi, 1e-7);

    // Same left arc given clockwise from the other end: same region.
    auto cw = ccw;
    cw[3].geometry = CircularArc2d{{0, 10}, {0, 0}, {0, 20}, false};
    auto b = SketchProfile::build(xy(), cw);
    ASSERT_TRUE(b.ok()) << b.error().message;
    EXPECT_NEAR(b.value().regions()[0].area, 800 + 100 * kPi, 1e-7);

    // The direction selects the other half circle: rectangle with the right end bulging in.
    std::vector<SketchCurve> inward{
        {1, LineSegment2d{{0, 0}, {40, 0}}, false},
        {2, CircularArc2d{{40, 10}, {40, 0}, {40, 20}, false}, false},
        {3, LineSegment2d{{40, 20}, {0, 20}}, false},
        {4, LineSegment2d{{0, 20}, {0, 0}}, false},
    };
    auto c = SketchProfile::build(xy(), inward);
    ASSERT_TRUE(c.ok()) << c.error().message;
    EXPECT_NEAR(c.value().regions()[0].area, 800 - 50 * kPi, 1e-7);
    inward[1].geometry = CircularArc2d{{40, 10}, {40, 0}, {40, 20}, true};
    auto d = SketchProfile::build(xy(), inward);
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_NEAR(d.value().regions()[0].area, 800 + 50 * kPi, 1e-7);
}

TEST(ModelingSketchProfile, EllipseAndSplineLoops) {
    std::vector<SketchCurve> curves;
    addRect(curves, 1, -50, -50, 100, 100);
    curves.push_back({5, Ellipse2d{{0, 0}, {1, 1}, 30, 10}, false});
    // Upper half of an ellipse with radii (8, 3) as an elliptical arc closed by its chord;
    // minor > major in the input exercises frame normalisation.
    curves.push_back({6, EllipticalArc2d{Ellipse2d{{0, 30}, {0, 1}, 3, 8}, -kPi / 2, kPi / 2},
                      false});
    curves.push_back({7, LineSegment2d{{-8, 30}, {8, 30}}, false});
    auto built = SketchProfile::build(xy(), curves);
    ASSERT_TRUE(built.ok()) << built.error().message;
    ASSERT_EQ(built.value().loops().size(), 3u);
    EXPECT_NEAR(built.value().loops()[1].area, kPi * 300, 1e-6);
    EXPECT_NEAR(built.value().loops()[2].area, kPi * 24 / 2, 1e-6);
    EXPECT_NEAR(built.value().regions()[0].area, 10000 - 300 * kPi - 12 * kPi, 1e-6);

    // Closed periodic B-spline (a smooth closed curve) inside a square.
    BSpline2d spline;
    spline.degree = 3;
    spline.periodic = true;
    spline.poles = {{10, 0}, {0, 10}, {-10, 0}, {0, -10}};
    spline.knots = {0, 1, 2, 3, 4};
    spline.multiplicities = {1, 1, 1, 1, 1};
    std::vector<SketchCurve> withSpline;
    addRect(withSpline, 1, -50, -50, 100, 100);
    withSpline.push_back({5, spline, false});
    auto s = SketchProfile::build(xy(), withSpline);
    ASSERT_TRUE(s.ok()) << s.error().message;
    ASSERT_EQ(s.value().loops().size(), 2u);
    EXPECT_EQ(s.value().loops()[1].depth, 1);
    // Area enclosed by the spline (Green quadrature in the profile) matches the adaptive
    // high-precision surface integral of the face built by OCCT.
    GProp_GProps precise;
    BRepGProp::SurfaceProperties(s.value().regions()[1].face, precise, 1e-12);
    EXPECT_NEAR(s.value().loops()[1].area, precise.Mass(), 1e-8);
    EXPECT_NEAR(s.value().regions()[0].area + precise.Mass(), 10000, 1e-8);
}

TEST(ModelingSketchProfile, OpenContourReportsEntityIds) {
    std::vector<SketchCurve> curves;
    addRect(curves, 1, 0, 0, 100, 60);  // closed, fine
    curves.push_back({10, LineSegment2d{{120, 0}, {150, 0}}, false});
    curves.push_back({11, LineSegment2d{{150, 0}, {150, 30}}, false});
    curves.push_back({12, LineSegment2d{{150, 30}, {120, 30}}, false});
    const auto issues = checkSketchCurves(curves);
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].kind, ProfileIssueKind::OpenContour);
    EXPECT_EQ(issues[0].entities, (std::vector<SketchEntityId>{10, 11, 12}));
    auto built = SketchProfile::build(xy(), curves);
    ASSERT_FALSE(built.ok());
    EXPECT_EQ(built.error().code, ErrorCode::InvalidInput);
    EXPECT_NE(built.error().message.find("open contour [entities 10, 11, 12]"), std::string::npos)
        << built.error().message;
}

TEST(ModelingSketchProfile, InvalidTopologyIsReported) {
    // Branching: a diagonal joins two corners.
    std::vector<SketchCurve> branching;
    addRect(branching, 1, 0, 0, 10, 10);
    branching.push_back({5, LineSegment2d{{0, 0}, {10, 10}}, false});
    auto issues = checkSketchCurves(branching);
    EXPECT_TRUE(hasIssue(issues, ProfileIssueKind::Branching, {1, 4, 5})) << issues.size();
    EXPECT_TRUE(hasIssue(issues, ProfileIssueKind::Branching, {2, 3, 5}));

    // Two crossing rectangles.
    std::vector<SketchCurve> crossing;
    addRect(crossing, 1, 0, 0, 10, 10);
    addRect(crossing, 5, 5, 5, 10, 10);
    issues = checkSketchCurves(crossing);
    EXPECT_TRUE(hasIssue(issues, ProfileIssueKind::Intersection, {2, 5}));
    EXPECT_TRUE(hasIssue(issues, ProfileIssueKind::Intersection, {3, 8}));
    EXPECT_EQ(issues.size(), 2u);

    // Collinear overlap: two closed triangles sharing a piece of an edge.
    std::vector<SketchCurve> overlap{
        {1, LineSegment2d{{0, 0}, {10, 0}}, false},
        {2, LineSegment2d{{10, 0}, {5, 5}}, false},
        {3, LineSegment2d{{5, 5}, {0, 0}}, false},
        {4, LineSegment2d{{5, 0}, {15, 0}}, false},
        {5, LineSegment2d{{15, 0}, {10, -5}}, false},
        {6, LineSegment2d{{10, -5}, {5, 0}}, false},
    };
    issues = checkSketchCurves(overlap);
    EXPECT_TRUE(hasIssue(issues, ProfileIssueKind::Overlap, {1, 4}));

    // Figure-eight B-spline.
    BSpline2d eight;
    eight.degree = 3;
    eight.periodic = true;
    eight.poles = {{-10, 5}, {-10, -5}, {10, 5}, {10, -5}};
    eight.knots = {0, 1, 2, 3, 4};
    eight.multiplicities = {1, 1, 1, 1, 1};
    issues = checkSketchCurves({{7, eight, false}});
    EXPECT_TRUE(hasIssue(issues, ProfileIssueKind::SelfIntersection, {7}));

    // Degenerate curves, duplicate ids, nothing but construction geometry.
    issues = checkSketchCurves({{1, LineSegment2d{{1, 1}, {1, 1}}, false},
                                {2, CircularArc2d{{0, 0}, {5, 0}, {0, 6}, true}, false},
                                {3, Circle2d{{0, 0}, 0.0}, false}});
    EXPECT_TRUE(hasIssue(issues, ProfileIssueKind::Degenerate, {1}));
    EXPECT_TRUE(hasIssue(issues, ProfileIssueKind::Degenerate, {2}));
    EXPECT_TRUE(hasIssue(issues, ProfileIssueKind::Degenerate, {3}));
    issues = checkSketchCurves({{4, Circle2d{{0, 0}, 1}, false}, {4, Circle2d{{9, 0}, 1}, false}});
    EXPECT_TRUE(hasIssue(issues, ProfileIssueKind::DuplicateId, {4}));
    issues = checkSketchCurves({{1, Circle2d{{0, 0}, 1}, true}});
    ASSERT_EQ(issues.size(), 1u);
    EXPECT_EQ(issues[0].kind, ProfileIssueKind::Empty);
}

TEST(ModelingSketchProfile, ConstructionGeometryIsIgnored) {
    std::vector<SketchCurve> curves;
    addRect(curves, 1, 0, 0, 20, 10);
    curves.push_back({5, LineSegment2d{{-5, -5}, {25, 15}}, true});  // crosses everything
    curves.push_back({6, Circle2d{{0, 0}, 3}, true});
    auto built = SketchProfile::build(xy(), curves);
    ASSERT_TRUE(built.ok()) << built.error().message;
    EXPECT_EQ(built.value().loops().size(), 1u);
    EXPECT_EQ(built.value().edges().size(), 4u);
    EXPECT_NEAR(built.value().regions()[0].area, 200, 1e-9);
}

TEST(ModelingSketchProfile, InclinedPlaneNormalAndArea) {
    const double s = std::sin(kPi / 6), c = std::cos(kPi / 6);
    auto placement = SketchPlacement::fromAxes(gp_Pnt(5, 5, 5), gp_Dir(0, -s, c), gp_Dir(1, 0, 0));
    ASSERT_TRUE(placement.ok());
    std::vector<SketchCurve> curves;
    addRect(curves, 1, 0, 0, 30, 20);
    curves.push_back({5, Circle2d{{15, 10}, 4}, false});
    auto built = SketchProfile::build(placement.value(), curves);
    ASSERT_TRUE(built.ok()) << built.error().message;
    auto face = built.value().makeMaterialFaces();
    ASSERT_TRUE(face.ok());
    EXPECT_NEAR(area(face.value()), 600 - 16 * kPi, 1e-7);
    expectDir(faceNormal(TopoDS::Face(face.value())), gp_Dir(0, -s, c), 1e-12);

    Bnd_Box box;
    BRepBndLib::AddOptimal(face.value(), box, false, false);
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    EXPECT_NEAR(x0, 5, 1e-7);
    EXPECT_NEAR(x1, 35, 1e-7);
    EXPECT_NEAR(y0, 5, 1e-7);
    EXPECT_NEAR(y1, 5 + 20 * c, 1e-7);
    EXPECT_NEAR(z0, 5, 1e-7);
    EXPECT_NEAR(z1, 5 + 20 * s, 1e-7);
    for (TopExp_Explorer e(face.value(), TopAbs_VERTEX); e.More(); e.Next()) {
        EXPECT_NEAR(placement.value().signedDistance(BRep_Tool::Pnt(TopoDS::Vertex(e.Current()))),
                    0.0, 1e-12);
    }
}

TEST(ModelingSketchProfile, SketchOnBoxFaces) {
    const kernel::Shape box(BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape());
    auto topology = kernel::TopologyIndex::build(box);
    ASSERT_TRUE(topology.ok());
    std::optional<std::uint32_t> top, bottom, side;
    for (std::uint32_t i = 0; i < topology.value().faces().size(); ++i) {
        GProp_GProps props;
        BRepGProp::SurfaceProperties(topology.value().faces()[i].face, props);
        const gp_Pnt c = props.CentreOfMass();
        if (std::abs(c.Z() - 30) < 1e-9) top = i;
        if (std::abs(c.Z()) < 1e-9) bottom = i;
        if (std::abs(c.X() - 10) < 1e-9) side = i;
    }
    ASSERT_TRUE(top && bottom && side);

    std::vector<SketchCurve> curves;
    addRect(curves, 1, 1, 1, 4, 4);
    curves.push_back({5, Circle2d{{3, 3}, 1}, false});

    struct Case {
        std::uint32_t face;
        gp_Dir normal;
        double level;  // coordinate along the normal
    };
    for (const Case& k : {Case{*top, gp_Dir(0, 0, 1), 30}, Case{*bottom, gp_Dir(0, 0, -1), 0},
                          Case{*side, gp_Dir(1, 0, 0), 10}}) {
        auto placement = SketchPlacement::onFace(topology.value(), k.face);
        ASSERT_TRUE(placement.ok()) << placement.error().message;
        expectDir(placement.value().normal(), k.normal);
        auto profile = SketchProfile::build(placement.value(), curves);
        ASSERT_TRUE(profile.ok()) << profile.error().message;
        auto face = profile.value().makeMaterialFaces();
        ASSERT_TRUE(face.ok());
        EXPECT_NEAR(area(face.value()), 16 - kPi, 1e-9);
        expectDir(faceNormal(TopoDS::Face(face.value())), k.normal);
        const gp_Pnt corner = placement.value().toWorld({1, 1});
        EXPECT_NEAR(gp_Vec(corner.XYZ()).Dot(gp_Vec(k.normal)), k.level, 1e-12);

        // Extruding along the normal leaves the box: volume (16 - pi) * 5, centroid 2.5 outside.
        const TopoDS_Shape prism =
            BRepPrimAPI_MakePrism(face.value(), gp_Vec(placement.value().normal()) * 5.0).Shape();
        GProp_GProps vol;
        BRepGProp::VolumeProperties(prism, vol);
        EXPECT_NEAR(vol.Mass(), (16 - kPi) * 5, 1e-7);
        EXPECT_NEAR(gp_Vec(vol.CentreOfMass().XYZ()).Dot(gp_Vec(k.normal)), k.level + 2.5, 1e-7);
    }

    // Origin and X direction options.
    FacePlacementOptions options;
    options.origin = gp_Pnt(5, 10, 100);
    options.xDirection = gp_Dir(0, 1, 0);
    auto custom = SketchPlacement::onFace(topology.value(), *top, options);
    ASSERT_TRUE(custom.ok());
    EXPECT_LT(custom.value().origin().Distance(gp_Pnt(5, 10, 30)), 1e-12);
    expectDir(custom.value().yDirection(), gp_Dir(-1, 0, 0));

    EXPECT_FALSE(SketchPlacement::onFace(topology.value(), 99).ok());
}

TEST(ModelingSketchProfile, BoundSketchRebuildAfterDimensionChange) {
    BoundSketch bound;
    auto& sk = bound.sketch();
    const auto p0 = sk.add_point(0, 0);
    const auto p1 = sk.add_point(45, 1);
    const auto p2 = sk.add_point(44, 31);
    const auto p3 = sk.add_point(-1, 29);
    const auto pc = sk.add_point(20, 15);
    const auto l0 = bound.addLine(p0, p1);
    const auto l1 = bound.addLine(p1, p2);
    const auto l2 = bound.addLine(p2, p3);
    const auto l3 = bound.addLine(p3, p0);
    const auto hole = bound.addCircle(pc, 4);
    const auto axis = bound.addLine(p0, p2, true);  // construction diagonal
    sk.fix_point(p0);
    sk.constrain_horizontal(p0, p1);
    sk.constrain_vertical(p1, p2);
    sk.constrain_horizontal(p2, p3);
    sk.constrain_vertical(p3, p0);
    const auto width = sk.constrain_distance(p0, p1, 50);
    sk.constrain_distance(p1, p2, 30);
    sk.fix_point(pc);
    sk.constrain_radius(bound.entity(hole).circle, 5);
    ASSERT_EQ(bound.solve().status, "определено");

    auto curves = bound.curves();
    ASSERT_TRUE(curves.ok()) << curves.error().message;
    auto first = SketchProfile::build(xy(), curves.value());
    ASSERT_TRUE(first.ok()) << first.error().message;
    EXPECT_NEAR(first.value().regions()[0].area, 1500 - 25 * kPi, 1e-6);
    EXPECT_EQ(first.value().loops()[0].entities,
              (std::vector<SketchEntityId>{l0, l1, l2, l3}));
    EXPECT_EQ(first.value().loops()[1].entities, (std::vector<SketchEntityId>{hole}));
    ASSERT_EQ(first.value().edges().size(), 5u);
    for (const ProfileEdge& e : first.value().edges()) EXPECT_NE(e.entity, axis);

    // Change the width dimension: same entities, same loops, new area.
    sk.remove_constraint(width);
    sk.constrain_distance(p0, p1, 80);
    ASSERT_EQ(bound.solve().status, "определено");
    curves = bound.curves();
    ASSERT_TRUE(curves.ok());
    auto second = SketchProfile::build(xy(), curves.value());
    ASSERT_TRUE(second.ok()) << second.error().message;
    EXPECT_NEAR(second.value().regions()[0].area, 2400 - 25 * kPi, 1e-6);
    EXPECT_EQ(second.value().loops()[0].entities, first.value().loops()[0].entities);
    EXPECT_EQ(second.value().loops()[1].entities, first.value().loops()[1].entities);
    EXPECT_EQ(second.value().loops()[1].parent, 0);
}

TEST(ModelingSketchProfile, BoundSketchArc) {
    BoundSketch bound;
    auto& sk = bound.sketch();
    const auto a = sk.add_point(0, 0);
    const auto b = sk.add_point(0, 20);
    const auto c = sk.add_point(0, 10);
    bound.addLine(a, b);
    const auto arc = bound.addArc(c, b, a, true);  // from (0,20) through (-10,10) to (0,0)
    sk.fix_point(a);
    sk.fix_point(b);
    sk.fix_point(c);
    bound.solve();
    auto curves = bound.curves();
    ASSERT_TRUE(curves.ok()) << curves.error().message;
    auto profile = SketchProfile::build(xy(), curves.value());
    ASSERT_TRUE(profile.ok()) << profile.error().message;
    EXPECT_NEAR(profile.value().regions()[0].area, 50 * kPi, 1e-6);
    EXPECT_TRUE(profile.value().regionAt({-5, 10}).has_value());
    EXPECT_FALSE(profile.value().regionAt({5, 10}).has_value());

    // An end point moved off the circle without solving is reported.
    BoundSketch unsolved;
    const auto u0 = unsolved.sketch().add_point(0, 0);
    const auto u1 = unsolved.sketch().add_point(10, 0);
    const auto u2 = unsolved.sketch().add_point(0, 12);
    unsolved.addArc(u0, u1, u2, true);
    auto bad = unsolved.curves();
    ASSERT_FALSE(bad.ok());
    EXPECT_NE(bad.error().message.find("arc entity 1"), std::string::npos);
    EXPECT_EQ(bound.entity(arc).kind, BoundSketch::EntityKind::Arc);
}

// Path: one chain of curves, listed in any order and direction, traversed from a free end.
TEST(ModelingSketchPath, OpenChainFromFreeEnd) {
    std::vector<SketchCurve> curves;
    // Entity 3: arc around (10, 10) counter-clockwise from (10, 0) (-90°) to (20, 10) (0°);
    // entities 1, 2: lines (20, 30)-(20, 10) and (0, 0)-(10, 0).
    curves.push_back({1, LineSegment2d{{20, 30}, {20, 10}}, false});
    curves.push_back({2, LineSegment2d{{0, 0}, {10, 0}}, false});
    curves.push_back({3, CircularArc2d{{10, 10}, {10, 0}, {20, 10}, true}, false});
    curves.push_back({4, LineSegment2d{{-5, -5}, {50, 50}}, true});  // construction: ignored
    auto path = buildSketchPath(xy(), curves);
    ASSERT_TRUE(path.ok()) << path.error().message;
    EXPECT_FALSE(path.value().closed);
    EXPECT_EQ(path.value().entities, (std::vector<SketchEntityId>{1, 3, 2}));
    GProp_GProps g;
    BRepGProp::LinearProperties(path.value().wire, g);
    EXPECT_NEAR(g.Mass(), 20 + 10 + 3.14159265358979323846 * 10 / 2, 1e-9);
    // The first edge starts at the free end of the smallest entity, (20, 30).
    TopoDS_Vertex v1, v2;
    TopExp::Vertices(path.value().edges.front(), v1, v2, true);
    EXPECT_NEAR(BRep_Tool::Pnt(v1).Distance(gp_Pnt(20, 30, 0)), 0.0, 1e-9);
}

TEST(ModelingSketchPath, ClosedChainAndErrors) {
    std::vector<SketchCurve> circle{{1, Circle2d{{0, 0}, 5}, false}};
    auto closed = buildSketchPath(xy(), circle);
    ASSERT_TRUE(closed.ok()) << closed.error().message;
    EXPECT_TRUE(closed.value().closed);
    std::vector<SketchCurve> twoChains{{1, LineSegment2d{{0, 0}, {10, 0}}, false}, {2, LineSegment2d{{0, 5}, {10, 5}}, false}};
    auto two = buildSketchPath(xy(), twoChains);
    ASSERT_FALSE(two.ok());
    EXPECT_NE(two.error().message.find("separate chains"), std::string::npos) << two.error().message;
    std::vector<SketchCurve> tee{{1, LineSegment2d{{0, 0}, {10, 0}}, false}, {2, LineSegment2d{{10, 0}, {20, 0}}, false},
                                 {3, LineSegment2d{{10, 0}, {10, 10}}, false}};
    EXPECT_FALSE(buildSketchPath(xy(), tee).ok());
    EXPECT_FALSE(buildSketchPath(xy(), {}).ok());
}
