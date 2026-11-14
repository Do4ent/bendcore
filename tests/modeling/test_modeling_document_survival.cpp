// Phase 1 criterion (docs/ROADMAP_GENERAL_CAD.md): after a dimension change the model is rebuilt
// without losing fillets and holes in >= 95 % of the naming test scenarios.
//
// Model: box L x W x H; rectangular pocket on the top face; round hole in the front face;
// fillet on the top-front edge; fillet on one rim edge of the pocket; fillet on the hole rim;
// chamfer on the back-right vertical edge. Six topological references. Each scenario changes
// dimensions and checks every reference by independent geometry (axis positions, plane
// offsets) and the total volume against the analytic formula.

#include "modeling_document_test_util.hpp"

#include <cstdio>
#include <map>

using namespace bendcore::modeling;
using namespace bendcore::modeling::testutil;

namespace {

using Values = std::map<std::string, double>;

const Values kBase{{"L", 80}, {"W", 50}, {"H", 30}, {"a", 20}, {"b", 12}, {"d", 8},  {"px", 0},
                   {"py", 5}, {"rho", 4}, {"hx", 0}, {"hz", 10}, {"hd", 15}, {"r1", 3}, {"r2", 1.5},
                   {"r3", 1}, {"c", 2}};

double analyticVolume(const Values& v) {
    const double L = v.at("L"), W = v.at("W"), H = v.at("H");
    return L * W * H - v.at("a") * v.at("b") * v.at("d") - kPi * v.at("rho") * v.at("rho") * v.at("hd") -
           filletLoss(v.at("r1"), L) - filletLoss(v.at("r2"), v.at("a")) - holeRimFilletLoss(v.at("r3"), v.at("rho")) -
           chamferLoss(v.at("c"), H);
}

bool hasTorus(const TopoDS_Shape& shape, double major, double minor, double x, double z) {
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface s(TopoDS::Face(ex.Current()), false);
        if (s.GetType() != GeomAbs_Torus) continue;
        const gp_Torus t = s.Torus();
        if (std::abs(t.MajorRadius() - major) < 1e-6 && std::abs(t.MinorRadius() - minor) < 1e-6 &&
            std::abs(std::abs(t.Axis().Direction().Y()) - 1) < 1e-9 && std::abs(t.Location().X() - x) < 1e-6 &&
            std::abs(t.Location().Z() - z) < 1e-6) {
            return true;
        }
    }
    return false;
}

struct Model {
    PartDocument doc{partRegistry()};

    void set(const Values& v) {
        for (const auto& [name, value] : v) setVar(doc, name, value);
    }

    Model() {
        set(kBase);
        add(doc, {"Box", "Body", "", {{"length", "L"}, {"width", "W"}, {"height", "H"}}, {}, {}, false});
        doc.rebuild();
        NamedShape s = doc.finalShape();
        // Pocket and round hole: sketches on the top and front faces, cut extrusions.
        addSketchCut(doc, "Pocket", refTo(s, planarFace(s.shape(), gp::DZ(), 30)), "L/2 + px", "W/2 + py", "H",
                     centredRectangle("a", "b"), "d");
        addSketchCut(doc, "Hole", refTo(s, planarFace(s.shape(), gp::DY().Reversed(), 0)), "L/2 + hx", "0", "hz",
                     centredCircle("rho"), "hd");
        doc.rebuild();
        s = doc.finalShape();

        // Fillet on the top-front edge.
        add(doc, {"Fillet", "TopRound", "", {{"radius", "r1"}}, {{"edges", {refTo(s, singleLine(s.shape(), 0, 0, 30))}}}, {}, false});
        // Fillet on the pocket rim edge on the +Y wall (y = W/2 + py + b/2 = 36, z = H).
        add(doc, {"Fillet", "PocketRound", "", {{"radius", "r2"}}, {{"edges", {refTo(s, singleLine(s.shape(), 0, 36, 30))}}}, {}, false});
        // Fillet on the hole rim (circle of radius rho in the plane y = 0).
        TopoDS_Shape rim;
        for (TopExp_Explorer ex(s.shape(), TopAbs_EDGE); ex.More(); ex.Next()) {
            BRepAdaptor_Curve c(TopoDS::Edge(ex.Current()));
            if (c.GetType() == GeomAbs_Circle && std::abs(c.Circle().Location().Y()) < 1e-9) rim = ex.Current();
        }
        EXPECT_FALSE(rim.IsNull());
        add(doc, {"Fillet", "HoleRound", "", {{"radius", "r3"}}, {{"edges", {refTo(s, rim)}}}, {}, false});
        // Chamfer on the back-right vertical edge (x = L, y = W).
        add(doc, {"Chamfer", "Bevel", "", {{"distance", "c"}}, {{"edges", {refTo(s, singleLine(s.shape(), 2, 80, 50))}}}, {}, false});
        doc.rebuild();
    }
};

struct Tally {
    int total = 0;
    int survived = 0;
    std::map<ResolveMethod, int> methods;
};

