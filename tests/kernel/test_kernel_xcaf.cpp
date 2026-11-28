// readStepAssembly / writeStepAssembly on the committed STEP corpus
// (tests/corpus/generated, docs/kernel/XCAF_IO.md).
//
// Expected values come from tests/corpus/generated/manifest.json, computed by the
// generator from the model dimensions (tests/corpus/generate_corpus.cpp), and, for the
// placement of every occurrence of the assembly, from the construction in make_assembly():
// base 80x40x10 at the origin, PIN = cylinder r 4 h 30 placed at (20,20,10) and (60,20,10),
// SPACER = tube R 6 r 3 h 15 placed at (40,20,10), instances named <PART>_<k>.

#include <bendcore/kernel/xcaf_io.hpp>

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace bendcore::kernel;
using nlohmann::json;

const std::string kCorpus = BENDCORE_CORPUS_DIR;

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

json manifest() {
    return json::parse(readFile(kCorpus + "/manifest.json"));
}

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

double volumeOf(const TopoDS_Shape& s) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(s, props);
    return props.Mass();
}

double areaOf(const TopoDS_Shape& s) {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(s, props);
    return props.Mass();
}

int countOf(const TopoDS_Shape& s, TopAbs_ShapeEnum type) {
    int n = 0;
    for (TopExp_Explorer ex(s, type); ex.More(); ex.Next()) {
        ++n;
    }
    return n;
}

void expectBox(const Box3& got, const std::array<double, 3>& min, const std::array<double, 3>& max, double tol,
               const std::string& what) {
    for (int k = 0; k < 3; ++k) {
        EXPECT_NEAR(got.min[k], min[k], tol) << what << " min[" << k << "]";
        EXPECT_NEAR(got.max[k], max[k], tol) << what << " max[" << k << "]";
    }
}

AssemblyDocument readCorpus(const std::string& file) {
    auto doc = readStepAssembly(readFile(kCorpus + "/" + file));
    EXPECT_TRUE(doc.ok()) << file << ": " << (doc.ok() ? "" : doc.error().message);
    return doc.ok() ? std::move(doc).value() : AssemblyDocument{};
}

// ---------------------------------------------------------------- every corpus model

TEST(KernelXcaf, CorpusModelsMatchManifest) {
    const json m = manifest();
    ASSERT_TRUE(m.contains("models"));
    ASSERT_GE(m["models"].size(), 13u);
    for (const json& model : m["models"]) {
        const std::string file = model["file"];
        SCOPED_TRACE(file);
        const AssemblyDocument doc = readCorpus(file);
        ASSERT_EQ(doc.roots.size(), model["free_shapes"].get<std::size_t>());
        EXPECT_EQ(doc.definitions[doc.roots[0]].name, model["top_name"].get<std::string>());
        ASSERT_EQ(doc.fileLengthUnits.size(), 1u);
        EXPECT_EQ(doc.fileLengthUnits[0], "millimetre");
        ASSERT_TRUE(doc.fileUnitMm.has_value());
        EXPECT_DOUBLE_EQ(*doc.fileUnitMm, 1.0);
        EXPECT_TRUE(doc.warnings.empty());

        auto placed = flattenAssembly(doc);
        ASSERT_TRUE(placed.ok()) << placed.error().message;
        double volume = 0.0;
        double area = 0.0;
        int solids = 0;
        int faces = 0;
        Bnd_Box world;
        for (const PlacedPart& p : placed.value()) {
            volume += volumeOf(p.shape.occt());
            area += areaOf(p.shape.occt());
            solids += countOf(p.shape.occt(), TopAbs_SOLID);
            faces += countOf(p.shape.occt(), TopAbs_FACE);
            BRepBndLib::AddOptimal(p.shape.occt(), world, false, false);
        }
        const double vTol = model["volume_rel_tol"].get<double>();
        const double aTol = model["area_rel_tol"].get<double>();
        EXPECT_NEAR(volume, model["volume"].get<double>(), vTol * model["volume"].get<double>());
        EXPECT_NEAR(area, model["area"].get<double>(), aTol * model["area"].get<double>());
        EXPECT_EQ(solids, model["solids"].get<int>());
        EXPECT_EQ(faces, model["faces"].get<int>());
        Box3 b;
        world.Get(b.min[0], b.min[1], b.min[2], b.max[0], b.max[1], b.max[2]);
        const auto mn = model["bbox_min"].get<std::array<double, 3>>();
        const auto mx = model["bbox_max"].get<std::array<double, 3>>();
        expectBox(b, mn, mx, model["bbox_abs_tol"].get<double>(), "world bbox");

        if (!model.contains("components")) {
            // A single part: the root is a part definition holding the whole shape.
            EXPECT_EQ(doc.definitions.size(), 1u);
            EXPECT_EQ(doc.definitions[0].kind, DefinitionKind::Part);
            EXPECT_EQ(placed.value().size(), 1u);
        }
    }
}

