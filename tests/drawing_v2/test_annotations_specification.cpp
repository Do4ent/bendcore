// Спецификация ГОСТ 2.106 из BOM тестовой сборки: разделы и их порядок, сортировка внутри
// разделов, сквозные позиции, количества на сборку; листы (форма 2 / 2а, ёмкость, перенос);
// перечень ISO 7573; синхронизация позиций с выносками.

#include <bendcore/drawing_v2/specification.hpp>

#include <BRepPrimAPI_MakeBox.hxx>

#include <gtest/gtest.h>

using namespace bendcore::drawing_v2;
using namespace bendcore::drawing_v2::annotations;
namespace asm2 = bendcore::assembly_v2;

namespace {

TopoDS_Shape box(double a) { return BRepPrimAPI_MakeBox(a, a, a).Shape(); }

// Редуктор: корпус, вал, крышка в сборе (крышка + прокладка), 4 болта М10, 6 болтов М6,
// 6 шайб, кольцо (прочее изделие).
struct Gearbox {
    asm2::AssemblyDocument doc{"Gearbox"};
    asm2::DefinitionId housing = 0, shaft = 0, cover = 0, gasket = 0, coverAsm = 0, boltM10 = 0, boltM6 = 0,
                       washer = 0, ring = 0;
    Gearbox(int extraStandard = 0) {
        EXPECT_TRUE(doc.setProperties(doc.root(), {"РД.00.000", "Редуктор", "", std::nullopt}).ok());
        housing = doc.addPart("Housing", {box(50), nullptr}, {"РД.00.001", "Корпус", "СЧ20", std::nullopt}).value();
        shaft = doc.addPart("Shaft", {box(10), nullptr}, {"РД.00.002", "Вал", "Сталь 45", std::nullopt}).value();
        cover = doc.addPart("Cover", {box(20), nullptr}, {"РД.01.001", "Крышка", "СЧ20", std::nullopt}).value();
        gasket = doc.addPart("Gasket", {box(5), nullptr}, {"РД.01.002", "Прокладка", "Паронит", std::nullopt}).value();
        coverAsm = doc.addSubassembly("CoverAsm", {"РД.01.000", "Крышка в сборе", "", std::nullopt}).value();
        boltM10 = doc.addPart("BoltM10", {box(3), nullptr}, {"", "Болт М10×30 ГОСТ 7798-70", "", std::nullopt}).value();
        boltM6 = doc.addPart("BoltM6", {box(2), nullptr}, {"", "Болт М6×20 ГОСТ 7798-70", "", std::nullopt}).value();
        washer = doc.addPart("Washer", {box(1), nullptr}, {"", "Шайба 6 ГОСТ 11371-78", "", std::nullopt}).value();
        ring = doc.addPart("Ring", {box(4), nullptr}, {"", "Кольцо 040-045-30", "", std::nullopt}).value();
        const auto I = asm2::Transform::identity();
        EXPECT_TRUE(doc.addOccurrence(cover, I, "", coverAsm).ok());
        EXPECT_TRUE(doc.addOccurrence(gasket, I, "", coverAsm).ok());
        // Порядок вставки нарочно не совпадает с порядком спецификации.
        for (int i = 0; i < 6; ++i) EXPECT_TRUE(doc.addOccurrence(boltM6, I).ok());
        EXPECT_TRUE(doc.addOccurrence(shaft, I).ok());
        for (int i = 0; i < 4; ++i) EXPECT_TRUE(doc.addOccurrence(boltM10, I).ok());
        EXPECT_TRUE(doc.addOccurrence(housing, I).ok());
        EXPECT_TRUE(doc.addOccurrence(coverAsm, I).ok());
        for (int i = 0; i < 6; ++i) EXPECT_TRUE(doc.addOccurrence(washer, I).ok());
        EXPECT_TRUE(doc.addOccurrence(ring, I).ok());
        for (int i = 0; i < extraStandard; ++i) {
            auto d = doc.addPart("Pin" + std::to_string(i), {box(1), nullptr},
                                 {"", "Штифт 3×" + std::to_string(10 + i) + " ГОСТ 3128-70", "", std::nullopt});
            EXPECT_TRUE(doc.addOccurrence(d.value(), I).ok());
        }
    }
    SpecificationOptions options() const {
        SpecificationOptions o;
        o.sectionOverride[ring] = SpecSection::OtherProducts;
        o.formats = {{housing, "А2"}, {shaft, "А3"}, {coverAsm, "А4"}};
        return o;
    }
};

}  // namespace