// Checks every reference of the current state against the expected geometry.
void check(const PartDocument& doc, const Values& v, const std::string& scenario, Tally& tally) {
    const TopoDS_Shape s = doc.finalShape().shape();
    const double L = v.at("L"), W = v.at("W"), H = v.at("H");
    const double cy = W / 2 + v.at("py");
    const std::map<std::string, bool> geometry{
        {"Pocket", planarFacesWithNormal(s, gp::DZ(), H - v.at("d")) == 1},
        {"Hole", hasCylinder(s, v.at("rho"), 1, L / 2 + v.at("hx"), v.at("hz"))},
        {"TopRound", hasCylinder(s, v.at("r1"), 0, v.at("r1"), H - v.at("r1"))},
        {"PocketRound", hasCylinder(s, v.at("r2"), 0, cy + v.at("b") / 2 + v.at("r2"), H - v.at("r2"))},
        {"HoleRound", hasTorus(s, v.at("rho") + v.at("r3"), v.at("r3"), L / 2 + v.at("hx"), v.at("hz"))},
        {"Bevel", planarFacesWithNormal(s, gp_Dir(1, 1, 0), (L + W - v.at("c")) / std::sqrt(2.0)) == 1},
    };
    for (const auto& [id, geomOk] : geometry) {
        // The pocket and the hole refer to their faces through their sketches.
        const bool sketched = id == "Pocket" || id == "Hole";
        FeatureInfo fi = info(doc, sketched ? id + "Sketch" : id);
        ASSERT_EQ(fi.referenceReports.size(), 1u) << scenario << " " << id;
        const ResolveMethod m = fi.referenceReports[0].resolution.method;
        ++tally.methods[m];
        ++tally.total;
        const bool built = !sketched || info(doc, id).state == FeatureState::Ok;
        const bool ok = fi.state == FeatureState::Ok && built && m != ResolveMethod::Broken && geomOk;
        if (ok) {
            ++tally.survived;
        } else {
            ADD_FAILURE() << scenario << ": reference of " << id << " lost (state " << toString(fi.state)
                          << ", method " << toString(m) << ", geometry " << geomOk << ")\n"
                          << messages(fi);
        }
    }
    EXPECT_NEAR(volume(s), analyticVolume(v), 1e-6 * analyticVolume(v)) << scenario;
}

}  // namespace

TEST(ModelingDocumentSurvival, DimensionChangesKeepReferences) {
    Model m;
    Tally tally;
    check(m.doc, kBase, "base", tally);

    const std::vector<std::pair<std::string, Values>> scenarios{
        {"longer box", {{"L", 100}}},
        {"shorter box", {{"L", 60}}},
        {"wider box", {{"W", 70}}},
        {"narrower box", {{"W", 40}}},
        {"taller box", {{"H", 40}}},
        {"lower box", {{"H", 24}}},
        {"longer pocket", {{"a", 30}}},
        {"narrower pocket", {{"b", 8}}},
        {"deeper pocket", {{"d", 12}}},
        {"pocket moved in y", {{"py", -3}}},
        {"pocket moved in x", {{"px", 12}}},
        {"bigger hole", {{"rho", 6}}},
        {"hole moved in x", {{"hx", 15}}},
        {"hole moved down", {{"hz", 8}}},
        {"deeper hole", {{"hd", 20}}},
        {"bigger top fillet", {{"r1", 5}}},
        {"bigger pocket fillet", {{"r2", 3}}},
        {"bigger hole fillet", {{"r3", 2}}},
        {"bigger chamfer", {{"c", 4}}},
        {"box scaled", {{"L", 120}, {"W", 60}, {"H", 36}}},
        {"everything x1.5",
         {{"L", 120}, {"W", 75}, {"H", 45}, {"a", 30}, {"b", 18}, {"d", 12}, {"py", 7.5}, {"rho", 6},
          {"hz", 15}, {"hd", 22.5}, {"r1", 4.5}, {"r2", 2.25}, {"r3", 1.5}, {"c", 3}}},
        {"pocket reshaped", {{"a", 10}, {"b", 20}}},
        {"tall box, deep pocket", {{"H", 50}, {"d", 20}}},
        {"short box", {{"L", 50}}},
        {"hole and pocket moved", {{"hx", -12}, {"px", -15}, {"py", 0}}},
    };
    ASSERT_GE(scenarios.size(), 20u);
    for (const auto& [name, changes] : scenarios) {
        Values v = kBase;
        for (const auto& [k, x] : changes) v[k] = x;
        m.set(v);
        m.doc.rebuild();
        check(m.doc, v, name, tally);
    }
    // Back to the base dimensions: same result as at the start.
    m.set(kBase);
    m.doc.rebuild();
    check(m.doc, kBase, "back to base", tally);

    const double rate = static_cast<double>(tally.survived) / tally.total;
    std::printf("[naming survival] scenarios: %zu, references checked: %d, survived: %d, rate: %.1f %%\n",
                scenarios.size() + 2, tally.total, tally.survived, 100.0 * rate);
    std::printf("[naming survival] resolved exact: %d, history: %d, signature: %d, broken: %d\n",
                tally.methods[ResolveMethod::Exact], tally.methods[ResolveMethod::History],
                tally.methods[ResolveMethod::Signature], tally.methods[ResolveMethod::Broken]);
    RecordProperty("survival_rate_percent", std::to_string(100.0 * rate));
    EXPECT_GE(rate, 0.95);
}
