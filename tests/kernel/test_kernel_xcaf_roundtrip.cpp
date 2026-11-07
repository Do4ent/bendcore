// writeStepAssembly -> readStepAssembly on structures built in memory: nested
// sub-assemblies with rotated placements, colours on definitions, components and faces,
// transparency, layers, UTF-8 names, file units; flattenAssembly placements against
// analytic bounding boxes; structural validation errors (docs/kernel/XCAF_IO.md).

#include <bendcore/kernel/xcaf_io.hpp>

#include <gtest/gtest.h>

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <gp_Ax1.hxx>
#include <gp_Trsf.hxx>

#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <string>
#include <vector>

namespace {

using namespace bendcore::kernel;

constexpr double kPi = std::numbers::pi;

struct Box3 {
    std::array<double, 3> min{};
    std::array<double, 3> max{};
};

Box3 exactBox(const TopoDS_Shape& s) {
    Bnd_Box box;
    BRepBndLib::AddOptimal(s, box, false, false);
    Box3 b;
    box.Get(b.min[0], b.min[1], b.min[2], b.max[0], b.max[1], b.max[2]);
    return b;
}

void expectBox(const Box3& got, const std::array<double, 3>& min, const std::array<double, 3>& max,
               const std::string& what) {
    for (int k = 0; k < 3; ++k) {
        EXPECT_NEAR(got.min[k], min[k], 1e-6) << what << " min[" << k << "]";
        EXPECT_NEAR(got.max[k], max[k], 1e-6) << what << " max[" << k << "]";
    }
}

double volumeOf(const TopoDS_Shape& s) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(s, props);
    return props.Mass();
}

TopLoc_Location translation(double x, double y, double z) {
    gp_Trsf t;
    t.SetTranslation(gp_Vec(x, y, z));
    return TopLoc_Location(t);
}

// Rotation by 90 degrees about +Z, then translation.
TopLoc_Location rotZ90Then(double x, double y, double z) {
    gp_Trsf r;
    r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), kPi / 2);
    gp_Trsf t;
    t.SetTranslation(gp_Vec(x, y, z));
    return TopLoc_Location(t * r);
}

// Index of the face of a box part whose centroid lies on z = zTop (the top face).
std::uint32_t topFaceIndex(const Shape& part, double zTop) {
    auto index = TopologyIndex::build(part);
    EXPECT_TRUE(index.ok());
    for (std::uint32_t i = 0; i < index.value().faces().size(); ++i) {
        GProp_GProps props;
        BRepGProp::SurfaceProperties(index.value().faces()[i].face, props);
        if (std::abs(props.CentreOfMass().Z() - zTop) < 1e-9) {
            return i;
        }
    }
    ADD_FAILURE() << "no face at z = " << zTop;
    return 0;
}

// TOP (assembly)
//   PLATE_1   -> PLATE  100x50x10 box, grey, top face orange, layer "Base"
//   SUB_1     -> SUB at (10, 10, 10), no rotation
//   SUB_2     -> SUB rotated 90 deg about Z, at (90, 10, 10), coloured yellow
// SUB (assembly)
//   PIN_1     -> PIN at (0, 0, 0)
//   PIN_2     -> PIN at (20, 0, 0), half transparent red
// PIN: cylinder r 2 h 15 on the XY plane, blue, layers "Pins" and "Turned"
// Definition order: TOP, PLATE, SUB, PIN (read order is depth first from the roots).
AssemblyDocument makeNested() {
    AssemblyDocument doc;
    AssemblyDefinition top;
    top.kind = DefinitionKind::Assembly;
    top.name = "TOP";
    AssemblyDefinition plate;
    plate.name = "PLATE";
    plate.shape = Shape(BRepPrimAPI_MakeBox(100, 50, 10).Shape());
    plate.color = AssemblyColor{0.5, 0.5, 0.5, 1.0};
    plate.faceColors.push_back({topFaceIndex(plate.shape, 10.0), AssemblyColor{1.0, 0.5, 0.0, 1.0}});
    plate.layers = {"Base"};
    AssemblyDefinition sub;
    sub.kind = DefinitionKind::Assembly;
    sub.name = "SUB";
    AssemblyDefinition pin;
    pin.name = "PIN";
    pin.shape = Shape(BRepPrimAPI_MakeCylinder(2, 15).Shape());
    pin.color = AssemblyColor{0.0, 0.0, 1.0, 1.0};
    pin.layers = {"Pins", "Turned"};

    top.components.push_back({"PLATE_1", 1, TopLoc_Location(), std::nullopt, {}});
    top.components.push_back({"SUB_1", 2, translation(10, 10, 10), std::nullopt, {}});
    top.components.push_back({"SUB_2", 2, rotZ90Then(90, 10, 10), AssemblyColor{1.0, 1.0, 0.0, 1.0}, {}});
    sub.components.push_back({"PIN_1", 3, TopLoc_Location(), std::nullopt, {}});
    sub.components.push_back({"PIN_2", 3, translation(20, 0, 0), AssemblyColor{1.0, 0.0, 0.0, 0.5}, {}});

    doc.definitions = {top, plate, sub, pin};
    doc.roots = {0};
    return doc;
}

