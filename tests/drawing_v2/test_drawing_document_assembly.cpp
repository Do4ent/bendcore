// Сквозной тест фазы 4 для сборки: узел «вал + втулка фланцевая + крепёж» (болты ГОСТ 7798,
// шайбы ГОСТ 11371, гайки ГОСТ 5915 из bendcore_fasteners) → сборочный чертёж с позициями по
// BOM (ГОСТ 2.109) и спецификация ГОСТ 2.106 отдельным документом; комплект — один PDF
// (лист А3 + лист спецификации А4), DXF и SVG каждого листа. Проверки: состав и номера позиций
// против построения сборки, выноски на изображениях своих деталей, перенумерация при изменении
// состава, pdfinfo (страницы, форматы), pdftotext (кириллица), ezdxf.

#include "test_drawing_document_support.hpp"

#include <bendcore/assembly_v2/fasteners.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <set>

using namespace ddtest;
using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::fasteners;

namespace {

struct Unit {
    std::shared_ptr<AssemblyDocument> doc = std::make_shared<AssemblyDocument>("Узел вала");
    DefinitionId shaft = 0, bushing = 0, bolt = 0, washer = 0, nut = 0;
    OccurrenceId shaftOcc = 0, bushingOcc = 0;
};

Unit buildUnit() {
    Unit u;
    auto rod = makeRod(15, 120);
    auto bush = makeBushing();
    EXPECT_TRUE(u.doc->setProperties(u.doc->root(), {"ВТ.01.000", "Узел вала", "", std::nullopt}).ok());
    u.shaft = u.doc->addPart("Вал", partDataFromDocument(*rod).value(), {"ВТ.01.001", "Вал", "Сталь 45", 7850}).value();
    u.bushing = u.doc->addPart("Втулка", partDataFromDocument(*bush).value(), {"ВТ.01.002", "Втулка", "Сталь 20", 7850}).value();
    u.bushingOcc = u.doc->addOccurrence(u.bushing, Transform::identity(), "Втулка").value();
    EXPECT_TRUE(u.doc->setFixed(u.bushingOcc, true).ok());
    u.shaftOcc = u.doc->addOccurrence(u.shaft, Transform::fromTranslation(bendcore::assembly_v2::Vec3(0, 0, -40)), "Вал").value();
    u.bolt = addFastenerPart(*u.doc, makeFastener({FastenerStandard::Gost7798, 10, 30}).value()).value();
    u.washer = addFastenerPart(*u.doc, makeFastener({FastenerStandard::Gost11371, 10, 0}).value()).value();
    u.nut = addFastenerPart(*u.doc, makeFastener({FastenerStandard::Gost5915, 10, 0}).value()).value();
    auto sites = findHoleSites(*u.doc, {u.bushingOcc}, bendcore::assembly_v2::Vec3(0, 0, -1), 5.5);
    EXPECT_TRUE(sites.ok()) << (sites.ok() ? "" : sites.error().message);
    EXPECT_EQ(sites.value().size(), 4u);
    auto placed = insertFastenerStacks(*u.doc, {u.bolt, {u.washer}, {u.washer}, u.nut}, sites.value());
    EXPECT_TRUE(placed.ok()) << (placed.ok() ? "" : placed.error().message);
    return u;
}

DrawingDocument assemblyDrawing() {
    DrawingDocument d;
    d.model().kind = ModelKind::Assembly;
    d.model().path = "unit.json";
    SheetDef s;
    s.id = "S1";
    s.format = {GostFormat::A3, 1, Orientation::Landscape};
    s.title.organization = "bendcore";
    ViewDef f;
    f.id = "F";
    f.hidden = false;
    f.autoBalloons = true;
    s.views.push_back(f);
    ViewDef t;
    t.id = "T";
    t.kind = ViewKind::Projected;
    t.parent = "F";
    t.side = views::SheetSide::Below;
    t.hidden = false;
    s.views.push_back(t);
    ViewDef iso;
    iso.id = "I";
    iso.kind = ViewKind::Axonometric;
    iso.parent = "F";
    iso.hidden = false;
    iso.centerLines = false;
    s.views.push_back(iso);
    s.technicalRequirements = {"* Размеры для справок.", "Болты затянуть равномерно."};
    d.sheets().push_back(std::move(s));
    return d;
}

std::string pdfInfo(const std::string& path) {
#ifdef BENDCORE_PDFINFO
    int code = 0;
    std::string out = dv2test::runCommand(std::string(BENDCORE_PDFINFO) + " -f 1 -l 9 '" + path + "'", &code);
    EXPECT_EQ(code, 0);
    return out;
#else
    (void)path;
    return {};
#endif
}

std::string pdfText(const std::string& path) {
#ifdef BENDCORE_PDFTOTEXT
    int code = 0;
    std::string out = dv2test::runCommand(std::string(BENDCORE_PDFTOTEXT) + " '" + path + "' -", &code);
    EXPECT_EQ(code, 0);
    return out;
#else
    (void)path;
    return {};
#endif
}

}  // namespace

