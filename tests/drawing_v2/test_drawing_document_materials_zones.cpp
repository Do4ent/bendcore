// Документ чертежа: кратные форматы ГОСТ 2.301 / ISO 5457 на листе документа, центровые метки
// кругового массива на эталонном фланце, обозначение материала в разрезе по свойствам документа
// детали и по деталям сборки (ГОСТ 2.306, табл. 1), разметка зон и графа «Зона» спецификации
// ГОСТ 2.106. Эталоны — размеры деталей и таблицы стандартов, а не результат кода.

#include "test_drawing_document_support.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gp_Ax2.hxx>

#include <gtest/gtest.h>

#include <cmath>
#include <set>

using namespace ddtest;
using namespace bendcore::assembly_v2;
namespace an = bendcore::drawing_v2::annotations;

namespace {

ViewDef view(const std::string& id, ViewKind kind) {
    ViewDef v;
    v.id = id;
    v.kind = kind;
    return v;
}

// Главный вид F (взгляд по +Y), вид сверху T, разрез А–А плоскостью y = 0 вне связи.
SheetDef sheetWithSection(SheetFormat format) {
    SheetDef s;
    s.id = "S1";
    s.format = format;
    ViewDef f = view("F", ViewKind::Base);
    f.hidden = false;
    s.views.push_back(f);
    ViewDef t = view("T", ViewKind::Projected);
    t.parent = "F";
    t.side = views::SheetSide::Below;
    t.hidden = false;
    s.views.push_back(t);
    ViewDef a = view("A", ViewKind::Section);
    a.toward = {0, -1, 0};
    a.right = {1, 0, 0};
    a.planePoint = {0, 0, 0};
    a.markView = "T";
    s.views.push_back(a);
    return s;
}

const ComputedView& computedView(const DrawingDocument& d, const std::string& id) {
    for (const ComputedView& v : d.computedSheets()[0].views)
        if (v.id == id) return v;
    throw std::runtime_error("no view " + id);
}

std::vector<const Hatch*> hatchesOf(const Drawing& d) {
    std::vector<const Hatch*> out;
    for (const bendcore::drawing_v2::Entity& e : d.entities())
        if (e.layer == "HATCH")
            if (const auto* h = std::get_if<Hatch>(&e.geometry)) out.push_back(h);
    return out;
}

TopoDS_Shape ring(double d, double D, double B, double z0) {
    const gp_Ax2 ax(gp_Pnt(0, 0, z0), gp::DZ());
    BRepAlgoAPI_Cut cut(BRepPrimAPI_MakeCylinder(ax, D / 2, B).Shape(), BRepPrimAPI_MakeCylinder(ax, d / 2, B).Shape());
    cut.Build();
    return cut.Shape();
}

// Наклон отрезка DXF в градусах 0…180.
double slopeDeg(const DxfContent::Line& l) {
    double a = std::atan2(l.y1 - l.y0, l.x1 - l.x0) * 180.0 / 3.14159265358979323846;
    if (a < 0) a += 180.0;
    if (a >= 180.0 - 1e-9) a -= 180.0;
    return a;
}

}  // namespace

