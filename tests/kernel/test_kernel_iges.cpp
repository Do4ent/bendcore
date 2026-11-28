// readIgesAssembly / writeIgesAssembly (docs/kernel/XCAF_IO.md, "IGES").
//
// Expected volumes and face counts are computed from the dimensions (formulas below and the
// generator manifest tests/corpus/generated/manifest.json, section "iges"), not measured by
// OCCT on the written shape.

#include <bendcore/kernel/xcaf_io.hpp>

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Ax2.hxx>
#include <gp_Trsf.hxx>

#include <cmath>
#include <fstream>
#include <numbers>
#include <sstream>
#include <string>

namespace {

using namespace bendcore::kernel;
using nlohmann::json;

const std::string kCorpus = BENDCORE_CORPUS_DIR;
constexpr double kPi = std::numbers::pi;

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int count(const TopoDS_Shape& s, TopAbs_ShapeEnum type) {
    int n = 0;
    for (TopExp_Explorer ex(s, type); ex.More(); ex.Next()) ++n;
    return n;
}

double volume(const TopoDS_Shape& s) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(s, props);
    return props.Mass();
}

AssemblyDocument singlePart(const std::string& name, const TopoDS_Shape& shape) {
    AssemblyDocument doc;
    AssemblyDefinition def;
    def.kind = DefinitionKind::Part;
    def.name = name;
    def.shape = Shape(shape);
    doc.definitions.push_back(def);
    doc.roots = {0};
    return doc;
}

struct Totals {
    int parts = 0;
    int solids = 0;
    int faces = 0;
    double volume = 0.0;
};

Totals totals(const AssemblyDocument& doc) {
    auto placed = flattenAssembly(doc);
    EXPECT_TRUE(placed.ok()) << placed.error().message;
    Totals t;
    if (!placed) return t;
    for (const PlacedPart& p : placed.value()) {
        ++t.parts;
        t.solids += count(p.shape.occt(), TopAbs_SOLID);
        t.faces += count(p.shape.occt(), TopAbs_FACE);
        t.volume += volume(p.shape.occt());
    }
    return t;
}

TEST(Iges, BoxRoundTripInBRepMode) {
    auto text = writeIgesAssembly(singlePart("BOX", BRepPrimAPI_MakeBox(100, 60, 20).Shape()));
    ASSERT_TRUE(text.ok()) << text.error().message;
    // Manifold solid B-rep object (type 186) in the directory entry section.
    EXPECT_NE(text.value().find("     186"), std::string::npos);
    auto doc = readIgesAssembly(text.value());
    ASSERT_TRUE(doc.ok()) << doc.error().message;
    const Totals t = totals(doc.value());
    EXPECT_EQ(t.parts, 1);
    EXPECT_EQ(t.solids, 1);
    EXPECT_EQ(t.faces, 6);
    EXPECT_NEAR(t.volume, 100.0 * 60.0 * 20.0, 1e-6 * 120000.0);
    EXPECT_EQ(doc.value().definitions[doc.value().roots[0]].name, "BOX");
    ASSERT_TRUE(doc.value().fileUnitMm.has_value());
    EXPECT_DOUBLE_EQ(*doc.value().fileUnitMm, 1.0);
    EXPECT_TRUE(doc.value().warnings.empty());
}

TEST(Iges, FacesModeIsSewnIntoASolid) {
    const double r = 20.0, h = 50.0;
    const TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), r, h).Shape();
    auto text = writeIgesAssembly(singlePart("CYL", cyl), {.mode = IgesWriteMode::Faces});
    ASSERT_TRUE(text.ok()) << text.error().message;
    EXPECT_EQ(text.value().find("     186"), std::string::npos);  // no B-rep solid entity

    auto sewn = readIgesAssembly(text.value());
    ASSERT_TRUE(sewn.ok()) << sewn.error().message;
    const Totals t = totals(sewn.value());
    EXPECT_EQ(t.solids, 1);
    EXPECT_EQ(t.faces, 3);
    EXPECT_NEAR(t.volume, kPi * r * r * h, 1e-5 * kPi * r * r * h);
    ASSERT_EQ(sewn.value().warnings.size(), 1u);
    EXPECT_NE(sewn.value().warnings[0].find("sewn into 1 solid"), std::string::npos);

    auto loose = readIgesAssembly(text.value(), {.sewingTolerance = 0.0});
    ASSERT_TRUE(loose.ok());
    EXPECT_EQ(totals(loose.value()).solids, 0);
}

