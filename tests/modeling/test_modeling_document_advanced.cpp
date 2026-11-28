// Phase 2 operations in the part document: Sweep (sketch path), Loft, Rib, ThreadedHole.
// Each model is rebuilt over several dimension sets; the dependent operations (threaded hole on
// the end cap of a sweep, threaded hole on the top of a loft, hole through a rib side) must keep
// their references, and the volume must match the analytic formula. Save/load, errors.

#include "modeling_document_test_util.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;

namespace {

using Values = std::map<std::string, double>;

FeatureSpec sketchSpec(const std::string& id, const std::string& plane, SketchData data, bool open = false,
                       const std::string& offset = "0") {
    FeatureSpec s;
    s.type = "Sketch";
    s.id = id;
    s.options = {{"plane", plane}, {"contour", open ? "open" : "closed"}};
    s.parameters = {{"offset", offset}};
    s.sketch = std::move(data);
    return s;
}

// Survival bookkeeping shared by the scenarios of this file.
struct Tally {
    int scenarios = 0, checked = 0, survived = 0;
    void check(bool ok) {
        ++checked;
        if (ok) ++survived;
    }
};

Tally& tally() {
    static Tally t;
    return t;
}

// Cylinder of radius r whose axis is parallel to coordinate axis `axis` through (c1, c2).
bool hasHoleWall(const TopoDS_Shape& s, double r, int axis, double c1, double c2) { return hasCylinder(s, r, axis, c1, c2); }

}  // namespace

// Sweep of a circle R along a line L followed by a quarter arc of radius Rb (path sketch in XZ),
// threaded hole M6 (drill 5, length 8, drill depth 10) on the end cap.
TEST(DocumentAdvanced, SweepAlongSketchPathKeepsThreadedHole) {
    PartDocument doc(partRegistry());
    const Values base{{"R", 6}, {"L", 20}, {"Rb", 15}};
    for (const auto& [n, v] : base) setVar(doc, n, v);
    SketchBuilder prof;
    prof.fixedCircle("0", "0", "R");
    add(doc, sketchSpec("Profile", "XY", prof.build()));
    SketchBuilder path;  // XZ: sketch (u, v) -> world (u, 0, v)
    const int p0 = path.point("0", "0", true), p1 = path.point("0", "L", true), c = path.point("Rb", "L", true),
              p2 = path.point("Rb", "L + Rb", true);
    path.line(p0, p1);
    path.arc(c, p1, p2, false);
    add(doc, sketchSpec("Path", "XZ", path.build(), true));
    add(doc, {"Sweep", "Pipe", "", {}, {}, {{"path", "sketch"}}, false, {{"sketch", "Profile"}, {"pathSketch", "Path"}}});
    doc.rebuild();
    auto pipe = info(doc, "Pipe");
    ASSERT_EQ(pipe.state, FeatureState::Ok) << messages(pipe);
    NamedShape s = doc.finalShape();
    const double v0 = kPi * 36 * (20 + kPi / 2 * 15);
    ASSERT_NEAR(volume(s.shape()), v0, 1e-5 * v0);
    // The end cap: plane x = Rb with outward normal +X.
    const TopoDS_Shape cap = planarFace(s.shape(), gp::DX(), 15);
    ASSERT_FALSE(cap.IsNull());
    const TopoReference capRef = refTo(s, cap);
    EXPECT_EQ(capRef.name, "G[Pipe](Profile:R1)");
    add(doc, {"ThreadedHole", "Tap", "", {{"nominal", "6"}, {"threadLength", "8"}, {"drillDepth", "10"},
                                          {"x", "Rb"}, {"y", "0"}, {"z", "L + Rb"}},
              {{"face", {capRef}}}, {{"drillPoint", "false"}}, false});

    const std::vector<Values> scenarios = {{{"R", 6}, {"L", 20}, {"Rb", 15}}, {{"R", 8}, {"L", 30}, {"Rb", 15}},
                                           {{"R", 7}, {"L", 25}, {"Rb", 25}}, {{"R", 10}, {"L", 12}, {"Rb", 20}},
                                           {{"R", 6}, {"L", 20}, {"Rb", 15}}};
    for (const auto& v : scenarios) {
        for (const auto& [n, x] : v) setVar(doc, n, x);
        doc.rebuild();
        ++tally().scenarios;
        auto tap = info(doc, "Tap");
        const bool ok = tap.state == FeatureState::Ok;
        tally().check(ok);
        ASSERT_TRUE(ok) << messages(tap);
        const double R = v.at("R"), L = v.at("L"), Rb = v.at("Rb");
        const TopoDS_Shape shape = doc.finalShape().shape();
        const double expected = kPi * R * R * (L + kPi / 2 * Rb) - kPi * 2.5 * 2.5 * 10;
        EXPECT_NEAR(volume(shape), expected, 1e-5 * expected) << "R=" << R << " L=" << L << " Rb=" << Rb;
        EXPECT_TRUE(hasHoleWall(shape, 2.5, 0, 0, L + Rb)) << "hole wall along X at z = L + Rb";
        const auto threads = threadsOf(doc.finalShape());
        ASSERT_EQ(threads.size(), 1u);
        EXPECT_EQ(threads.front().thread.designation, "M6");
        EXPECT_DOUBLE_EQ(threads.front().thread.length, 8.0);
        EXPECT_TRUE(threads.front().thread.direction.IsEqual(gp_Dir(-1, 0, 0), 1e-9));
        EXPECT_NEAR(threads.front().thread.entry.X(), Rb, 1e-9);
        EXPECT_EQ(threads.front().name, "Tap:Wall");
    }
}

