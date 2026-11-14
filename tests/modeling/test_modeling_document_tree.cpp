// Feature tree scenarios: dimension change, upstream insertion, deleted reference,
// reordering, suppression, rollback, error isolation, variables, signature fallback.

#include "modeling_document_test_util.hpp"

#include <algorithm>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;

namespace {

bool contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

// Box L x W x H (variables L, W, H) with a fillet of radius R on the top-front edge.
struct BoxFillet {
    PartDocument doc{partRegistry()};
    std::string box, fillet;

    BoxFillet(double l, double w, double h, double r) {
        setVar(doc, "L", l);
        setVar(doc, "W", w);
        setVar(doc, "H", h);
        setVar(doc, "R", r);
        box = add(doc, {"Box", "", "", {{"length", "L"}, {"width", "W"}, {"height", "H"}}, {}, {}, false});
        doc.rebuild();
        const NamedShape s = doc.finalShape();
        TopoReference edge = refTo(s, singleLine(s.shape(), 0, 0, h));
        fillet = add(doc, {"Fillet", "", "", {{"radius", "R"}}, {{"edges", {edge}}}, {}, false});
    }
};

// Rectangular pocket a x b centred at (cx, cy) on the top face z = h: sketch + cut extrusion.
SketchCut pocketOnTop(PartDocument& doc, const NamedShape& s, double h, double cx, double cy, double a, double b,
                      double depth, std::optional<std::size_t> pos = std::nullopt, const std::string& id = "Pocket") {
    return addSketchCut(doc, id, refTo(s, planarFace(s.shape(), gp::DZ(), h)), num(cx), num(cy), num(h),
                        centredRectangle(num(a), num(b)), num(depth), pos);
}

}  // namespace

TEST(ModelingDocumentTree, HeightChangeKeepsFilletOnTopEdge) {
    BoxFillet m(40, 30, 20, 3);
    RebuildReport rep = m.doc.rebuild();
    EXPECT_TRUE(rep.failed.empty());
    EXPECT_EQ(info(m.doc, m.fillet).state, FeatureState::Ok) << messages(info(m.doc, m.fillet));
    TopoDS_Shape s = m.doc.finalShape().shape();
    EXPECT_NEAR(volume(s), 40 * 30 * 20 - filletLoss(3, 40), 1e-6);
    EXPECT_TRUE(hasCylinder(s, 3, 0, 3, 17)) << "fillet axis at y = r, z = H - r";

    for (double h : {35.0, 12.0, 50.0}) {
        setVar(m.doc, "H", h);
        rep = m.doc.rebuild();
        FeatureInfo fi = info(m.doc, m.fillet);
        EXPECT_EQ(fi.state, FeatureState::Ok) << messages(fi);
        ASSERT_EQ(fi.referenceReports.size(), 1u);
        EXPECT_EQ(fi.referenceReports[0].resolution.method, ResolveMethod::Exact);
        s = m.doc.finalShape().shape();
        EXPECT_NEAR(volume(s), 40 * 30 * h - filletLoss(3, 40), 1e-6) << "H = " << h;
        EXPECT_TRUE(hasCylinder(s, 3, 0, 3, h - 3)) << "fillet stays on the top edge at z = " << h;
        // The rounded edge ends: tangent lines at z = H (top) and y = 0 (front).
        EXPECT_EQ(linesAlong(s, 0, 3, h).size(), 1u);
        EXPECT_EQ(linesAlong(s, 0, 0, h - 3).size(), 1u);
        EXPECT_TRUE(contains(rep.recomputed, m.box));
    }
}

TEST(ModelingDocumentTree, RebuildStartsAtTheChangedFeature) {
    BoxFillet m(40, 30, 20, 3);
    m.doc.rebuild();
    RebuildReport idle = m.doc.rebuild();
    EXPECT_TRUE(idle.recomputed.empty()) << "nothing changed";
    EXPECT_EQ(idle.reused.size(), 2u);

    setVar(m.doc, "R", 4);
    RebuildReport rep = m.doc.rebuild();
    EXPECT_TRUE(contains(rep.reused, m.box)) << "box does not depend on R";
    EXPECT_TRUE(contains(rep.recomputed, m.fillet));
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), 40 * 30 * 20 - filletLoss(4, 40), 1e-6);

    ASSERT_TRUE(m.doc.setParameter(m.box, "length", "L + 10").ok());
    rep = m.doc.rebuild();
    EXPECT_TRUE(contains(rep.recomputed, m.box));
    EXPECT_TRUE(contains(rep.recomputed, m.fillet)) << "downstream of the box";
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), 50 * 30 * 20 - filletLoss(4, 50), 1e-6);
}

