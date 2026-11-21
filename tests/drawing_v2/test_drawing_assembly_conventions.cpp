// Условности сборочного чертежа: подшипники качения в разрезе — упрощённо по ГОСТ 2.420 (контур
// сечения с диагоналями, без штриховки), зацепление зубчатых колёс по ГОСТ 2.402 (зуб шестерни
// перед зубом колеса, общая делительная, нет лишних основных линий от пересечения цилиндров
// вершин), косой зуб тремя тонкими линиями, позиции ГОСТ 2.109 на фигурах сечения и на скрытых
// деталях через ближайший разрез, разнесение полок без пересечения выносок по порядку номеров.
// Эталоны — размеры деталей и положения в сборке (аналитика), а не результат кода.

#include "test_drawing_document_support.hpp"
#include "balloon_checks.hpp"

#include <bendcore/drawing_v2/balloon_layout.hpp>
#include <bendcore/drawing_v2/bearings.hpp>
#include <bendcore/drawing_v2/export.hpp>
#include <bendcore/drawing_v2/gears.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gtest/gtest.h>

#include <cmath>
#include <set>

using namespace ddtest;
using namespace bendcore::assembly_v2;
namespace an = bendcore::drawing_v2::annotations;
namespace md = bendcore::modeling;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Кольцо d×D×B вдоль Z от z = z0.
TopoDS_Shape ring(double d, double D, double B, double z0 = 0.0) {
    const gp_Ax2 ax(gp_Pnt(0, 0, z0), gp::DZ());
    BRepAlgoAPI_Cut cut(BRepPrimAPI_MakeCylinder(ax, D / 2, B).Shape(), BRepPrimAPI_MakeCylinder(ax, d / 2, B).Shape());
    cut.Build();
    return cut.Shape();
}

// «Каталожный» шариковый подшипник: внутреннее и наружное кольца толщиной t/4 (t = (D − d)/2) и
// 8 шариков радиуса 0,24t на окружности (d + D)/4 в середине ширины — одна деталь из 10 тел.
TopoDS_Shape ballBearing(double d, double D, double B) {
    const double t = (D - d) / 2;
    BRep_Builder bb;
    TopoDS_Compound c;
    bb.MakeCompound(c);
    bb.Add(c, ring(d, d + t / 2, B));
    bb.Add(c, ring(D - t / 2, D, B));
    const double rc = (d + D) / 4;
    for (int i = 0; i < 8; ++i) {
        const double a = 2 * kPi * i / 8;
        bb.Add(c, BRepPrimAPI_MakeSphere(gp_Pnt(rc * std::cos(a), rc * std::sin(a), B / 2), 0.24 * t).Shape());
    }
    return c;
}

ViewDef view(const std::string& id, ViewKind kind) {
    ViewDef v;
    v.id = id;
    v.kind = kind;
    return v;
}

// Лист: главный вид F (взгляд по +Y), вид сверху T, разрез А–А плоскостью y = 0 справа от F.
SheetDef sheetWithSection() {
    SheetDef s;
    s.id = "S1";
    s.format = {GostFormat::A2, 1, Orientation::Landscape};
    s.views.push_back(view("F", ViewKind::Base));
    ViewDef t = view("T", ViewKind::Projected);
    t.parent = "F";
    t.side = views::SheetSide::Below;
    s.views.push_back(t);
    ViewDef a = view("A", ViewKind::Section);
    a.toward = {0, -1, 0};
    a.right = {1, 0, 0};
    a.planePoint = {0, 0, 0};
    a.markView = "T";
    a.centerLines = false;
    s.views.push_back(a);
    return s;
}

const ComputedView& computedView(const DrawingDocument& d, const std::string& id) {
    for (const ComputedView& v : d.computedSheets()[0].views)
        if (v.id == id) return v;
    throw std::runtime_error("no view " + id);
}

std::optional<std::size_t> childOfEdge(const ModelSnapshot& m, const views::ViewEdge& e) {
    if (e.edgeIndex && *e.edgeIndex < m.edgeChild.size() && m.edgeChild[*e.edgeIndex] != UINT32_MAX) return m.edgeChild[*e.edgeIndex];
    if (e.faceIndex && *e.faceIndex < m.faceChild.size() && m.faceChild[*e.faceIndex] != UINT32_MAX) return m.faceChild[*e.faceIndex];
    return std::nullopt;
}

// Отрезки вида, все точки которых (концы и середина) лежат в прямоугольнике [x0, x1] × [z0, z1]
// координат вида.
std::vector<views::ViewEdge> edgesIn(const ComputedView& cv, double x0, double x1, double z0, double z1,
                                     std::optional<views::Visibility> vis = std::nullopt) {
    std::vector<views::ViewEdge> out;
    for (const views::ViewEdge& e : cv.sheetView.view.edges) {
        if (vis && e.visibility != *vis) continue;
        const auto* l = std::get_if<LineSeg>(&e.curve);
        if (!l) continue;
        auto in = [&](Point2 p) { return p.x >= x0 && p.x <= x1 && p.y >= z0 && p.y <= z1; };
        if (in(l->a) && in(l->b)) out.push_back(e);
    }
    return out;
}

std::unique_ptr<md::PartDocument> gearPart(double module, int teeth, double width) {
    const double da = module * (teeth + 2);
    auto part = makeRod(da / 2, width);
    const md::NamedShape s = part->finalShape();
    TopoDS_Shape rim;
    for (TopExp_Explorer ex(s.shape(), TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface a(TopoDS::Face(ex.Current()), false);
        if (a.GetType() == GeomAbs_Cylinder) rim = ex.Current();
    }
    md::FeatureSpec g;
    g.type = "GearTeeth";
    g.id = "Rim";
    g.parameters = {{"module", std::to_string(module)}, {"teeth", std::to_string(teeth)}};
    g.references["face"] = {md::makeReference(s, rim).value()};
    g.options = {{"axis", "face"}};
    EXPECT_TRUE(part->addFeature(g).ok());
    part->rebuild();
    EXPECT_EQ(part->feature("Rim").value().state, md::FeatureState::Ok);
    return part;
}

}  // namespace

// Распознавание подшипника: размеры кольца по геометрии; простое кольцо — только по наименованию,
// деталь из колец и шариков — по геометрии; не кольцо — ошибка.
TEST(DrawingAssemblyConventions, BearingRecognition) {
    auto g = an::bearingGeometry(ring(25, 52, 15, 3));
    ASSERT_TRUE(g.ok()) << g.error().message;
    EXPECT_NEAR(g.value().d, 25, 1e-9);
    EXPECT_NEAR(g.value().D, 52, 1e-9);
    EXPECT_NEAR(g.value().B, 15, 1e-9);
    EXPECT_NEAR(std::abs(g.value().axis.z), 1, 1e-12);
    EXPECT_NEAR(views::dot(g.value().origin, g.value().axis), g.value().axis.z > 0 ? 3 : -18, 1e-9);
    EXPECT_FALSE(g.value().rollingElements);
    EXPECT_FALSE(an::recognizeBearing(ring(25, 52, 15), {"Втулка распорная", "spacer"}));
    EXPECT_TRUE(an::recognizeBearing(ring(25, 52, 15), {"Подшипник 205 ГОСТ 8338-75"}));
    EXPECT_TRUE(an::recognizeBearing(ring(25, 52, 15), {"", "Bearing 6205"}));
    EXPECT_FALSE(an::recognizeBearing(ring(25, 52, 15), {"Подшипники"}));
    auto bb = an::recognizeBearing(ballBearing(25, 52, 15), {"Изделие"});
    ASSERT_TRUE(bb);
    EXPECT_TRUE(bb->rollingElements);
    EXPECT_NEAR(bb->d, 25, 1e-9);
    EXPECT_NEAR(bb->D, 52, 1e-9);
    EXPECT_NEAR(bb->B, 15, 1e-9);
    EXPECT_FALSE(an::bearingGeometry(BRepPrimAPI_MakeBox(10, 10, 10).Shape()).ok());
    EXPECT_FALSE(an::bearingGeometry(BRepPrimAPI_MakeCylinder(10, 10).Shape()).ok());  // один диаметр
}

