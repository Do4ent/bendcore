// Исполнения деталей в сборке: вхождения разных исполнений одного документа детали — масса и
// BOM по исполнению, повторное использование определения, файл сборки, явные ошибки
// (docs/assembly/ASSEMBLY_DOCUMENT.md, раздел «Исполнения деталей»).

#include "assembly_document_helpers.hpp"
#include "../modeling/modeling_document_test_util.hpp"

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <filesystem>
#include <fstream>

using namespace bendcore::assembly_v2;
using bendcore::kernel::ErrorCode;
namespace fs = std::filesystem;
namespace m = bendcore::modeling;
namespace tu = bendcore::modeling::testutil;

namespace {

constexpr double kPiA = 3.14159265358979323846;
constexpr double kSteel = 7850;  // steel_s235
constexpr double kAlu = 2700;    // al_6061_t6

// Брусок W×D×H со сквозным отверстием r = 5 в центре верхней грани (подавлено в «-02»).
double blockVolume(double w, double d, double h, bool hole) {
    return w * d * h - (hole ? kPiA * 25.0 * h : 0.0);
}
double kg(double mm3, double rho) { return mm3 * 1e-9 * rho; }

fs::path writeBlock(const std::string& name) {
    const fs::path dir = fs::path(BENDCORE_ASSEMBLY_TEST_TMP) / name;
    fs::create_directories(dir);
    m::PartDocument doc(m::partRegistry());
    EXPECT_TRUE(doc.setVariable("W", "60").ok());
    EXPECT_TRUE(doc.setVariable("D", "40").ok());
    EXPECT_TRUE(doc.setVariable("H", "20").ok());
    tu::add(doc, {"Box", "Body", "", {{"length", "W"}, {"width", "D"}, {"height", "H"}}, {}, {}, false});
    doc.rebuild();
    const m::NamedShape s = doc.finalShape();
    tu::addSketchCut(doc, "Hole", tu::refTo(s, tu::planarFace(s.shape(), gp::DZ(), 20)), "W / 2", "D / 2", "H",
                     tu::centredCircle("5"), "");
    m::ConfigurationTable t;
    t.columns = {{m::ConfigurationColumnKind::Variable, "W"},
                 {m::ConfigurationColumnKind::Variable, "D"},
                 {m::ConfigurationColumnKind::Variable, "H"},
                 {m::ConfigurationColumnKind::Suppression, "Hole"},
                 {m::ConfigurationColumnKind::Material, ""}};
    t.rows = {{"РЦ.080.00.010", "Брусок", {"60", "40", "20", "0", "steel_s235"}},
              {"-01", "Брусок большой", {"80", "50", "30", "0", "al_6061_t6"}},
              {"-02", "Брусок без отверстия", {"50", "30", "15", "1", ""}},
              {"-03", "Ошибочный", {"60", "40", "-5", "0", ""}}};
    t.active = "РЦ.080.00.010";
    EXPECT_TRUE(doc.setConfigurationTable(t).ok());
    doc.rebuild();
    std::ofstream(dir / "block.json") << doc.toJson().value();
    return dir;
}

const BomRow* rowOf(const Bom& bom, const std::string& designation) {
    for (const BomRow& r : bom.rows) {
        if (r.designation == designation) return &r;
    }
    return nullptr;
}

}  // namespace