TEST(ModelingDocumentTree, CutInsertedUpstreamKeepsReferences) {
    BoxFillet m(40, 30, 20, 3);
    m.doc.rebuild();
    NamedShape boxShape = info(m.doc, m.box).output ? *info(m.doc, m.box).output : NamedShape{};
    SketchCut pocket = pocketOnTop(m.doc, boxShape, 20, 20, 15, 10, 8, 6, 1);
    RebuildReport rep = m.doc.rebuild();
    EXPECT_TRUE(rep.failed.empty());
    EXPECT_EQ(m.doc.featureIds(), (std::vector<std::string>{m.box, pocket.sketch, pocket.cut, m.fillet}));
    FeatureInfo fi = info(m.doc, m.fillet);
    EXPECT_EQ(fi.state, FeatureState::Ok) << messages(fi);
    EXPECT_EQ(fi.referenceReports.at(0).resolution.method, ResolveMethod::Exact);
    TopoDS_Shape s = m.doc.finalShape().shape();
    EXPECT_NEAR(volume(s), 40 * 30 * 20 - 10 * 8 * 6 - filletLoss(3, 40), 1e-6);
    EXPECT_TRUE(hasCylinder(s, 3, 0, 3, 17));
    EXPECT_EQ(planarFacesWithNormal(s, gp::DZ(), 14), 1) << "pocket floor at z = 20 - 6";

    // The pocket follows the top face when the height changes.
    setVar(m.doc, "H", 26);
    m.doc.rebuild();
    s = m.doc.finalShape().shape();
    EXPECT_NEAR(volume(s), 40 * 30 * 26 - 10 * 8 * 6 - filletLoss(3, 40), 1e-6);
    EXPECT_EQ(planarFacesWithNormal(s, gp::DZ(), 20), 1);
}

TEST(ModelingDocumentTree, NotchSplitsReferencedEdgeAndFilletFollowsBothParts) {
    BoxFillet m(40, 30, 20, 3);
    m.doc.rebuild();
    NamedShape boxShape = *info(m.doc, m.box).output;
    // Notch 10 wide (x), across the front edge (y in [-5, 5]), 6 deep.
    std::string notch = pocketOnTop(m.doc, boxShape, 20, 20, 0, 10, 10, 6, 1, "Notch").cut;
    m.doc.rebuild();
    EXPECT_EQ(info(m.doc, notch).state, FeatureState::Ok) << messages(info(m.doc, notch));
    FeatureInfo fi = info(m.doc, m.fillet);
    EXPECT_EQ(fi.state, FeatureState::Ok) << messages(fi);
    ASSERT_EQ(fi.referenceReports.size(), 1u);
    EXPECT_EQ(fi.referenceReports[0].resolution.method, ResolveMethod::History);
    EXPECT_EQ(fi.referenceReports[0].resolution.indices.size(), 2u) << fi.referenceReports[0].resolution.detail;
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), 40 * 30 * 20 - 10 * 5 * 6 - filletLoss(3, 30), 1e-6);
}

TEST(ModelingDocumentTree, DeletedReferencedEdgeBreaksOnlyThatFeature) {
    BoxFillet m(40, 30, 20, 3);
    m.doc.rebuild();
    NamedShape boxShape = *info(m.doc, m.box).output;
    // A later pocket on the top face, independent of the fillet.
    std::string pocket = pocketOnTop(m.doc, boxShape, 20, 20, 15, 10, 8, 6).cut;
    // A chamfer inserted before the fillet consumes the edge the fillet refers to.
    TopoReference edge = refTo(boxShape, singleLine(boxShape.shape(), 0, 0, 20));
    std::string chamfer = add(m.doc, {"Chamfer", "", "", {{"distance", "2"}}, {{"edges", {edge}}}, {}, false}, 1);
    RebuildReport rep = m.doc.rebuild();

    FeatureInfo fi = info(m.doc, m.fillet);
    EXPECT_EQ(fi.state, FeatureState::Error);
    EXPECT_TRUE(contains(rep.failed, m.fillet));
    ASSERT_FALSE(fi.messages.empty());
    EXPECT_NE(fi.messages[0].find("broken reference edges[0] 'E(" + m.box + ":Front|" + m.box + ":Top)'"),
              std::string::npos)
        << fi.messages[0];
    EXPECT_NE(fi.messages[0].find("removed by feature '" + chamfer + "'"), std::string::npos) << fi.messages[0];
    EXPECT_EQ(fi.output, nullptr);

    // The tree continues with the last valid shape (the chamfered box).
    EXPECT_EQ(info(m.doc, chamfer).state, FeatureState::Ok);
    EXPECT_EQ(info(m.doc, pocket).state, FeatureState::Ok) << messages(info(m.doc, pocket));
    TopoDS_Shape s = m.doc.finalShape().shape();
    EXPECT_NEAR(volume(s), 40 * 30 * 20 - chamferLoss(2, 40) - 10 * 8 * 6, 1e-6);
    EXPECT_EQ(planarFacesWithNormal(s, gp_Dir(0, -1, 1), (20 - 2) / std::sqrt(2.0)), 1) << "chamfer face";

    // Removing the chamfer repairs the fillet.
    ASSERT_TRUE(m.doc.removeFeature(chamfer).ok());
    m.doc.rebuild();
    EXPECT_EQ(info(m.doc, m.fillet).state, FeatureState::Ok);
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), 40 * 30 * 20 - filletLoss(3, 40) - 10 * 8 * 6, 1e-6);
}