// Узел: корпус-кольцо 52×90×40, вал ⌀25 (z ∈ [−20, 80]), подшипник 205 — кольцо 25×52×15
// (по наименованию, z ∈ [0, 15]) и шариковый 25×52×15 (по геометрии, z ∈ [25, 40]). Разрез А–А
// через ось: подшипники не штрихуются, их линий модели нет, контур — 4 основных отрезка и
// 2 тонкие диагонали с каждой стороны оси; DXF — те же отрезки на слоях VISIBLE и THIN, штриховки
// внутри контура нет. Позиции: подшипники на главном виде скрыты корпусом — их позиции ставятся на
// разрезе А–А внутри контура подшипника; полки — по линии, без пересечения выносок, по номерам.
TEST(DrawingAssemblyConventions, BearingsInAssemblySection) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    auto doc = std::make_shared<AssemblyDocument>("Опора");
    ASSERT_TRUE(doc->setProperties(doc->root(), {"ОП.01.000", "Опора", "", std::nullopt}).ok());
    auto rod = makeRod(12.5, 100);
    const DefinitionId housing = doc->addPart("housing", {ring(52, 90, 40), nullptr}, {"ОП.01.001", "Корпус", "СЧ 20", 7200}).value();
    const DefinitionId shaft = doc->addPart("shaft", partDataFromDocument(*rod).value(), {"ОП.01.002", "Вал", "Сталь 45", 7850}).value();
    const DefinitionId b1 = doc->addPart("b205", {ring(25, 52, 15), nullptr}, {"", "Подшипник 205 ГОСТ 8338-75", "ШХ15", 7810}).value();
    const DefinitionId b2 = doc->addPart("b2", {ballBearing(25, 52, 15), nullptr}, {"", "Шарикоподшипник 1", "ШХ15", 7810}).value();
    const OccurrenceId ho = doc->addOccurrence(housing, Transform::identity(), "Корпус").value();
    ASSERT_TRUE(doc->setFixed(ho, true).ok());
    doc->addOccurrence(shaft, Transform::fromTranslation(bendcore::assembly_v2::Vec3(0, 0, -20)), "Вал").value();
    doc->addOccurrence(b1, Transform::identity(), "П1").value();
    doc->addOccurrence(b2, Transform::fromTranslation(bendcore::assembly_v2::Vec3(0, 0, 25)), "П2").value();
    auto snap = snapshotAssembly(doc);
    ASSERT_TRUE(snap.ok()) << snap.error().message;
    const ModelSnapshot& m = snap.value();
    ASSERT_EQ(m.bearings.size(), 2u);
    std::set<std::size_t> bearingChildren;
    for (const auto& b : m.bearings) {
        bearingChildren.insert(b.child);
        EXPECT_NEAR(b.d, 25, 1e-9);
        EXPECT_NEAR(b.D, 52, 1e-9);
        EXPECT_NEAR(b.B, 15, 1e-9);
        const DefinitionId def = m.childOwner[b.child];
        EXPECT_TRUE(def == b1 || def == b2);
    }
    EXPECT_EQ(bearingChildren.size(), 2u);

    DrawingDocument d;
    d.model().kind = ModelKind::Assembly;
    SheetDef s = sheetWithSection();
    s.views[0].autoBalloons = true;
    d.sheets().push_back(s);
    d.setFont(font);
    views::ViewCache cache(16, 2);
    auto rep = d.update(m, cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const ComputedView& a = computedView(d, "A");
    ASSERT_EQ(a.bearings.size(), 2u);
    ASSERT_TRUE(a.section);
    // Фигуры сечения: подшипники не штрихуются; корпус — 2 × (45 − 26) × 40.
    double housingArea = 0;
    for (std::size_t i = 0; i < a.section->regions.size(); ++i) {
        const int owner = a.section->regionOwner[i];
        EXPECT_FALSE(bearingChildren.count(std::size_t(owner))) << "a bearing is hatched";
        if (m.childOwner[std::size_t(owner)] != housing) continue;
        for (const auto& loop : a.section->regions[i]) {
            double l = 0;
            for (std::size_t k = 0; k < loop.size(); ++k)
                l += loop[k].x * loop[(k + 1) % loop.size()].y - loop[(k + 1) % loop.size()].x * loop[k].y;
            housingArea += std::abs(l) / 2;
        }
    }
    EXPECT_NEAR(housingArea, 2 * 19.0 * 40.0, 1e-6);
    for (const views::ViewEdge& e : a.sheetView.view.edges) {
        const auto c = childOfEdge(m, e);
        EXPECT_FALSE(c && bearingChildren.count(*c)) << "an edge of a bearing is drawn";
    }
    // Нет рёбер модели строго внутри контуров подшипников (12,5 < |x| < 26, 0 < z < 40, кроме z ≈ 15…25).
    for (const views::ViewEdge& e : a.sheetView.view.edges) {
        const Point2 p = an::edgeMidpoint(e.curve);
        const bool inside = std::abs(p.x) > 12.5 + 1e-3 && std::abs(p.x) < 26 - 1e-3 &&
                            ((p.y > 1e-3 && p.y < 15 - 1e-3) || (p.y > 25 + 1e-3 && p.y < 40 - 1e-3));
        EXPECT_FALSE(inside) << "line at (" << p.x << ", " << p.y << ") inside a bearing contour";
    }
    // Линии условного изображения: контур по размерам d, D, B и диагонали прямоугольника.
    struct Rect4 {
        double x0, x1, z0, z1;
    };
    std::vector<Rect4> contours;
    for (const auto& img : a.bearings) {
        const auto lines = an::bearingLines(img, a.sheetView);
        int contour = 0, diag = 0;
        for (const auto& l : lines) {
            const LineSeg& sg = std::get<LineSeg>(l.curve);
            if (l.role == an::BearingLine::Role::Contour) {
                ++contour;
                EXPECT_EQ(l.lineType, LineType::SolidThick);
                EXPECT_EQ(l.layer, "VISIBLE");
            } else {
                ++diag;
                EXPECT_EQ(l.lineType, LineType::SolidThin);
                EXPECT_EQ(l.layer, "THIN");
                // Диагональ — из угла (d/2, z0) в угол (D/2, z0 + B) прямоугольника.
                const double len = std::hypot(sg.b.x - sg.a.x, sg.b.y - sg.a.y) / a.sheetView.scale;
                EXPECT_NEAR(len, std::hypot(13.5, 15.0), 1e-9);
            }
        }
        EXPECT_EQ(contour, 8);
        EXPECT_EQ(diag, 4);
        const double z0 = views::dot(img.info.origin, {0, 0, 1}) + (img.info.axis.z > 0 ? 0 : -15);
        for (double side : {1.0, -1.0}) {
            const double xa = side * 12.5, xb = side * 26;
            contours.push_back({std::min(xa, xb), std::max(xa, xb), z0, z0 + 15});
        }
    }
    std::set<long> z0s;
    for (const auto& c : contours) z0s.insert(std::lround(c.z0));
    EXPECT_EQ(z0s, (std::set<long>{0, 25}));
    // Позиции: подшипники скрыты на F → поставлены на А–А, точка выноски внутри контура.
    ASSERT_FALSE(rep.value().balloons.empty());
    const BalloonReport& fr = rep.value().balloons[0];
    EXPECT_EQ(fr.view, "F");
    EXPECT_TRUE(fr.withoutBalloon.empty());
    std::set<DefinitionId> moved;
    for (const auto& [def, v] : fr.movedTo) {
        EXPECT_EQ(v, "A");
        moved.insert(def);
    }
    EXPECT_EQ(moved, (std::set<DefinitionId>{b1, b2}));
    const ComputedSheet& cs = d.computedSheets()[0];
    std::map<std::string, std::vector<an::Balloon>> byView;
    for (const an::Balloon& b : cs.balloons) {
        const bool onA = b.definition == b1 || b.definition == b2;
        byView[onA ? "A" : "F"].push_back(b);
        if (!onA) continue;
        bool inside = false;
        for (const auto& c : contours) {
            const Point2 lo = a.sheetView.toSheet({c.x0, c.z0}), hi = a.sheetView.toSheet({c.x1, c.z1});
            if (b.target.x > lo.x && b.target.x < hi.x && b.target.y > lo.y && b.target.y < hi.y) inside = true;
        }
        EXPECT_TRUE(inside) << "position " << b.text << " is not on the bearing image";
    }
    EXPECT_EQ(cs.balloons.size(), 4u);
    EXPECT_EQ(byView["F"].size(), 2u);
    EXPECT_EQ(byView["A"].size(), 2u);
    // Выноски не пересекаются, номера на каждой линии полок по порядку. На А–А подшипники один над
    // другим: позиция 3 (нижний) над 4 на одной колонне без пересечения невозможна — позиции
    // разносятся на разные стороны вида (ГОСТ 2.109: порядок уступает, только если иначе никак).
    for (const BalloonReport& br : rep.value().balloons) {
        EXPECT_EQ(br.crossings, 0u) << br.view;
        EXPECT_TRUE(br.ordered) << br.view;
        EXPECT_TRUE(br.outOfOrder.empty()) << br.view;
    }
    for (auto& [v, list] : byView) {
        EXPECT_EQ(an::leaderCrossings(list), 0u) << v;
        const auto lines = bendcore::drawing_v2::test_balloons::shelfLines(list, std::vector<std::string>(list.size(), v));
        EXPECT_TRUE(bendcore::drawing_v2::test_balloons::unorderedNumbers(lines).empty()) << v;
        for (const auto& l : lines)
            for (std::size_t i = 1; i < l.balloons.size(); ++i)
                if (l.column) EXPECT_GE(l.balloons[i - 1]->at.y - l.balloons[i]->at.y, 10.0 - 1e-9) << v;
    }

    // DXF: контур подшипников на слое VISIBLE, диагонали на THIN, штриховки внутри контура нет.
    auto sheets = d.render(font);
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    const std::string path = dv2test::outPath("bearings_section.dxf");
    ASSERT_TRUE(saveToFile(path, writeDxf(sheets.value()[0]).value()).ok());
    const DxfContent dxf = readDxf(path);
    for (const auto& c : contours) {
        const Point2 lo = a.sheetView.toSheet({c.x0, c.z0}), hi = a.sheetView.toSheet({c.x1, c.z1});
        auto has = [&](const std::string& layer, Point2 p, Point2 q) {
            for (const auto& l : dxf.lines) {
                if (l.layer != layer) continue;
                const bool fwd = std::hypot(l.x0 - p.x, l.y0 - p.y) < 1e-6 && std::hypot(l.x1 - q.x, l.y1 - q.y) < 1e-6;
                const bool rev = std::hypot(l.x0 - q.x, l.y0 - q.y) < 1e-6 && std::hypot(l.x1 - p.x, l.y1 - p.y) < 1e-6;
                if (fwd || rev) return true;
            }
            return false;
        };
        EXPECT_TRUE(has("VISIBLE", {lo.x, lo.y}, {hi.x, lo.y}));
        EXPECT_TRUE(has("VISIBLE", {lo.x, hi.y}, {hi.x, hi.y}));
        EXPECT_TRUE(has("VISIBLE", {lo.x, lo.y}, {lo.x, hi.y}));
        EXPECT_TRUE(has("VISIBLE", {hi.x, lo.y}, {hi.x, hi.y}));
        EXPECT_TRUE(has("THIN", {lo.x, lo.y}, {hi.x, hi.y}) || has("THIN", {hi.x, lo.y}, {lo.x, hi.y}));
        EXPECT_TRUE(has("THIN", {lo.x, hi.y}, {hi.x, lo.y}) || has("THIN", {hi.x, hi.y}, {lo.x, lo.y}));
        for (const auto& l : dxf.lines) {
            if (l.layer != "HATCH") continue;
            const double mx = 0.5 * (l.x0 + l.x1), my = 0.5 * (l.y0 + l.y1);
            EXPECT_FALSE(mx > lo.x + 1e-6 && mx < hi.x - 1e-6 && my > lo.y + 1e-6 && my < hi.y - 1e-6) << "hatch in a bearing";
        }
    }
    // Символ тела качения: окружность ⌀0,3(D − d) в центре прямоугольника вместо диагоналей.
    const auto el = an::bearingLines(a.bearings[0], a.sheetView, {}, an::BearingSymbol::RollingElement);
    int circles = 0;
    for (const auto& l : el) {
        EXPECT_NE(l.role, an::BearingLine::Role::Diagonal);
        if (const auto* c = std::get_if<Circle>(&l.curve)) {
            ++circles;
            EXPECT_NEAR(c->radius / a.sheetView.scale, 0.15 * 27, 1e-9);
        }
    }
    EXPECT_EQ(circles, 2);
    // Вид без разреза — подшипники не изображаются условно.
    EXPECT_TRUE(computedView(d, "F").bearings.empty());
}