void expectColor(const std::optional<AssemblyColor>& c, std::array<double, 4> want, double tol,
                 const std::string& what) {
    ASSERT_TRUE(c.has_value()) << what;
    EXPECT_NEAR(c->r, want[0], tol) << what;
    EXPECT_NEAR(c->g, want[1], tol) << what;
    EXPECT_NEAR(c->b, want[2], tol) << what;
    EXPECT_NEAR(c->alpha, want[3], tol) << what;
}

// Expected world boxes of the five part occurrences of makeNested(), by hand:
// SUB_2 maps (x, y, z) -> (90 - y, 10 + x, 10 + z); PIN occupies [-2,2]x[-2,2]x[0,15].
void expectNestedPlacements(const AssemblyDocument& doc) {
    auto placed = flattenAssembly(doc);
    ASSERT_TRUE(placed.ok()) << placed.error().message;
    ASSERT_EQ(placed.value().size(), 5u);
    struct Want {
        std::array<double, 3> min, max;
        std::array<double, 4> color;
    };
    const std::map<std::string, Want> want = {
        {"TOP/PLATE_1", {{0, 0, 0}, {100, 50, 10}, {0.5, 0.5, 0.5, 1}}},
        {"TOP/SUB_1/PIN_1", {{8, 8, 10}, {12, 12, 25}, {0, 0, 1, 1}}},
        {"TOP/SUB_1/PIN_2", {{28, 8, 10}, {32, 12, 25}, {1, 0, 0, 0.5}}},
        // SUB_2 is yellow, but the colour of the PIN definition replaces the inherited one
        // (XCAFPrs rule), so PIN_1 stays blue; PIN_2 has its own component colour.
        {"TOP/SUB_2/PIN_1", {{88, 8, 10}, {92, 12, 25}, {0, 0, 1, 1}}},
        {"TOP/SUB_2/PIN_2", {{88, 28, 10}, {92, 32, 25}, {1, 0, 0, 0.5}}},
    };
    for (const PlacedPart& p : placed.value()) {
        SCOPED_TRACE(p.path);
        ASSERT_EQ(want.count(p.path), 1u);
        const Want& w = want.at(p.path);
        expectBox(exactBox(p.shape.occt()), w.min, w.max, "occurrence bbox");
        expectColor(p.color, w.color, 1e-4, "effective colour");
        EXPECT_NEAR(volumeOf(p.shape.occt()),
                    p.path == "TOP/PLATE_1" ? 100.0 * 50.0 * 10.0 : kPi * 4.0 * 15.0, 1e-6);
    }
}

// ---------------------------------------------------------------- tests

TEST(KernelXcafRoundTrip, FlattenNestedPlacementsAndColours) {
    expectNestedPlacements(makeNested());
}

TEST(KernelXcafRoundTrip, InheritedColourWhenDefinitionHasNone) {
    AssemblyDocument doc = makeNested();
    doc.definitions[3].color.reset();  // PIN without own colour
    auto placed = flattenAssembly(doc);
    ASSERT_TRUE(placed.ok());
    for (const PlacedPart& p : placed.value()) {
        if (p.path == "TOP/SUB_2/PIN_1") {
            expectColor(p.color, {1, 1, 0, 1}, 1e-12, p.path);  // from component SUB_2
        }
        if (p.path == "TOP/SUB_1/PIN_1") {
            EXPECT_FALSE(p.color.has_value());
        }
    }
}

class NestedRoundTrip : public ::testing::TestWithParam<StepSchema> {};