TEST(ModelingDocumentTree, ReorderingFeatures) {
    BoxFillet m(40, 30, 20, 3);
    m.doc.rebuild();
    NamedShape boxShape = *info(m.doc, m.box).output;
    SketchCut pocket = pocketOnTop(m.doc, boxShape, 20, 20, 15, 10, 8, 6);
    m.doc.rebuild();
    const double expected = 40 * 30 * 20 - filletLoss(3, 40) - 10 * 8 * 6;
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), expected, 1e-6);

    ASSERT_TRUE(m.doc.moveFeature(pocket.sketch, 1).ok());
    ASSERT_TRUE(m.doc.moveFeature(pocket.cut, 2).ok());
    RebuildReport rep = m.doc.rebuild();
    EXPECT_EQ(m.doc.featureIds(), (std::vector<std::string>{m.box, pocket.sketch, pocket.cut, m.fillet}));
    EXPECT_TRUE(contains(rep.reused, m.box));
    EXPECT_TRUE(rep.failed.empty());
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), expected, 1e-6);
    EXPECT_EQ(info(m.doc, m.fillet).referenceReports.at(0).resolution.method, ResolveMethod::Exact);

    // The fillet before the box has nothing to work on; the others still build.
    ASSERT_TRUE(m.doc.moveFeature(m.fillet, 0).ok());
    m.doc.rebuild();
    FeatureInfo fi = info(m.doc, m.fillet);
    EXPECT_EQ(fi.state, FeatureState::Error);
    EXPECT_NE(messages(fi).find("there is no shape before this feature"), std::string::npos) << messages(fi);
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), 40 * 30 * 20 - 10 * 8 * 6, 1e-6);

    EXPECT_FALSE(m.doc.moveFeature(m.fillet, 7).ok());
    EXPECT_FALSE(m.doc.moveFeature("Nope", 0).ok());
}

TEST(ModelingDocumentTree, SuppressionPassesTheShapeThrough) {
    BoxFillet m(40, 30, 20, 3);
    m.doc.rebuild();
    NamedShape boxShape = *info(m.doc, m.box).output;
    std::string pocket = pocketOnTop(m.doc, boxShape, 20, 20, 15, 10, 8, 6, 1).cut;
    m.doc.rebuild();
    ASSERT_TRUE(m.doc.setSuppressed(pocket, true).ok());
    m.doc.rebuild();
    EXPECT_EQ(info(m.doc, pocket).state, FeatureState::Suppressed);
    EXPECT_EQ(info(m.doc, m.fillet).state, FeatureState::Ok);
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), 40 * 30 * 20 - filletLoss(3, 40), 1e-6);
    ASSERT_TRUE(m.doc.setSuppressed(pocket, false).ok());
    m.doc.rebuild();
    EXPECT_EQ(info(m.doc, pocket).state, FeatureState::Ok);
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), 40 * 30 * 20 - filletLoss(3, 40) - 10 * 8 * 6, 1e-6);
}