// Loft from a × a at z = 0 to b × b at z = h (ruled), threaded hole M8 on the top.
TEST(DocumentAdvanced, LoftKeepsThreadedHoleOnTop) {
    PartDocument doc(partRegistry());
    for (const auto& [n, v] : Values{{"a", 40}, {"b", 20}, {"h", 30}}) setVar(doc, n, v);
    add(doc, sketchSpec("S1", "XY", centredRectangle("a", "a")));
    add(doc, sketchSpec("S2", "XY", centredRectangle("b", "b"), false, "h"));
    add(doc, {"Loft", "Frustum", "", {}, {}, {{"surface", "ruled"}}, false, {{"section1", "S1"}, {"section2", "S2"}}});
    doc.rebuild();
    ASSERT_EQ(info(doc, "Frustum").state, FeatureState::Ok) << messages(info(doc, "Frustum"));
    NamedShape s = doc.finalShape();
    const TopoDS_Shape top = planarFace(s.shape(), gp::DZ(), 30);
    ASSERT_FALSE(top.IsNull());
    const TopoReference topRef = refTo(s, top);
    add(doc, {"ThreadedHole", "Tap", "", {{"nominal", "8"}, {"threadLength", "12"}, {"drillDepth", "15"}},
              {{"face", {topRef}}}, {{"drillPoint", "false"}}, false});
    for (const Values& v : {Values{{"a", 40}, {"b", 20}, {"h", 30}}, Values{{"a", 50}, {"b", 30}, {"h", 25}},
                            Values{{"a", 30}, {"b", 12}, {"h", 40}}, Values{{"a", 40}, {"b", 20}, {"h", 30}}}) {
        for (const auto& [n, x] : v) setVar(doc, n, x);
        doc.rebuild();
        ++tally().scenarios;
        const bool ok = info(doc, "Tap").state == FeatureState::Ok;
        tally().check(ok);
        ASSERT_TRUE(ok) << messages(info(doc, "Tap"));
        const double a = v.at("a"), b = v.at("b"), h = v.at("h");
        const double frustum = h / 3 * (a * a + b * b + a * b);
        const double expected = frustum - kPi * 3.375 * 3.375 * 15;
        const TopoDS_Shape shape = doc.finalShape().shape();
        EXPECT_NEAR(volume(shape), expected, 1e-6 * expected);
        EXPECT_TRUE(hasCylinder(shape, 3.375, 2, 0, 0));
        ASSERT_EQ(threadsOf(doc.finalShape()).size(), 1u);
        EXPECT_EQ(threadsOf(doc.finalShape()).front().thread.designation, "M8");
    }
}