// Пара колёс внешнего зацепления: шестерня m = 2, z = 20 (d = 40, da = 44, df = 35), b = 25 и
// колесо z = 40 (d = 80, da = 84, df = 75), b = 20, оси Z, aw = 60, колесо — z ∈ [2,5; 22,5].
// Разрез через оси и вид спереди: образующая вершин колеса (x = 18) в зоне зацепления —
// невидимая, вершин шестерни (x = 22) — видимая; между ними нет видимых линий; делительная x = 20
// одна. Вид сверху: делительные окружности касаются, окружности вершин — видимые.
TEST(DrawingAssemblyConventions, GearMeshOnAssemblyDrawing) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    auto pinionDoc = gearPart(2, 20, 25);
    auto wheelDoc = gearPart(2, 40, 20);
    auto doc = std::make_shared<AssemblyDocument>("Передача");
    ASSERT_TRUE(doc->setProperties(doc->root(), {"ЗП.01.000", "Передача", "", std::nullopt}).ok());
    const DefinitionId pinion = doc->addPart("pinion", partDataFromDocument(*pinionDoc).value(), {"ЗП.01.001", "Шестерня", "", 7850}).value();
    const DefinitionId wheel = doc->addPart("wheel", partDataFromDocument(*wheelDoc).value(), {"ЗП.01.002", "Колесо зубчатое", "", 7850}).value();
    const OccurrenceId po = doc->addOccurrence(pinion, Transform::identity(), "Шестерня").value();
    ASSERT_TRUE(doc->setFixed(po, true).ok());
    doc->addOccurrence(wheel, Transform::fromTranslation(bendcore::assembly_v2::Vec3(60, 0, 2.5)), "Колесо").value();
    auto snap = snapshotAssembly(doc);
    ASSERT_TRUE(snap.ok()) << snap.error().message;
    const ModelSnapshot& m = snap.value();
    ASSERT_EQ(m.gears.size(), 2u);
    ASSERT_EQ(m.gearChild.size(), 2u);
    std::size_t pc = 0, wc = 0;
    for (std::size_t i = 0; i < 2; ++i) {
        const auto& g = m.gears[i].gear;
        if (g.teeth == 20) {
            pc = m.gearChild[i];
            EXPECT_NEAR(g.axis.Location().X(), 0, 1e-9);
        } else {
            wc = m.gearChild[i];
            EXPECT_NEAR(g.axis.Location().X(), 60, 1e-9);  // ось перенесена положением вхождения
            // Начало венца — торец с меньшей координатой вдоль оси венца (ось ±Z).
            EXPECT_NEAR(g.axis.Location().Z(), g.axis.Direction().Z() > 0 ? 2.5 : 22.5, 1e-9);
        }
    }
    EXPECT_EQ(m.childOwner[pc], pinion);
    EXPECT_EQ(m.childOwner[wc], wheel);

    DrawingDocument d;
    d.model().kind = ModelKind::Assembly;
    d.sheets().push_back(sheetWithSection());
    d.setFont(font);
    views::ViewCache cache(16, 2);
    auto rep = d.update(m, cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;

    for (const char* id : {"A", "F"}) {
        const ComputedView& cv = computedView(d, id);
        SCOPED_TRACE(id);
        ASSERT_EQ(cv.gears.size(), 2u);
        ASSERT_EQ(cv.gearMeshes.size(), 1u);
        EXPECT_EQ(*cv.gears[cv.gearMeshes[0].front].child, pc);  // шестерня — перед колесом
        if (std::string(id) == "A") {
            EXPECT_TRUE(cv.gears[0].section);
            EXPECT_TRUE(cv.gears[1].section);
        }
        // Колесо в полосе зубьев шестерни (x ∈ [17,5; 22], z ∈ [0; 25]) — только невидимые линии.
        bool wheelTipHidden = false;
        for (const views::ViewEdge& e : edgesIn(cv, 17.5 - 1e-6, 22 + 1e-6, -1e-6, 25 + 1e-6)) {
            const auto c = childOfEdge(m, e);
            if (c && *c == wc) {
                EXPECT_EQ(e.visibility, views::Visibility::Hidden);
                const auto& l = std::get<LineSeg>(e.curve);
                if (std::abs(l.a.x - 18) < 1e-6 && std::abs(l.b.x - 18) < 1e-6) wheelTipHidden = true;
            }
        }
        EXPECT_TRUE(wheelTipHidden);
        // Нет лишних основных линий: видимых отрезков вдоль оси строго между 17,5 и 22 нет.
        for (const views::ViewEdge& e : edgesIn(cv, 17.5 + 1e-3, 22 - 1e-3, -1, 26, views::Visibility::Visible)) {
            const auto& l = std::get<LineSeg>(e.curve);
            ADD_FAILURE() << "extra visible line (" << l.a.x << ", " << l.a.y << ") – (" << l.b.x << ", " << l.b.y << ")";
        }
        // Образующая вершин шестерни x = 22 — видимая на всей ширине колеса.
        double visibleTip = 0;
        for (const views::ViewEdge& e : edgesIn(cv, 22 - 1e-6, 22 + 1e-6, -1, 26, views::Visibility::Visible)) {
            const auto& l = std::get<LineSeg>(e.curve);
            const double lo = std::max(2.5, std::min(l.a.y, l.b.y)), hi = std::min(22.5, std::max(l.a.y, l.b.y));
            if (hi > lo) visibleTip += hi - lo;
        }
        EXPECT_NEAR(visibleTip, 20.0, 1e-3);
        // Делительная x = 20 — одна линия от z = 0 до z = 25 (+ выход 3 мм листа).
        int pitchAt20 = 0;
        for (const auto& g : cv.gears)
            for (const auto& l : an::gearLines(g, cv.sheetView)) {
                if (l.role != an::GearLine::Role::Pitch) continue;
                const LineSeg& sg = std::get<LineSeg>(l.curve);
                const Point2 p = cv.sheetView.toSheet({20, 0});
                if (std::abs(sg.a.x - p.x) > 1e-6) continue;
                ++pitchAt20;
                EXPECT_EQ(*g.child, pc);
                EXPECT_NEAR(std::abs(sg.b.y - sg.a.y), 25 * cv.sheetView.scale + 6.0, 1e-9);
            }
        EXPECT_EQ(pitchAt20, 1);
    }
    // Разрез: штриховка колеса не заходит в полосу зубьев шестерни (зуб не рассекается).
    {
        const ComputedView& a = computedView(d, "A");
        for (std::size_t i = 0; i < a.section->regions.size(); ++i)
            for (const auto& loop : a.section->regions[i])
                for (const Point2& p : loop) {
                    if (std::size_t(a.section->regionOwner[i]) == wc) EXPECT_GE(p.x, 60 - 37.5 - 1e-6);
                    if (std::size_t(a.section->regionOwner[i]) == pc) EXPECT_LE(std::abs(p.x), 17.5 + 1e-6);
                }
    }
    // Вид сверху: делительные окружности касаются (расстояние центров = 20 + 40), вершины видимые.
    {
        const ComputedView& t = computedView(d, "T");
        ASSERT_EQ(t.gears.size(), 2u);
        ASSERT_EQ(t.gearMeshes.size(), 1u);
        std::vector<Circle> pitch;
        for (const auto& g : t.gears)
            for (const auto& l : an::gearLines(g, t.sheetView))
                if (l.role == an::GearLine::Role::PitchCircle) pitch.push_back(std::get<Circle>(l.curve));
        ASSERT_EQ(pitch.size(), 2u);
        const double dist = std::hypot(pitch[0].center.x - pitch[1].center.x, pitch[0].center.y - pitch[1].center.y);
        EXPECT_NEAR(dist, pitch[0].radius + pitch[1].radius, 1e-9);
        EXPECT_NEAR(dist / t.sheetView.scale, 60, 1e-9);
        int tips = 0;
        for (const views::ViewEdge& e : t.sheetView.view.edges) {
            double r = 0;
            if (const auto* a = std::get_if<Arc>(&e.curve)) r = a->radius;
            if (const auto* c = std::get_if<Circle>(&e.curve)) r = c->radius;
            if (std::abs(r - 22) < 1e-6 || std::abs(r - 42) < 1e-6) {
                ++tips;
                EXPECT_EQ(e.visibility, views::Visibility::Visible) << "tip circle r = " << r;
            }
        }
        EXPECT_GE(tips, 2);
    }
    // DXF разреза: в зоне зацепления по x = 18 — штриховые (HIDDEN), основных нет.
    auto sheets = d.render(font);
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    const std::string path = dv2test::outPath("gear_mesh.dxf");
    ASSERT_TRUE(saveToFile(path, writeDxf(sheets.value()[0]).value()).ok());
    const DxfContent dxf = readDxf(path);
    const ComputedView& a = computedView(d, "A");
    const Point2 p0 = a.sheetView.toSheet({18, 2.5}), p1 = a.sheetView.toSheet({18, 22.5});
    int hidden = 0;
    for (const auto& l : dxf.lines) {
        if (std::abs(l.x0 - p0.x) > 1e-6 || std::abs(l.x1 - p0.x) > 1e-6) continue;
        if (std::max(l.y0, l.y1) < p0.y - 1e-6 || std::min(l.y0, l.y1) > p1.y + 1e-6) continue;
        EXPECT_NE(l.layer, "VISIBLE") << "main line along the wheel tip in the mesh zone";
        if (l.layer == "HIDDEN") ++hidden;
    }
    EXPECT_GT(hidden, 0);
}