TEST_P(NestedRoundTrip, StructureColoursLayersAndPlacementsSurvive) {
    const AssemblyDocument original = makeNested();
    auto text = writeStepAssembly(original, {.schema = GetParam()});
    ASSERT_TRUE(text.ok()) << text.error().message;
    auto back = readStepAssembly(text.value());
    ASSERT_TRUE(back.ok()) << back.error().message;
    const AssemblyDocument& doc = back.value();
    EXPECT_TRUE(doc.warnings.empty());

    ASSERT_EQ(doc.roots.size(), 1u);
    ASSERT_EQ(doc.definitions.size(), 4u);
    const AssemblyDefinition& top = doc.definitions[doc.roots[0]];
    EXPECT_EQ(top.name, "TOP");
    ASSERT_EQ(top.kind, DefinitionKind::Assembly);
    ASSERT_EQ(top.components.size(), 3u);
    EXPECT_EQ(top.components[0].name, "PLATE_1");
    EXPECT_EQ(top.components[1].name, "SUB_1");
    EXPECT_EQ(top.components[2].name, "SUB_2");
    EXPECT_EQ(top.components[1].definition, top.components[2].definition) << "one SUB prototype";
    expectColor(top.components[2].color, {1, 1, 0, 1}, 1e-4, "SUB_2 colour");

    const AssemblyDefinition& plate = doc.definitions[top.components[0].definition];
    EXPECT_EQ(plate.name, "PLATE");
    EXPECT_EQ(plate.layers, std::vector<std::string>{"Base"});
    expectColor(plate.color, {0.5, 0.5, 0.5, 1}, 1e-4, "PLATE colour");
    ASSERT_EQ(plate.faceColors.size(), 1u);
    // The coloured face is found again by geometry, independent of face numbering.
    EXPECT_EQ(plate.faceColors[0].faceIndex, topFaceIndex(plate.shape, 10.0));
    expectColor(plate.faceColors[0].color, {1, 0.5, 0, 1}, 1e-4, "top face colour");

    const AssemblyDefinition& sub = doc.definitions[top.components[1].definition];
    EXPECT_EQ(sub.name, "SUB");
    EXPECT_EQ(sub.kind, DefinitionKind::Assembly);
    EXPECT_TRUE(sub.layers.empty());
    ASSERT_EQ(sub.components.size(), 2u);
    expectColor(sub.components[1].color, {1, 0, 0, 0.5}, 1e-4, "PIN_2 colour");
    const AssemblyDefinition& pin = doc.definitions[sub.components[0].definition];
    EXPECT_EQ(pin.name, "PIN");
    EXPECT_EQ(pin.layers, (std::vector<std::string>{"Pins", "Turned"}));
    expectColor(pin.color, {0, 0, 1, 1}, 1e-4, "PIN colour");

    expectNestedPlacements(doc);

    // A second round trip keeps the same placements and colours.
    auto text2 = writeStepAssembly(doc, {.schema = GetParam()});
    ASSERT_TRUE(text2.ok());
    auto back2 = readStepAssembly(text2.value());
    ASSERT_TRUE(back2.ok());
    expectNestedPlacements(back2.value());
}

INSTANTIATE_TEST_SUITE_P(Schemas, NestedRoundTrip, ::testing::Values(StepSchema::AP214, StepSchema::AP242));

TEST(KernelXcafRoundTrip, FileUnits) {
    const AssemblyDocument original = makeNested();
    struct Case {
        StepLengthUnit unit;
        std::string token;  // in the written file
        double mm;
    };
    for (const Case& c : {Case{StepLengthUnit::Metre, "SI_UNIT($,.METRE.)", 1000.0},
                          Case{StepLengthUnit::Inch, "'INCH'", 25.4},
                          Case{StepLengthUnit::Millimetre, "SI_UNIT(.MILLI.,.METRE.)", 1.0}}) {
        SCOPED_TRACE(c.token);
        auto text = writeStepAssembly(original, {.fileUnit = c.unit});
        ASSERT_TRUE(text.ok()) << text.error().message;
        EXPECT_NE(text.value().find(c.token), std::string::npos);
        auto back = readStepAssembly(text.value());
        ASSERT_TRUE(back.ok()) << back.error().message;
        ASSERT_TRUE(back.value().fileUnitMm.has_value());
        EXPECT_NEAR(*back.value().fileUnitMm, c.mm, 1e-12);
        expectNestedPlacements(back.value());  // the model is in millimetres again
    }
}

TEST(KernelXcafRoundTrip, Utf8Names) {
    AssemblyDocument doc = makeNested();
    doc.definitions[1].name = "Плита основания";
    doc.definitions[0].components[0].name = "Плита:1";
    auto text = writeStepAssembly(doc);
    ASSERT_TRUE(text.ok()) << text.error().message;
    auto back = readStepAssembly(text.value());
    ASSERT_TRUE(back.ok());
    const AssemblyDefinition& top = back.value().definitions[back.value().roots[0]];
    EXPECT_EQ(top.components[0].name, "Плита:1");
    EXPECT_EQ(back.value().definitions[top.components[0].definition].name, "Плита основания");
}