TEST(AssemblyDocumentConfigurations, TwoOccurrencesOfDifferentConfigurations) {
    const fs::path dir = writeBlock("two_configurations");
    const PartResolver resolver = filePartResolver(m::partRegistry(), dir.string());
    AssemblyDocument doc("Стойка");
    auto base = doc.addPartFromSource("Брусок", "block.json", resolver);
    ASSERT_TRUE(base.ok()) << base.error().message;
    EXPECT_EQ(doc.definition(base.value()).properties.designation, "РЦ.080.00.010");
    EXPECT_EQ(doc.definition(base.value()).properties.density, kSteel);
    const OccurrenceId a = doc.addOccurrence(base.value(), Transform::identity()).value();
    const OccurrenceId b = doc.addOccurrence(base.value(), Transform::fromTranslation(Vec3(0, 100, 0))).value();

    auto rep = doc.setOccurrenceConfiguration(b, "-01", resolver);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const DefinitionId v01 = doc.findOccurrence(b)->definition;
    EXPECT_NE(v01, base.value());
    EXPECT_EQ(doc.definition(v01).source, "block.json#-01");
    EXPECT_EQ(doc.definition(v01).properties.designation, "РЦ.080.00.010-01");
    EXPECT_EQ(doc.definition(v01).properties.name, "Брусок большой");
    EXPECT_EQ(doc.definition(v01).properties.material, "6061-T6");
    EXPECT_EQ(doc.findOccurrence(a)->definition, base.value());

    auto bom = flatBom(doc);
    ASSERT_TRUE(bom.ok()) << bom.error().message;
    ASSERT_EQ(bom.value().rows.size(), 2u);
    const BomRow* r0 = rowOf(bom.value(), "РЦ.080.00.010");
    const BomRow* r1 = rowOf(bom.value(), "РЦ.080.00.010-01");
    ASSERT_TRUE(r0 && r1);
    EXPECT_EQ(r0->quantity, 1);
    EXPECT_EQ(r1->quantity, 1);
    EXPECT_NEAR(r0->unitVolume, blockVolume(60, 40, 20, true), 1e-6);
    EXPECT_NEAR(r1->unitVolume, blockVolume(80, 50, 30, true), 1e-6);
    EXPECT_NEAR(*r0->unitMass, kg(blockVolume(60, 40, 20, true), kSteel), 1e-9);
    EXPECT_NEAR(*r1->unitMass, kg(blockVolume(80, 50, 30, true), kAlu), 1e-9);
    EXPECT_NEAR(*bom.value().totalMass,
                kg(blockVolume(60, 40, 20, true), kSteel) + kg(blockVolume(80, 50, 30, true), kAlu), 1e-9);

    // A third occurrence of "-01" reuses its definition: one BOM row, quantity 2.
    const OccurrenceId c = doc.addOccurrence(base.value(), Transform::fromTranslation(Vec3(0, 200, 0))).value();
    const std::size_t defs = doc.definitions().size();
    ASSERT_TRUE(doc.setOccurrenceConfiguration(c, "-01", resolver).ok());
    EXPECT_EQ(doc.definitions().size(), defs);
    EXPECT_EQ(doc.findOccurrence(c)->definition, v01);
    bom = flatBom(doc);
    EXPECT_EQ(rowOf(bom.value(), "РЦ.080.00.010-01")->quantity, 2);

    // "-02" has no material: material and density of the part as saved (base definition).
    ASSERT_TRUE(doc.setOccurrenceConfiguration(c, "-02", resolver).ok());
    const Definition& d02 = doc.definition(doc.findOccurrence(c)->definition);
    EXPECT_EQ(d02.properties.designation, "РЦ.080.00.010-02");
    EXPECT_EQ(d02.properties.density, kSteel);
    bom = flatBom(doc);
    ASSERT_EQ(bom.value().rows.size(), 3u);
    EXPECT_NEAR(*rowOf(bom.value(), "РЦ.080.00.010-02")->unitMass, kg(blockVolume(50, 30, 15, false), kSteel), 1e-9);

    // Explicit errors.
    auto none = doc.setOccurrenceConfiguration(a, "-09", resolver);
    ASSERT_FALSE(none.ok());
    EXPECT_NE(none.error().message.find("no configuration '-09'"), std::string::npos) << none.error().message;
    auto broken = doc.setOccurrenceConfiguration(a, "-03", resolver);
    ASSERT_FALSE(broken.ok());
    EXPECT_NE(broken.error().message.find("does not build"), std::string::npos) << broken.error().message;
    EXPECT_NE(broken.error().message.find("Body"), std::string::npos) << broken.error().message;
    EXPECT_EQ(doc.findOccurrence(a)->definition, base.value()) << "a failed switch changes nothing";
    const DefinitionId plain = doc.addPart("Plain", PartData{bendcore::assembly_v2::testing::box(0, 0, 0, 1, 1, 1), nullptr, std::nullopt}).value();
    const OccurrenceId p = doc.addOccurrence(plain, Transform::identity()).value();
    auto noSource = doc.setOccurrenceConfiguration(p, "-01", resolver);
    ASSERT_FALSE(noSource.ok());
    EXPECT_NE(noSource.error().message.find("no part document source"), std::string::npos);

    // Back to the saved part: the occurrence returns to the base definition.
    ASSERT_TRUE(doc.setOccurrenceConfiguration(c, "", resolver).ok());
    EXPECT_EQ(doc.findOccurrence(c)->definition, base.value());

    // The assembly file keeps the configuration sources; reload from the part file.
    ASSERT_TRUE(doc.removeOccurrence(p).ok());
    ASSERT_TRUE(doc.saveFile((dir / "stand.json").string(), SaveOptions{true, 1, false}).ok());
    auto loaded = AssemblyDocument::loadFile((dir / "stand.json").string(), m::partRegistry());
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    for (const SourceDiagnostic& sd : loaded.value().report.sources) {
        if (!sd.source.empty()) EXPECT_EQ(sd.status, SourceStatus::Loaded) << sd.source << ": " << sd.message;
    }
    auto bom2 = flatBom(loaded.value().document);
    ASSERT_TRUE(bom2.ok());
    ASSERT_NE(rowOf(bom2.value(), "РЦ.080.00.010-01"), nullptr);
    EXPECT_EQ(rowOf(bom2.value(), "РЦ.080.00.010")->quantity, 2);
    EXPECT_EQ(rowOf(bom2.value(), "РЦ.080.00.010-01")->quantity, 1);
    EXPECT_NEAR(*bom2.value().totalMass, *flatBom(doc).value().totalMass, 1e-12);
}

TEST(AssemblyDocumentConfigurations, ResolverAndSourceSyntax) {
    EXPECT_EQ(configurationSource("parts/block.json", "-01"), "parts/block.json#-01");
    EXPECT_EQ(configurationSource("parts/block.json", ""), "parts/block.json");
    EXPECT_EQ(splitConfigurationSource("parts/block.json#-01"), std::make_pair(std::string("parts/block.json"), std::string("-01")));
    EXPECT_EQ(splitConfigurationSource("a.step").second, "");
    const fs::path dir = writeBlock("resolver");
    const PartResolver resolver = filePartResolver(m::partRegistry(), dir.string());
    auto d02 = resolver("block.json#-02");
    ASSERT_TRUE(d02.ok()) << d02.error().message;
    EXPECT_NEAR(tu::volume(d02.value().shape), blockVolume(50, 30, 15, false), 1e-6);
    ASSERT_TRUE(d02.value().properties.has_value());
    EXPECT_EQ(d02.value().properties->designation, "РЦ.080.00.010-02");
    EXPECT_FALSE(d02.value().properties->density.has_value());
    std::ofstream(dir / "cube.brep") << "x";
    auto wrong = resolver("cube.brep#-01");
    ASSERT_FALSE(wrong.ok());
    EXPECT_NE(wrong.error().message.find("needs a part document"), std::string::npos);
}