// Косой зуб на виде: три параллельные сплошные тонкие линии под углом β к оси, направление —
// по ходу линии зуба на ближней к наблюдателю стороне (правый зуб на виде спереди с осью Z вверх
// — «/»: x растёт с z); левый — зеркально; в разрезе и у прямого зуба линий нет.
TEST(DrawingAssemblyConventions, HelicalToothLines) {
    md::GearInfo g;
    g.gear.module = 2;
    g.gear.teeth = 30;
    g.gear.width = 30;
    g.gear.helix = 15.0 * kPi / 180.0;
    g.gear.axis = gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    views::SheetView sv;
    sv.scale = 2.0;
    sv.anchor = {100, 50};
    const auto geo = md::gearGeometry(g.gear).value();
    for (md::GearHand hand : {md::GearHand::Right, md::GearHand::Left}) {
        g.gear.hand = hand;
        auto img = an::gearImages({g}, views::defaultFrontFrame());
        ASSERT_TRUE(img.ok());
        std::vector<LineSeg> helix;
        for (const auto& l : an::gearLines(img.value()[0], sv)) {
            if (l.role != an::GearLine::Role::Helix) continue;
            EXPECT_EQ(l.lineType, LineType::SolidThin);
            EXPECT_EQ(l.layer, "THIN");
            helix.push_back(std::get<LineSeg>(l.curve));
        }
        ASSERT_EQ(helix.size(), 3u);
        for (const LineSeg& s : helix) {
            const double dx = s.b.x - s.a.x, dz = s.b.y - s.a.y;
            const double len = std::hypot(dx, dz);
            EXPECT_GT(len, 5.0);
            EXPECT_NEAR(std::acos(std::abs(dz) / len), 15.0 * kPi / 180.0, 1e-9);  // угол с осью Z
            const double slope = dx * dz;  // правый: x растёт с z
            if (hand == md::GearHand::Right) EXPECT_GT(slope, 0);
            else EXPECT_LT(slope, 0);
            // В пределах венца: z ∈ [0, b], |x| ≤ da/2 (мм модели).
            for (Point2 p : {s.a, s.b}) {
                const double x = (p.x - 100) / 2, z = (p.y - 50) / 2;
                EXPECT_GE(z, -1e-9);
                EXPECT_LE(z, 30 + 1e-9);
                EXPECT_LE(std::abs(x), geo.da / 2 + 1e-9);
            }
        }
        // Параллельные и не совпадающие.
        const double c01 = (helix[0].a.x - helix[1].a.x) * (helix[1].b.y - helix[1].a.y) -
                           (helix[0].a.y - helix[1].a.y) * (helix[1].b.x - helix[1].a.x);
        EXPECT_GT(std::abs(c01), 1e-3);
    }
    auto cut = an::gearImages({g}, views::defaultFrontFrame(), views::CuttingPlane{{0, 0, 0}, {0, 1, 0}});
    for (const auto& l : an::gearLines(cut.value()[0], sv)) EXPECT_NE(l.role, an::GearLine::Role::Helix);
    g.gear.helix = 0;
    auto spur = an::gearImages({g}, views::defaultFrontFrame());
    for (const auto& l : an::gearLines(spur.value()[0], sv)) EXPECT_NE(l.role, an::GearLine::Role::Helix);
}

