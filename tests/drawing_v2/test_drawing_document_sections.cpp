// Документ чертежа: местный и ступенчатый разрезы на листе (ГОСТ 2.305, разд. 6), условности
// разрезов сборки «вал + втулка + болты» (ГОСТ 2.305, разд. 9; ГОСТ 2.109) — крепёж и вал в
// продольном разрезе не рассекаются, у смежных рассечённых деталей разная штриховка (ГОСТ 2.306);
// отмена и повтор правок (DrawingHistory). Эталон — площади фигур сечения по размерам деталей.

#include "test_drawing_document_support.hpp"

#include <bendcore/assembly_v2/fasteners.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <numbers>
#include <set>

using namespace ddtest;
using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::fasteners;
using bendcore::drawing_v2::views::LocalSection;

namespace {

const double kPi = std::numbers::pi;

// Узел: вал Ø30 (z ∈ [−40, 80]), втулка фланцевая (диск Ø90×12 с отверстием Ø30, ступица Ø50×20,
// 4 отверстия Ø11 на окружности Ø70 — в (±35, 0), (0, ±35)), 4 пакета «болт М10×30 ГОСТ 7798 —
// шайба ГОСТ 11371 — шайба — гайка ГОСТ 5915».
struct Unit {
    std::shared_ptr<AssemblyDocument> doc = std::make_shared<AssemblyDocument>("Узел вала");
    DefinitionId shaft = 0, bushing = 0, bolt = 0, washer = 0, nut = 0;
};

Unit buildUnit() {
    Unit u;
    auto rod = makeRod(15, 120);
    auto bush = makeBushing();
    EXPECT_TRUE(u.doc->setProperties(u.doc->root(), {"ВТ.01.000", "Узел вала", "", std::nullopt}).ok());
    u.shaft = u.doc->addPart("Вал", partDataFromDocument(*rod).value(), {"ВТ.01.001", "Вал", "Сталь 45", 7850}).value();
    u.bushing = u.doc->addPart("Втулка", partDataFromDocument(*bush).value(), {"ВТ.01.002", "Втулка", "Сталь 20", 7850}).value();
    const OccurrenceId bushingOcc = u.doc->addOccurrence(u.bushing, Transform::identity(), "Втулка").value();
    EXPECT_TRUE(u.doc->setFixed(bushingOcc, true).ok());
    u.doc->addOccurrence(u.shaft, Transform::fromTranslation(bendcore::assembly_v2::Vec3(0, 0, -40)), "Вал").value();
    u.bolt = addFastenerPart(*u.doc, makeFastener({FastenerStandard::Gost7798, 10, 30}).value()).value();
    u.washer = addFastenerPart(*u.doc, makeFastener({FastenerStandard::Gost11371, 10, 0}).value()).value();
    u.nut = addFastenerPart(*u.doc, makeFastener({FastenerStandard::Gost5915, 10, 0}).value()).value();
    auto sites = findHoleSites(*u.doc, {bushingOcc}, bendcore::assembly_v2::Vec3(0, 0, -1), 5.5);
    EXPECT_TRUE(sites.ok());
    EXPECT_EQ(sites.value().size(), 4u);
    EXPECT_TRUE(insertFastenerStacks(*u.doc, {u.bolt, {u.washer}, {u.washer}, u.nut}, sites.value()).ok());
    return u;
}

ViewDef view(const std::string& id, ViewKind kind) {
    ViewDef v;
    v.id = id;
    v.kind = kind;
    v.hidden = false;
    return v;
}

// Площадь фигур сечения по деталям (определениям) — по контурам областей (мм² модели).
std::map<DefinitionId, double> areaByDefinition(const ComputedView& cv, const ModelSnapshot& snap) {
    std::map<DefinitionId, double> out;
    const auto& s = *cv.section;
    for (std::size_t i = 0; i < s.regions.size(); ++i) {
        double a = 0;
        for (const auto& loop : s.regions[i]) {
            double l = 0;
            for (std::size_t k = 0; k < loop.size(); ++k) {
                const Point2 p = loop[k], q = loop[(k + 1) % loop.size()];
                l += p.x * q.y - q.x * p.y;
            }
            a += 0.5 * l;
        }
        out[snap.childOwner.at(std::size_t(s.regionOwner[i]))] += std::abs(a);
    }
    return out;
}

const ComputedView& computedView(const DrawingDocument& d, const std::string& id) {
    for (const ComputedView& v : d.computedSheets()[0].views)
        if (v.id == id) return v;
    throw std::runtime_error("no view " + id);
}

}  // namespace