// L-bracket (base T thick, wall T thick, wall height H), rib in the middle plane y = 10 from the
// wall (x = T, z = Hr) to the base (x = Lr, z = T), hole through the rib side.
TEST(DocumentAdvanced, RibKeepsHoleThroughItsSide) {
    PartDocument doc(partRegistry());
    for (const auto& [n, v] : Values{{"T", 5}, {"Lr", 30}, {"Hr", 30}, {"t", 2}}) setVar(doc, n, v);
    add(doc, {"Box", "Base", "", {{"length", "40"}, {"width", "20"}, {"height", "T"}}, {}, {}, false});
    add(doc, {"Box", "Wall", "", {{"length", "T"}, {"width", "20"}, {"height", "40"}}, {}, {}, false});
    SketchBuilder line;
    line.line(line.point("T", "Hr", true), line.point("Lr", "T", true));
    // XZ plane: normal -Y; offset -10 puts it at y = 10.
    add(doc, sketchSpec("RibLine", "XZ", line.build(), true, "-10"));
    add(doc, {"Rib", "Rib", "", {{"thickness", "t"}}, {}, {}, false, {{"sketch", "RibLine"}}});
    doc.rebuild();
    ASSERT_EQ(info(doc, "Rib").state, FeatureState::Ok) << messages(info(doc, "Rib"));
    NamedShape s = doc.finalShape();
    // Sketch normal is -Y: SideA (offset +t/2 along the normal) lies at y = 9.
    auto side = s.find(ElementKind::Face, "Rib:SideA");
    ASSERT_TRUE(side.has_value());
    EXPECT_EQ(planarFacesWithNormal(s.shape(), gp_Dir(0, -1, 0), -9), 1);
    const TopoReference sideRef = refTo(s, s.element(ElementKind::Face, *side));
    add(doc, {"Hole", "Lightening", "", {{"diameter", "4"}, {"x", "(2*T + Lr)/3"}, {"y", "9"}, {"z", "(2*T + Hr)/3"}},
              {{"face", {sideRef}}}, {{"end", "through"}}, false});
    for (const Values& v : {Values{{"T", 5}, {"Lr", 30}, {"Hr", 30}, {"t", 2}}, Values{{"T", 5}, {"Lr", 35}, {"Hr", 25}, {"t", 3}},
                            Values{{"T", 6}, {"Lr", 28}, {"Hr", 34}, {"t", 2.5}}, Values{{"T", 5}, {"Lr", 30}, {"Hr", 30}, {"t", 2}}}) {
        for (const auto& [n, x] : v) setVar(doc, n, x);
        doc.rebuild();
        ++tally().scenarios;
        const bool ok = info(doc, "Lightening").state == FeatureState::Ok && info(doc, "Rib").state == FeatureState::Ok;
        tally().check(ok);
        ASSERT_TRUE(ok) << messages(info(doc, "Rib")) << messages(info(doc, "Lightening"));
        const double T = v.at("T"), Lr = v.at("Lr"), Hr = v.at("Hr"), t = v.at("t");
        const double bracket = 40 * 20 * T + T * 20 * (40 - T);
        const double ribV = (Lr - T) * (Hr - T) / 2 * t;
        const double expected = bracket + ribV - kPi * 4 * t;
        EXPECT_NEAR(volume(doc.finalShape().shape()), expected, 1e-6 * expected) << "Lr=" << Lr << " Hr=" << Hr;
        EXPECT_TRUE(hasCylinder(doc.finalShape().shape(), 2, 1, (2 * T + Lr) / 3, (2 * T + Hr) / 3));
    }
}