// Разнесение полок: колонна по номерам, выбор точки выноски без пересечений; если порядок номеров
// невозможен без пересечения — порядок уступает (ordered = false), пересечений нет; строка.
TEST(DrawingAssemblyConventions, BalloonShelvesOnOneLine) {
    an::BalloonArrangeOptions o;
    o.line = an::BalloonLine::Right;
    o.view = {0, 0, 100, 100};
    o.area = {-200, -200, 400, 300};
    // №1 может быть выноской в (90, 90) или (10, 10); №2 — только в (80, 20). Ближайшая точка
    // №1 к верхней полке — (90, 90); пересечений нет, номера сверху вниз.
    std::vector<an::BalloonCandidate> c{{7, 1, {{10, 10}, {90, 90}}}, {8, 2, {{80, 20}}}};
    auto r = an::arrangeBalloons(c, o);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(r.value().ordered);
    EXPECT_EQ(r.value().crossings, 0u);
    EXPECT_EQ(r.value().line, an::BalloonLine::Right);
    const auto& b = r.value().balloons;
    EXPECT_NEAR(b[0].at.x, 115, 1e-12);
    EXPECT_NEAR(b[1].at.x, 115, 1e-12);
    EXPECT_GT(b[0].at.y, b[1].at.y);
    EXPECT_NEAR(b[0].at.y - b[1].at.y, 10, 1e-12);
    EXPECT_EQ(b[0].text, "1");
    // Единственные точки: №1 внизу слева, №2 выше — по порядку выноски пересеклись бы.
    std::vector<an::BalloonCandidate> bad{{7, 1, {{0, 0}}}, {8, 2, {{50, 100}}}};
    {
        // Проверка эталона: при порядке номеров выноски пересекаются.
        an::Balloon x{7, {0, 0}, {115, 55}, "1"}, y{8, {50, 100}, {115, 45}, "2"};
        ASSERT_TRUE(an::leadersCross(x, y));
    }
    r = an::arrangeBalloons(bad, o);
    ASSERT_TRUE(r.ok());
    EXPECT_FALSE(r.value().ordered);
    EXPECT_EQ(r.value().crossings, 0u);
    EXPECT_EQ(an::leaderCrossings(r.value().balloons), 0u);
    EXPECT_GT(r.value().balloons[1].at.y, r.value().balloons[0].at.y);  // №2 выше
    // Строка над изображением: одна горизонталь, слева направо по номерам, ячейки не перекрываются.
    o.line = an::BalloonLine::Above;
    std::vector<an::BalloonCandidate> row{{1, 3, {{70, 50}}}, {2, 1, {{10, 50}}}, {3, 2, {{40, 50}}}};
    r = an::arrangeBalloons(row, o);
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(r.value().ordered);
    EXPECT_EQ(r.value().crossings, 0u);
    for (const auto& x : r.value().balloons) EXPECT_NEAR(x.at.y, 115, 1e-12);
    EXPECT_LT(r.value().balloons[1].at.x, r.value().balloons[2].at.x);  // №1 левее №2
    EXPECT_LT(r.value().balloons[2].at.x, r.value().balloons[0].at.x);  // №2 левее №3
    // Авто: справа нет места → слева.
    o.line = an::BalloonLine::Auto;
    o.area = {-200, -200, 110, 300};
    r = an::arrangeBalloons(c, o);
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value().line, an::BalloonLine::Left);
    EXPECT_NEAR(r.value().balloons[0].at.x, -15, 1e-12);
    // Не помещаются.
    o.line = an::BalloonLine::Right;
    o.area = {-200, 0, 400, 12};
    std::vector<an::BalloonCandidate> many;
    for (int i = 1; i <= 5; ++i) many.push_back({std::uint32_t(i), i, {{50, 5}}});
    EXPECT_FALSE(an::arrangeBalloons(many, o).ok());
    EXPECT_FALSE(an::arrangeBalloons({{1, 1, {}}}, o).ok());
}

// Ручные позиции «разнести»: arrangeBalloons ставит их полки в колонну с автоматическими позициями
// вида по порядку номеров; признак сохраняется в файле; перенос полки снимает его.
TEST(DrawingAssemblyConventions, ArrangeManualPositions) {
    auto doc = std::make_shared<AssemblyDocument>("Узел");
    ASSERT_TRUE(doc->setProperties(doc->root(), {"У.01.000", "Узел", "", std::nullopt}).ok());
    auto rod = makeRod(12.5, 100);
    const DefinitionId housing = doc->addPart("housing", {ring(52, 90, 40), nullptr}, {"У.01.001", "Корпус", "", 7200}).value();
    const DefinitionId shaft = doc->addPart("shaft", partDataFromDocument(*rod).value(), {"У.01.002", "Вал", "", 7850}).value();
    const OccurrenceId ho = doc->addOccurrence(housing, Transform::identity(), "Корпус").value();
    ASSERT_TRUE(doc->setFixed(ho, true).ok());
    doc->addOccurrence(shaft, Transform::fromTranslation(bendcore::assembly_v2::Vec3(0, 0, -20)), "Вал").value();
    auto snap = snapshotAssembly(doc);
    ASSERT_TRUE(snap.ok());
    DrawingDocument d;
    d.model().kind = ModelKind::Assembly;
    SheetDef s = sheetWithSection();
    s.views[0].autoBalloons = true;
    d.sheets().push_back(s);
    views::ViewCache cache(16, 2);
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    // Ручная позиция на ребре вала на главном виде, полка далеко.
    const ComputedView& f = computedView(d, "F");
    // Ребро вала ниже корпуса (вал — позиция 2, корпус — 1): по порядку номеров полка вала — ниже.
    std::optional<std::uint32_t> edge;
    double lowest = 1e300;
    for (const auto& e : f.sheetView.view.edges)
        if (e.visibility == views::Visibility::Visible && e.edgeIndex && snap.value().edgeOwner[*e.edgeIndex] == shaft &&
            an::edgeMidpoint(e.curve).y < lowest) {
            lowest = an::edgeMidpoint(e.curve).y;
            edge = *e.edgeIndex;
        }
    ASSERT_TRUE(edge);
    ASSERT_LT(lowest, 0.0);
    BalloonItem item;
    item.id = "P1";
    item.view = "F";
    item.edge = elementReference(snap.value(), md::ElementKind::Edge, *edge).value();
    item.at = Point2{30, 280};
    d.sheets()[0].balloons.push_back(item);
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    EXPECT_NEAR(d.computedSheets()[0].manualBalloons[0]->at.x, 30, 1e-12);
    EXPECT_FALSE(d.arrangeBalloons("S1", "X").ok());
    EXPECT_FALSE(d.arrangeBalloons("S9").ok());
    auto n = d.arrangeBalloons("S1");
    ASSERT_TRUE(n.ok());
    EXPECT_EQ(n.value(), 1u);
    EXPECT_TRUE(d.sheets()[0].balloons[0].arranged);
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    const ComputedSheet& cs = d.computedSheets()[0];
    ASSERT_EQ(cs.balloons.size(), 1u);  // корпус — автоматическая, вал — ручная
    ASSERT_TRUE(cs.manualBalloons[0]);
    const an::Balloon& man = *cs.manualBalloons[0];
    const an::Balloon& aut = cs.balloons[0];
    EXPECT_NEAR(man.at.x, aut.at.x, 1e-9);  // одна вертикаль
    EXPECT_EQ(std::stoi(man.text), 2);
    EXPECT_EQ(std::stoi(aut.text), 1);
    EXPECT_GT(aut.at.y, man.at.y);  // номера сверху вниз
    EXPECT_NEAR(aut.at.y - man.at.y, 10.0, 1e-9);
    EXPECT_FALSE(an::leadersCross(man, aut));
    auto text = d.toJson();
    ASSERT_TRUE(text.ok());
    EXPECT_NE(text.value().find("\"arranged\": true"), std::string::npos);
    auto back = DrawingDocument::fromJson(text.value());
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_TRUE(back.value().sheets()[0].balloons[0].arranged);
    EXPECT_EQ(back.value().toJson().value(), text.value());
    // Линия полок слева: все полки вида — на вертикали в 15 мм левее изображения.
    d.sheets()[0].views[0].balloonLine = an::BalloonLine::Left;
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    {
        const ComputedSheet& c2 = d.computedSheets()[0];
        const views::Box2 vb = computedView(d, "F").sheetView.sheetBounds();
        ASSERT_EQ(c2.balloons.size(), 1u);
        EXPECT_NEAR(c2.balloons[0].at.x, vb.minX - 15.0, 1e-9);
        EXPECT_NEAR(c2.manualBalloons[0]->at.x, vb.minX - 15.0, 1e-9);
    }
    d.sheets()[0].views[0].balloonLine = an::BalloonLine::Auto;
    // Линия полок вида и символ подшипника сохраняются.
    d.sheets()[0].views[2].balloonLine = an::BalloonLine::Below;
    d.sheets()[0].views[2].bearingSymbol = an::BearingSymbol::RollingElement;
    auto t2 = DrawingDocument::fromJson(d.toJson().value());
    ASSERT_TRUE(t2.ok()) << t2.error().message;
    EXPECT_EQ(t2.value().sheets()[0].views[2].balloonLine, an::BalloonLine::Below);
    EXPECT_EQ(t2.value().sheets()[0].views[2].bearingSymbol, an::BearingSymbol::RollingElement);
}

