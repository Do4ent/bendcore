// Datum nodes of the part document (docs/modeling/PART_DOCUMENT.md, "Вспомогательная геометрия"):
// DatumPoint (coordinates, vertex, edge midpoint, circle centre, edge-plane intersection),
// DatumPlane through three points and tangent to a cylinder, DatumCsys (directions, face + edge).
// Values are checked against analytic formulas; a change of a sketch dimension rebuilds them
// through topological naming; sketches stand on them; mirror takes them; file round trip.

#include "modeling_document_test_util.hpp"

#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Vec.hxx>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;

namespace {

FeatureSpec spec(const std::string& type, const std::string& id, std::map<std::string, std::string> params = {},
                 std::map<std::string, std::string> options = {}, std::map<std::string, std::string> links = {},
                 std::map<std::string, std::vector<TopoReference>> refs = {}) {
    FeatureSpec s;
    s.type = type;
    s.id = id;
    s.parameters = std::move(params);
    s.options = std::move(options);
    s.links = std::move(links);
    s.references = std::move(refs);
    return s;
}

FeatureSpec sketchSpec(const std::string& id, const std::string& plane, SketchData data,
                       std::map<std::string, std::string> links = {}) {
    FeatureSpec s = spec("Sketch", id, {}, {{"plane", plane}}, std::move(links));
    s.sketch = std::move(data);
    return s;
}

FeatureSpec extrudeSpec(const std::string& id, const std::string& sketch, std::string distance) {
    return spec("Extrude", id, {{"distance", std::move(distance)}}, {}, {{"sketch", sketch}});
}

TopoDS_Shape vertexAt(const TopoDS_Shape& shape, const gp_Pnt& p) {
    TopoDS_Shape found;
    int count = 0;
    for (TopExp_Explorer ex(shape, TopAbs_VERTEX); ex.More(); ex.Next()) {
        if (BRep_Tool::Pnt(TopoDS::Vertex(ex.Current())).Distance(p) > 1e-7) continue;
        if (!found.IsNull() && found.IsSame(ex.Current())) continue;
        found = ex.Current();
        ++count;
    }
    EXPECT_EQ(count, 1) << "vertices at (" << p.X() << ", " << p.Y() << ", " << p.Z() << ")";
    return found;
}

void expectPnt(const gp_Pnt& a, const gp_Pnt& b, double tol = 1e-9) {
    EXPECT_NEAR(a.X(), b.X(), tol);
    EXPECT_NEAR(a.Y(), b.Y(), tol);
    EXPECT_NEAR(a.Z(), b.Z(), tol);
}

void expectDir(const gp_Dir& a, const gp_Dir& b, double tol = 1e-12) {
    EXPECT_NEAR(a.X(), b.X(), tol);
    EXPECT_NEAR(a.Y(), b.Y(), tol);
    EXPECT_NEAR(a.Z(), b.Z(), tol);
}

std::shared_ptr<const DatumResult> datumOf(const PartDocument& doc, const std::string& id) {
    const FeatureInfo i = info(doc, id);
    EXPECT_TRUE(i.state == FeatureState::Ok) << id << ": " << messages(i);
    EXPECT_TRUE(i.datum) << id;
    return i.datum;
}

// Block W x D x H from a sketch rectangle at the origin (variables W, D, H).
void block(PartDocument& doc, double w, double d, double h) {
    setVar(doc, "W", w);
    setVar(doc, "D", d);
    setVar(doc, "H", h);
    SketchBuilder b;
    b.rectangle("0", "0", "W", "D");
    add(doc, sketchSpec("Profile", "XY", b.build()));
    add(doc, extrudeSpec("Block", "Profile", "H"));
}

}  // namespace