TEST(ModelingDocumentTree, RollbackMarker) {
    BoxFillet m(40, 30, 20, 3);
    m.doc.rebuild();
    ASSERT_TRUE(m.doc.setRollback(1).ok());
    m.doc.rebuild();
    EXPECT_EQ(info(m.doc, m.fillet).state, FeatureState::RolledBack);
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), 40 * 30 * 20, 1e-6);

    // New features go to the marker, which moves after them.
    NamedShape boxShape = m.doc.finalShape();
    SketchCut pocket = pocketOnTop(m.doc, boxShape, 20, 20, 15, 10, 8, 6);
    EXPECT_EQ(m.doc.featureIds(), (std::vector<std::string>{m.box, pocket.sketch, pocket.cut, m.fillet}));
    EXPECT_EQ(m.doc.rollback(), std::optional<std::size_t>(3));
    m.doc.rebuild();
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), 40 * 30 * 20 - 10 * 8 * 6, 1e-6);

    ASSERT_TRUE(m.doc.setRollback(std::nullopt).ok());
    m.doc.rebuild();
    EXPECT_EQ(info(m.doc, m.fillet).state, FeatureState::Ok);
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), 40 * 30 * 20 - 10 * 8 * 6 - filletLoss(3, 40), 1e-6);
    EXPECT_FALSE(m.doc.setRollback(9).ok());
}

TEST(ModelingDocumentTree, FailingFeatureDoesNotBreakTheTree) {
    BoxFillet m(40, 30, 20, 3);
    m.doc.rebuild();
    NamedShape boxShape = *info(m.doc, m.box).output;
    std::string pocket = pocketOnTop(m.doc, boxShape, 20, 20, 15, 10, 8, 6).cut;
    setVar(m.doc, "R", 45);  // larger than the box
    RebuildReport rep = m.doc.rebuild();
    FeatureInfo fi = info(m.doc, m.fillet);
    EXPECT_EQ(fi.state, FeatureState::Error);
    EXPECT_FALSE(fi.messages.empty());
    EXPECT_TRUE(contains(rep.failed, m.fillet));
    EXPECT_EQ(info(m.doc, pocket).state, FeatureState::Ok);
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), 40 * 30 * 20 - 10 * 8 * 6, 1e-6);

    setVar(m.doc, "R", 2);
    m.doc.rebuild();
    EXPECT_EQ(info(m.doc, m.fillet).state, FeatureState::Ok);
    EXPECT_NEAR(volume(m.doc.finalShape().shape()), 40 * 30 * 20 - 10 * 8 * 6 - filletLoss(2, 40), 1e-6);
}

TEST(ModelingDocumentTree, VariablesAndExpressions) {
    PartDocument doc(partRegistry());
    ASSERT_TRUE(doc.setVariable("W", "30").ok());
    ASSERT_TRUE(doc.setVariable("L", "2 * W - 5").ok());
    ASSERT_TRUE(doc.setVariable("H", "sqrt(W * 4 + 4) - 1").ok());  // sqrt(124) - 1
    std::string box = add(doc, {"Box", "", "", {{"length", "L"}, {"width", "W"}, {"height", "H"}}, {}, {}, false});
    doc.rebuild();
    const double h = std::sqrt(124.0) - 1;
    EXPECT_NEAR(doc.variableValue("L").value(), 55, 1e-12);
    EXPECT_NEAR(volume(doc.finalShape().shape()), 55 * 30 * h, 1e-6);
    EXPECT_NEAR(info(doc, box).values.at("height"), h, 1e-12);

    // A cycle is reported on the variables and on the feature using them.
    ASSERT_TRUE(doc.setVariable("W", "L / 2").ok());
    doc.rebuild();
    EXPECT_FALSE(doc.variableValue("W").ok());
    EXPECT_NE(doc.variableValue("W").error().message.find("cyclic"), std::string::npos);
    FeatureInfo fi = info(doc, box);
    EXPECT_EQ(fi.state, FeatureState::Error);
    EXPECT_NE(messages(fi).find("cyclic"), std::string::npos) << messages(fi);
    EXPECT_TRUE(doc.finalShape().isNull());

    // Syntax errors and unknown names are rejected at edit time / at rebuild.
    EXPECT_FALSE(doc.setVariable("W", "3 +* 4").ok());
    EXPECT_FALSE(doc.setVariable("1bad", "3").ok());
    EXPECT_FALSE(doc.setParameter(box, "nonexistent", "1").ok());
    ASSERT_TRUE(doc.setVariable("W", "30").ok());
    ASSERT_TRUE(doc.setParameter(box, "height", "Q + 1").ok());
    doc.rebuild();
    EXPECT_NE(messages(info(doc, box)).find("unknown variable 'Q'"), std::string::npos);
    ASSERT_TRUE(doc.setParameter(box, "height", "-3").ok());
    doc.rebuild();
    EXPECT_NE(messages(info(doc, box)).find("must be positive"), std::string::npos);
}

