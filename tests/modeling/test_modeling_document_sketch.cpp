// Sketch nodes of the part document: element names from sketch entities, region selection,
// projection of model edges through the element map, solver errors, revolve about a sketch
// line, datum planes, undo/redo of edits and migration of format v2 prism cuts.

#include "modeling_document_test_util.hpp"

#include <nlohmann/json.hpp>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;

namespace {

FeatureSpec sketchOn(const std::string& id, const std::string& plane, SketchData data) {
    FeatureSpec s;
    s.type = "Sketch";
    s.id = id;
    s.options = {{"plane", plane}};
    s.sketch = std::move(data);
    return s;
}

FeatureSpec extrudeOf(const std::string& id, const std::string& sketch, std::string distance,
                      std::map<std::string, std::string> options = {}) {
    FeatureSpec s;
    s.type = "Extrude";
    s.id = id;
    s.parameters = {{"distance", std::move(distance)}};
    s.options = std::move(options);
    s.links = {{"sketch", sketch}};
    return s;
}

std::string nameOf(const NamedShape& s, ElementKind kind, const TopoDS_Shape& element) {
    auto i = s.indexOf(kind, element);
    EXPECT_TRUE(i.has_value());
    return i ? s.record(kind, *i).name : std::string();
}

}  // namespace

TEST(ModelingDocumentSketch, ExtrusionElementsAreNamedAfterSketchEntities) {
    PartDocument doc(partRegistry());
    setVar(doc, "W", 40);
    setVar(doc, "D", 25);
    SketchBuilder b;
    const auto e = b.rectangle("0", "0", "W", "D");  // bottom, right, top, left
    add(doc, sketchOn("Profile", "XY", b.build()));
    add(doc, extrudeOf("Body", "Profile", "10"));
    doc.rebuild();
    NamedShape s = doc.finalShape();
    ASSERT_FALSE(s.isNull()) << messages(info(doc, "Profile")) << messages(info(doc, "Body"));
    EXPECT_NEAR(volume(s.shape()), 40 * 25 * 10, 1e-6);
    EXPECT_EQ(info(doc, "Profile").sketchResult->dof, 0) << "the rectangle is fully constrained";

    // Side faces are generated from the sketch lines, the caps from the region.
    EXPECT_EQ(nameOf(s, ElementKind::Face, planarFace(s.shape(), gp::DY().Reversed(), 0)), "G[Body](Profile:E1)");
    EXPECT_EQ(nameOf(s, ElementKind::Face, planarFace(s.shape(), gp::DX(), 40)), "G[Body](Profile:E2)");
    EXPECT_EQ(nameOf(s, ElementKind::Face, planarFace(s.shape(), gp::DY(), 25)), "G[Body](Profile:E3)");
    EXPECT_EQ(nameOf(s, ElementKind::Face, planarFace(s.shape(), gp::DZ(), 10)), "G[Body](Profile:R1)");
    EXPECT_EQ(nameOf(s, ElementKind::Face, planarFace(s.shape(), gp::DZ().Reversed(), 0)), "Profile:R1");
    EXPECT_EQ(nameOf(s, ElementKind::Edge, singleLine(s.shape(), 2, 0, 0)), "G[Body](Profile:V1_4)");
    EXPECT_EQ(e.size(), 4u);

    // A fillet on the vertical edge at the sketch origin keeps its reference exactly when the
    // sketch dimensions change.
    add(doc, {"Fillet", "Round", "", {{"radius", "3"}}, {{"edges", {refTo(s, singleLine(s.shape(), 2, 0, 0))}}}, {}, false});
    for (double w : {60.0, 30.0, 45.0}) {
        setVar(doc, "W", w);
        setVar(doc, "D", w / 2);
        doc.rebuild();
        FeatureInfo fi = info(doc, "Round");
        ASSERT_EQ(fi.state, FeatureState::Ok) << messages(fi);
        EXPECT_EQ(fi.referenceReports.at(0).resolution.method, ResolveMethod::Exact);
        EXPECT_NEAR(volume(doc.finalShape().shape()), w * w / 2 * 10 - filletLoss(3, 10), 1e-6);
        EXPECT_TRUE(hasCylinder(doc.finalShape().shape(), 3, 2, 3, 3)) << "fillet stays at the origin corner";
    }
}