TEST(ModelingDocumentDatum, PointsFollowTheModel) {
    PartDocument doc(partRegistry());
    double W = 40, D = 30, H = 20;
    block(doc, W, D, H);
    // A boss cylinder R = 6 on top, centre (10, 12), height 5: its top circle has a centre.
    setVar(doc, "R", 6);
    SketchBuilder c;
    c.fixedCircle("10", "12", "R");
    FeatureSpec boss = sketchSpec("BossSketch", "XY", c.build());
    boss.parameters = {{"offset", "H"}};
    add(doc, boss);
    add(doc, extrudeSpec("Boss", "BossSketch", "5"));
    doc.rebuild();
    const NamedShape s0 = doc.finalShape();
    ASSERT_FALSE(s0.isNull());

    const TopoReference corner = refTo(s0, vertexAt(s0.shape(), gp_Pnt(W, D, H)));
    const TopoReference topEdge = refTo(s0, singleLine(s0.shape(), 0, 0, H));  // along X at y = 0, z = H
    const TopoReference postEdge = refTo(s0, singleLine(s0.shape(), 2, W, 0));  // along Z at x = W, y = 0
    TopoReference circle;
    for (TopExp_Explorer ex(s0.shape(), TopAbs_EDGE); ex.More(); ex.Next()) {
        BRepAdaptor_Curve cu(TopoDS::Edge(ex.Current()));
        if (cu.GetType() == GeomAbs_Circle && std::abs(cu.Circle().Location().Z() - (H + 5)) < 1e-9) {
            circle = refTo(s0, ex.Current());
        }
    }
    ASSERT_FALSE(circle.name.empty());

    add(doc, spec("DatumPoint", "PCoord", {{"x", "W / 4"}, {"y", "D + 1"}, {"z", "-2"}}));
    add(doc, spec("DatumPoint", "PVertex", {}, {{"method", "vertex"}}, {}, {{"vertex", {corner}}}));
    add(doc, spec("DatumPoint", "PMid", {}, {{"method", "midpoint"}}, {}, {{"edge", {topEdge}}}));
    add(doc, spec("DatumPoint", "PCenter", {}, {{"method", "center"}}, {}, {{"edge", {circle}}}));
    add(doc, spec("DatumPlane", "Third", {{"offset", "H / 3"}}, {{"base", "XY"}}));
    add(doc, spec("DatumPoint", "PCross", {}, {{"method", "edgePlane"}, {"base", "datum"}}, {{"plane", "Third"}},
                  {{"edge", {postEdge}}}));

    auto check = [&](double w, double d, double h) {
        const auto rep = doc.rebuild();
        (void)rep;
        expectPnt(*datumOf(doc, "PCoord")->point, gp_Pnt(w / 4, d + 1, -2));
        expectPnt(*datumOf(doc, "PVertex")->point, gp_Pnt(w, d, h));
        expectPnt(*datumOf(doc, "PMid")->point, gp_Pnt(w / 2, 0, h));
        expectPnt(*datumOf(doc, "PCenter")->point, gp_Pnt(10, 12, h + 5));
        expectPnt(*datumOf(doc, "PCross")->point, gp_Pnt(w, 0, h / 3));
    };
    check(W, D, H);
    // Sketch dimensions change: every point follows its vertex / edge through the element map.
    setVar(doc, "W", 55);
    setVar(doc, "H", 26);
    check(55, D, 26);
    setVar(doc, "D", 18);
    check(55, 18, 26);

    // Errors: the centre of a straight edge, an edge parallel to the plane.
    add(doc, spec("DatumPoint", "Bad", {}, {{"method", "center"}}, {}, {{"edge", {topEdge}}}));
    add(doc, spec("DatumPoint", "Bad2", {}, {{"method", "edgePlane"}, {"base", "XY"}}, {}, {{"edge", {topEdge}}}));
    add(doc, spec("DatumPoint", "Bad3", {}, {{"method", "vertex"}}));
    doc.rebuild();
    EXPECT_EQ(info(doc, "Bad").state, FeatureState::Error);
    EXPECT_NE(messages(info(doc, "Bad")).find("edge is not circular"), std::string::npos) << messages(info(doc, "Bad"));
    EXPECT_NE(messages(info(doc, "Bad2")).find("edge is parallel to the plane"), std::string::npos)
        << messages(info(doc, "Bad2"));
    EXPECT_NE(messages(info(doc, "Bad3")).find("reference 'vertex' must give one vertex, has 0"), std::string::npos)
        << messages(info(doc, "Bad3"));
}

