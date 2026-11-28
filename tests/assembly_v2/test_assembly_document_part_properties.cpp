// Свойства документа детали в сборке и STEP (docs/modeling/PART_DOCUMENT.md, «Свойства
// документа»; docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 8): имя определения и вхождения, BOM
// (обозначение, наименование, материал, масса = V·ρ по аналитическому объёму), примечание,
// исполнение с суффиксом, STEP (PRODUCT id и description) — после записи и чтения файла.

#include "assembly_document_helpers.hpp"
#include "../modeling/modeling_document_test_util.hpp"

#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <filesystem>
#include <fstream>

using namespace bendcore::assembly_v2;
namespace fs = std::filesystem;
namespace m = bendcore::modeling;
namespace tu = bendcore::modeling::testutil;

namespace {

constexpr double kW = 60, kD = 40, kH = 20;

m::PartDocument plate() {
    m::PartDocument doc(m::partRegistry());
    EXPECT_TRUE(doc.setVariable("W", "60").ok());
    tu::add(doc, {"Box", "Body", "", {{"length", "W"}, {"width", "40"}, {"height", "20"}}, {}, {}, false});
    EXPECT_TRUE(doc.rebuild().failed.empty());
    m::DocumentProperties p;
    p.designation = "АБВГ.715311.001";
    p.name = "Плита опорная";
    p.material = "al_6061_t6";  // 2700 кг/м³
    p.developer = "Иванов";
    p.organization = "ООО «Ромашка»";
    p.note = "Анодировать";
    EXPECT_TRUE(doc.setProperties(p).ok());
    return doc;
}

const PlacedOccurrence* placedNamed(const std::vector<PlacedOccurrence>& placed, const std::string& name) {
    for (const auto& p : placed)
        if (p.pathName == name) return &p;
    return nullptr;
}

}  // namespace

TEST(PartPropertiesInAssembly, DefinitionNameBomAndNote) {
    m::PartDocument part = plate();
    auto data = partDataFromDocument(part);
    ASSERT_TRUE(data.ok()) << data.error().message;
    ASSERT_TRUE(data.value().properties);
    EXPECT_EQ(data.value().properties->designation, "АБВГ.715311.001");
    EXPECT_EQ(data.value().properties->name, "Плита опорная");
    EXPECT_EQ(data.value().properties->material, "6061-T6");
    EXPECT_EQ(data.value().properties->density, std::optional<double>(2700.0));
    EXPECT_EQ(data.value().properties->note, "Анодировать");

    AssemblyDocument doc("Узел");
    // Без имени: определение — наименование детали, вхождения — «<наименование>:<n>».
    auto def = doc.addPart("", data.value());
    ASSERT_TRUE(def.ok()) << def.error().message;
    EXPECT_EQ(doc.definition(def.value()).name, "Плита опорная");
    auto o1 = doc.addOccurrence(def.value(), Transform::identity());
    auto o2 = doc.addOccurrence(def.value(), Transform::fromTranslation({100, 0, 0}));
    ASSERT_TRUE(o1.ok() && o2.ok());
    EXPECT_EQ(doc.findOccurrence(o1.value())->name, "Плита опорная:1");
    EXPECT_EQ(doc.findOccurrence(o2.value())->name, "Плита опорная:2");
    auto placed = doc.placedParts();
    ASSERT_TRUE(placed.ok());
    EXPECT_NE(placedNamed(placed.value(), "Плита опорная:2"), nullptr);

    // Свойства, заданные явно, не заменяются свойствами документа; пустые — дополняются.
    PartProperties own;
    own.name = "Плита (своя)";
    auto def2 = doc.addPart("Своя", data.value(), own);
    ASSERT_TRUE(def2.ok());
    EXPECT_EQ(doc.definition(def2.value()).properties.name, "Плита (своя)");
    EXPECT_EQ(doc.definition(def2.value()).properties.designation, "АБВГ.715311.001");

    auto bom = flatBom(doc);
    ASSERT_TRUE(bom.ok());
    const BomRow* row = nullptr;
    for (const BomRow& r : bom.value().rows)
        if (r.definition == def.value()) row = &r;
    ASSERT_NE(row, nullptr);
    EXPECT_EQ(row->designation, "АБВГ.715311.001");
    EXPECT_EQ(row->name, "Плита опорная");
    EXPECT_EQ(row->quantity, 2);
    ASSERT_TRUE(row->unitMass);
    EXPECT_NEAR(*row->unitMass, kW * kD * kH * 1e-9 * 2700.0, 1e-12);

    // Файл сборки хранит примечание.
    auto text = doc.toJson();
    ASSERT_TRUE(text.ok());
    auto back = AssemblyDocument::fromJson(text.value(), LoadOptions{});
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_EQ(back.value().document.definition(def.value()).properties.note, "Анодировать");
}

