// Multibody part in the document: a new body by extrusion, list of bodies with names and volumes,
// boolean between bodies, pattern/mirror and deletion of bodies, a sweep along model edges as a
// new body. Body references are face references (makeBodyReference); after dimension changes
// every body node keeps its target and the volumes per body match the formulas.

#include "modeling_document_test_util.hpp"

#include <map>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;

namespace {

std::map<std::string, double> volumes(const PartDocument& doc) {
    std::map<std::string, double> out;
    for (const auto& b : doc.bodies()) out[b.name] = b.volume;
    return out;
}

FeatureSpec sketchXY(const std::string& id, SketchData data, const std::string& offset = "0") {
    FeatureSpec s;
    s.type = "Sketch";
    s.id = id;
    s.options = {{"plane", "XY"}};
    s.parameters = {{"offset", offset}};
    s.sketch = std::move(data);
    return s;
}

SketchData rectangle(const std::string& x, const std::string& y, const std::string& w, const std::string& h) {
    SketchBuilder b;
    b.rectangle(x, y, w, h);
    return b.build();
}

}  // namespace

TEST(DocumentMultibody, BodiesBooleansAndCopiesSurviveDimensionChanges) {
    PartDocument doc(partRegistry());
    setVar(doc, "L", 40);
    setVar(doc, "S", 30);
    // Body "Plate": L × 20 × 10. Body "Peg": 10 × 10 × 20, overlapping the plate by 5 in x.
    add(doc, {"Box", "Plate", "", {{"length", "L"}, {"width", "20"}, {"height", "10"}}, {}, {}, false});
    add(doc, sketchXY("PegSketch", rectangle("L - 5", "5", "10", "10")));
    add(doc, {"Extrude", "Peg", "", {{"distance", "20"}}, {}, {{"mode", "newBody"}}, false, {{"sketch", "PegSketch"}}});
    doc.rebuild();
    ASSERT_EQ(info(doc, "Peg").state, FeatureState::Ok) << messages(info(doc, "Peg"));
    auto v = volumes(doc);
    ASSERT_EQ(v.size(), 2u);
    EXPECT_NEAR(v["Plate"], 40 * 20 * 10, 1e-6);
    EXPECT_NEAR(v["Peg"], 10 * 10 * 20, 1e-6);

    NamedShape s = doc.finalShape();
    const TopoReference plateRef = makeBodyReference(s, "Plate").value();
    const TopoReference pegRef = makeBodyReference(s, "Peg").value();
    EXPECT_FALSE(makeBodyReference(s, "Nothing").ok());
    // Plate minus peg, the peg stays a body.
    add(doc, {"BodyBoolean", "Notch", "", {}, {{"target", {plateRef}}, {"tools", {pegRef}}},
              {{"operation", "subtract"}, {"keepTools", "true"}}, false});
    doc.rebuild();
    ASSERT_EQ(info(doc, "Notch").state, FeatureState::Ok) << messages(info(doc, "Notch"));
    s = doc.finalShape();
    // Two copies of the peg along +Y at spacing S, as new bodies.
    add(doc, {"BodyLinearPattern", "Pegs", "", {{"count1", "3"}, {"spacing1", "S"}},
              {{"bodies", {makeBodyReference(s, "Peg").value()}}}, {{"direction1", "Y"}}, false});
    // Mirror of the plate about x = 0 (YZ), merged with it: a plate 2L long with two notches.
    add(doc, {"BodyMirror", "Twin", "", {}, {{"bodies", {makeBodyReference(s, "Plate").value()}}},
              {{"plane", "YZ"}, {"merge", "true"}}, false});
    doc.rebuild();
    for (const char* id : {"Pegs", "Twin"}) ASSERT_EQ(info(doc, id).state, FeatureState::Ok) << id << messages(info(doc, id));
    s = doc.finalShape();
    // Delete the last peg copy (furthest along +Y).
    TopoDS_Shape far;
    for (std::size_t i = 0; i < s.bodyCount(); ++i)
        if (s.bodyName(i).rfind("Pegs", 0) == 0) {
            GProp_GProps g;
            BRepGProp::VolumeProperties(s.body(i), g);
            if (g.CentreOfMass().Y() > 60) far = s.body(i);
        }
    ASSERT_FALSE(far.IsNull());
    TopExp_Explorer fx(far, TopAbs_FACE);
    add(doc, {"DeleteBody", "Drop", "", {}, {{"bodies", {refTo(s, fx.Current())}}}, {}, false});

    int scenarios = 0, checked = 0, survived = 0;
    for (const auto& [L, S] : std::vector<std::pair<double, double>>{{40, 30}, {60, 30}, {50, 45}, {35, 25}, {40, 30}}) {
        setVar(doc, "L", L);
        setVar(doc, "S", S);
        doc.rebuild();
        ++scenarios;
        for (const char* id : {"Notch", "Pegs", "Twin", "Drop"}) {
            ++checked;
            if (info(doc, id).state == FeatureState::Ok) ++survived;
            else ADD_FAILURE() << id << ": " << messages(info(doc, id));
        }
        v = volumes(doc);
        ASSERT_EQ(v.size(), 3u) << "Plate, Peg, Pegs;B1";
        const double notched = L * 20 * 10 - 5 * 10 * 10;
        EXPECT_NEAR(v["Plate"], 2 * notched, 1e-6 * notched) << "L=" << L;
        EXPECT_NEAR(v["Peg"], 2000, 1e-6);
        ASSERT_EQ(v.count("Pegs;B1"), 1u);
        EXPECT_NEAR(v["Pegs;B1"], 2000, 1e-6);
        // The kept copy is the nearer one (y from 5 + S to 15 + S).
        const NamedShape f = doc.finalShape();
        GProp_GProps g;
        BRepGProp::VolumeProperties(f.body(*f.findBody("Pegs;B1")), g);
        EXPECT_NEAR(g.CentreOfMass().Y(), 10 + S, 1e-6);
        EXPECT_NEAR(g.CentreOfMass().X(), L, 1e-6);
    }
    std::printf("[multibody survival] scenarios: %d, references checked: %d, survived: %d, rate: %.1f %%\n", scenarios,
                checked, survived, 100.0 * survived / checked);
    EXPECT_EQ(survived, checked);

    // Save and load: the same body names and volumes, also from the cache.
    for (bool cache : {false, true}) {
        auto json = doc.toJson(SaveOptions{cache});
        ASSERT_TRUE(json.ok());
        auto loaded = PartDocument::fromJson(json.value(), partRegistry());
        ASSERT_TRUE(loaded.ok()) << loaded.error().message;
        auto lv = volumes(loaded.value());
        ASSERT_EQ(lv.size(), v.size());
        for (const auto& [name, vol] : v) EXPECT_NEAR(lv[name], vol, 1e-9) << name;
    }
}