TEST(ModelingDocumentTree, ReplacedBaseFeatureResolvesBySignatureWithWarning) {
    PartDocument doc(partRegistry());
    std::string box = add(doc, {"Box", "", "", {{"length", "40"}, {"width", "30"}, {"height", "20"}}, {}, {}, false});
    doc.rebuild();
    SketchCut pocket = pocketOnTop(doc, doc.finalShape(), 20, 20, 15, 10, 8, 6);
    doc.rebuild();
    ASSERT_EQ(info(doc, pocket.cut).state, FeatureState::Ok);

    // The user deletes the box and creates a new one: all names change.
    ASSERT_TRUE(doc.removeFeature(box).ok());
    std::string box2 = add(doc, {"Box", "Base", "", {{"length", "40"}, {"width", "30"}, {"height", "22"}}, {}, {}, false}, 0);
    doc.rebuild();
    // The sketch on the top face follows the new face by its signature.
    FeatureInfo fi = info(doc, pocket.sketch);
    EXPECT_EQ(fi.state, FeatureState::Warning) << messages(fi);
    EXPECT_EQ(fi.referenceReports.at(0).resolution.method, ResolveMethod::Signature);
    EXPECT_EQ(fi.references.at("face").at(0).name, "Base:Top") << "the reference now follows the new face";
    EXPECT_NEAR(volume(doc.finalShape().shape()), 40 * 30 * 22 - 10 * 8 * 6, 1e-6);
    EXPECT_EQ(planarFacesWithNormal(doc.finalShape().shape(), gp::DZ(), 16), 1);

    // Next rebuild resolves exactly and the result is reused.
    RebuildReport rep = doc.rebuild();
    EXPECT_TRUE(rep.recomputed.empty());
    ASSERT_TRUE(doc.setParameter(box2, "height", "25").ok());
    doc.rebuild();
    EXPECT_EQ(info(doc, pocket.sketch).state, FeatureState::Ok);
    EXPECT_EQ(info(doc, pocket.sketch).referenceReports.at(0).resolution.method, ResolveMethod::Exact);
    EXPECT_EQ(info(doc, pocket.cut).state, FeatureState::Ok);
}

TEST(ModelingDocumentTree, EditingErrors) {
    PartDocument doc(partRegistry());
    EXPECT_EQ(doc.addFeature({"Helix", "", "", {}, {}, {}, false}).error().code, ErrorCode::Unsupported);
    EXPECT_FALSE(doc.addFeature({"Box", "", "", {{"depth", "1"}}, {}, {}, false}).ok());
    EXPECT_FALSE(doc.addFeature({"Extrude", "", "", {}, {}, {{"extent", "hexagon"}}, false}).ok());
    std::string id = add(doc, {"Box", "", "", {}, {}, {}, false});
    EXPECT_EQ(id, "Box001");
    EXPECT_FALSE(doc.addFeature({"Box", "Box001", "", {}, {}, {}, false}).ok()) << "duplicate id";
    EXPECT_FALSE(doc.setOption(id, "profile", "circle").ok());
    EXPECT_FALSE(doc.setReferences(id, "edges", {}).ok());
    EXPECT_FALSE(doc.addFeature({"Extrude", "", "", {}, {}, {}, false, {{"nolink", "Box001"}}}).ok());
    EXPECT_FALSE(doc.setLink(id, "sketch", "Box001").ok()) << "Box has no link slots";
    // A hole without its face reference fails with an explanation.
    std::string hole = add(doc, {"Hole", "", "", {}, {}, {}, false});
    // An extrusion without a sketch link fails too.
    std::string ext = add(doc, {"Extrude", "", "", {}, {}, {}, false});
    // ... and with a link to a feature that is not a sketch.
    std::string ext2 = add(doc, {"Extrude", "", "", {}, {}, {}, false, {{"sketch", "Box001"}}});
    doc.rebuild();
    EXPECT_NE(messages(info(doc, hole)).find("needs exactly one face"), std::string::npos) << messages(info(doc, hole));
    EXPECT_NE(messages(info(doc, ext)).find("link 'sketch' is not set"), std::string::npos) << messages(info(doc, ext));
    EXPECT_NE(messages(info(doc, ext2)).find("is a Box, expected one of Sketch"), std::string::npos)
        << messages(info(doc, ext2));
}