TEST(Iges, AssemblyColoursAndPlacements) {
    // Part A: box 10x10x10 (red) twice, at x = 0 and x = 30; part B: cylinder r 2 h 5 (blue).
    AssemblyDocument doc;
    AssemblyDefinition top;
    top.kind = DefinitionKind::Assembly;
    top.name = "TOP";
    AssemblyDefinition a;
    a.kind = DefinitionKind::Part;
    a.name = "BLOCK";
    a.shape = Shape(BRepPrimAPI_MakeBox(10, 10, 10).Shape());
    a.color = AssemblyColor{1.0, 0.0, 0.0, 1.0};
    AssemblyDefinition b;
    b.kind = DefinitionKind::Part;
    b.name = "PIN";
    b.shape = Shape(BRepPrimAPI_MakeCylinder(2, 5).Shape());
    b.color = AssemblyColor{0.0, 0.0, 1.0, 1.0};
    doc.definitions = {top, a, b};
    for (double x : {0.0, 30.0}) {
        gp_Trsf t;
        t.SetTranslation(gp_Vec(x, 0, 0));
        doc.definitions[0].components.push_back({.name = "BLOCK", .definition = 1, .location = TopLoc_Location(t)});
    }
    gp_Trsf tp;
    tp.SetTranslation(gp_Vec(15, 5, 0));
    doc.definitions[0].components.push_back({.name = "PIN", .definition = 2, .location = TopLoc_Location(tp)});
    doc.roots = {0};

    auto text = writeIgesAssembly(doc);
    ASSERT_TRUE(text.ok()) << text.error().message;
    auto back = readIgesAssembly(text.value());
    ASSERT_TRUE(back.ok()) << back.error().message;
    auto placed = flattenAssembly(back.value());
    ASSERT_TRUE(placed.ok());
    const Totals t = totals(back.value());
    EXPECT_EQ(t.parts, 3);
    EXPECT_EQ(t.solids, 3);
    EXPECT_EQ(t.faces, 6 + 6 + 3);
    EXPECT_NEAR(t.volume, 2 * 1000.0 + kPi * 4.0 * 5.0, 1e-6 * 2100.0);

    // World bounding box of everything: x 0..40, y 0..10, z 0..10.
    Bnd_Box box;
    int red = 0, blue = 0;
    for (const PlacedPart& p : placed.value()) {
        BRepBndLib::AddOptimal(p.shape.occt(), box, false, false);
        ASSERT_TRUE(p.color.has_value()) << p.path;
        if (p.color->r > 0.99 && p.color->b < 0.01) ++red;
        if (p.color->b > 0.99 && p.color->r < 0.01) ++blue;
    }
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    EXPECT_NEAR(x0, 0.0, 1e-6);
    EXPECT_NEAR(x1, 40.0, 1e-6);
    EXPECT_NEAR(y1, 10.0, 1e-6);
    EXPECT_NEAR(z1, 10.0, 1e-6);
    EXPECT_EQ(red, 2);
    EXPECT_EQ(blue, 1);

    // The root is an assembly of one part per solid; the solid names are those of the parts.
    const AssemblyDefinition& root = back.value().definitions[back.value().roots[0]];
    EXPECT_EQ(root.kind, DefinitionKind::Assembly);
    ASSERT_EQ(root.components.size(), 3u);
    int blocks = 0, pins = 0;
    for (const AssemblyComponent& c : root.components) {
        const std::string& name = back.value().definitions[c.definition].name;
        blocks += name == "BLOCK";
        pins += name == "PIN";
    }
    EXPECT_EQ(blocks, 2);
    EXPECT_EQ(pins, 1);

    // Without splitting: one multi-body part.
    auto whole = readIgesAssembly(text.value(), {.splitSolids = false});
    ASSERT_TRUE(whole.ok());
    EXPECT_EQ(totals(whole.value()).parts, 1);
    EXPECT_EQ(totals(whole.value()).solids, 3);
}

TEST(Iges, FileUnitInchIsConvertedBackToMillimetres) {
    auto text = writeIgesAssembly(singlePart("BOX", BRepPrimAPI_MakeBox(25.4, 50.8, 10).Shape()),
                                  {.fileUnit = StepLengthUnit::Inch});
    ASSERT_TRUE(text.ok());
    auto doc = readIgesAssembly(text.value());
    ASSERT_TRUE(doc.ok()) << doc.error().message;
    ASSERT_TRUE(doc.value().fileUnitMm.has_value());
    EXPECT_DOUBLE_EQ(*doc.value().fileUnitMm, 25.4);
    EXPECT_NEAR(totals(doc.value()).volume, 25.4 * 50.8 * 10.0, 1e-6 * 12903.2);
}

TEST(Iges, Errors) {
    EXPECT_EQ(readIgesAssembly("").error().code, ErrorCode::ParseError);
    EXPECT_EQ(readIgesAssembly("ISO-10303-21;\nHEADER;\n").error().code, ErrorCode::ParseError);
    std::string notIges(80, ' ');
    notIges[72] = 'X';
    EXPECT_EQ(readIgesAssembly(notIges + "\n").error().code, ErrorCode::ParseError);
    EXPECT_EQ(readIgesAssembly(std::string(200, ' '), {.maxInputBytes = 100}).error().code, ErrorCode::InvalidInput);
    AssemblyDocument empty;
    EXPECT_EQ(writeIgesAssembly(empty).error().code, ErrorCode::InvalidInput);
}

// Committed IGES files of the corpus (tests/corpus/generate_corpus.cpp): totals against the
// formulas of the STEP model each file was written from.
TEST(Iges, CommittedCorpus) {
    const json manifest = json::parse(readFile(kCorpus + "/manifest.json"));
    ASSERT_TRUE(manifest.contains("iges"));
    ASSERT_GE(manifest["iges"].size(), 4u);
    for (const json& entry : manifest["iges"]) {
        const std::string file = entry["file"];
        const json* model = nullptr;
        for (const json& m : manifest["models"])
            if (m["name"] == entry["model"]) model = &m;
        ASSERT_NE(model, nullptr) << file;
        auto doc = readIgesAssembly(readFile(kCorpus + "/" + file));
        ASSERT_TRUE(doc.ok()) << file << ": " << doc.error().message;
        const Totals t = totals(doc.value());
        const double v = (*model)["volume"];
        EXPECT_NEAR(t.volume, v, double((*model)["volume_rel_tol"]) * v * 10.0) << file;
        EXPECT_EQ(t.solids, int((*model)["solids"])) << file;
        EXPECT_EQ(t.faces, int((*model)["faces"])) << file;
    }
}

}  // namespace