TEST(ModelingDocumentDatum, PlaneThroughThreePointsCarriesASketch) {
    PartDocument doc(partRegistry());
    double W = 40, D = 30, H = 20;
    block(doc, W, D, H);
    doc.rebuild();
    const NamedShape s0 = doc.finalShape();
    // Corner plane through (W, 0, 0), (0, D, 0), (0, 0, H): normal (DH, WH, WD) (normalised),
    // distance from the origin WDH / |(DH, WH, WD)|.
    add(doc, spec("DatumPlane", "Corner", {}, {{"method", "threePoints"}}, {},
                  {{"vertex1", {refTo(s0, vertexAt(s0.shape(), gp_Pnt(W, 0, 0)))}},
                   {"vertex2", {refTo(s0, vertexAt(s0.shape(), gp_Pnt(0, D, 0)))}},
                   {"vertex3", {refTo(s0, vertexAt(s0.shape(), gp_Pnt(0, 0, H)))}}}));
    // Top plane through (W, D, H), (0, D, H), (W, 0, H): normal +Z, X = -X, Y = Z x X = -Y.
    add(doc, spec("DatumPlane", "Top", {}, {{"method", "threePoints"}}, {},
                  {{"vertex1", {refTo(s0, vertexAt(s0.shape(), gp_Pnt(W, D, H)))}},
                   {"vertex2", {refTo(s0, vertexAt(s0.shape(), gp_Pnt(0, D, H)))}},
                   {"vertex3", {refTo(s0, vertexAt(s0.shape(), gp_Pnt(W, 0, H)))}}}));
    // A pad a x b at local (2, 3) on the top plane, h high: volume W D H + a b h.
    const double a = 12, b = 7, h = 9;
    SketchBuilder pad;
    pad.fixedPolygon({{"2", "3"}, {"2 + 12", "3"}, {"2 + 12", "3 + 7"}, {"2", "3 + 7"}});
    add(doc, sketchSpec("PadSketch", "datum", pad.build(), {{"plane", "Top"}}));
    add(doc, extrudeSpec("Pad", "PadSketch", num(h)));

    auto check = [&](double w, double d, double ht) {
        doc.rebuild();
        const gp_Pln corner = *datumOf(doc, "Corner")->plane;
        const gp_Vec n(d * ht, w * ht, w * d);
        expectDir(corner.Axis().Direction(), gp_Dir(n), 1e-12);
        EXPECT_NEAR(corner.Distance(gp::Origin()), w * d * ht / n.Magnitude(), 1e-9);
        expectPnt(corner.Location(), gp_Pnt(w, 0, 0));
        expectDir(corner.XAxis().Direction(), gp_Dir(gp_Vec(gp_Pnt(w, 0, 0), gp_Pnt(0, d, 0))), 1e-12);
        for (const gp_Pnt& p : {gp_Pnt(w, 0, 0), gp_Pnt(0, d, 0), gp_Pnt(0, 0, ht)}) EXPECT_NEAR(corner.Distance(p), 0, 1e-9);

        const gp_Pln top = *datumOf(doc, "Top")->plane;
        expectDir(top.Axis().Direction(), gp_Dir(0, 0, 1));
        expectDir(top.XAxis().Direction(), gp_Dir(-1, 0, 0));
        expectPnt(top.Location(), gp_Pnt(w, d, ht));
        const FeatureInfo p = info(doc, "Pad");
        ASSERT_EQ(p.state, FeatureState::Ok) << messages(p);
        EXPECT_NEAR(volume(doc.finalShape().shape()), w * d * ht + a * b * h, 1e-6);
        // The pad stands on the top face at x in [w - 14, w - 2], y in [d - 10, d - 3].
        EXPECT_EQ(planarFacesWithNormal(doc.finalShape().shape(), gp_Dir(0, 0, 1), ht + h), 1);
        EXPECT_EQ(planarFacesWithNormal(doc.finalShape().shape(), gp_Dir(1, 0, 0), w - 2), 1);
        EXPECT_EQ(planarFacesWithNormal(doc.finalShape().shape(), gp_Dir(0, 1, 0), d - 3), 1);
    };
    check(W, D, H);
    setVar(doc, "W", 52);
    setVar(doc, "H", 14);
    check(52, D, 14);
    setVar(doc, "D", 24);
    check(52, 24, 14);

    // Three datum points instead of vertices; collinear points are an error.
    add(doc, spec("DatumPoint", "P1", {{"x", "0"}, {"y", "0"}, {"z", "5"}}));
    add(doc, spec("DatumPoint", "P2", {{"x", "1"}, {"y", "0"}, {"z", "5"}}));
    add(doc, spec("DatumPoint", "P3", {{"x", "0"}, {"y", "1"}, {"z", "6"}}));
    add(doc, spec("DatumPlane", "ByPoints", {}, {{"method", "threePoints"}}, {{"point1", "P1"}, {"point2", "P2"}, {"point3", "P3"}}));
    add(doc, spec("DatumPoint", "P4", {{"x", "2"}, {"y", "0"}, {"z", "5"}}));
    add(doc, spec("DatumPlane", "Collinear", {}, {{"method", "threePoints"}}, {{"point1", "P1"}, {"point2", "P2"}, {"point3", "P4"}}));
    doc.rebuild();
    expectDir(datumOf(doc, "ByPoints")->plane->Axis().Direction(), gp_Dir(0, -1, 1));
    EXPECT_EQ(info(doc, "Collinear").state, FeatureState::Error);
    EXPECT_NE(messages(info(doc, "Collinear")).find("the three points are collinear"), std::string::npos);
}