TEST(DrawingDocumentSections, SectionRules) {
    PartProperties fastener;
    fastener.name = "Болт М10×30 ГОСТ 7798";
    EXPECT_EQ(sectionRule(fastener, "Болт М10×30 ГОСТ 7798"), SectionRule::WholeInLongitudinal);
    EXPECT_EQ(sectionRule({"", "Шарик 10 ГОСТ 3722", "", std::nullopt}, "x"), SectionRule::NeverCut);
    EXPECT_EQ(sectionRule({"ВТ.01.001", "Вал", "", std::nullopt}, "Вал"), SectionRule::WholeInLongitudinal);
    EXPECT_EQ(sectionRule({"ВТ.01.003", "ось", "", std::nullopt}, "p"), SectionRule::WholeInLongitudinal);
    EXPECT_EQ(sectionRule({"ВТ.01.004", "Валик", "", std::nullopt}, "Валик"), SectionRule::Cut);
    EXPECT_EQ(sectionRule({"ВТ.01.002", "Втулка", "", std::nullopt}, "Втулка"), SectionRule::Cut);
    PartProperties forced{"ВТ.01.001", "Вал", "", std::nullopt, true};
    EXPECT_EQ(sectionRule(forced, "Вал"), SectionRule::Cut);
    PartProperties kept{"ВТ.01.002", "Корпус", "", std::nullopt, false};
    EXPECT_EQ(sectionRule(kept, "Корпус"), SectionRule::WholeInLongitudinal);
}