namespace {

// Сборка «корпус-кольцо 52×90×40 + вал ⌀25 × 100 (z = −20…80)».
std::shared_ptr<AssemblyDocument> housingAndShaft(DefinitionId& housing, DefinitionId& shaft) {
    auto doc = std::make_shared<AssemblyDocument>("Узел");
    EXPECT_TRUE(doc->setProperties(doc->root(), {"У.01.000", "Узел", "", std::nullopt}).ok());
    auto rod = makeRod(12.5, 100);
    housing = doc->addPart("housing", {ring(52, 90, 40), nullptr}, {"У.01.001", "Корпус", "", 7200}).value();
    shaft = doc->addPart("shaft", partDataFromDocument(*rod).value(), {"У.01.002", "Вал", "", 7850}).value();
    const OccurrenceId ho = doc->addOccurrence(housing, Transform::identity(), "Корпус").value();
    EXPECT_TRUE(doc->setFixed(ho, true).ok());
    doc->addOccurrence(shaft, Transform::fromTranslation(bendcore::assembly_v2::Vec3(0, 0, -20)), "Вал").value();
    return doc;
}

bool rectsOverlap(const Rect& a, const Rect& b) {
    return std::min(a.x1, b.x1) > std::max(a.x0, b.x0) && std::min(a.y1, b.y1) > std::max(a.y0, b.y0);
}

// Отрезок (горизонтальный или вертикальный) проходит через прямоугольник.
bool axisSegmentInRect(Point2 a, Point2 b, const Rect& r) {
    const double x0 = std::min(a.x, b.x), x1 = std::max(a.x, b.x), y0 = std::min(a.y, b.y), y1 = std::max(a.y, b.y);
    return x1 >= r.x0 && x0 <= r.x1 && y1 >= r.y0 && y0 <= r.y1;
}

}  // namespace

// Препятствия: вертикальный размер справа от изображения (размерная линия в 17 мм от контура,
// выносные линии, надпись) — колонна полок в 15 мм пересекла бы размерную линию. Полки
// сдвигаются так, что ни полка, ни номер не задевают линий и надписи размера; полки — на одной
// вертикали, по порядку номеров. Строка над изображением — выше горизонтального размера.
TEST(DrawingAssemblyConventions, BalloonShelvesAvoidDimensionObstacles) {
    an::BalloonArrangeOptions o;
    o.line = an::BalloonLine::Right;
    o.view = {0, 0, 100, 100};
    o.area = {-200, -200, 400, 300};
    an::DimensionGeometry g;
    g.dimensionLines.push_back({{117, 20}, {117, 80}});
    g.extensionLines.push_back({{101, 20}, {119, 20}});
    g.extensionLines.push_back({{101, 80}, {119, 80}});
    an::DimensionLabel lab;
    lab.position = {116, 50};
    lab.text = "60";
    lab.height = 3.5;
    lab.rotationDeg = 90;
    lab.hAlign = HAlign::Center;
    g.labels.push_back(lab);
    an::addDimensionObstacles(o.obstacles, g);
    ASSERT_EQ(o.obstacles.lines.size(), 3u);
    ASSERT_EQ(o.obstacles.boxes.size(), 1u);
    std::vector<an::BalloonCandidate> c{{7, 1, {{90, 60}}}, {8, 2, {{80, 45}}}, {9, 3, {{70, 40}}}};
    {
        // Без препятствий колонна в x = 115 задевает размерную линию x = 117.
        an::BalloonArrangeOptions plain = o;
        plain.obstacles = {};
        auto r0 = an::arrangeBalloons(c, plain);
        ASSERT_TRUE(r0.ok());
        EXPECT_NEAR(r0.value().balloons[0].at.x, 115, 1e-12);
        std::size_t hits = 0;
        for (const an::Balloon& b : r0.value().balloons) hits += axisSegmentInRect({117, 20}, {117, 80}, an::balloonShelfBox(b)) ? 1 : 0;
        ASSERT_GT(hits, 0u);
    }
    auto r = an::arrangeBalloons(c, o);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r.value().shelfHits, 0u);
    EXPECT_EQ(r.value().crossings, 0u);
    EXPECT_TRUE(r.value().ordered);
    const auto& b = r.value().balloons;
    for (std::size_t i = 0; i < b.size(); ++i) {
        EXPECT_NEAR(b[i].at.x, b[0].at.x, 1e-12);  // одна вертикаль
        if (i > 0) EXPECT_GE(b[i - 1].at.y - b[i].at.y, 10.0 - 1e-9);
        const Rect box = an::balloonShelfBox(b[i]);
        for (const LineSeg& l : o.obstacles.lines) EXPECT_FALSE(axisSegmentInRect(l.a, l.b, box)) << "shelf " << b[i].text;
        EXPECT_FALSE(rectsOverlap(o.obstacles.boxes[0], box)) << "shelf " << b[i].text;
    }
    an::BalloonArrangeOptions row = o;
    row.line = an::BalloonLine::Above;
    row.obstacles = {};
    an::DimensionGeometry h;
    h.dimensionLines.push_back({{0, 117}, {100, 117}});
    h.extensionLines.push_back({{0, 101}, {0, 119}});
    h.extensionLines.push_back({{100, 101}, {100, 119}});
    an::addDimensionObstacles(row.obstacles, h);
    auto rr = an::arrangeBalloons(c, row);
    ASSERT_TRUE(rr.ok());
    EXPECT_EQ(rr.value().shelfHits, 0u);
    for (const an::Balloon& x : rr.value().balloons) {
        EXPECT_NEAR(x.at.y, rr.value().balloons[0].at.y, 1e-12);
        for (const LineSeg& l : row.obstacles.lines) EXPECT_FALSE(axisSegmentInRect(l.a, l.b, an::balloonShelfBox(x)));
    }
}