TEST(ModelingDocumentDatum, PlaneTangentToCylinder) {
    PartDocument doc(partRegistry());
    setVar(doc, "R", 8);
    SketchBuilder c;
    c.fixedCircle("0", "0", "R");
    add(doc, sketchSpec("Disc", "XY", c.build()));
    add(doc, extrudeSpec("Rod", "Disc", "30"));
    doc.rebuild();
    TopoDS_Shape sideFace;
    for (TopExp_Explorer ex(doc.finalShape().shape(), TopAbs_FACE); ex.More(); ex.Next()) {
        if (BRepAdaptor_Surface(TopoDS::Face(ex.Current())).GetType() == GeomAbs_Cylinder) sideFace = ex.Current();
    }
    ASSERT_FALSE(sideFace.IsNull());
    const TopoReference side = refTo(doc.finalShape(), sideFace);

    // Line x = 20, y = 0 (planes x = 20 and XZ), parallel to the axis: cos(phi) = R / 20.
    add(doc, spec("DatumPlane", "X20", {{"offset", "20"}}, {{"base", "YZ"}}));
    add(doc, spec("DatumPlane", "XZ0", {}, {{"base", "XZ"}}));
    add(doc, spec("DatumAxis", "Line", {}, {{"method", "planes"}}, {{"plane", "X20"}, {"plane2", "XZ0"}}));
    add(doc, spec("DatumPlane", "ThroughLine", {}, {{"method", "tangent"}, {"tangent", "line"}, {"axis", "datum"}},
                  {{"axis", "Line"}}, {{"face", {side}}}));
    add(doc, spec("DatumPlane", "ThroughLine2", {}, {{"method", "tangent"}, {"tangent", "line"}, {"axis", "datum"}, {"flip", "true"}},
                  {{"axis", "Line"}}, {{"face", {side}}}));
    add(doc, spec("DatumPlane", "ParallelX", {}, {{"method", "tangent"}, {"tangent", "parallel"}, {"axis", "X"}}, {},
                  {{"face", {side}}}));
    add(doc, spec("DatumPlane", "At30", {{"angle", "30"}}, {{"method", "tangent"}, {"tangent", "angle"}, {"base", "XZ"}}, {},
                  {{"face", {side}}}));
    // Mirror of the rod in the plane tangent at 30 deg: a second rod touching it.
    add(doc, spec("Mirror", "Twin", {}, {{"what", "body"}, {"plane", "datum"}, {"merge", "false"}}, {{"plane", "At30"}}));

    auto check = [&](double r) {
        doc.rebuild();
        auto tangent = [&](const std::string& id) {
            const gp_Pln p = *datumOf(doc, id)->plane;
            EXPECT_NEAR(p.Distance(gp_Pnt(0, 0, 0)), r, 1e-9) << id;  // distance from the axis = R
            EXPECT_NEAR(p.Distance(gp_Pnt(0, 0, 30)), r, 1e-9) << id;
            return p;
        };
        const double phi = std::acos(r / 20);
        const gp_Pln l1 = tangent("ThroughLine"), l2 = tangent("ThroughLine2");
        expectDir(l1.Axis().Direction(), gp_Dir(std::cos(phi), std::sin(phi), 0));
        expectDir(l2.Axis().Direction(), gp_Dir(std::cos(phi), -std::sin(phi), 0));
        EXPECT_NEAR(l1.Distance(gp_Pnt(20, 0, 7)), 0, 1e-9);
        EXPECT_NEAR(l2.Distance(gp_Pnt(20, 0, 7)), 0, 1e-9);
        const gp_Pln px = tangent("ParallelX");
        expectDir(px.Axis().Direction(), gp_Dir(0, 1, 0));
        const gp_Pln a30 = tangent("At30");
        const double a = M_PI / 6;
        expectDir(a30.Axis().Direction(), gp_Dir(std::sin(a), -std::cos(a), 0));
        const FeatureInfo m = info(doc, "Twin");
        EXPECT_TRUE(m.state == FeatureState::Ok || m.state == FeatureState::Warning) << messages(m);
        EXPECT_NEAR(volume(doc.finalShape().shape()), 2 * kPi * r * r * 30, 1e-6);
        // The copy's axis is the rod axis mirrored: at distance 2R along the tangent normal.
        EXPECT_TRUE(hasCylinder(doc.finalShape().shape(), r, 2, 2 * r * std::sin(a), -2 * r * std::cos(a)));
    };
    check(8);
    setVar(doc, "R", 11);  // the face reference follows the sketch circle
    check(11);
}