TEST(ModelingDocumentSketch, RegionSelection) {
    PartDocument doc(partRegistry());
    SketchBuilder b;
    b.fixedPolygon({{"0", "0"}, {"50", "0"}, {"50", "30"}, {"0", "30"}});
    const SketchEntityId circle = b.fixedCircle("20", "15", "6");
    add(doc, sketchOn("S", "XY", b.build()));
    add(doc, extrudeOf("Plate", "S", "4"));
    doc.rebuild();
    EXPECT_NEAR(volume(doc.finalShape().shape()), (50 * 30 - kPi * 36) * 4, 1e-6) << "material region: plate with hole";

    SketchData only = info(doc, "S").sketch.value();
    only.regions = {circle};
    ASSERT_TRUE(doc.setSketch("S", only).ok());
    doc.rebuild();
    EXPECT_NEAR(volume(doc.finalShape().shape()), kPi * 36 * 4, 1e-6) << "only the disc";
    EXPECT_TRUE(hasCylinder(doc.finalShape().shape(), 6, 2, 20, 15));

    only.regions = {1, circle};  // outer contour (with the hole) and the disc
    ASSERT_TRUE(doc.setSketch("S", only).ok());
    doc.rebuild();
    EXPECT_NEAR(volume(doc.finalShape().shape()), 50 * 30 * 4, 1e-6) << "both regions fill the rectangle";

    only.regions = {99};
    EXPECT_FALSE(doc.setSketch("S", only).ok()) << "no entity 99";
}

TEST(ModelingDocumentSketch, ProjectedEdgeFollowsTheModel) {
    PartDocument doc(partRegistry());
    setVar(doc, "R", 10);
    add(doc, {"Cylinder", "Post", "", {{"radius", "R"}, {"height", "20"}}, {}, {}, false});
    doc.rebuild();
    NamedShape s = doc.finalShape();
    // Sketch on the top face; the top circle of the post is projected and used as the profile.
    SketchBuilder b;
    b.projected(SketchGeometryType::Circle, 0, false);
    FeatureSpec sk = sketchOn("Cap", "face", b.build());
    sk.references["face"] = {refTo(s, planarFace(s.shape(), gp::DZ(), 20))};
    TopoDS_Shape topCircle;
    for (TopExp_Explorer ex(s.shape(), TopAbs_EDGE); ex.More(); ex.Next()) {
        BRepAdaptor_Curve c(TopoDS::Edge(ex.Current()));
        if (c.GetType() == GeomAbs_Circle && std::abs(c.Circle().Location().Z() - 20) < 1e-9) topCircle = ex.Current();
    }
    sk.references["projected"] = {refTo(s, topCircle)};
    add(doc, sk);
    add(doc, extrudeOf("Boss", "Cap", "5"));
    for (double r : {10.0, 14.0, 7.0}) {
        setVar(doc, "R", r);
        RebuildReport rep = doc.rebuild();
        ASSERT_TRUE(rep.failed.empty()) << messages(info(doc, "Cap")) << messages(info(doc, "Boss"));
        EXPECT_EQ(info(doc, "Cap").referenceReports.at(1).resolution.method, ResolveMethod::Exact);
        EXPECT_NEAR(volume(doc.finalShape().shape()), kPi * r * r * 25, 1e-6) << "projection follows R = " << r;
    }
}

TEST(ModelingDocumentSketch, SolverAndDataErrors) {
    PartDocument doc(partRegistry());
    SketchBuilder b;
    const int p0 = b.point("0", "0", true);
    const int p1 = b.point("10", "0");
    b.line(p0, p1);
    b.constrain(SketchConstraintType::Horizontal, p0, p1);
    b.constrain(SketchConstraintType::Vertical, p0, p1);
    b.constrain(SketchConstraintType::Distance, p0, p1, "10");
    add(doc, sketchOn("Bad", "XY", b.build()));
    doc.rebuild();
    FeatureInfo fi = info(doc, "Bad");
    EXPECT_EQ(fi.state, FeatureState::Error);
    EXPECT_NE(messages(fi).find("conflict"), std::string::npos) << messages(fi);

    SketchBuilder u;
    u.fixedCircle("0", "0", "Q");
    add(doc, sketchOn("Unknown", "XY", u.build()));
    doc.rebuild();
    EXPECT_NE(messages(info(doc, "Unknown")).find("unknown variable 'Q'"), std::string::npos);

    SketchData broken;
    broken.geometry.push_back({SketchGeometryType::Line, 0, 1});
    EXPECT_FALSE(doc.addFeature(sketchOn("NoPoints", "XY", broken)).ok()) << "line without points";
    EXPECT_FALSE(doc.addFeature({"Box", "", "", {}, {}, {}, false, {}, SketchData{}}).ok()) << "Box takes no sketch";

    // An extrusion of a sketch without a closed profile.
    SketchBuilder pts;
    pts.point("1", "1", true);
    add(doc, sketchOn("Points", "XY", pts.build()));
    add(doc, extrudeOf("Nothing", "Points", "5"));
    doc.rebuild();
    EXPECT_NE(messages(info(doc, "Nothing")).find("has no closed profile"), std::string::npos)
        << messages(info(doc, "Nothing"));
}