// Сборочный разрез с размером между позициями: вертикальный размер 40 справа от разреза А–А стоит
// там, где была бы колонна полок. После обновления ни полки, ни номера позиций не задевают
// размерных и выносных линий и надписи размера (геометрия листа и DXF).
TEST(DrawingAssemblyConventions, SectionBalloonsDoNotOverlapDimensions) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    DefinitionId housing = 0, shaft = 0;
    auto doc = housingAndShaft(housing, shaft);
    auto snap = snapshotAssembly(doc);
    ASSERT_TRUE(snap.ok());
    const ModelSnapshot& m = snap.value();
    DrawingDocument d;
    d.model().kind = ModelKind::Assembly;
    SheetDef s = sheetWithSection();
    s.views[2].autoBalloons = true;
    s.views[2].balloonLine = an::BalloonLine::Right;
    // Высота корпуса 40 справа от разреза: offset < 0 — вправо, линия в 17 мм от контура.
    auto v = [&](double x, double z) { return anchorOn(m, md::ElementKind::Vertex, vertexAt(m, {x, 0, z}), an::AnchorKind::Point); };
    s.dimensions.push_back(dimension("D1", "A", an::DimensionType::Vertical, {v(45, 0), v(45, 40)}, -17.0));
    d.sheets().push_back(s);
    d.setFont(font);
    views::ViewCache cache(16, 2);
    auto rep = d.update(m, cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const ComputedSheet& cs = d.computedSheets()[0];
    ASSERT_EQ(cs.dimensions.size(), 1u);
    ASSERT_EQ(cs.dimensions[0].state, an::DimensionState::Ok) << cs.dimensions[0].reason;
    EXPECT_NEAR(cs.dimensions[0].value, 40.0, 1e-9);
    ASSERT_EQ(cs.balloons.size(), 2u);
    const views::Box2 vb = computedView(d, "A").sheetView.sheetBounds();
    const an::DimensionGeometry& g = cs.dimensions[0].geometry;
    ASSERT_FALSE(g.dimensionLines.empty());
    // Размерная линия — там, где без учёта размеров стояла бы колонна полок (15 мм от изображения).
    const double dimX = g.dimensionLines[0].a.x;
    EXPECT_GT(dimX, vb.maxX + 15.0);
    EXPECT_LT(dimX, vb.maxX + 22.0);
    std::vector<LineSeg> lines = g.dimensionLines;
    lines.insert(lines.end(), g.extensionLines.begin(), g.extensionLines.end());
    an::AnnotationStyle style;
    style.font = d.textMetrics(font);  // шрифт листа (ГОСТ 2.304 по умолчанию)
    const auto labels = an::labelBoxes(g, style);
    ASSERT_FALSE(labels.empty());
    for (const an::Balloon& b : cs.balloons) {
        const Rect box = an::balloonShelfBox(b, {}, style);
        for (const LineSeg& l : lines) EXPECT_FALSE(axisSegmentInRect(l.a, l.b, box)) << "position " << b.text << " on a dimension line";
        for (const auto& lb : labels)
            EXPECT_FALSE(rectsOverlap(box, {lb.minX, lb.minY, lb.maxX, lb.maxY})) << "position " << b.text << " on the dimension text";
        EXPECT_NEAR(b.at.x, cs.balloons[0].at.x, 1e-9);  // одна вертикаль
    }
    bool reported = false;
    for (const BalloonReport& br : rep.value().balloons)
        if (br.view == "A") {
            reported = true;
            EXPECT_EQ(br.shelfHits, 0u);
        }
    EXPECT_TRUE(reported);
    // DXF: полки (горизонтальные отрезки от начала полки на ширину номера) есть; ни один отрезок
    // слоя размеров не проходит через место полки с номером.
    auto sheets = d.render(font);
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    const std::string path = dv2test::outPath("section_balloons_dimensions.dxf");
    ASSERT_TRUE(saveToFile(path, writeDxf(sheets.value()[0]).value()).ok());
    const DxfContent dxf = readDxf(path);
    EXPECT_NE(std::find(dxf.texts.begin(), dxf.texts.end(), "40"), dxf.texts.end());
    std::size_t dimLines = 0;
    for (const an::Balloon& b : cs.balloons) {
        const Rect box = an::balloonShelfBox(b, {}, style);
        bool shelf = false;
        for (const auto& l : dxf.lines) {
            // DXF — 6 знаков после запятой.
            if (std::abs(l.y0 - b.at.y) < 1e-5 && std::abs(l.y1 - b.at.y) < 1e-5 &&
                std::abs(std::min(l.x0, l.x1) - box.x0) < 1e-5 && std::abs(std::max(l.x0, l.x1) - box.x1) < 1e-5)
                shelf = true;
            if (l.layer != "DIM") continue;
            // Полки и выноски самих позиций (тот же слой) — концом в начале полки.
            bool own = false;
            for (const an::Balloon& o : cs.balloons)
                for (Point2 p : {Point2{l.x0, l.y0}, Point2{l.x1, l.y1}}) own = own || std::hypot(p.x - o.at.x, p.y - o.at.y) < 1e-5;
            if (own) continue;
            ++dimLines;
            EXPECT_FALSE(axisSegmentInRect({l.x0, l.y0}, {l.x1, l.y1}, box)) << "DXF: position " << b.text;
        }
        EXPECT_TRUE(shelf) << "DXF: no shelf of position " << b.text;
    }
    EXPECT_GT(dimLines, 0u);
}