TEST(ModelingDocumentDatum, CoordinateSystemsOrientAndCarrySketches) {
    PartDocument doc(partRegistry());
    double W = 40, D = 30, H = 20;
    block(doc, W, D, H);
    doc.rebuild();
    const NamedShape s0 = doc.finalShape();
    const TopoReference corner = refTo(s0, vertexAt(s0.shape(), gp_Pnt(W, 0, H)));
    const TopoReference topFace = refTo(s0, planarFace(s0.shape(), gp_Dir(0, 0, 1), H));
    const TopoReference xEdge = refTo(s0, singleLine(s0.shape(), 0, 0, H));
    const TopoReference yEdge = refTo(s0, singleLine(s0.shape(), 1, W, H));

    // Origin at the corner (W, 0, H), X = +Y, XY plane through +X: Z = Y x X = -Z.
    add(doc, spec("DatumCsys", "Flipped", {}, {{"origin", "vertex"}, {"xDirection", "Y"}, {"xyDirection", "X"}}, {},
                  {{"vertex", {corner}}}));
    // Edges as directions: X along the top edge at y = 0, XY through the top edge at x = W.
    add(doc, spec("DatumCsys", "Edges", {}, {{"origin", "vertex"}}, {}, {{"vertex", {corner}}, {"edge", {xEdge}}, {"edge2", {yEdge}}}));
    // Face + edge with the origin at a datum point in the middle of the top face.
    add(doc, spec("DatumPoint", "Centre", {{"x", "W / 2"}, {"y", "D / 2"}, {"z", "H"}}));
    add(doc, spec("DatumCsys", "OnTop", {}, {{"method", "faceEdge"}, {"origin", "datum"}}, {{"point", "Centre"}},
                  {{"face", {topFace}}, {"edge", {xEdge}}}));
    // A point given in the local system of "Flipped": (1, 2, 3) -> corner + 1 Y + 2 X - 3 Z.
    add(doc, spec("DatumPoint", "Local", {{"x", "1"}, {"y", "2"}, {"z", "3"}}, {}, {{"csys", "Flipped"}}));
    // A sketch on the local system: centred square a x a, extruded h along its Z (+Z).
    const double a = 10, h = 6;
    add(doc, sketchSpec("Square", "datum", centredRectangle(num(a), num(a)), {{"plane", "OnTop"}}));
    add(doc, extrudeSpec("Post", "Square", num(h)));

    auto check = [&](double w, double d, double ht) {
        doc.rebuild();
        const gp_Ax3 f = *datumOf(doc, "Flipped")->csys;
        EXPECT_TRUE(f.Direct());
        expectPnt(f.Location(), gp_Pnt(w, 0, ht));
        expectDir(f.XDirection(), gp_Dir(0, 1, 0));
        expectDir(f.YDirection(), gp_Dir(1, 0, 0));
        expectDir(f.Direction(), gp_Dir(0, 0, -1));
        // A coordinate system is also a plane (XY), an axis (Z) and a point (origin).
        const auto fd = datumOf(doc, "Flipped");
        expectDir(fd->plane->Axis().Direction(), gp_Dir(0, 0, -1));
        expectDir(fd->axis->Direction(), gp_Dir(0, 0, -1));
        expectPnt(*fd->point, gp_Pnt(w, 0, ht));

        const gp_Ax3 e = *datumOf(doc, "Edges")->csys;
        EXPECT_TRUE(e.Direct());
        EXPECT_NEAR(std::abs(e.XDirection().X()), 1, 1e-12);  // along the edge (its orientation)
        EXPECT_NEAR(std::abs(e.Direction().Z()), 1, 1e-12);   // both edges lie in z = H
        EXPECT_NEAR(gp_Vec(e.Direction()).Dot(gp_Vec(e.XDirection()).Crossed(gp_Vec(e.YDirection()))), 1, 1e-12);

        const gp_Ax3 t = *datumOf(doc, "OnTop")->csys;
        EXPECT_TRUE(t.Direct());
        expectPnt(t.Location(), gp_Pnt(w / 2, d / 2, ht));
        expectDir(t.Direction(), gp_Dir(0, 0, 1));
        EXPECT_NEAR(std::abs(t.XDirection().X()), 1, 1e-12);

        expectPnt(*datumOf(doc, "Local")->point, gp_Pnt(w + 2, 1, ht - 3));
        const FeatureInfo p = info(doc, "Post");
        ASSERT_EQ(p.state, FeatureState::Ok) << messages(p);
        EXPECT_NEAR(volume(doc.finalShape().shape()), w * d * ht + a * a * h, 1e-6);
        EXPECT_EQ(planarFacesWithNormal(doc.finalShape().shape(), gp_Dir(0, 0, 1), ht + h), 1);
        EXPECT_EQ(planarFacesWithNormal(doc.finalShape().shape(), gp_Dir(1, 0, 0), w / 2 + a / 2), 1);
    };
    check(W, D, H);
    setVar(doc, "W", 64);
    setVar(doc, "H", 12);
    check(64, D, 12);

    // Parallel directions and an edge normal to the face are errors.
    add(doc, spec("DatumCsys", "Parallel", {}, {{"origin", "coordinates"}, {"xDirection", "X"}, {"xyDirection", "X"}}));
    add(doc, spec("DatumCsys", "Normal", {}, {{"method", "faceEdge"}, {"origin", "coordinates"}}, {},
                  {{"face", {topFace}}, {"edge", {refTo(s0, singleLine(s0.shape(), 2, 0, 0))}}}));
    doc.rebuild();
    EXPECT_NE(messages(info(doc, "Parallel")).find("X and XY-plane directions are parallel"), std::string::npos)
        << messages(info(doc, "Parallel"));
    EXPECT_NE(messages(info(doc, "Normal")).find("edge is normal to the face"), std::string::npos)
        << messages(info(doc, "Normal"));
}