TEST(ModelingDocumentSketch, RevolveAboutSketchLineAndDatumPlane) {
    PartDocument doc(partRegistry());
    SketchBuilder b;
    b.fixedPolygon({{"5", "0"}, {"15", "0"}, {"15", "8"}, {"5", "8"}});
    const int a0 = b.point("0", "0", true);
    const int a1 = b.point("0", "1", true);
    const SketchEntityId axis = b.line(a0, a1, true);
    add(doc, sketchOn("Section", "XZ", b.build()));
    add(doc, {"Revolve", "Ring", "", {{"axisEntity", std::to_string(axis)}}, {}, {{"axis", "entity"}}, false,
              {{"sketch", "Section"}}});
    doc.rebuild();
    EXPECT_NEAR(volume(doc.finalShape().shape()), kPi * (15 * 15 - 5 * 5) * 8, 1e-6) << messages(info(doc, "Ring"));

    // A datum plane 8 mm above XY carries a sketch whose extrusion stands on the ring.
    add(doc, {"DatumPlane", "Top", "", {{"offset", "8"}}, {}, {{"base", "XY"}}, false});
    SketchBuilder c;
    c.fixedCircle("10", "0", "2");
    FeatureSpec sk = sketchOn("PinSketch", "datum", c.build());
    sk.links = {{"plane", "Top"}};
    add(doc, sk);
    add(doc, extrudeOf("Pin", "PinSketch", "6"));
    doc.rebuild();
    EXPECT_NEAR(volume(doc.finalShape().shape()), kPi * (15 * 15 - 5 * 5) * 8 + kPi * 4 * 6, 1e-6)
        << messages(info(doc, "Pin"));
    EXPECT_TRUE(hasCylinder(doc.finalShape().shape(), 2, 2, 10, 0));

    // Links must point upstream.
    ASSERT_TRUE(doc.moveFeature("Top", doc.size() - 1).ok());
    doc.rebuild();
    EXPECT_NE(messages(info(doc, "PinSketch")).find("comes after this feature"), std::string::npos)
        << messages(info(doc, "PinSketch"));
}

TEST(ModelingDocumentSketch, UndoRedoOfEdits) {
    PartDocument doc(partRegistry());
    setVar(doc, "W", 30);
    SketchBuilder b;
    b.rectangle("0", "0", "W", "20");
    add(doc, sketchOn("S", "XY", b.build()));
    add(doc, extrudeOf("E", "S", "10"));
    doc.rebuild();
    const double v0 = volume(doc.finalShape().shape());
    EXPECT_EQ(doc.undoHistory().size(), 3u) << "variable, sketch, extrusion";

    ASSERT_TRUE(doc.setParameter("E", "distance", "15").ok());
    doc.rebuild();
    EXPECT_NEAR(volume(doc.finalShape().shape()), 30 * 20 * 15, 1e-6);
    ASSERT_TRUE(doc.setSuppressed("E", true).ok());
    doc.rebuild();
    EXPECT_TRUE(doc.finalShape().isNull());

    ASSERT_TRUE(doc.undo().ok());
    EXPECT_NEAR(volume(doc.finalShape().shape()), 30 * 20 * 15, 1e-6);
    ASSERT_TRUE(doc.undo().ok());
    EXPECT_EQ(volume(doc.finalShape().shape()), v0);
    EXPECT_EQ(doc.redoHistory().size(), 2u);
    ASSERT_TRUE(doc.redo().ok());
    EXPECT_NEAR(volume(doc.finalShape().shape()), 30 * 20 * 15, 1e-6);

    // A sketch edit grouped with a variable change is one entry; abort restores the state.
    ASSERT_TRUE(doc.beginTransaction("resize").ok());
    EXPECT_FALSE(doc.beginTransaction("nested").ok());
    setVar(doc, "W", 50);
    SketchData sd = info(doc, "S").sketch.value();
    sd.constraints.back().value = "30";
    ASSERT_TRUE(doc.setSketch("S", sd).ok());
    EXPECT_FALSE(doc.undo().ok()) << "transaction open";
    ASSERT_TRUE(doc.commitTransaction().ok());
    doc.rebuild();
    EXPECT_NEAR(volume(doc.finalShape().shape()), 50 * 30 * 15, 1e-6);
    EXPECT_EQ(doc.undoHistory().back().name, "resize");
    EXPECT_EQ(doc.undoHistory().back().edits, 2u);
    ASSERT_TRUE(doc.undo().ok());
    EXPECT_NEAR(volume(doc.finalShape().shape()), 30 * 20 * 15, 1e-6);

    ASSERT_TRUE(doc.beginTransaction("discard").ok());
    ASSERT_TRUE(doc.removeFeature("E").ok());
    ASSERT_TRUE(doc.abortTransaction().ok());
    EXPECT_EQ(doc.featureIds(), (std::vector<std::string>{"S", "E"}));
    doc.rebuild();
    EXPECT_NEAR(volume(doc.finalShape().shape()), 30 * 20 * 15, 1e-6);
    EXPECT_TRUE(doc.canRedo()) << "abort does not touch the redo history";
}