TEST(DocumentMultibody, IntersectAndUnionNodes) {
    PartDocument doc(partRegistry());
    add(doc, {"Box", "A", "", {{"length", "10"}, {"width", "10"}, {"height", "10"}}, {}, {}, false});
    add(doc, sketchXY("BS", rectangle("5", "0", "10", "10")));
    add(doc, {"Extrude", "B", "", {{"distance", "10"}}, {}, {{"mode", "newBody"}}, false, {{"sketch", "BS"}}});
    doc.rebuild();
    NamedShape s = doc.finalShape();
    add(doc, {"BodyBoolean", "Common", "", {}, {{"target", {makeBodyReference(s, "A").value()}}, {"tools", {makeBodyReference(s, "B").value()}}},
              {{"operation", "intersect"}}, false});
    doc.rebuild();
    ASSERT_EQ(info(doc, "Common").state, FeatureState::Ok) << messages(info(doc, "Common"));
    auto v = volumes(doc);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_NEAR(v["A"], 500, 1e-6);
    ASSERT_TRUE(doc.setOption("Common", "operation", "union").ok());
    doc.rebuild();
    v = volumes(doc);
    ASSERT_EQ(v.size(), 1u);
    EXPECT_NEAR(v["A"], 1500, 1e-6);
    // A sweep along the top edge of A (model edges as the path) as a new body.
    s = doc.finalShape();
    std::vector<TopoReference> pathEdges;
    for (const auto& e : linesAlong(s.shape(), 0, 0, 10)) pathEdges.push_back(refTo(s, e));
    ASSERT_FALSE(pathEdges.empty());
    FeatureSpec prof;
    prof.type = "Sketch";
    prof.id = "Ring";
    prof.options = {{"plane", "YZ"}};
    SketchBuilder ring;
    ring.fixedCircle("0", "10", "1");
    prof.sketch = ring.build();
    add(doc, prof);
    add(doc, {"Sweep", "Rod", "", {}, {{"pathEdges", pathEdges}}, {{"mode", "newBody"}, {"path", "edges"}}, false,
              {{"sketch", "Ring"}}});
    doc.rebuild();
    ASSERT_EQ(info(doc, "Rod").state, FeatureState::Ok) << messages(info(doc, "Rod"));
    v = volumes(doc);
    ASSERT_EQ(v.size(), 2u);
    EXPECT_NEAR(v["Rod"], kPi * 15, 1e-6 * kPi * 15);
    // Errors: a body boolean on a single-body part, deleting every body.
    PartDocument one(partRegistry());
    add(one, {"Box", "Only", "", {}, {}, {}, false});
    one.rebuild();
    NamedShape o = one.finalShape();
    add(one, {"BodyBoolean", "Self", "", {}, {{"target", {makeBodyReference(o, "Only").value()}}, {"tools", {makeBodyReference(o, "Only").value()}}}, {}, false});
    add(one, {"DeleteBody", "All", "", {}, {{"bodies", {makeBodyReference(o, "Only").value()}}}, {}, false});
    one.rebuild();
    EXPECT_EQ(info(one, "Self").state, FeatureState::Error);
    EXPECT_NE(messages(info(one, "Self")).find("two bodies"), std::string::npos);
    EXPECT_EQ(info(one, "All").state, FeatureState::Error);
    EXPECT_NE(messages(info(one, "All")).find("every body"), std::string::npos) << messages(info(one, "All"));
}