TEST(DrawingDocumentSections, AssemblyLongitudinalSectionKeepsFastenersAndShaftWhole) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    Unit u = buildUnit();
    auto snap = snapshotAssembly(u.doc);
    ASSERT_TRUE(snap.ok()) << snap.error().message;
    ASSERT_EQ(snap.value().childOwner.size(), 2u + 4u * 4u);

    DrawingDocument d;
    d.model().kind = ModelKind::Assembly;
    SheetDef s;
    s.id = "S1";
    s.format = {GostFormat::A2, 1, Orientation::Landscape};
    s.views.push_back(view("F", ViewKind::Base));
    ViewDef t = view("T", ViewKind::Projected);
    t.parent = "F";
    t.side = views::SheetSide::Below;
    s.views.push_back(t);
    // Разрез А–А плоскостью y = 0 через ось вала и оси двух болтов (продольный).
    ViewDef a = view("A", ViewKind::Section);
    a.toward = {0, -1, 0};
    a.right = {1, 0, 0};
    a.planePoint = {0, 0, 0};
    a.markView = "T";
    a.centerLines = false;
    s.views.push_back(a);
    // Разрез Б–Б плоскостью z = 6 (поперёк оси вала, через диск втулки).
    ViewDef b = view("B", ViewKind::Section);
    b.toward = {0, 0, 1};
    b.right = {1, 0, 0};
    b.planePoint = {0, 0, 6};
    b.markView = "F";
    b.centerLines = false;
    s.views.push_back(b);
    d.sheets().push_back(s);
    d.setFont(font);
    views::ViewCache cache(16, 2);
    auto rep = d.update(snap.value(), cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;

    // Продольный разрез: рассечена только втулка. Сечение втулки: диск (45 − 15 − 11)·12 и
    // ступица (25 − 15)·20 с каждой стороны: 2·(228 + 200) = 856 мм².
    const ComputedView& av = computedView(d, "A");
    ASSERT_TRUE(av.section);
    auto areas = areaByDefinition(av, snap.value());
    EXPECT_EQ(areas.size(), 1u);
    EXPECT_NEAR(areas[u.bushing], 856.0, 1e-3);
    EXPECT_NEAR(av.section->sectionArea, 856.0, 1e-3);
    for (DefinitionId def : {u.shaft, u.bolt, u.washer, u.nut}) EXPECT_EQ(areas.count(def), 0u) << def;
    // Вал нерассечённый — его очерк x = ±15 виден по всей длине 120.
    double shaftEdge = 0;
    for (const views::ViewEdge& e : av.sheetView.view.edges) {
        const auto* l = std::get_if<LineSeg>(&e.curve);
        if (!l || e.visibility != views::Visibility::Visible || std::abs(l->a.x - 15) > 1e-6 || std::abs(l->b.x - 15) > 1e-6) continue;
        shaftEdge += std::abs(l->b.y - l->a.y);
    }
    EXPECT_NEAR(shaftEdge, 120.0, 1e-3);

    // Поперечный разрез: вал рассечён (круг π·15²), болты рассечены, втулка — кольцо с отверстиями.
    const ComputedView& bv = computedView(d, "B");
    auto bAreas = areaByDefinition(bv, snap.value());
    EXPECT_NEAR(bAreas[u.shaft], kPi * 225.0, 0.002 * kPi * 225.0);
    EXPECT_NEAR(bAreas[u.bushing], kPi * (45.0 * 45.0 - 15.0 * 15.0) - 4 * kPi * 5.5 * 5.5, 0.002 * kPi * 1800.0);
    EXPECT_GT(bAreas[u.bolt], 0.0);
    // Смежные вал и втулка — встречная штриховка.
    const auto pat = views::hatchPatterns(*bv.section);
    std::map<DefinitionId, double> angle;
    for (std::size_t i = 0; i < pat.size(); ++i) angle[snap.value().childOwner[std::size_t(bv.section->regionOwner[i])]] = pat[i].angleDeg;
    EXPECT_NE(angle[u.shaft], angle[u.bushing]);

    // Свойство компонента «рассекать» у вала: в продольном разрезе вал рассечён (30·120 = 3600) и
    // штрихуется иначе, чем смежная втулка.
    PartProperties p = u.doc->definition(u.shaft).properties;
    p.sectionCut = true;
    ASSERT_TRUE(u.doc->setProperties(u.shaft, p).ok());
    auto snap2 = snapshotAssembly(u.doc);
    ASSERT_TRUE(snap2.ok());
    ASSERT_TRUE(d.update(snap2.value(), cache).ok());
    const ComputedView& av2 = computedView(d, "A");
    auto areas2 = areaByDefinition(av2, snap2.value());
    EXPECT_NEAR(areas2[u.shaft], 3600.0, 1e-3);
    EXPECT_NEAR(areas2[u.bushing], 856.0, 1e-3);
    const auto pat2 = views::hatchPatterns(*av2.section);
    std::map<DefinitionId, std::set<double>> angles2;
    for (std::size_t i = 0; i < pat2.size(); ++i)
        angles2[snap2.value().childOwner[std::size_t(av2.section->regionOwner[i])]].insert(pat2[i].angleDeg);
    ASSERT_EQ(angles2[u.shaft].size(), 1u);
    ASSERT_EQ(angles2[u.bushing].size(), 1u);
    EXPECT_NE(*angles2[u.shaft].begin(), *angles2[u.bushing].begin());

    // На листе: штриховки двух направлений.
    auto sheets = d.render(font);
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    std::set<double> hatchAngles;
    for (const bendcore::drawing_v2::Entity& e : sheets.value()[0].entities())
        if (const auto* h = std::get_if<Hatch>(&e.geometry)) hatchAngles.insert(h->angleDeg);
    EXPECT_TRUE(hatchAngles.count(45.0));
    EXPECT_TRUE(hatchAngles.count(135.0));
}