TEST(ModelingDocumentDatum, SaveAndLoad) {
    PartDocument doc(partRegistry());
    block(doc, 40, 30, 20);
    doc.rebuild();
    const NamedShape s0 = doc.finalShape();
    add(doc, spec("DatumPoint", "Mid", {}, {{"method", "midpoint"}}, {}, {{"edge", {refTo(s0, singleLine(s0.shape(), 0, 0, 20))}}}));
    add(doc, spec("DatumPlane", "Top", {}, {{"method", "threePoints"}}, {},
                  {{"vertex1", {refTo(s0, vertexAt(s0.shape(), gp_Pnt(40, 30, 20)))}},
                   {"vertex2", {refTo(s0, vertexAt(s0.shape(), gp_Pnt(0, 30, 20)))}},
                   {"vertex3", {refTo(s0, vertexAt(s0.shape(), gp_Pnt(40, 0, 20)))}}}));
    add(doc, spec("DatumCsys", "Cs", {}, {{"origin", "datum"}, {"xDirection", "Y"}, {"xyDirection", "Z"}}, {{"point", "Mid"}}));
    add(doc, sketchSpec("S", "datum", centredCircle("3"), {{"plane", "Cs"}}));
    add(doc, extrudeSpec("Pin", "S", "4"));
    doc.rebuild();
    const double v0 = volume(doc.finalShape().shape());
    // The pin stands on the plane x = 20 centred on the edge y = 0, z = 20: a quarter is inside.
    EXPECT_NEAR(v0, 40 * 30 * 20 + 0.75 * kPi * 9 * 4, 1e-6);

    for (bool cache : {false, true}) {
        SaveOptions opt;
        opt.includeCache = cache;
        auto json = doc.toJson(opt);
        ASSERT_TRUE(json.ok());
        auto loaded = PartDocument::fromJson(json.value(), partRegistry());
        ASSERT_TRUE(loaded.ok()) << loaded.error().message;
        PartDocument& d2 = loaded.value();
        EXPECT_NEAR(volume(d2.finalShape().shape()), v0, 1e-9);
        expectPnt(*datumOf(d2, "Mid")->point, gp_Pnt(20, 0, 20));
        expectPnt(datumOf(d2, "Cs")->csys->Location(), gp_Pnt(20, 0, 20));
        expectDir(datumOf(d2, "Cs")->csys->Direction(), gp_Dir(1, 0, 0));  // Y x Z
        expectDir(datumOf(d2, "Top")->plane->Axis().Direction(), gp_Dir(0, 0, 1));
        EXPECT_EQ(info(d2, "Top").options.at("method"), "threePoints");
        EXPECT_EQ(info(d2, "Top").references.at("vertex3").size(), 1u);
    }
}