// ---------------------------------------------------------------- assembly structure

class CorpusAssembly : public ::testing::TestWithParam<std::string> {};

TEST_P(CorpusAssembly, StructureNamesColoursAndPlacements) {
    const std::string file = GetParam();
    const json m = manifest();
    json model;
    for (const json& x : m["models"]) {
        if (x["file"] == file) {
            model = x;
        }
    }
    ASSERT_FALSE(model.is_null());
    const AssemblyDocument doc = readCorpus(file);
    ASSERT_EQ(doc.roots.size(), 1u);
    const AssemblyDefinition& top = doc.definitions[doc.roots[0]];
    ASSERT_EQ(top.kind, DefinitionKind::Assembly);
    EXPECT_EQ(top.name, model["top_name"].get<std::string>());
    EXPECT_FALSE(top.color.has_value());

    // Prototypes: one definition per manifest component, used `instances` times, coloured.
    const json& comps = model["components"];
    std::size_t totalInstances = 0;
    for (const json& c : comps) {
        totalInstances += c["instances"].get<std::size_t>();
    }
    ASSERT_EQ(top.components.size(), totalInstances);
    EXPECT_EQ(doc.definitions.size(), 1 + comps.size());
    std::map<std::string, std::vector<const AssemblyComponent*>> byPart;
    for (const AssemblyComponent& c : top.components) {
        const AssemblyDefinition& d = doc.definitions[c.definition];
        EXPECT_EQ(d.kind, DefinitionKind::Part);
        EXPECT_FALSE(c.color.has_value()) << "the generator colours prototypes, not instances";
        byPart[d.name].push_back(&c);
    }
    ASSERT_EQ(byPart.size(), comps.size());
    for (const json& c : comps) {
        const std::string name = c["name"];
        SCOPED_TRACE(name);
        ASSERT_EQ(byPart.count(name), 1u);
        const auto& uses = byPart[name];
        ASSERT_EQ(uses.size(), c["instances"].get<std::size_t>());
        const AssemblyDefinition& proto = doc.definitions[uses[0]->definition];
        for (const AssemblyComponent* u : uses) {
            EXPECT_EQ(u->definition, uses[0]->definition) << "instances share one prototype";
        }
        for (std::size_t k = 0; k < uses.size(); ++k) {
            EXPECT_EQ(uses[k]->name, name + "_" + std::to_string(k + 1));
        }
        ASSERT_TRUE(proto.color.has_value());
        const auto rgb = c["color"].get<std::array<double, 3>>();
        EXPECT_NEAR(proto.color->r, rgb[0], 1e-6);
        EXPECT_NEAR(proto.color->g, rgb[1], 1e-6);
        EXPECT_NEAR(proto.color->b, rgb[2], 1e-6);
        EXPECT_NEAR(proto.color->alpha, 1.0, 1e-6);
        EXPECT_TRUE(proto.faceColors.empty());
    }

    // Prototype shapes: analytic volumes from the generator dimensions.
    const double pi = std::numbers::pi;
    const std::map<std::string, double> partVolume = {
        {"BASE_PLATE", 80.0 * 40.0 * 10.0},
        {"PIN", pi * 4.0 * 4.0 * 30.0},
        {"SPACER", pi * (6.0 * 6.0 - 3.0 * 3.0) * 15.0},
    };
    for (const auto& [name, uses] : byPart) {
        const AssemblyDefinition& proto = doc.definitions[uses[0]->definition];
        EXPECT_NEAR(volumeOf(proto.shape.occt()), partVolume.at(name), 1e-6 * partVolume.at(name)) << name;
    }

    // Occurrences in world coordinates: translation of each instance and its bounding box.
    auto placed = flattenAssembly(doc);
    ASSERT_TRUE(placed.ok()) << placed.error().message;
    ASSERT_EQ(placed.value().size(), totalInstances);
    struct Expect {
        std::array<double, 3> offset;
        std::array<double, 3> min;
        std::array<double, 3> max;
        std::array<double, 3> rgb;
    };
    const std::map<std::string, Expect> expect = {
        {"BASE_PLATE_1", {{0, 0, 0}, {0, 0, 0}, {80, 40, 10}, {1, 0, 0}}},
        {"PIN_1", {{20, 20, 10}, {16, 16, 10}, {24, 24, 40}, {0, 0, 1}}},
        {"PIN_2", {{60, 20, 10}, {56, 16, 10}, {64, 24, 40}, {0, 0, 1}}},
        {"SPACER_1", {{40, 20, 10}, {34, 14, 10}, {46, 26, 25}, {0, 1, 0}}},
    };
    std::map<std::string, int> seen;
    for (const PlacedPart& p : placed.value()) {
        const std::string leaf = p.path.substr(p.path.rfind('/') + 1);
        SCOPED_TRACE(p.path);
        EXPECT_EQ(p.path, top.name + "/" + leaf);
        ASSERT_EQ(expect.count(leaf), 1u);
        ++seen[leaf];
        const Expect& e = expect.at(leaf);
        const gp_Trsf t = p.location.Transformation();
        EXPECT_NEAR(t.TranslationPart().X(), e.offset[0], 1e-9);
        EXPECT_NEAR(t.TranslationPart().Y(), e.offset[1], 1e-9);
        EXPECT_NEAR(t.TranslationPart().Z(), e.offset[2], 1e-9);
        expectBox(exactBox(p.shape.occt()), e.min, e.max, 1e-4, "occurrence bbox");
        ASSERT_TRUE(p.color.has_value());
        EXPECT_NEAR(p.color->r, e.rgb[0], 1e-6);
        EXPECT_NEAR(p.color->g, e.rgb[1], 1e-6);
        EXPECT_NEAR(p.color->b, e.rgb[2], 1e-6);
        ASSERT_EQ(p.componentPath.size(), 1u);
    }
    EXPECT_EQ(seen.size(), expect.size());
}