TEST(DrawingDocumentAssembly, AssemblyDrawingWithPositionsAndSpecification) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    Unit u = buildUnit();
    auto snap = snapshotAssembly(u.doc);
    ASSERT_TRUE(snap.ok()) << snap.error().message;
    ASSERT_TRUE(snap.value().massKg.has_value());

    // Спецификация — отдельный документ.
    SpecificationDocument spec;
    spec.model().kind = ModelKind::Assembly;
    spec.model().path = "unit.json";
    spec.settings().formats = {{"ВТ.01.001", "А4"}, {"ВТ.01.002", "А4"}};
    spec.title().organization = "bendcore";
    auto su = spec.update(snap.value());
    ASSERT_TRUE(su.ok()) << su.error().message;
    EXPECT_TRUE(su.value());
    const annotations::Specification& sp = *spec.specification();
    EXPECT_EQ(sp.designation, "ВТ.01.000");
    // Разделы и позиции по ГОСТ 2.106: документация без позиции, детали по обозначению,
    // стандартные изделия по наименованию.
    ASSERT_EQ(sp.sections.size(), 3u);
    EXPECT_EQ(sp.sections[0].section, annotations::SpecSection::Documentation);
    EXPECT_EQ(sp.sections[0].items[0].designation, "ВТ.01.000 СБ");
    ASSERT_EQ(sp.sections[1].items.size(), 2u);
    EXPECT_EQ(sp.positionOf(u.shaft), 1);
    EXPECT_EQ(sp.positionOf(u.bushing), 2);
    EXPECT_EQ(sp.sections[1].items[0].format, "А4");
    ASSERT_EQ(sp.sections[2].items.size(), 3u);
    std::map<DefinitionId, int> qty;
    for (const auto& it : sp.sections[2].items) qty[*it.definition] = *it.quantity;
    EXPECT_EQ(qty[u.bolt], 4);
    EXPECT_EQ(qty[u.nut], 4);
    EXPECT_EQ(qty[u.washer], 8);
    std::set<int> stdPositions = {*sp.positionOf(u.bolt), *sp.positionOf(u.nut), *sp.positionOf(u.washer)};
    EXPECT_EQ(stdPositions, (std::set<int>{3, 4, 5}));

    DrawingDocument d = assemblyDrawing();
    d.setFont(font);
    views::ViewCache cache(32, 4);
    auto rep = d.update(snap.value(), cache, spec.specification());
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const ComputedSheet cs = d.computedSheets()[0];
    EXPECT_EQ(cs.title.designation, "ВТ.01.000 СБ");
    EXPECT_EQ(cs.title.name, "Узел вала");
    EXPECT_EQ(cs.title.material, "");
    // Масса по BOM: Σ объём · плотность.
    double mass = 0;
    const auto placedParts = u.doc->placedParts();
    ASSERT_TRUE(placedParts.ok());
    for (const PlacedOccurrence& p : placedParts.value()) {
        mass += std::abs(bendcore::kernel::Shape(p.shape).volumeProperties().value().volume) * 1e-9 * 7850.0;
    }
    EXPECT_NEAR(*snap.value().massKg, mass, 1e-9 * mass);
    EXPECT_EQ(cs.title.mass, annotations::formatNumber(mass, mass < 1 ? 3 : 2));

    // Позиции: на главном виде видна каждая составная часть, номера — по спецификации.
    ASSERT_EQ(rep.value().balloons.size(), 1u);
    const BalloonReport& br = rep.value().balloons[0];
    EXPECT_TRUE(br.withoutBalloon.empty());
    ASSERT_EQ(cs.balloons.size(), 5u);
    std::set<int> numbers;
    const ComputedView& front = cs.views[0];
    for (const annotations::Balloon& b : cs.balloons) {
        numbers.insert(std::stoi(b.text));
        EXPECT_EQ(std::stoi(b.text), *sp.positionOf(b.definition));
        // Точка выноски лежит на видимой линии изображения своей детали.
        bool onOwnPart = false;
        for (const views::ViewEdge& e : front.sheetView.view.edges) {
            if (e.visibility != views::Visibility::Visible || !e.edgeIndex) continue;
            if (snap.value().edgeOwner[*e.edgeIndex] != b.definition) continue;
            if (const auto* l = std::get_if<LineSeg>(&e.curve)) {
                const Point2 a = front.sheetView.toSheet(l->a), c = front.sheetView.toSheet(l->b);
                const double cross = (c.x - a.x) * (b.target.y - a.y) - (c.y - a.y) * (b.target.x - a.x);
                const double len = std::hypot(c.x - a.x, c.y - a.y);
                if (len > 0 && std::abs(cross) / len < 1e-6) onOwnPart = true;
            }
        }
        EXPECT_TRUE(onOwnPart) << "balloon " << b.text;
    }
    EXPECT_EQ(numbers, (std::set<int>{1, 2, 3, 4, 5}));
    // Полки — в колонну (ГОСТ 2.109), шаг не менее 10 мм.
    for (std::size_t i = 1; i < cs.balloons.size(); ++i) {
        EXPECT_NEAR(cs.balloons[i].at.x, cs.balloons[0].at.x, 1e-9);
        EXPECT_GE(cs.balloons[i - 1].at.y - cs.balloons[i].at.y, 10.0 - 1e-9);
    }

    // Комплект: PDF (чертёж + спецификация), DXF и SVG каждого листа.
    const std::string dir = dv2test::outPath("assembly");
    ExportKitOptions ko;
    ko.directory = dir;
    ko.baseName = "unit";
    ko.font = font;
    auto kit = exportKit(d, &spec, ko);
    ASSERT_TRUE(kit.ok()) << kit.error().message;
    EXPECT_EQ(kit.value().drawingPages, 1);
    EXPECT_EQ(kit.value().specificationPages, 1);
    EXPECT_EQ(kit.value().files.size(), 5u);  // PDF + 2 × (DXF, SVG)
    const DxfContent dxf = readDxf(dir + "/unit_S1.dxf");
    for (int n = 1; n <= 5; ++n) EXPECT_TRUE(hasText(dxf, std::to_string(n))) << "position " << n;
    EXPECT_TRUE(hasText(dxf, "ВТ.01.000 СБ"));
    const DxfContent sdxf = readDxf(dir + "/unit_SP1.dxf");
    EXPECT_TRUE(hasText(sdxf, "Болт М10×30 ГОСТ")) << "specification DXF";
    EXPECT_NEAR(sdxf.limMaxX, 210.0, 1e-9);
    EXPECT_NEAR(sdxf.limMaxY, 297.0, 1e-9);
    if (std::string err = ezdxfCheck(dir + "/unit_S1.dxf", "ВТ.01.000 СБ"); !err.empty()) ADD_FAILURE() << err;
    if (std::string err = ezdxfCheck(dir + "/unit_SP1.dxf", "Детали"); !err.empty()) ADD_FAILURE() << err;
    if (const std::string info = pdfInfo(dir + "/unit.pdf"); !info.empty()) {
        EXPECT_NE(info.find("Pages:           2\n"), std::string::npos) << info;
        EXPECT_NE(info.find("Page    1 size:  1190.55 x 841.89 pts (A3)"), std::string::npos) << info;
        EXPECT_NE(info.find("Page    2 size:  595.276 x 841.89 pts (A4)"), std::string::npos) << info;
    }
    if (const std::string t = pdfText(dir + "/unit.pdf"); !t.empty()) {
        for (const char* want : {"Узел вала", "ВТ.01.000 СБ", "Сборочный чертёж", "Детали", "Стандартные изделия", "Втулка",
                                 "Гайка М10 ГОСТ", "Болты затянуть равномерно."})
            EXPECT_NE(t.find(want), std::string::npos) << "pdftotext: '" << want << "' missing";
    }

    // Изменение состава: вал подавлен → спецификация и позиции перенумерованы.
    ASSERT_TRUE(u.doc->setOccurrenceSuppressed(u.shaftOcc, true).ok());
    auto snap2 = snapshotAssembly(u.doc);
    ASSERT_TRUE(snap2.ok());
    EXPECT_TRUE(d.checkModel(snap2.value()).modelChanged);
    auto su2 = spec.update(snap2.value());
    ASSERT_TRUE(su2.ok());
    EXPECT_TRUE(su2.value());
    EXPECT_FALSE(spec.specification()->positionOf(u.shaft).has_value());
    EXPECT_EQ(spec.specification()->positionOf(u.bushing), 1);
    auto rep2 = d.update(snap2.value(), cache, spec.specification());
    ASSERT_TRUE(rep2.ok()) << rep2.error().message;
    EXPECT_EQ(d.computedSheets()[0].balloons.size(), 4u);
    for (const auto& b : d.computedSheets()[0].balloons) EXPECT_NE(b.definition, u.shaft);
    std::printf("[drawing document] assembly: %zu balloons, mass %s kg, specification %d sheet(s)\n", cs.balloons.size(),
                cs.title.mass.c_str(), kit.value().specificationPages);
}
