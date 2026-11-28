// Свойства документа детали в основной надписи чертежа детали и в спецификации ГОСТ 2.106
// (docs/drawing/DRAWING_DOCUMENT.md, «Свойства детали в основной надписи»): при создании и при
// обновлении по модели; графы листа (ручная правка) поверх; свойства, записанные в чертеже, —
// только для пустых полей документа; исполнение с суффиксом; масса = πR²L·ρ.

#include "test_drawing_document_support.hpp"

#include <bendcore/assembly_v2/assembly_document.hpp>
#include <bendcore/drawing_v2/specification.hpp>

#include <gtest/gtest.h>

using namespace ddtest;
namespace m = bendcore::modeling;
namespace asmv = bendcore::assembly_v2;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kR = 10, kL = 50;

DrawingDocument oneView() {
    DrawingDocument d;
    SheetDef sd;
    sd.id = "S1";
    sd.format = {GostFormat::A4, 1, Orientation::Portrait};
    ViewDef f;
    f.id = "F";
    sd.views = {f};
    d.sheets().push_back(std::move(sd));
    return d;
}

m::DocumentProperties rodProperties() {
    m::DocumentProperties p;
    p.designation = "АБВГ.711141.002";
    p.name = "Ось";
    p.material = "steel_s235";
    p.density = 7850.0;
    p.developer = "Сидоров";
    p.organization = "АО «Завод»";
    p.note = "Закалить";
    return p;
}

TitleBlockData titleAfterUpdate(DrawingDocument& d, const m::PartDocument& part, views::ViewCache& cache) {
    auto snap = snapshotPart(part, d.model().properties);
    EXPECT_TRUE(snap.ok()) << snap.error().message;
    auto rep = d.update(snap.value(), cache);
    EXPECT_TRUE(rep.ok()) << rep.error().message;
    return d.computedSheets().at(0).title;
}

std::string massText(double kg) { return annotations::formatNumber(kg, kg < 1 ? 3 : 2); }

}  // namespace

TEST(PartPropertiesTitle, CreateUpdateAndManualPriority) {
    auto part = makeRod(kR, kL);
    ASSERT_TRUE(part->setProperties(rodProperties()).ok());
    DrawingDocument d = oneView();
    views::ViewCache cache(8, 0);

    // Создание: графы 1, 2, 3, масса, 9 и 11 — из документа детали.
    TitleBlockData t = titleAfterUpdate(d, *part, cache);
    EXPECT_EQ(t.designation, "АБВГ.711141.002");
    EXPECT_EQ(t.name, "Ось");
    EXPECT_EQ(t.material, "S235 (EN 10025-2)");
    EXPECT_EQ(t.organization, "АО «Завод»");
    EXPECT_EQ(t.developer.surname, "Сидоров");
    EXPECT_EQ(t.mass, massText(kPi * kR * kR * kL * 1e-9 * 7850.0));
    EXPECT_EQ(d.computedSheets()[0].isoTitle.legalOwner, "АО «Завод»");
    EXPECT_EQ(d.computedSheets()[0].isoTitle.creator, "Сидоров");

    // Обновление по модели: новое обозначение, наименование и плотность (алюминий 2700).
    m::DocumentProperties p = rodProperties();
    p.designation = "АБВГ.711141.003";
    p.name = "Ось удлинённая";
    p.material = "al_6061_t6";
    p.density.reset();
    ASSERT_TRUE(part->setProperties(p).ok());
    t = titleAfterUpdate(d, *part, cache);
    EXPECT_EQ(t.designation, "АБВГ.711141.003");
    EXPECT_EQ(t.name, "Ось удлинённая");
    EXPECT_EQ(t.material, "6061-T6");
    EXPECT_EQ(t.mass, massText(kPi * kR * kR * kL * 1e-9 * 2700.0));

    // Ручная правка надписи (графы листа) имеет приоритет и переживает обновление.
    d.sheets()[0].title.designation = "РУЧН.000.001";
    d.sheets()[0].title.organization = "Своя организация";
    t = titleAfterUpdate(d, *part, cache);
    EXPECT_EQ(t.designation, "РУЧН.000.001");
    EXPECT_EQ(t.organization, "Своя организация");
    EXPECT_EQ(t.name, "Ось удлинённая");

    // Свойства, записанные в чертеже, — только для полей, пустых в документе детали.
    d.sheets()[0].title = TitleFields{};
    d.model().properties.designation = "СТАР.000.001";
    d.model().properties.name = "Старое имя";
    p.name.clear();
    ASSERT_TRUE(part->setProperties(p).ok());
    t = titleAfterUpdate(d, *part, cache);
    EXPECT_EQ(t.designation, "АБВГ.711141.003");
    EXPECT_EQ(t.name, "Старое имя");
}

TEST(PartPropertiesTitle, ConfigurationSuffixCompletesTheDesignation) {
    auto part = makeRod(kR, kL);
    m::DocumentProperties p = rodProperties();
    ASSERT_TRUE(part->setProperties(p).ok());
    m::ConfigurationTable table;
    table.columns = {{m::ConfigurationColumnKind::Variable, "L"}};
    table.rows = {{"-00", "", {"50"}}, {"-01", "Ось длинная", {"80"}}};
    table.active = "-01";
    ASSERT_TRUE(part->setConfigurationTable(table).ok());
    ASSERT_TRUE(part->rebuild().failed.empty());
    DrawingDocument d = oneView();
    views::ViewCache cache(8, 0);
    TitleBlockData t = titleAfterUpdate(d, *part, cache);
    EXPECT_EQ(t.designation, "АБВГ.711141.002-01");
    EXPECT_EQ(t.name, "Ось длинная");
    EXPECT_EQ(t.mass, massText(kPi * kR * kR * 80.0 * 1e-9 * 7850.0));
}

TEST(PartPropertiesSpecification, DesignationNameAndNoteColumns) {
    auto part = makeRod(kR, kL);
    ASSERT_TRUE(part->setProperties(rodProperties()).ok());
    asmv::AssemblyDocument doc("Узел оси");
    auto data = asmv::partDataFromDocument(*part);
    ASSERT_TRUE(data.ok());
    auto def = doc.addPart("", data.value());
    ASSERT_TRUE(def.ok());
    ASSERT_TRUE(doc.addOccurrence(def.value(), asmv::Transform::identity()).ok());
    ASSERT_TRUE(doc.addOccurrence(def.value(), asmv::Transform::fromTranslation({0, 40, 0})).ok());
    annotations::SpecificationOptions plain;
    plain.addAssemblyDrawing = false;
    auto spec = annotations::buildSpecification(doc, plain);
    ASSERT_TRUE(spec.ok()) << spec.error().message;
    const annotations::SpecSectionData* parts = spec.value().section(annotations::SpecSection::Parts);
    ASSERT_NE(parts, nullptr);
    ASSERT_EQ(parts->items.size(), 1u);
    EXPECT_EQ(parts->items[0].designation, "АБВГ.711141.002");
    EXPECT_EQ(parts->items[0].name, "Ось");
    EXPECT_EQ(parts->items[0].quantity, std::optional<int>(2));
    EXPECT_EQ(parts->items[0].note, "Закалить");
    // Примечание, заданное в спецификации, заменяет примечание детали.
    annotations::SpecificationOptions o = plain;
    o.notes[def.value()] = "См. ТТ";
    auto spec2 = annotations::buildSpecification(doc, o);
    ASSERT_TRUE(spec2.ok());
    EXPECT_EQ(spec2.value().section(annotations::SpecSection::Parts)->items[0].note, "См. ТТ");
}