INSTANTIATE_TEST_SUITE_P(Ap214AndAp242, CorpusAssembly,
                         ::testing::Values("assembly_bracket_pins_ap214.step", "assembly_bracket_pins_ap242.step"));

// ---------------------------------------------------------------- read -> write -> read

void expectColorEq(const std::optional<AssemblyColor>& a, const std::optional<AssemblyColor>& b,
                   const std::string& what) {
    ASSERT_EQ(a.has_value(), b.has_value()) << what;
    if (!a) {
        return;
    }
    EXPECT_NEAR(a->r, b->r, 1e-6) << what;
    EXPECT_NEAR(a->g, b->g, 1e-6) << what;
    EXPECT_NEAR(a->b, b->b, 1e-6) << what;
    EXPECT_NEAR(a->alpha, b->alpha, 1e-6) << what;
}

void expectSameDocument(const AssemblyDocument& a, const AssemblyDocument& b) {
    ASSERT_EQ(a.definitions.size(), b.definitions.size());
    ASSERT_EQ(a.roots, b.roots);
    for (std::size_t i = 0; i < a.definitions.size(); ++i) {
        const AssemblyDefinition& x = a.definitions[i];
        const AssemblyDefinition& y = b.definitions[i];
        SCOPED_TRACE("definition " + x.name);
        EXPECT_EQ(x.kind, y.kind);
        EXPECT_EQ(x.name, y.name);
        EXPECT_EQ(x.layers, y.layers);
        expectColorEq(x.color, y.color, "definition colour");
        ASSERT_EQ(x.components.size(), y.components.size());
        for (std::size_t k = 0; k < x.components.size(); ++k) {
            const AssemblyComponent& c = x.components[k];
            const AssemblyComponent& d = y.components[k];
            EXPECT_EQ(c.name, d.name);
            EXPECT_EQ(c.definition, d.definition);
            EXPECT_EQ(c.layers, d.layers);
            expectColorEq(c.color, d.color, "component colour");
            const gp_Trsf tc = c.location.Transformation();
            const gp_Trsf td = d.location.Transformation();
            for (int r = 1; r <= 3; ++r) {
                for (int col = 1; col <= 4; ++col) {
                    EXPECT_NEAR(tc.Value(r, col), td.Value(r, col), 1e-9) << c.name;
                }
            }
        }
        ASSERT_EQ(x.faceColors.size(), y.faceColors.size());
        for (std::size_t k = 0; k < x.faceColors.size(); ++k) {
            EXPECT_EQ(x.faceColors[k].faceIndex, y.faceColors[k].faceIndex);
            expectColorEq(x.faceColors[k].color, y.faceColors[k].color, "face colour");
        }
        if (x.kind == DefinitionKind::Part) {
            const double vx = volumeOf(x.shape.occt());
            EXPECT_NEAR(volumeOf(y.shape.occt()), vx, 1e-9 * std::abs(vx));
            EXPECT_EQ(countOf(x.shape.occt(), TopAbs_FACE), countOf(y.shape.occt(), TopAbs_FACE));
            expectBox(exactBox(y.shape.occt()), exactBox(x.shape.occt()).min, exactBox(x.shape.occt()).max, 1e-7,
                      "part bbox");
        }
    }
}