namespace {

// Деталь «вал» Ø30×120 (ось Z, от z = 0): вид спереди, местный разрез через ось.
DrawingDocument rodDrawing(const std::vector<LocalSection>& cuts) {
    DrawingDocument d;
    d.model().kind = ModelKind::Part;
    d.model().properties = {"РД.20.001", "Вал", "Сталь 45", 7850.0};
    SheetDef s;
    s.id = "S1";
    s.format = {GostFormat::A3, 1, Orientation::Landscape};
    ViewDef f = view("F", ViewKind::Base);
    f.localSections = cuts;
    s.views.push_back(f);
    ViewDef t = view("T", ViewKind::Projected);
    t.parent = "F";
    t.side = views::SheetSide::Below;
    s.views.push_back(t);
    d.sheets().push_back(s);
    return d;
}

LocalSection rect(double x0, double y0, double x1, double y1, double depth) {
    return LocalSection{{{{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, false}, depth};
}

}  // namespace

TEST(DrawingDocumentSections, LocalSectionOnThePartDrawing) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    auto rod = makeRod(15, 120);
    auto snap = snapshotPart(*rod, {});
    ASSERT_TRUE(snap.ok());
    // Глубина 15 от передней точки (y = −15) — плоскость y = 0 через ось: сечение 10×20 = 200.
    DrawingDocument d = rodDrawing({rect(-5, 40, 5, 60, 15)});
    d.setFont(font);
    views::ViewCache cache(8, 1);
    auto rep = d.update(snap.value(), cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const ComputedView& f = computedView(d, "F");
    ASSERT_TRUE(f.section);
    EXPECT_NEAR(f.section->sectionArea, 200.0, 1e-6);
    ASSERT_EQ(f.section->breakLines.size(), 1u);
    // Лист: штриховка Σ длин · шаг = 200·M², волнистая линия на слое THIN.
    auto sheets = d.render(font);
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    const double M = f.scale.value();
    double hatch = 0, spacing = 0;
    int wavy = 0;
    for (const bendcore::drawing_v2::Entity& e : sheets.value()[0].entities()) {
        if (const auto* h = std::get_if<Hatch>(&e.geometry)) {
            spacing = h->spacing;
            const auto segs = hatchSegments(*h);
            ASSERT_TRUE(segs.ok());
            for (const LineSeg& l : segs.value()) hatch += std::hypot(l.b.x - l.a.x, l.b.y - l.a.y);
        }
        if (e.lineType == LineType::SolidWavy) ++wavy;
    }
    EXPECT_EQ(wavy, 1);
    EXPECT_NEAR(hatch * spacing, 200.0 * M * M, 0.03 * 200.0 * M * M);

    // Повторное обновление — из кэша документа.
    auto again = d.update(snap.value(), cache);
    ASSERT_TRUE(again.ok());
    EXPECT_EQ(again.value().viewsComputed, 0u);

    // Сохранение и загрузка сохраняют область, сплайн и глубину.
    d.sheets()[0].views[0].localSections.push_back(LocalSection{{{{-10, 90}, {0, 100}, {10, 90}, {0, 80}}, true}, 5});
    auto json = d.toJson();
    ASSERT_TRUE(json.ok());
    auto back = DrawingDocument::fromJson(json.value());
    ASSERT_TRUE(back.ok()) << back.error().message;
    const auto& ls = back.value().sheets()[0].views[0].localSections;
    ASSERT_EQ(ls.size(), 2u);
    EXPECT_TRUE(ls[1].boundary.spline);
    EXPECT_DOUBLE_EQ(ls[1].depth, 5.0);
    EXPECT_DOUBLE_EQ(ls[0].boundary.points[2].y, 60.0);
    EXPECT_EQ(back.value().toJson().value(), json.value());

    // Ошибки: область на аксонометрии, глубина 0, область вне детали.
    DrawingDocument bad = rodDrawing({rect(-5, 40, 5, 60, 0)});
    bad.setFont(font);
    EXPECT_FALSE(bad.update(snap.value(), cache).ok());
    DrawingDocument miss = rodDrawing({rect(100, 40, 120, 60, 10)});
    miss.setFont(font);
    auto mr = miss.update(snap.value(), cache);
    ASSERT_FALSE(mr.ok());
    EXPECT_NE(mr.error().message.find("does not cut"), std::string::npos);
}

TEST(DrawingDocumentSections, SteppedSectionOnTheSheet) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    auto bush = makeBushing();
    auto snap = snapshotPart(*bush, {});
    ASSERT_TRUE(snap.ok());
    // Втулка: плоскость y = 0 (ось) левее x = 17,5 и плоскость y = 35 через отверстие (0, 35)
    // правее. Сечение плоскостью y = 35 — диск с отверстием Ø11 в x ∈ [−5,5; 5,5]: часть x > 17,5
    // в нём — √(45² − 35²) ≈ 28,28 > 17,5, т. е. полоса x ∈ [17,5; 28,28] толщиной 12.
    DrawingDocument d;
    d.model().kind = ModelKind::Part;
    SheetDef s;
    s.id = "S1";
    s.format = {GostFormat::A3, 1, Orientation::Landscape};
    s.views.push_back(view("F", ViewKind::Base));
    ViewDef t = view("T", ViewKind::Projected);
    t.parent = "F";
    t.side = views::SheetSide::Below;
    s.views.push_back(t);
    ViewDef a = view("A", ViewKind::Section);
    a.toward = {0, -1, 0};
    a.right = {1, 0, 0};
    a.planePoint = {0, 0, 0};
    a.stepPoints = {{35, 35, 0}};
    a.markView = "T";
    s.views.push_back(a);
    d.sheets().push_back(s);
    d.setFont(font);
    views::ViewCache cache(8, 1);
    auto rep = d.update(snap.value(), cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const ComputedView& av = computedView(d, "A");
    ASSERT_TRUE(av.section);
    // Левее x = 17,5 плоскость y = 0: диск x ∈ [−45; −15] без отверстия (−35, 0): (30 − 11)·12,
    // ступица (25 − 15)·20, и диск x ∈ [15; 17,5]·12; правее — плоскость y = 35.
    const double half = std::sqrt(45.0 * 45.0 - 35.0 * 35.0);
    const double expected = 19.0 * 12 + 200.0 + 2.5 * 12 + 2.5 * 20 + (half - 17.5) * 12;
    EXPECT_NEAR(av.section->sectionArea, expected, 1e-3);
    auto sheets = d.render(font);
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    // Обозначение: штрихи разомкнутой линии у концов (2) и в перегибе (2 вдоль следов + перемычка).
    int open = 0;
    for (const bendcore::drawing_v2::Entity& e : sheets.value()[0].entities())
        if (e.lineType == LineType::Open) ++open;
    EXPECT_GE(open, 5);
    auto json = d.toJson();
    ASSERT_TRUE(json.ok());
    auto back = DrawingDocument::fromJson(json.value());
    ASSERT_TRUE(back.ok());
    ASSERT_EQ(back.value().sheets()[0].views[2].stepPoints.size(), 1u);
    EXPECT_DOUBLE_EQ(back.value().sheets()[0].views[2].stepPoints[0].y, 35.0);
}

TEST(DrawingDocumentSections, UndoRedoRestoresTheDocument) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    auto rod = makeRod(15, 120);
    auto snap = snapshotPart(*rod, {});
    ASSERT_TRUE(snap.ok());
    DrawingDocument d = rodDrawing({});
    d.setFont(font);
    views::ViewCache cache(8, 1);
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    DrawingHistory h(3);
    const std::string s0 = d.toJson().value();

    // Правка 1: местный разрез; правка 2: технические требования и свойства; правка 3: масштаб.
    h.record(captureEditState(d), "local section");
    d.sheets()[0].views[0].localSections.push_back(rect(-5, 40, 5, 60, 15));
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    const std::string s1 = d.toJson().value();
    h.record(captureEditState(d), "requirements");
    d.sheets()[0].technicalRequirements = {"Острые кромки притупить."};
    d.model().properties.designation = "РД.20.002";
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    const std::string s2 = d.toJson().value();
    EXPECT_NE(s0, s1);
    EXPECT_NE(s1, s2);
    EXPECT_EQ(h.undoLabel(), "requirements");

    auto u1 = h.undo(d);
    ASSERT_TRUE(u1.ok());
    EXPECT_EQ(u1.value(), "requirements");
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    EXPECT_EQ(d.toJson().value(), s1);
    ASSERT_TRUE(h.undo(d).ok());
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    EXPECT_EQ(d.toJson().value(), s0);
    EXPECT_FALSE(computedView(d, "F").section);
    EXPECT_FALSE(h.undo(d).ok());
    EXPECT_EQ(h.redoDepth(), 2u);
    ASSERT_TRUE(h.redo(d).ok());
    ASSERT_TRUE(h.redo(d).ok());
    ASSERT_TRUE(d.update(snap.value(), cache).ok());
    EXPECT_EQ(d.toJson().value(), s2);
    EXPECT_FALSE(h.redo(d).ok());

    // Новая правка после отмены очищает повтор; глубина ограничена (3).
    ASSERT_TRUE(h.undo(d).ok());
    h.record(captureEditState(d), "scale");
    d.sheets()[0].scale = views::ScaleRatio{1, 2};
    EXPECT_FALSE(h.canRedo());
    for (int i = 0; i < 5; ++i) h.record(captureEditState(d), "noop " + std::to_string(i));
    EXPECT_EQ(h.undoDepth(), 3u);
}