TEST(KernelXcafRoundTrip, SeveralRootsKeepOrder) {
    AssemblyDocument doc;
    AssemblyDefinition a;
    a.name = "FIRST";
    a.shape = Shape(BRepPrimAPI_MakeBox(1, 2, 3).Shape());
    AssemblyDefinition b;
    b.name = "SECOND";
    b.shape = Shape(BRepPrimAPI_MakeBox(4, 5, 6).Shape());
    doc.definitions = {a, b};
    doc.roots = {0, 1};
    auto text = writeStepAssembly(doc);
    ASSERT_TRUE(text.ok()) << text.error().message;
    auto back = readStepAssembly(text.value());
    ASSERT_TRUE(back.ok());
    ASSERT_EQ(back.value().roots.size(), 2u);
    const auto& d = back.value().definitions;
    EXPECT_EQ(d[back.value().roots[0]].name, "FIRST");
    EXPECT_EQ(d[back.value().roots[1]].name, "SECOND");
    EXPECT_NEAR(volumeOf(d[back.value().roots[0]].shape.occt()), 6.0, 1e-9);
    EXPECT_NEAR(volumeOf(d[back.value().roots[1]].shape.occt()), 120.0, 1e-9);
}

TEST(KernelXcafRoundTrip, LayersOfAssembliesAndComponentsAreUnsupported) {
    AssemblyDocument doc = makeNested();
    doc.definitions[2].layers = {"Sub"};
    auto w = writeStepAssembly(doc);
    ASSERT_FALSE(w.ok());
    EXPECT_EQ(w.error().code, ErrorCode::Unsupported);
    EXPECT_NE(w.error().message.find("'SUB'"), std::string::npos) << w.error().message;

    doc = makeNested();
    doc.definitions[0].components[1].layers = {"Sub"};
    w = writeStepAssembly(doc);
    ASSERT_FALSE(w.ok());
    EXPECT_EQ(w.error().code, ErrorCode::Unsupported);
    EXPECT_NE(w.error().message.find("SUB_1"), std::string::npos) << w.error().message;
}

// ---------------------------------------------------------------- validation

void expectInvalid(const AssemblyDocument& doc, const std::string& fragment) {
    auto w = writeStepAssembly(doc);
    ASSERT_FALSE(w.ok()) << fragment;
    EXPECT_EQ(w.error().code, ErrorCode::InvalidInput);
    EXPECT_NE(w.error().message.find(fragment), std::string::npos) << w.error().message;
    auto f = flattenAssembly(doc);
    ASSERT_FALSE(f.ok());
    EXPECT_EQ(f.error().code, ErrorCode::InvalidInput);
}

TEST(KernelXcafRoundTrip, InvalidStructuresAreRejected) {
    expectInvalid(AssemblyDocument{}, "no definitions");
    {
        AssemblyDocument d = makeNested();
        d.roots.clear();
        expectInvalid(d, "no roots");
    }
    {
        AssemblyDocument d = makeNested();
        d.definitions[0].components[0].definition = 17;
        expectInvalid(d, "out of range");
    }
    {
        AssemblyDocument d = makeNested();
        d.definitions[2].components[0].definition = 0;  // SUB uses TOP: cycle, TOP used
        expectInvalid(d, "root but also used");
    }
    {
        AssemblyDocument d = makeNested();
        d.roots = {2};  // TOP unused and SUB both root and used
        expectInvalid(d, "root but also used");
    }
    {
        AssemblyDocument d = makeNested();
        AssemblyDefinition extra;
        extra.name = "ORPHAN";
        extra.shape = Shape(BRepPrimAPI_MakeBox(1, 1, 1).Shape());
        d.definitions.push_back(extra);
        expectInvalid(d, "neither a root nor used");
    }
    {
        AssemblyDocument d = makeNested();
        // SUB -> X -> SUB: a cycle not reachable from the root.
        AssemblyDefinition x;
        x.kind = DefinitionKind::Assembly;
        x.name = "X";
        x.components.push_back({"SUB_X", 2, TopLoc_Location(), std::nullopt, {}});
        d.definitions.push_back(x);
        d.definitions[2].components.push_back({"X_1", 4, TopLoc_Location(), std::nullopt, {}});
        expectInvalid(d, "cycle");
    }
    {
        AssemblyDocument d = makeNested();
        d.definitions[3].shape = Shape();
        expectInvalid(d, "shape is null");
    }
    {
        AssemblyDocument d = makeNested();
        d.definitions[2].components.clear();
        expectInvalid(d, "no components");
    }
    {
        AssemblyDocument d = makeNested();
        d.definitions[3].color = AssemblyColor{1.2, 0, 0, 1};
        expectInvalid(d, "outside [0, 1]");
    }
    {
        AssemblyDocument d = makeNested();
        d.definitions[1].faceColors[0].faceIndex = 6;  // a box has faces 0..5
        expectInvalid(d, "face index 6 out of range");
    }
    {
        AssemblyDocument d = makeNested();
        d.definitions[1].faceColors.push_back(d.definitions[1].faceColors[0]);
        expectInvalid(d, "ascending unique");
    }
}

}  // namespace