TEST(DocumentAdvanced, SaveLoadKeepsThreadsAndPaths) {
    PartDocument doc(partRegistry());
    add(doc, {"Box", "Block", "", {{"length", "40"}, {"width", "30"}, {"height", "20"}}, {}, {}, false});
    doc.rebuild();
    NamedShape s = doc.finalShape();
    add(doc, {"ThreadedHole", "Tap", "", {{"nominal", "10"}, {"pitch", "1.25"}, {"threadLength", "12"}, {"x", "20"}, {"y", "15"}},
              {{"face", {refTo(s, planarFace(s.shape(), gp::DZ(), 20))}}}, {{"hand", "left"}}, false});
    doc.rebuild();
    ASSERT_EQ(info(doc, "Tap").state, FeatureState::Ok) << messages(info(doc, "Tap"));
    const auto before = threadsOf(doc.finalShape());
    ASSERT_EQ(before.size(), 1u);
    EXPECT_EQ(before.front().thread.designation, "M10x1.25LH");
    EXPECT_EQ(before.front().body, "Block");
    for (bool cache : {false, true}) {
        auto json = doc.toJson(SaveOptions{cache});
        ASSERT_TRUE(json.ok());
        RebuildReport rep;
        auto loaded = PartDocument::fromJson(json.value(), partRegistry(), &rep);
        ASSERT_TRUE(loaded.ok()) << loaded.error().message;
        if (cache) EXPECT_FALSE(rep.fromCache.empty());
        const auto after = threadsOf(loaded.value().finalShape());
        ASSERT_EQ(after.size(), 1u);
        EXPECT_EQ(after.front().name, before.front().name);
        EXPECT_EQ(after.front().thread.designation, "M10x1.25LH");
        EXPECT_DOUBLE_EQ(after.front().thread.length, 12.0);
        EXPECT_NEAR(volume(loaded.value().finalShape().shape()), volume(doc.finalShape().shape()), 1e-9);
    }
    // A nominal outside the ISO 261 table is an error of the node, not a guess.
    ASSERT_TRUE(doc.setParameter("Tap", "nominal", "11").ok());
    doc.rebuild();
    EXPECT_EQ(info(doc, "Tap").state, FeatureState::Error);
    EXPECT_NE(messages(info(doc, "Tap")).find("ISO 261"), std::string::npos) << messages(info(doc, "Tap"));
}

TEST(DocumentAdvanced, Errors) {
    PartDocument doc(partRegistry());
    SketchBuilder prof;
    prof.fixedCircle("0", "0", "2");
    add(doc, sketchSpec("Profile", "XY", prof.build()));
    // A branching path (T junction) is not a chain.
    SketchBuilder tee;
    const int a = tee.point("0", "0", true), b = tee.point("0", "10", true), c = tee.point("-5", "10", true),
              d = tee.point("5", "10", true);
    tee.line(a, b);
    tee.line(c, b);
    tee.line(b, d);
    add(doc, sketchSpec("Tee", "XZ", tee.build(), true));
    // Crossing lines are rejected by the open-contour sketch itself.
    SketchBuilder cross;
    cross.line(cross.point("0", "0", true), cross.point("10", "10", true));
    cross.line(cross.point("0", "10", true), cross.point("10", "0", true));
    add(doc, sketchSpec("Cross", "XZ", cross.build(), true));
    add(doc, {"Sweep", "Bad", "", {}, {}, {}, false, {{"sketch", "Profile"}, {"pathSketch", "Tee"}}});
    add(doc, {"Loft", "One", "", {}, {}, {}, false, {{"section1", "Profile"}}});
    doc.rebuild();
    // The open-contour sketch checks the chain itself: a branching is its error, and the sweep
    // that links it is not built.
    EXPECT_EQ(info(doc, "Tee").state, FeatureState::Error);
    EXPECT_NE(messages(info(doc, "Tee")).find("branching"), std::string::npos) << messages(info(doc, "Tee"));
    EXPECT_EQ(info(doc, "Cross").state, FeatureState::Error);
    auto bad = info(doc, "Bad");
    EXPECT_EQ(bad.state, FeatureState::Error);
    EXPECT_NE(messages(bad).find("is not built"), std::string::npos) << messages(bad);
    auto one = info(doc, "One");
    EXPECT_EQ(one.state, FeatureState::Error);
    EXPECT_NE(messages(one).find("section2"), std::string::npos) << messages(one);
}