// Перенос полки автоматической позиции мышью: полка встаёт в заданную точку и сходит с линии,
// точка выноски — ближайшая к полке точка изображения детали; положение сохраняется в файле;
// «Разнести позиции» и at = nullopt возвращают полку на линию.
TEST(DrawingAssemblyConventions, MovedAutomaticBalloonShelf) {
    DefinitionId housing = 0, shaft = 0;
    auto doc = housingAndShaft(housing, shaft);
    auto snap = snapshotAssembly(doc);
    ASSERT_TRUE(snap.ok());
    DrawingDocument d;
    d.model().kind = ModelKind::Assembly;
    SheetDef s = sheetWithSection();
    s.views[0].autoBalloons = true;
    d.sheets().push_back(s);
    views::ViewCache cache(16, 2);
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    ASSERT_EQ(d.computedSheets()[0].balloons.size(), 2u);
    std::vector<Point2> before;
    for (const an::Balloon& b : d.computedSheets()[0].balloons)
        if (b.definition == shaft) before.push_back(b.target);
    ASSERT_EQ(before.size(), 1u);
    EXPECT_FALSE(d.setAutoBalloon("S9", "F", shaft, Point2{0, 0}).ok());
    EXPECT_FALSE(d.setAutoBalloon("S1", "X", shaft, Point2{0, 0}).ok());
    EXPECT_FALSE(d.setAutoBalloon("S1", "F", shaft, Point2{std::nan(""), 0}).ok());
    ASSERT_TRUE(d.setAutoBalloon("S1", "F", shaft, Point2{60, 300}).ok());
    auto rep = d.update(snap.value(), cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    {
        const ComputedSheet& cs = d.computedSheets()[0];
        ASSERT_EQ(cs.balloons.size(), 2u);
        ASSERT_EQ(cs.balloonViews.size(), 2u);
        ASSERT_EQ(cs.balloonMoved.size(), 2u);
        std::optional<std::size_t> moved, other;
        for (std::size_t i = 0; i < cs.balloons.size(); ++i) (cs.balloons[i].definition == shaft ? moved : other) = i;
        ASSERT_TRUE(moved && other);
        EXPECT_TRUE(cs.balloonMoved[*moved]);
        EXPECT_FALSE(cs.balloonMoved[*other]);
        EXPECT_EQ(cs.balloonViews[*moved], "F");
        EXPECT_NEAR(cs.balloons[*moved].at.x, 60, 1e-12);
        EXPECT_NEAR(cs.balloons[*moved].at.y, 300, 1e-12);
        EXPECT_EQ(cs.balloons[*moved].text, "2");
        // Точка выноски — на изображении вала (внутри габарита вида) и не дальше от полки, чем
        // точка выноски до переноса.
        const views::Box2 fb = computedView(d, "F").sheetView.sheetBounds();
        const Point2 t = cs.balloons[*moved].target;
        EXPECT_GE(t.x, fb.minX - 1e-6);
        EXPECT_LE(t.x, fb.maxX + 1e-6);
        EXPECT_GE(t.y, fb.minY - 1e-6);
        EXPECT_LE(t.y, fb.maxY + 1e-6);
        // До переноса выноска шла к полке на линии справа; теперь — к ближайшей к (60, 300) точке:
        // не дальше прежней точки выноски вала.
        for (const Point2& p : before) EXPECT_LE(std::hypot(t.x - 60, t.y - 300), std::hypot(p.x - 60, p.y - 300) + 1e-9);
    }
    // Файл: сохраняется и читается обратно.
    auto text = d.toJson();
    ASSERT_TRUE(text.ok());
    EXPECT_NE(text.value().find("\"balloonShelves\""), std::string::npos);
    auto back = DrawingDocument::fromJson(text.value());
    ASSERT_TRUE(back.ok()) << back.error().message;
    ASSERT_EQ(back.value().sheets()[0].views[0].balloonShelves.size(), 1u);
    EXPECT_NEAR(back.value().sheets()[0].views[0].balloonShelves.at(shaft).y, 300, 1e-12);
    EXPECT_EQ(back.value().toJson().value(), text.value());
    // Разнести: полка обратно на линию (одна вертикаль с другой позицией).
    auto n = d.arrangeBalloons("S1");
    ASSERT_TRUE(n.ok());
    EXPECT_EQ(n.value(), 1u);
    EXPECT_TRUE(d.sheets()[0].views[0].balloonShelves.empty());
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    const ComputedSheet& c2 = d.computedSheets()[0];
    ASSERT_EQ(c2.balloons.size(), 2u);
    EXPECT_NEAR(c2.balloons[0].at.x, c2.balloons[1].at.x, 1e-9);
    EXPECT_FALSE(c2.balloonMoved[0] || c2.balloonMoved[1]);
    ASSERT_TRUE(d.setAutoBalloon("S1", "F", shaft, Point2{60, 300}).ok());
    ASSERT_TRUE(d.setAutoBalloon("S1", "F", shaft, std::nullopt).ok());
    EXPECT_TRUE(d.sheets()[0].views[0].balloonShelves.empty());
}

// Сборка из 12 позиций (ГОСТ 2.109): детали — клетки 4 × 3 на виде, номера не совпадают с
// положением, у каждой детали 5 допустимых точек выноски. Справа между видом и полками — размер
// (размерная линия, выносные, надпись), рядом буква и стрелка обозначения разреза. Раскладка:
// в каждой колонне и строке номера по возрастанию, выноски попарно не пересекаются (геометрическая
// проверка отрезков), не пересекают размерные и выносные линии, полки не налезают на букву и
// стрелку, выноски не проходят через букву.
TEST(DrawingAssemblyConventions, BalloonLayoutTwelvePositionsOrderedWithoutCrossings) {
    namespace tb = bendcore::drawing_v2::test_balloons;
    an::BalloonArrangeOptions o;
    o.view = {0, 0, 200, 99};
    o.area = {-150, -100, 400, 220};
    const int numbers[12] = {7, 2, 11, 4, 9, 1, 12, 5, 3, 10, 6, 8};
    std::vector<an::BalloonCandidate> c;
    for (int r = 0; r < 3; ++r)
        for (int q = 0; q < 4; ++q) {
            const double x0 = 50.0 * q, y0 = 33.0 * r, x1 = x0 + 50, y1 = y0 + 33, xc = x0 + 25, yc = y0 + 16.5;
            const int k = r * 4 + q;
            c.push_back({std::uint32_t(100 + k), numbers[k], {{x0 + 3, yc}, {x1 - 3, yc}, {xc, y0 + 3}, {xc, y1 - 3}, {xc + 4, yc + 2}}});
        }
    // Размер 60 справа: выносные от контура, размерная линия x = 210.
    an::DimensionGeometry g;
    g.dimensionLines.push_back({{210, 20}, {210, 80}});
    g.extensionLines.push_back({{201, 20}, {212, 20}});
    g.extensionLines.push_back({{201, 80}, {212, 80}});
    an::DimensionLabel lab;
    lab.position = {209, 50};
    lab.text = "60";
    lab.rotationDeg = 90;
    lab.hAlign = HAlign::Center;
    g.labels.push_back(lab);
    an::addDimensionObstacles(o.obstacles, g);
    // Обозначение разреза справа внизу: буква «Б» (7 мм) и стрелка.
    const Rect letter{214, 2, 221, 9};
    o.obstacles.boxes.push_back(letter);
    o.obstacles.lines.push_back({{205, 12}, {217, 12}});
    o.obstacles.lines.push_back({{205, 10}, {205, 14}});
    auto r = an::arrangeBalloons(c, o);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const auto& b = r.value().balloons;
    ASSERT_EQ(b.size(), 12u);
    EXPECT_TRUE(r.value().ordered) << r.value().outOfOrder.size();
    EXPECT_EQ(r.value().crossings, 0u);
    EXPECT_EQ(r.value().shelfHits, 0u);
    // Независимая проверка по итоговой геометрии.
    const std::vector<std::string> views(b.size(), "V");
    const auto lines = tb::shelfLines(b, views);
    EXPECT_TRUE(tb::unorderedNumbers(lines).empty()) << tb::join(tb::unorderedNumbers(lines));
    EXPECT_TRUE(tb::crossingLeaders(b).empty()) << tb::join(tb::crossingLeaders(b));
    std::vector<LineSeg> dims = g.dimensionLines;
    dims.insert(dims.end(), g.extensionLines.begin(), g.extensionLines.end());
    EXPECT_TRUE(tb::leadersCrossingLines(b, dims).empty()) << tb::join(tb::leadersCrossingLines(b, dims));
    an::AnnotationStyle st;
    EXPECT_TRUE(tb::shelvesHitting(b, o.obstacles.boxes, o.obstacles.lines, st).empty())
        << tb::join(tb::shelvesHitting(b, o.obstacles.boxes, o.obstacles.lines, st));
    for (const auto& x : b) EXPECT_FALSE(tb::segmentHitsRect(x.target, x.at, letter)) << x.text;
    // Каждая точка выноски — одна из допустимых точек своей детали; полка вне изображения; сторона
    // в lines — та, где стоит полка.
    ASSERT_EQ(r.value().lines.size(), b.size());
    for (std::size_t i = 0; i < b.size(); ++i) {
        EXPECT_EQ(b[i].text, std::to_string(c[i].number));
        EXPECT_NE(std::find_if(c[i].targets.begin(), c[i].targets.end(),
                               [&](const Point2& t) { return std::hypot(t.x - b[i].target.x, t.y - b[i].target.y) < 1e-12; }),
                  c[i].targets.end());
        switch (r.value().lines[i]) {
            case an::BalloonLine::Right: EXPECT_GT(b[i].at.x, o.view.x1) << b[i].text; break;
            case an::BalloonLine::Left: EXPECT_LT(b[i].at.x, o.view.x0) << b[i].text; break;
            case an::BalloonLine::Above: EXPECT_GT(b[i].at.y, o.view.y1) << b[i].text; break;
            default: EXPECT_LT(b[i].at.y, o.view.y0) << b[i].text; break;
        }
    }
    // Одна колонна справа (без разбиения): если порядок уступил, номера перечислены, а
    // оставшиеся пересечения сосчитаны верно.
    an::BalloonArrangeOptions one = o;
    one.line = an::BalloonLine::Right;
    one.obstacles = {};
    auto r1 = an::arrangeBalloons(c, one);
    ASSERT_TRUE(r1.ok());
    EXPECT_EQ(r1.value().lineCount, 1u);
    EXPECT_EQ(r1.value().ordered, r1.value().outOfOrder.empty());
    EXPECT_EQ(r1.value().crossings, an::leaderCrossings(r1.value().balloons));
    EXPECT_TRUE(tb::unorderedNumbers(tb::shelfLines(r1.value().balloons, views)).empty() == r1.value().ordered);
    // Другие расстановки номеров по клеткам — те же требования.
    const int others[3][12] = {{12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1},
                               {1, 5, 9, 2, 6, 10, 3, 7, 11, 4, 8, 12},
                               {3, 10, 6, 1, 12, 8, 2, 9, 5, 11, 4, 7}};
    for (const auto& perm : others) {
        std::vector<an::BalloonCandidate> cp = c;
        for (std::size_t i = 0; i < cp.size(); ++i) cp[i].number = perm[i];
        auto rp = an::arrangeBalloons(cp, o);
        ASSERT_TRUE(rp.ok());
        const auto& bp = rp.value().balloons;
        EXPECT_TRUE(rp.value().ordered) << perm[0];
        EXPECT_TRUE(tb::unorderedNumbers(tb::shelfLines(bp, views)).empty()) << tb::join(tb::unorderedNumbers(tb::shelfLines(bp, views)));
        EXPECT_TRUE(tb::crossingLeaders(bp).empty()) << tb::join(tb::crossingLeaders(bp));
        EXPECT_TRUE(tb::leadersCrossingLines(bp, dims).empty()) << tb::join(tb::leadersCrossingLines(bp, dims));
        EXPECT_TRUE(tb::shelvesHitting(bp, o.obstacles.boxes, o.obstacles.lines, st).empty()) << perm[0];
    }
}