TEST(PartPropertiesInAssembly, FileSourceAndConfigurationSuffix) {
    const fs::path dir = fs::path(BENDCORE_ASSEMBLY_TEST_TMP) / "part_properties";
    fs::create_directories(dir);
    m::PartDocument part = plate();
    m::ConfigurationTable t;
    t.columns = {{m::ConfigurationColumnKind::Variable, "W"}};
    t.rows = {{"-00", "", {"60"}}, {"-01", "Плита длинная", {"90"}}};
    ASSERT_TRUE(part.setConfigurationTable(t).ok());
    {
        std::ofstream(dir / "plate.json") << part.toJson().value();
    }
    AssemblyDocument doc("Узел");
    auto resolver = filePartResolver(m::partRegistry(), dir.string());
    auto def = doc.addPartFromSource("", configurationSource("plate.json", "-01"), resolver);
    ASSERT_TRUE(def.ok()) << def.error().message;
    const PartProperties& p = doc.definition(def.value()).properties;
    EXPECT_EQ(p.designation, "АБВГ.715311.001-01");  // суффикс исполнения дополняет обозначение детали
    EXPECT_EQ(p.name, "Плита длинная");
    EXPECT_EQ(doc.definition(def.value()).name, "Плита длинная");
    EXPECT_EQ(p.density, std::optional<double>(2700.0));
    auto base = doc.addPartFromSource("", "plate.json", resolver);
    ASSERT_TRUE(base.ok());
    EXPECT_EQ(doc.definition(base.value()).properties.designation, "АБВГ.715311.001");
    EXPECT_EQ(doc.definition(base.value()).properties.name, "Плита опорная");
}

TEST(PartPropertiesInStep, PartAndAssemblyProductAttributes) {
    m::PartDocument part = plate();
    // Деталь: один PRODUCT, id — обозначение, name/description — наименование.
    auto step = bendcore::kernel::writeStepAssembly(partStepDocument(part, "plate"), {});
    ASSERT_TRUE(step.ok()) << step.error().message;
    auto read = bendcore::kernel::readStepAssembly(step.value());
    ASSERT_TRUE(read.ok()) << read.error().message;
    ASSERT_EQ(read.value().definitions.size(), 1u);
    EXPECT_EQ(read.value().definitions[0].productId, std::optional<std::string>("АБВГ.715311.001"));
    EXPECT_EQ(read.value().definitions[0].description, "Плита опорная");
    EXPECT_EQ(read.value().definitions[0].name, "Плита опорная");
    auto box = read.value().definitions[0].shape.boundingBox();
    ASSERT_TRUE(box.ok());
    EXPECT_NEAR(box.value().max[0] - box.value().min[0], kW, 1e-6);

    // Деталь без свойств — имя файла, id = имя (умолчание OCCT).
    m::PartDocument bare(m::partRegistry());
    tu::add(bare, {"Box", "Body", "", {{"length", "10"}, {"width", "10"}, {"height", "10"}}, {}, {}, false});
    bare.rebuild();
    auto bareStep = bendcore::kernel::writeStepAssembly(partStepDocument(bare, "cube"), {});
    ASSERT_TRUE(bareStep.ok());
    auto bareRead = bendcore::kernel::readStepAssembly(bareStep.value());
    ASSERT_TRUE(bareRead.ok());
    EXPECT_EQ(bareRead.value().definitions[0].name, "cube");

    // Сборка: обозначение и наименование детали — PRODUCT детали, после чтения — свойства.
    AssemblyDocument doc("Узел");
    auto def = doc.addPart("", partDataFromDocument(part).value());
    ASSERT_TRUE(def.ok());
    ASSERT_TRUE(doc.addOccurrence(def.value(), Transform::identity()).ok());
    auto asmStep = doc.exportStep();
    ASSERT_TRUE(asmStep.ok()) << asmStep.error().message;
    auto imported = AssemblyDocument::importStep(asmStep.value());
    ASSERT_TRUE(imported.ok()) << imported.error().message;
    bool found = false;
    for (const Definition& d : imported.value().definitions()) {
        if (d.kind != DefinitionKind::Part) continue;
        found = true;
        EXPECT_EQ(d.properties.designation, "АБВГ.715311.001");
        EXPECT_EQ(d.properties.name, "Плита опорная");
        EXPECT_EQ(d.name, "Плита опорная");
    }
    EXPECT_TRUE(found);
}