TEST(AnnotationsSpecification, SectionsOrderAndPositions) {
    Gearbox g;
    auto spec = buildSpecification(g.doc, g.options());
    ASSERT_TRUE(spec.ok()) << spec.error().message;
    const auto& S = spec.value();
    EXPECT_EQ(S.designation, "РД.00.000");
    EXPECT_EQ(S.name, "Редуктор");
    // Порядок разделов ГОСТ 2.106: Документация, Сборочные единицы, Детали, Стандартные
    // изделия, Прочие изделия (пустые разделы не выводятся).
    ASSERT_EQ(S.sections.size(), 5u);
    EXPECT_EQ(S.sections[0].section, SpecSection::Documentation);
    EXPECT_EQ(S.sections[1].section, SpecSection::AssemblyUnits);
    EXPECT_EQ(S.sections[2].section, SpecSection::Parts);
    EXPECT_EQ(S.sections[3].section, SpecSection::StandardProducts);
    EXPECT_EQ(S.sections[4].section, SpecSection::OtherProducts);

    const auto& docs = S.sections[0].items;
    ASSERT_EQ(docs.size(), 1u);
    EXPECT_EQ(docs[0].designation, "РД.00.000 СБ");
    EXPECT_EQ(docs[0].name, "Сборочный чертёж");
    EXPECT_EQ(docs[0].position, 0);
    EXPECT_FALSE(docs[0].quantity);

    // Сборочные единицы и детали — по возрастанию обозначений; позиции сквозные.
    ASSERT_EQ(S.sections[1].items.size(), 1u);
    EXPECT_EQ(S.sections[1].items[0].designation, "РД.01.000");
    EXPECT_EQ(S.sections[1].items[0].position, 1);
    EXPECT_EQ(S.sections[1].items[0].format, "А4");
    ASSERT_EQ(S.sections[2].items.size(), 2u);
    EXPECT_EQ(S.sections[2].items[0].name, "Корпус");
    EXPECT_EQ(S.sections[2].items[0].position, 2);
    EXPECT_EQ(S.sections[2].items[1].name, "Вал");
    EXPECT_EQ(S.sections[2].items[1].position, 3);
    // Состав подсборки в спецификацию сборки не входит (у неё своя спецификация).
    for (const auto& s : S.sections)
        for (const auto& it : s.items) EXPECT_NE(it.name, "Прокладка");

    // Стандартные изделия — по наименованию, М6 раньше М10; количества на сборку.
    const auto& stdp = S.sections[3].items;
    ASSERT_EQ(stdp.size(), 3u);
    EXPECT_EQ(stdp[0].name, "Болт М6×20 ГОСТ 7798-70");
    EXPECT_EQ(*stdp[0].quantity, 6);
    EXPECT_EQ(stdp[0].position, 4);
    EXPECT_EQ(stdp[1].name, "Болт М10×30 ГОСТ 7798-70");
    EXPECT_EQ(*stdp[1].quantity, 4);
    EXPECT_EQ(stdp[2].name, "Шайба 6 ГОСТ 11371-78");
    EXPECT_EQ(stdp[2].position, 6);
    EXPECT_EQ(S.sections[4].items[0].position, 7);
    EXPECT_EQ(*S.positionOf(g.shaft), 3);
    EXPECT_FALSE(S.positionOf(g.gasket));

    // Резерв номеров после разделов.
    auto o = g.options();
    o.reservePositions = 2;
    auto reserved = buildSpecification(g.doc, o).value();
    EXPECT_EQ(*reserved.positionOf(g.housing), 4);  // 1 (СЕ), +2 резерв → 4
    EXPECT_TRUE(naturalLess("М6", "М10"));
    EXPECT_FALSE(naturalLess("М10", "М6"));
    EXPECT_TRUE(naturalLess("РД.00.002", "РД.01.000"));
}