// Copies of a threaded hole made by a feature pattern, a feature mirror and a body pattern get the
// thread attributes (FEATURES.md, «Резьбовое отверстие»; drawing_v2 draws them, ГОСТ 2.311): the
// copied entry is the seed entry moved by the copy transformation, the rest of the spec is the same.
TEST(DocumentAdvanced, PatternAndMirrorCopiesCarryTheThread) {
    PartDocument doc(partRegistry());
    add(doc, {"Box", "Block", "", {{"length", "60"}, {"width", "40"}, {"height", "20"}, {"x", "-30"}}, {}, {}, false});
    doc.rebuild();
    NamedShape s = doc.finalShape();
    add(doc, {"ThreadedHole", "Tap", "", {{"nominal", "8"}, {"threadLength", "12"}, {"drillDepth", "15"}, {"x", "10"}, {"y", "20"}},
              {{"face", {refTo(s, planarFace(s.shape(), gp::DZ(), 20))}}}, {{"drillPoint", "false"}}, false});
    add(doc, {"LinearPattern", "Row", "", {{"count1", "2"}, {"spacing1", "15"}}, {}, {{"direction1", "X"}}, false,
              {{"feature", "Tap"}}});
    add(doc, {"Mirror", "Twin", "", {}, {}, {{"what", "feature"}, {"plane", "YZ"}}, false, {{"feature", "Tap"}}});
    doc.rebuild();
    for (const char* id : {"Tap", "Row", "Twin"}) ASSERT_EQ(info(doc, id).state, FeatureState::Ok) << id << ": " << messages(info(doc, id));

    auto entries = [](const std::vector<ThreadInfo>& threads) {
        std::vector<std::array<double, 3>> e;
        for (const ThreadInfo& t : threads) {
            EXPECT_EQ(t.thread.designation, "M8");
            EXPECT_DOUBLE_EQ(t.thread.length, 12.0);
            EXPECT_NEAR(t.thread.drill, 8.0 - 1.25, 1e-12);  // D - P, ISO 2306
            EXPECT_TRUE(t.thread.direction.IsEqual(gp_Dir(0, 0, -1), 1e-9)) << t.name;
            e.push_back({std::round(t.thread.entry.X() * 1e6) / 1e6, std::round(t.thread.entry.Y() * 1e6) / 1e6,
                         std::round(t.thread.entry.Z() * 1e6) / 1e6});
        }
        std::sort(e.begin(), e.end());
        return e;
    };
    const auto threads = threadsOf(doc.finalShape());
    using E = std::vector<std::array<double, 3>>;
    EXPECT_EQ(entries(threads), (E{{-10, 20, 20}, {10, 20, 20}, {25, 20, 20}}));
    // The copy faces really are the hole walls at the copied centres.
    const TopoDS_Shape shape = doc.finalShape().shape();
    for (double x : {-10.0, 10.0, 25.0}) EXPECT_TRUE(hasHoleWall(shape, 3.375, 2, x, 20)) << "x = " << x;
    // The seed keeps its role name; the copies keep their generated names.
    EXPECT_EQ(std::count_if(threads.begin(), threads.end(), [](const ThreadInfo& t) { return t.name == "Tap:Wall"; }), 1);

    // Body pattern along Z: a second body with three more threads at z = 50.
    s = doc.finalShape();
    add(doc, {"BodyLinearPattern", "Stack", "", {{"count1", "2"}, {"spacing1", "30"}},
              {{"bodies", {refTo(s, planarFace(s.shape(), gp::DZ(), 20))}}}, {{"direction1", "Z"}}, false});
    doc.rebuild();
    ASSERT_EQ(info(doc, "Stack").state, FeatureState::Ok) << messages(info(doc, "Stack"));
    const E stacked{{-10, 20, 20}, {-10, 20, 50}, {10, 20, 20}, {10, 20, 50}, {25, 20, 20}, {25, 20, 50}};
    EXPECT_EQ(entries(threadsOf(doc.finalShape())), stacked);

    // The attributes survive the result cache of the file.
    auto json = doc.toJson(SaveOptions{true});
    ASSERT_TRUE(json.ok());
    RebuildReport rep;
    auto loaded = PartDocument::fromJson(json.value(), partRegistry(), &rep);
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    EXPECT_FALSE(rep.fromCache.empty());
    EXPECT_EQ(entries(threadsOf(loaded.value().finalShape())), stacked);
}

TEST(DocumentAdvanced, ZSurvivalSummary) {
    const Tally& t = tally();
    std::printf("[advanced survival] scenarios: %d, references checked: %d, survived: %d, rate: %.1f %%\n", t.scenarios,
                t.checked, t.survived, t.checked ? 100.0 * t.survived / t.checked : 0.0);
    EXPECT_GT(t.checked, 0);
    EXPECT_EQ(t.survived, t.checked);
}