TEST(ModelingDocumentSketch, MigratesVersion2PrismCut) {
    // Version 2 document: box, rectangular prism cut on the top face, fillet on the pocket rim.
    const char* v2 = R"json({
      "format": "bendcore-part", "version": 2,
      "variables": [{"name": "H", "expression": "20"}],
      "rollback": null,
      "features": [
        {"id": "Body", "type": "Box", "parameters": {"length": "40", "width": "30", "height": "H"}},
        {"id": "Pocket", "type": "PrismCut", "parameters": {"centerX": "20", "centerY": "15", "sizeU": "10",
          "sizeV": "8", "depth": "6"},
         "references": {"face": [{"name": "Body:Top", "kind": "face", "signature": null}]}},
        {"id": "Rim", "type": "Fillet", "parameters": {"radius": "1"},
         "references": {"edges": [{"name": "E(Body:Top|Pocket:Side+V)", "kind": "edge",
           "signature": {"geometry": "Line", "center": [20, 19, 20], "direction": [1, 0, 0], "size": 10}}]}}
      ]})json";
    RebuildReport rep;
    auto doc = PartDocument::fromJson(v2, partRegistry(), &rep);
    ASSERT_TRUE(doc.ok()) << doc.error().message;
    EXPECT_EQ(doc.value().featureIds(), (std::vector<std::string>{"Body", "PocketSketch", "Pocket", "Rim"}));
    EXPECT_TRUE(rep.failed.empty());
    FeatureInfo rim = info(doc.value(), "Rim");
    // The rim edge is now named after the sketch line ("PocketSketch:E3"): the reference is
    // recovered by its stored signature once, with a warning, and follows the new name.
    EXPECT_EQ(rim.state, FeatureState::Warning) << messages(rim);
    EXPECT_EQ(rim.referenceReports.at(0).resolution.method, ResolveMethod::Signature);
    EXPECT_EQ(rim.references.at("edges").at(0).name, "PocketSketch:E3");
    EXPECT_NEAR(volume(doc.value().finalShape().shape()), 40 * 30 * 20 - 10 * 8 * 6 - filletLoss(1, 10), 1e-6);
    EXPECT_TRUE(hasCylinder(doc.value().finalShape().shape(), 1, 0, 20, 19)) << "fillet on the +Y rim edge";
    auto saved = nlohmann::json::parse(doc.value().toJson().value());
    EXPECT_EQ(saved.at("version"), 3);
    EXPECT_EQ(saved.at("features").at(2).at("links").at("sketch"), "PocketSketch");

    // The migrated model is parametric.
    setVar(doc.value(), "H", 26);
    doc.value().rebuild();
    EXPECT_EQ(info(doc.value(), "Rim").state, FeatureState::Ok) << messages(info(doc.value(), "Rim"));
    EXPECT_NEAR(volume(doc.value().finalShape().shape()), 40 * 30 * 26 - 10 * 8 * 6 - filletLoss(1, 10), 1e-6);
}