TEST(KernelXcaf, CorpusRoundTripIsLossless) {
    const json m = manifest();
    ASSERT_GE(m["models"].size(), 13u);
    for (const json& model : m["models"]) {
        const std::string file = model["file"];
        const AssemblyDocument first = readCorpus(file);
        for (StepSchema schema : {StepSchema::AP214, StepSchema::AP242}) {
            SCOPED_TRACE(file + (schema == StepSchema::AP214 ? " AP214" : " AP242"));
            auto text = writeStepAssembly(first, {.schema = schema});
            ASSERT_TRUE(text.ok()) << text.error().message;
            const std::string keyword =
                schema == StepSchema::AP214 ? "AUTOMOTIVE_DESIGN" : "AP242_MANAGED_MODEL_BASED_3D_ENGINEERING";
            // OCCT wraps long header lines, so the keyword is searched after FILE_SCHEMA.
            const std::size_t schemaAt = text.value().find("FILE_SCHEMA((");
            ASSERT_NE(schemaAt, std::string::npos);
            const std::size_t keywordAt = text.value().find("'" + keyword, schemaAt);
            ASSERT_NE(keywordAt, std::string::npos);
            EXPECT_LT(keywordAt - schemaAt, 40u);
            auto second = readStepAssembly(text.value());
            ASSERT_TRUE(second.ok()) << second.error().message;
            expectSameDocument(first, second.value());
            EXPECT_EQ(second.value().fileLengthUnits, first.fileLengthUnits);
        }
    }
}

// ---------------------------------------------------------------- bad input

TEST(KernelXcaf, GarbageIsParseError) {
    const std::vector<std::string> inputs = {
        std::string(),
        "this is not a STEP file at all\n\x01\x02\x03",
        "ISO-10303-21;\nHEADER;\nENDSEC;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n",
    };
    for (const std::string& in : inputs) {
        auto r = readStepAssembly(in);
        ASSERT_FALSE(r.ok());
        EXPECT_EQ(r.error().code, ErrorCode::ParseError) << r.error().message;
        EXPECT_FALSE(r.error().message.empty());
    }
    std::string bytes(4096, '\0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<char>((i * 2654435761u) >> 13);
    }
    auto r = readStepAssembly(bytes);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::ParseError);
}

TEST(KernelXcaf, TruncatedAssemblyLosesNoStructureSilently) {
    // The DATA section cut before the first product: parsing fails or yields no roots.
    const std::string full = readFile(kCorpus + "/assembly_bracket_pins_ap214.step");
    const std::size_t cut = full.find("#3 =");
    ASSERT_NE(cut, std::string::npos);
    auto r = readStepAssembly(full.substr(0, cut));
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::ParseError) << r.error().message;
}

}  // namespace