TEST(DrawingDocumentMaterials, FlangeOnMultipleFormatWithPitchCircleAndPlasticHatch) {
    // Эталонный фланец (D = 120, d0 = 40, 6 отверстий Ø10 на Ø95) из полиамида PA6 — материал
    // задан в свойствах документа детали (id библиотеки ядра).
    auto flange = makeFlange();
    bendcore::modeling::DocumentProperties props;
    props.designation = "ФЛ.01.001";
    props.name = "Фланец";
    props.material = "plastic_pa6";
    ASSERT_TRUE(flange->setProperties(props).ok());
    auto snap = snapshotPart(*flange, {});
    ASSERT_TRUE(snap.ok()) << snap.error().message;

    DrawingDocument d;
    d.model().kind = ModelKind::Part;
    d.sheets().push_back(sheetWithSection({GostFormat::A4, 3, Orientation::Landscape}));
    views::ViewCache cache(16, 2);
    auto rep = d.update(snap.value(), cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const ComputedSheet& cs = d.computedSheets()[0];
    // А4×3 альбомная — 630 × 297 (ГОСТ 2.301, табл. 2).
    EXPECT_DOUBLE_EQ(cs.sheet.widthMm, 630);
    EXPECT_DOUBLE_EQ(cs.sheet.heightMm, 297);
    EXPECT_EQ(cs.title.material, "PA6 (dry)");

    // Вид сверху: центр фланца и 6 отверстий на осевой окружности Ø95.
    const ComputedView& top = computedView(d, "T");
    ASSERT_EQ(top.centers.circles.size(), 1u);
    EXPECT_NEAR(top.centers.circles[0].radius, 47.5, 1e-6);
    EXPECT_EQ(top.centers.circles[0].members.size(), 6u);

    // Разрез: обозначение неметалла — сетка под 45° и 135° (ГОСТ 2.306, табл. 1, п. 2).
    const ComputedView& sec = computedView(d, "A");
    ASSERT_TRUE(sec.section);
    ASSERT_EQ(sec.ownerMaterials.size(), 1u);
    EXPECT_EQ(sec.ownerMaterials[0], views::HatchMaterial::NonMetal);
    auto sheets = d.render(nullptr);
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    const auto hatches = hatchesOf(sheets.value()[0]);
    ASSERT_EQ(hatches.size(), 2 * sec.section->regions.size());
    int a45 = 0, a135 = 0;
    for (const Hatch* h : hatches) {
        if (std::abs(h->angleDeg - 45) < 1e-9) ++a45;
        if (std::abs(h->angleDeg - 135) < 1e-9) ++a135;
    }
    EXPECT_EQ(a45, a135);
    // Осевая окружность в листе: окружность на слое CENTER радиусом 47,5 · масштаб.
    int pitch = 0;
    for (const bendcore::drawing_v2::Entity& e : sheets.value()[0].entities())
        if (const auto* c = std::get_if<Circle>(&e.geometry); c && e.layer == "CENTER") {
            EXPECT_NEAR(c->radius, 47.5 * top.sheetView.scale, 1e-6);
            EXPECT_EQ(e.lineType, LineType::DashDotThin);
            ++pitch;
        }
    EXPECT_EQ(pitch, 1);
    // Основная надпись — в правом нижнем углу рамки кратного листа.
    bool titleCorner = false;
    for (const bendcore::drawing_v2::Entity& e : sheets.value()[0].entities())
        if (const auto* l = std::get_if<LineSeg>(&e.geometry); l && e.layer == "FRAME" && e.lineType == LineType::SolidThick)
            if (std::abs(l->a.x - (630 - 5 - 185)) < 1e-9 && std::abs(l->b.x - (630 - 5 - 185)) < 1e-9 &&
                std::abs(std::abs(l->b.y - l->a.y) - 55) < 1e-9)
                titleCorner = true;
    EXPECT_TRUE(titleCorner);

    // Вручную — древесина: волнистые ломаные вместо Hatch; сохраняется в файле.
    d.sheets()[0].views[2].hatchMaterial = views::HatchMaterial::Wood;
    auto json = d.toJson();
    ASSERT_TRUE(json.ok());
    EXPECT_NE(json.value().find("\"hatchMaterial\""), std::string::npos);
    EXPECT_NE(json.value().find("\"wood\""), std::string::npos);
    auto back = DrawingDocument::fromJson(json.value());
    ASSERT_TRUE(back.ok()) << back.error().message;
    ASSERT_TRUE(back.value().sheets()[0].views[2].hatchMaterial);
    EXPECT_EQ(*back.value().sheets()[0].views[2].hatchMaterial, views::HatchMaterial::Wood);
    EXPECT_FALSE(DrawingDocument::fromJson(std::string(json.value()).replace(json.value().find("\"wood\""), 6, "\"oak\"")).ok());
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    auto wood = d.render(nullptr);
    ASSERT_TRUE(wood.ok());
    EXPECT_TRUE(hatchesOf(wood.value()[0]).empty());
    int wavy = 0;
    for (const bendcore::drawing_v2::Entity& e : wood.value()[0].entities())
        if (e.layer == "HATCH" && std::holds_alternative<Polyline>(e.geometry)) ++wavy;
    EXPECT_GT(wavy, 0);

    // ISO: удлинённый формат ISO 5457 A3×3 — 420 × 891.
    d.settings().sheetStyle = SheetStyle::Iso7200;
    d.sheets()[0].format = {GostFormat::A3, 3, Orientation::Portrait};
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    EXPECT_EQ(d.computedSheets()[0].sheet.name, "ISO A3x3");
    EXPECT_DOUBLE_EQ(d.computedSheets()[0].sheet.widthMm, 420);
    EXPECT_DOUBLE_EQ(d.computedSheets()[0].sheet.heightMm, 891);
    ASSERT_TRUE(d.render(nullptr).ok());
}

TEST(DrawingDocumentMaterials, AssemblySectionWithPlasticPart) {
    // Втулка (сталь) и прилегающее к ней пластмассовое кольцо Ø50/Ø90 × 10 на диске втулки
    // (z = 12…22, вокруг ступицы Ø50): в разрезе у втулки — линии под 45°, у кольца — сетка.
    auto doc = std::make_shared<AssemblyDocument>("Узел");
    ASSERT_TRUE(doc->setProperties(doc->root(), {"УЗ.01.000", "Узел", "", std::nullopt}).ok());
    auto bush = makeBushing();
    const DefinitionId steel = doc->addPart("Втулка", partDataFromDocument(*bush).value(), {"УЗ.01.001", "Втулка", "Сталь 20", 7850}).value();
    const DefinitionId plastic =
        doc->addPart("Кольцо", {ring(50, 90, 10, 12), nullptr}, {"УЗ.01.002", "Кольцо", "Капролон В ТУ 2224-036-00203803", 1150}).value();
    const OccurrenceId so = doc->addOccurrence(steel, Transform::identity(), "Втулка").value();
    ASSERT_TRUE(doc->setFixed(so, true).ok());
    doc->addOccurrence(plastic, Transform::identity(), "Кольцо").value();
    auto snap = snapshotAssembly(doc);
    ASSERT_TRUE(snap.ok()) << snap.error().message;
    const ModelSnapshot& m = snap.value();

    DrawingDocument d;
    d.model().kind = ModelKind::Assembly;
    d.sheets().push_back(sheetWithSection({GostFormat::A3, 1, Orientation::Landscape}));
    views::ViewCache cache(16, 2);
    auto rep = d.update(m, cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const ComputedView& sec = computedView(d, "A");
    ASSERT_TRUE(sec.section);
    ASSERT_EQ(sec.ownerMaterials.size(), m.childOwner.size());
    for (std::size_t c = 0; c < m.childOwner.size(); ++c)
        EXPECT_EQ(sec.ownerMaterials[c], m.childOwner[c] == plastic ? views::HatchMaterial::NonMetal : views::HatchMaterial::Metal);

    // DXF листа: отрезки штриховки в области кольца (|x| 25…45, z 12…22) — обоих наклонов, в
    // сечении втулки ниже (z 0…12) — только 45°.
    const std::string dir = dv2test::outPath("materials_assembly");
    ExportKitOptions ko;
    ko.directory = dir;
    ko.baseName = "unit";
    ko.pdf = false;
    ko.svg = false;
    auto kit = exportKit(d, nullptr, ko);
    ASSERT_TRUE(kit.ok()) << kit.error().message;
    const DxfContent dxf = readDxf(dir + "/unit_S1.dxf");
    const views::SheetView& sv = sec.sheetView;
    auto toModel = [&](double x, double y) { return Point2{(x - sv.anchor.x) / sv.scale, (y - sv.anchor.y) / sv.scale}; };
    std::set<int> ringSlopes, steelSlopes;
    int ringLines = 0, steelLines = 0;
    for (const DxfContent::Line& l : dxf.lines) {
        if (l.layer != "HATCH") continue;
        const Point2 p = toModel((l.x0 + l.x1) / 2, (l.y0 + l.y1) / 2);
        const int slope = int(std::lround(slopeDeg(l)));
        if (std::abs(p.x) > 25 + 0.5 && std::abs(p.x) < 45 - 0.5 && p.y > 12 + 0.5 && p.y < 22 - 0.5) {
            ringSlopes.insert(slope);
            ++ringLines;
        } else if (std::abs(p.x) > 25 + 0.5 && std::abs(p.x) < 45 - 0.5 && p.y > 0.5 && p.y < 12 - 0.5) {
            steelSlopes.insert(slope);
            ++steelLines;
        }
    }
    EXPECT_GT(ringLines, 0);
    EXPECT_GT(steelLines, 0);
    EXPECT_EQ(ringSlopes, (std::set<int>{45, 135}));
    EXPECT_EQ(steelSlopes, (std::set<int>{45}));
}

TEST(DrawingDocumentZones, ZoneMarkingAndSpecificationZoneColumn) {
    // Втулка и вал; позиции на главном виде; зоны ЕСКД на А3 альбомной (рамка 20…415 × 5…292).
    auto doc = std::make_shared<AssemblyDocument>("Узел вала");
    ASSERT_TRUE(doc->setProperties(doc->root(), {"ВТ.02.000", "Узел вала", "", std::nullopt}).ok());
    auto rod = makeRod(15, 120);
    auto bush = makeBushing();
    const DefinitionId shaft = doc->addPart("Вал", partDataFromDocument(*rod).value(), {"ВТ.02.001", "Вал", "Сталь 45", 7850}).value();
    const DefinitionId bushing = doc->addPart("Втулка", partDataFromDocument(*bush).value(), {"ВТ.02.002", "Втулка", "Сталь 20", 7850}).value();
    const OccurrenceId bo = doc->addOccurrence(bushing, Transform::identity(), "Втулка").value();
    ASSERT_TRUE(doc->setFixed(bo, true).ok());
    doc->addOccurrence(shaft, Transform::fromTranslation(bendcore::assembly_v2::Vec3(0, 0, -40)), "Вал").value();
    auto snap = snapshotAssembly(doc);
    ASSERT_TRUE(snap.ok()) << snap.error().message;

    SpecificationDocument spec;
    spec.model().kind = ModelKind::Assembly;
    ASSERT_TRUE(spec.update(snap.value()).ok());

    DrawingDocument d;
    d.model().kind = ModelKind::Assembly;
    SheetDef s;
    s.id = "S1";
    s.format = {GostFormat::A3, 1, Orientation::Landscape};
    ViewDef f = view("F", ViewKind::Base);
    f.hidden = false;
    f.autoBalloons = true;
    s.views.push_back(f);
    d.sheets().push_back(s);
    d.settings().zones.enabled = true;
    views::ViewCache cache(16, 2);
    auto rep = d.update(snap.value(), cache, spec.specification());
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const ComputedSheet& cs = d.computedSheets()[0];
    ASSERT_TRUE(cs.zones);
    ASSERT_EQ(cs.balloons.size(), 2u);

    // Зоны позиций против границ, рассчитанных вручную (DRAWING_MODEL.md, «Зоны»).
    const double cols[] = {20, 67.5, 117.5, 167.5, 217.5, 267.5, 317.5, 367.5, 415};
    const double rows[] = {292, 248.5, 198.5, 148.5, 98.5, 48.5, 5};
    const char* letters[] = {"А", "Б", "В", "Г", "Д", "Е"};
    auto handZone = [&](Point2 p) {
        int c = 0, r = 0;
        while (c < 7 && p.x >= cols[c + 1]) ++c;
        while (r < 5 && p.y <= rows[r + 1]) ++r;
        return std::string(letters[r]) + std::to_string(c + 1);
    };
    const auto zones = d.positionZones();
    ASSERT_EQ(zones.size(), 2u);
    an::AnnotationStyle style;
    style.font = &gostStrokeFont();
    for (const an::Balloon& b : cs.balloons) {
        auto g = an::balloonGeometry(b, {}, style);
        ASSERT_TRUE(g.ok());
        const an::DimensionLabel& l = g.value().labels.front();
        EXPECT_EQ(zones.at(b.definition), handZone({l.position.x, l.position.y + l.height / 2})) << b.text;
    }

    // Спецификация: графа «Зона» заполнена у позиций, документация — без зоны.
    EXPECT_TRUE(spec.setPositionZones(zones));
    EXPECT_FALSE(spec.setPositionZones(zones));  // повтор — без изменений
    int filled = 0;
    for (const auto& sec : spec.specification()->sections)
        for (const auto& it : sec.items) {
            if (it.position == 0) {
                EXPECT_TRUE(it.zone.empty());
                continue;
            }
            EXPECT_EQ(it.zone, zones.at(*it.definition));
            ++filled;
        }
    EXPECT_EQ(filled, 2);
    // Обновление по модели сохраняет зоны.
    ASSERT_TRUE(spec.update(snap.value()).ok());
    EXPECT_EQ(spec.specification()->section(an::SpecSection::Parts)->items[0].zone, zones.at(shaft));
    auto sheets = spec.render(nullptr);
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    bool zoneCell = false;
    for (const auto& row : sheets.value().rows[0])
        if (row.kind == an::SpecRowKind::Item && row.cells[2] == "1") zoneCell = row.cells[1] == zones.at(shaft);
    EXPECT_TRUE(zoneCell);

    // Лист: обозначения зон на полях — 8 цифр и 6 букв, по два раза, в полосе 5 мм у края листа.
    auto drawn = d.render(nullptr);
    ASSERT_TRUE(drawn.ok()) << drawn.error().message;
    std::map<std::string, int> labels;
    for (const bendcore::drawing_v2::Entity& e : drawn.value()[0].entities()) {
        const auto* t = std::get_if<Text>(&e.geometry);
        if (!t || e.layer != "FRAME") continue;
        const bool band = t->position.x < 5 || t->position.x > 415 || t->position.y < 5 || t->position.y > 292;
        if (band) ++labels[t->utf8];
    }
    for (int c = 1; c <= 8; ++c) EXPECT_EQ(labels[std::to_string(c)], 2) << c;
    for (const char* l : letters) EXPECT_EQ(labels[l], 2) << l;

    // Файл: настройка зон сохраняется; выключенные зоны — пустая карта и пустая графа.
    auto json = d.toJson();
    ASSERT_TRUE(json.ok());
    auto back = DrawingDocument::fromJson(json.value());
    ASSERT_TRUE(back.ok());
    EXPECT_TRUE(back.value().settings().zones.enabled);
    d.settings().zones.enabled = false;
    ASSERT_TRUE(d.update(snap.value(), cache, spec.specification()).ok());
    EXPECT_TRUE(d.positionZones().empty());
    EXPECT_TRUE(spec.setPositionZones(d.positionZones()));
    for (const auto& sec : spec.specification()->sections)
        for (const auto& it : sec.items) EXPECT_TRUE(it.zone.empty());
}