TEST(AnnotationsSpecification, SheetsAndPagination) {
    Gearbox g(30);  // 30 штифтов — спецификация на два листа
    auto spec = buildSpecification(g.doc, g.options()).value();
    auto doc = renderSpecification(spec);
    ASSERT_TRUE(doc.ok()) << doc.error().message;
    // Ёмкость: (287 − 40 − 15)/8 = 29 строк на первом листе, (287 − 15 − 15)/8 = 32 на остальных.
    EXPECT_EQ(doc.value().firstSheetRows, 29);
    EXPECT_EQ(doc.value().otherSheetRows, 32);
    // Наименования стандартных изделий не помещаются в 61 мм графы при h = 3,5 (оценка ширины
    // шрифта типа Б) и переносятся на вторую строку — листов не меньше двух.
    ASSERT_GE(doc.value().sheets.size(), 2u);
    ASSERT_EQ(doc.value().rows.size(), doc.value().sheets.size());
    EXPECT_LE(doc.value().rows[0].size(), 29u);
    for (std::size_t p = 1; p < doc.value().rows.size(); ++p) EXPECT_LE(doc.value().rows[p].size(), 32u);
    bool continuation = false;
    for (const auto& page : doc.value().rows)
        for (const auto& r : page)
            if (r.continuation) {
                continuation = true;
                EXPECT_TRUE(r.cells[2].empty());  // номер позиции — только в первой строке
            }
    EXPECT_TRUE(continuation);
    // Порядок строк: заголовки разделов в порядке ГОСТ, после заголовка — пустая строка.
    std::vector<std::string> headers;
    int positions = 0, lastPos = 0;
    for (const auto& page : doc.value().rows) {
        for (std::size_t i = 0; i < page.size(); ++i) {
            if (page[i].kind == SpecRowKind::Header) {
                headers.push_back(page[i].cells[4]);
                ASSERT_LT(i + 2, page.size()) << "header must not end a sheet";
                EXPECT_EQ(page[i + 1].kind, SpecRowKind::Blank);
                EXPECT_EQ(page[i + 2].kind, SpecRowKind::Item);
            }
            if (page[i].kind == SpecRowKind::Item && !page[i].cells[2].empty()) {
                const int p = std::stoi(page[i].cells[2]);
                EXPECT_EQ(p, lastPos + 1);
                lastPos = p;
                ++positions;
            }
        }
    }
    EXPECT_EQ(headers, (std::vector<std::string>{"Документация", "Сборочные единицы", "Детали", "Стандартные изделия",
                                                 "Прочие изделия"}));
    EXPECT_EQ(positions, 37);
    // Лист 1 — основная надпись формы 2 (185×40), лист 2 — 2а (185×15): графа «Разраб.» только на первом.
    auto hasText = [](const Drawing& d, const std::string& s) {
        for (const auto& e : d.entities())
            if (const auto* t = std::get_if<Text>(&e.geometry); t && t->utf8 == s) return true;
        return false;
    };
    EXPECT_TRUE(hasText(doc.value().sheets[0], "Разраб."));
    EXPECT_FALSE(hasText(doc.value().sheets[1], "Разраб."));
    EXPECT_TRUE(hasText(doc.value().sheets[0], "РД.00.000"));
    EXPECT_TRUE(hasText(doc.value().sheets[1], "РД.00.000"));
    EXPECT_TRUE(hasText(doc.value().sheets[0], "Наименование"));
}

TEST(AnnotationsSpecification, PartsListAndBalloons) {
    Gearbox g;
    auto spec = buildSpecification(g.doc, g.options()).value();
    Drawing d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
    auto lay = addFrameAndTitleBlock(d, TitleBlockData{});
    ASSERT_TRUE(lay.ok());
    auto table = addPartsList(d, lay.value(), spec);
    ASSERT_TRUE(table.ok()) << table.error().message;
    // 7 позиций + заголовок по 8 мм над основной надписью, ширина 185.
    EXPECT_NEAR(table.value().y0, lay.value().titleBlock.y1, 1e-9);
    EXPECT_NEAR(table.value().height(), 64.0, 1e-9);
    EXPECT_NEAR(table.value().width(), 185.0, 1e-9);
    // Позиции снизу вверх: «1» ниже «7».
    double y1 = -1, y7 = -1;
    for (const auto& e : d.entities()) {
        if (const auto* t = std::get_if<Text>(&e.geometry); t && e.layer == "TABLE" && t->position.x < table.value().x0 + 15) {
            if (t->utf8 == "1") y1 = t->position.y;
            if (t->utf8 == "7") y7 = t->position.y;
        }
    }
    EXPECT_GT(y7, y1);
    EXPECT_GT(y1, table.value().y0 + 8);

    std::vector<Balloon> balloons = {{g.shaft, {0, 0}, {10, 10}, ""}, {g.housing, {0, 0}, {20, 10}, ""},
                                     {g.boltM6, {0, 0}, {30, 10}, ""}};
    auto rep = synchronizeBalloons(balloons, spec);
    ASSERT_TRUE(rep.ok());
    EXPECT_EQ(balloons[0].text, "3");
    EXPECT_EQ(balloons[1].text, "2");
    EXPECT_EQ(balloons[2].text, "4");
    EXPECT_EQ(rep.value().withoutBalloon.size(), 4u);  // крышка в сборе, М10, шайба, кольцо
    // После изменения состава номера пересчитываются: новая деталь с меньшим обозначением.
    auto early = g.doc.addPart("Base", {box(7), nullptr}, {"РД.00.0005", "Основание", "", std::nullopt}).value();
    ASSERT_TRUE(g.doc.addOccurrence(early, asm2::Transform::identity()).ok());
    spec = buildSpecification(g.doc, g.options()).value();
    ASSERT_TRUE(synchronizeBalloons(balloons, spec).ok());
    EXPECT_EQ(balloons[1].text, "2");  // РД.00.001 < РД.00.0005 (по значению 1 < 5)
    EXPECT_EQ(balloons[0].text, "3");  // РД.00.002 < РД.00.0005
    EXPECT_EQ(balloons[2].text, "5");
    std::vector<Balloon> bad = {{g.gasket, {0, 0}, {1, 1}, ""}};
    EXPECT_FALSE(synchronizeBalloons(bad, spec).ok());
}
