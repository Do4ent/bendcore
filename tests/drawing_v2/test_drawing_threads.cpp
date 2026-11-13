// Изображение резьбы по ГОСТ 2.311 (docs/drawing/ANNOTATIONS.md, раздел «Резьба»). Эталон —
// геометрия модели и таблица резьб (docs/modeling/FEATURES.md, раздел 11), а не сам код:
// брусок 60×40×20 с глухим отверстием M10×1,25LH (длина резьбы 12, глубина 18, сверло
// D − P = 8,75) и сквозным M8 (резьба на всю толщину 20, сверло 6,75).
//
//   вид спереди — ось резьбы ∥ плоскости вида, резьба внутри материала: штриховые линии по
//   наружному диаметру (x = 20 ± 5, 45 ± 4) от z = 20 на длину резьбы, граница резьбы у M10
//   (z = 8, длина 10 = D), у сквозной M8 границы нет;
//   разрез А–А через оси (y = 20) — те же линии сплошные тонкие, граница сплошная основная,
//   штриховка доходит до линии отверстия (x = 20 ± 4,375), а не до линии резьбы;
//   вид сверху — дуга 3/4 окружности R5 и R4 сплошной тонкой; вид снизу — M10 невидима
//   (штриховая дуга), M8 видна (сквозная, резьба доходит до нижнего края);
//   обозначения «M10×1,25LH-6H» и «M8»; копия массива и зеркала изображается так же;
//   экспорт DXF/PDF и документ чертежа (обновление, сохранение).

#include "test_drawing_document_support.hpp"

#include <bendcore/drawing_v2/threads.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <sstream>

using namespace ddtest;
using namespace bendcore::drawing_v2::annotations;
using namespace bendcore::drawing_v2::views;
namespace m = bendcore::modeling;

namespace {

constexpr double kScale = 2.0;

struct ThreadPart {
    m::PartDocument doc{m::partRegistry()};
    ThreadPart() {
        add({"Box", "Block", "", {{"length", "60"}, {"width", "40"}, {"height", "20"}}, {}, {}, false});
        doc.rebuild();
        const m::NamedShape s = doc.finalShape();
        const auto top = s.find(m::ElementKind::Face, "Block:Top");
        if (!top) throw std::runtime_error("no Block:Top");
        const m::TopoReference topRef = m::makeReference(s, s.element(m::ElementKind::Face, *top)).value();
        add({"ThreadedHole", "Tap", "",
             {{"nominal", "10"}, {"pitch", "1.25"}, {"threadLength", "12"}, {"drillDepth", "18"}, {"x", "20"}, {"y", "20"}, {"z", "20"}},
             {{"face", {topRef}}}, {{"hand", "left"}, {"drillPoint", "false"}}, false});
        add({"ThreadedHole", "Through", "", {{"nominal", "8"}, {"threadLength", "0"}, {"x", "45"}, {"y", "20"}, {"z", "20"}},
             {{"face", {topRef}}}, {{"end", "through"}}, false});
        doc.rebuild();
        for (const char* id : {"Tap", "Through"}) {
            auto n = doc.feature(id);
            if (!n || n.value().state != m::FeatureState::Ok) throw std::runtime_error(std::string("feature failed: ") + id);
        }
    }
    void add(m::FeatureSpec spec) {
        if (!doc.addFeature(std::move(spec))) throw std::runtime_error("addFeature");
    }
};

struct Model {
    m::NamedShape named;
    ModelBinding binding;
    std::vector<m::ThreadInfo> threads;
};

Model modelOf(const m::PartDocument& doc) {
    m::NamedShape n = doc.finalShape();
    auto b = ModelBinding::fromNamedShape(n).value();
    auto t = m::threadsOf(n);
    return {std::move(n), std::move(b), std::move(t)};
}

const ThreadImage* byNominal(const std::vector<ThreadImage>& v, double d) {
    for (const auto& t : v)
        if (std::abs(t.thread.nominal - d) < 1e-9) return &t;
    return nullptr;
}

bool sameSeg(const LineSeg& s, Point2 a, Point2 b, double tol = 1e-6) {
    auto near = [&](Point2 p, Point2 q) { return std::hypot(p.x - q.x, p.y - q.y) < tol; };
    return (near(s.a, a) && near(s.b, b)) || (near(s.a, b) && near(s.b, a));
}

std::vector<ThreadImage> imagesOn(const Model& md, const ProjectedView& v) {
    auto r = threadImages(md.threads, md.binding.topology(), v);
    EXPECT_TRUE(r.ok()) << r.error().message;
    return r.ok() ? r.value() : std::vector<ThreadImage>{};
}

}  // namespace

TEST(DrawingThreads, DesignationText) {
    m::ThreadSpec t;
    t.nominal = 10;
    t.pitch = 1.25;
    t.hand = m::ThreadHand::Left;
    EXPECT_EQ(threadDesignationText(t, "6H"), "M10×1,25LH-6H");
    t.hand = m::ThreadHand::Right;
    EXPECT_EQ(threadDesignationText(t), "M10×1,25");
    t.pitch = 1.5;  // крупный шаг M10 (ISO 261) не пишется
    EXPECT_EQ(threadDesignationText(t, "6H"), "M10-6H");
    t.nominal = 2.5;
    t.pitch = 0.45;
    EXPECT_EQ(threadDesignationText(t), "M2,5");
}

TEST(DrawingThreads, SideViewHiddenThreadLinesAndBoundary) {
    ThreadPart part;
    const Model md = modelOf(part.doc);
    ASSERT_EQ(md.threads.size(), 2u);
    const auto fv = projectShape(md.binding.shape(), defaultFrontFrame()).value();
    const auto imgs = imagesOn(md, fv);
    ASSERT_EQ(imgs.size(), 2u);
    const ThreadImage* m10 = byNominal(imgs, 10);
    const ThreadImage* m8 = byNominal(imgs, 8);
    ASSERT_TRUE(m10 && m8);
    // Внутри материала: невидимая резьба (ГОСТ 2.311, п. 2.6).
    EXPECT_EQ(m10->aspect, ThreadAspect::Side);
    EXPECT_EQ(m10->visibility, Visibility::Hidden);
    EXPECT_EQ(m8->visibility, Visibility::Hidden);
    // Наружный диаметр 10: линии x = 15 и 25 от входа z = 20 на длину резьбы 12.
    EXPECT_TRUE(sameSeg(m10->majorLines[0], {25, 20}, {25, 8}) || sameSeg(m10->majorLines[0], {15, 20}, {15, 8}));
    EXPECT_TRUE(sameSeg(m10->majorLines[1], {25, 20}, {25, 8}) || sameSeg(m10->majorLines[1], {15, 20}, {15, 8}));
    EXPECT_NEAR(std::abs(m10->majorLines[0].a.x - m10->majorLines[1].a.x), 10.0, 1e-9);
    // Граница резьбы на z = 8 длиной D = 10 (глухое отверстие глубже резьбы).
    ASSERT_TRUE(m10->boundary.has_value());
    EXPECT_TRUE(sameSeg(*m10->boundary, {15, 8}, {25, 8}));
    EXPECT_NEAR(curveLength(*m10->boundary), 10.0, 1e-9);
    // Сквозная резьба на всю толщину: линии x = 41 и 49 от z = 20 до 0, границы нет.
    EXPECT_FALSE(m8->boundary.has_value());
    EXPECT_NEAR(std::abs(m8->majorLines[0].a.x - m8->majorLines[1].a.x), 8.0, 1e-9);
    EXPECT_NEAR(curveLength(m8->majorLines[0]), 20.0, 1e-9);
    // Линии сверла (D − P) — штриховые очерки HLR, линия резьбы лежит снаружи них.
    bool drillLine = false;
    for (const auto& e : fv.edges) {
        if (const auto* l = std::get_if<LineSeg>(&e.curve); l && e.visibility == Visibility::Hidden &&
                                                            std::abs(l->a.x - (20 - 4.375)) < 1e-6 && std::abs(l->b.x - l->a.x) < 1e-9)
            drillLine = true;
    }
    EXPECT_TRUE(drillLine);

    // Вывод: все линии резьбы — штриховые, граница тоже.
    Drawing d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
    const SheetView sv{fv, kScale, {100, 150}};
    ASSERT_TRUE(renderThreads(d, sv, imgs).ok());
    int dashed = 0;
    for (const auto& e : d.entities()) {
        EXPECT_EQ(e.lineType, LineType::Dashed);
        EXPECT_EQ(e.layer, "HIDDEN");
        ++dashed;
    }
    EXPECT_EQ(dashed, 5);  // 2 + 1 граница (M10) + 2 (M8)
}

TEST(DrawingThreads, SectionShowsSolidThreadAndHatchToTheHoleLine) {
    ThreadPart part;
    const Model md = modelOf(part.doc);
    const ViewFrame front = defaultFrontFrame();
    auto sec = sectionShape(md.binding.shape(), front, CuttingPlane{{0, 20, 0}, {0, 1, 0}});
    ASSERT_TRUE(sec.ok()) << sec.error().message;
    const auto imgs = imagesOn(md, sec.value().view);
    ASSERT_EQ(imgs.size(), 2u);
    const ThreadImage* m10 = byNominal(imgs, 10);
    ASSERT_TRUE(m10);
    EXPECT_EQ(m10->visibility, Visibility::Visible);
    EXPECT_TRUE(sameSeg(*m10->boundary, {15, 8}, {25, 8}));
    // Штриховка: контуры фигуры сечения проходят по линии отверстия x = 20 ± 4,375 (D1-сторона
    // ≈ сверло, ГОСТ 2.311, п. 2.7) и не по линии резьбы x = 15 / 25.
    bool atDrill = false, atMajor = false;
    for (const auto& face : sec.value().regions)
        for (const auto& loop : face)
            for (const Point2& p : loop) {
                if (p.y > 2 + 1e-6 && p.y < 20 - 1e-6) continue;  // только вершины контура
                if (std::abs(p.x - 15.625) < 1e-6 || std::abs(p.x - 24.375) < 1e-6) atDrill = true;
                if (std::abs(p.x - 15.0) < 1e-6 || std::abs(p.x - 25.0) < 1e-6) atMajor = true;
            }
    EXPECT_TRUE(atDrill);
    EXPECT_FALSE(atMajor);
    Drawing d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
    const SheetView sv{sec.value().view, kScale, {100, 150}};
    ASSERT_TRUE(renderThreads(d, sv, imgs).ok());
    int thin = 0, thick = 0;
    for (const auto& e : d.entities()) {
        if (e.lineType == LineType::SolidThin && e.layer == "THIN") ++thin;
        if (e.lineType == LineType::SolidThick && e.layer == "VISIBLE") ++thick;
    }
    EXPECT_EQ(thin, 4);
    EXPECT_EQ(thick, 1);  // граница M10
}

TEST(DrawingThreads, EndViewsArcThreeQuartersVisibleAndHidden) {
    ThreadPart part;
    const Model md = modelOf(part.doc);
    const ViewFrame front = defaultFrontFrame();
    const auto top = projectShape(md.binding.shape(), standardFrame(front, StandardView::Top)).value();
    const auto imgs = imagesOn(md, top);
    ASSERT_EQ(imgs.size(), 2u);
    for (const ThreadImage& t : imgs) {
        EXPECT_EQ(t.aspect, ThreadAspect::End);
        EXPECT_EQ(t.visibility, Visibility::Visible);
        EXPECT_NEAR(t.majorArc.radius, t.thread.nominal / 2, 1e-12);
        // ≈ 3/4 окружности (ГОСТ 2.311, п. 2.4): раствор 270°, концы не на центровых линиях.
        double sweep = t.majorArc.endDeg - t.majorArc.startDeg;
        if (sweep <= 0) sweep += 360;
        EXPECT_NEAR(sweep, 270.0, 1e-9);
        for (double a : {t.majorArc.startDeg, t.majorArc.endDeg}) EXPECT_GT(std::abs(std::remainder(a, 90.0)), 5.0) << a;
    }
    const ThreadImage* m10 = byNominal(imgs, 10);
    EXPECT_NEAR(m10->majorArc.center.x, 20, 1e-9);
    EXPECT_NEAR(m10->majorArc.center.y, 20, 1e-9);
    // Длина дуги на листе 2:1: 3/4 · 2π · 10.
    Drawing d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
    ASSERT_TRUE(renderThreads(d, SheetView{top, kScale, {50, 50}}, imgs).ok());
    for (const auto& e : d.entities()) {
        const auto& a = std::get<Arc>(e.geometry);
        EXPECT_EQ(e.lineType, LineType::SolidThin);
        if (std::abs(a.radius - 10.0) < 1e-9) EXPECT_NEAR(curveLength(Curve2{a}), 0.75 * 2 * M_PI * 10, 1e-9);
    }

    // Снизу: глухая M10 не видна (дно сверления закрывает резьбу), сквозная M8 видна.
    const auto bottom = projectShape(md.binding.shape(), standardFrame(front, StandardView::Bottom)).value();
    const auto under = imagesOn(md, bottom);
    ASSERT_EQ(under.size(), 2u);
    EXPECT_EQ(byNominal(under, 10)->visibility, Visibility::Hidden);
    EXPECT_EQ(byNominal(under, 8)->visibility, Visibility::Visible);

    // Аксонометрия: резьба не изображается.
    const auto iso = projectShape(md.binding.shape(), axonometricFrame(front, Axonometry::Isometric)).value();
    EXPECT_TRUE(imagesOn(md, iso).empty());
}

TEST(DrawingThreads, DesignationDimensionOnSideAndEndViews) {
    ThreadPart part;
    const Model md = modelOf(part.doc);
    const auto fv = projectShape(md.binding.shape(), defaultFrontFrame()).value();
    const SheetView sv{fv, kScale, {100, 150}};
    const ThreadImage* m10 = byNominal(imagesOn(md, fv), 10);
    ASSERT_TRUE(m10);
    const ThreadImage img = *m10;
    ThreadDimensionDef def;
    def.offset = 10;
    def.toleranceField = "6H";
    auto r = threadDimension(img, sv, def);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r.value().text, "M10×1,25LH-6H");
    EXPECT_NEAR(r.value().value, 10.0, 1e-9);
    // Размерная линия над входом (z = 20 → y листа 150 + 40 = 190) на 10 мм, длина 2·10 = 20.
    ASSERT_EQ(r.value().geometry.dimensionLines.size(), 1u);
    const LineSeg dl = r.value().geometry.dimensionLines[0];
    EXPECT_NEAR(dl.a.y, 200.0, 1e-9);
    EXPECT_NEAR(dl.b.y, 200.0, 1e-9);
    EXPECT_NEAR(std::abs(dl.b.x - dl.a.x), 20.0, 1e-9);
    EXPECT_NEAR(std::min(dl.a.x, dl.b.x), 100 + kScale * 15, 1e-9);
    // Выносные линии от концов линий резьбы у входа.
    ASSERT_EQ(r.value().geometry.extensionLines.size(), 2u);
    for (const LineSeg& e : r.value().geometry.extensionLines) EXPECT_NEAR(std::min(e.a.y, e.b.y), 190.0, 1e-9);
    EXPECT_EQ(r.value().geometry.arrows.size(), 2u);

    const auto top = projectShape(md.binding.shape(), standardFrame(defaultFrontFrame(), StandardView::Top)).value();
    const SheetView tv{top, kScale, {100, 40}};
    const ThreadImage* m8 = byNominal(imagesOn(md, top), 8);
    ASSERT_TRUE(m8);
    const ThreadImage end = *m8;
    auto e = threadDimension(end, tv, {});
    ASSERT_TRUE(e.ok());
    EXPECT_EQ(e.value().text, "M8");
    EXPECT_NEAR(e.value().value, 8.0, 1e-12);
    // Стрелки — на дуге R4 (8 мм листа) под 45° и 225° — внутри дуги 3/4 (разрыв 260°…350°).
    ASSERT_EQ(e.value().geometry.arrows.size(), 2u);
    const Point2 c = tv.toSheet({45, 20});
    for (const SolidFill& a : e.value().geometry.arrows) {
        const Point2 tip = a.points[0];
        EXPECT_NEAR(std::hypot(tip.x - c.x, tip.y - c.y), 8.0, 1e-9);
        double ang = std::atan2(tip.y - c.y, tip.x - c.x) * 180 / M_PI;
        if (ang < 0) ang += 360;
        EXPECT_FALSE(ang > 260 && ang < 350) << ang;
    }
}

TEST(DrawingThreads, PatternAndMirrorCopiesAreDrawn) {
    ThreadPart part;
    part.add({"LinearPattern", "Row", "", {{"count1", "2"}, {"spacing1", "-15"}}, {}, {{"direction1", "X"}}, false,
              {{"feature", "Tap"}}});
    part.doc.rebuild();
    ASSERT_EQ(part.doc.feature("Row").value().state, m::FeatureState::Ok);
    const Model md = modelOf(part.doc);
    const auto fv = projectShape(md.binding.shape(), defaultFrontFrame()).value();
    const auto imgs = imagesOn(md, fv);
    ASSERT_EQ(imgs.size(), 3u);
    int copies = 0;
    for (const ThreadImage& t : imgs) {
        if (std::abs(t.entry.x - 5.0) > 1e-9) continue;
        ++copies;
        EXPECT_EQ(threadDesignationText(t.thread), "M10×1,25LH");
        ASSERT_TRUE(t.boundary.has_value());
        EXPECT_TRUE(sameSeg(*t.boundary, {0, 8}, {10, 8}));
    }
    EXPECT_EQ(copies, 1);
}

TEST(DrawingThreads, DocumentDrawsThreadsAndExportsDxfPdf) {
    const TrueTypeFont* font = dv2test::systemFont();
    if (!font) GTEST_SKIP() << "no system TrueType font";
    ThreadPart part;
    auto snap = snapshotPart(part.doc, {"РД.20.001", "Плита резьбовая", "Сталь 45", 7850.0});
    ASSERT_TRUE(snap.ok()) << snap.error().message;

    DrawingDocument doc;
    doc.model().path = "plate.json";
    SheetDef s;
    s.id = "S1";
    s.scale = ScaleRatio{2, 1};
    ViewDef f;
    f.id = "F";
    ViewDef t;
    t.id = "T";
    t.kind = ViewKind::Projected;
    t.parent = "F";
    t.side = SheetSide::Below;
    ViewDef a;
    a.id = "A";
    a.kind = ViewKind::Section;  // вне проекционной связи, ориентация как у главного вида
    a.markView = "T";
    a.planePoint = {0, 20, 0};
    a.toward = {0, -1, 0};
    s.views = {f, t, a};
    ThreadNoteItem side;
    side.id = "TH1";
    side.view = "A";
    const auto& faceRec = std::find_if(snap.value().named->records(m::ElementKind::Face).begin(),
                                       snap.value().named->records(m::ElementKind::Face).end(),
                                       [](const m::ElementRecord& r) { return r.name == "Tap:Wall"; });
    ASSERT_NE(faceRec, snap.value().named->records(m::ElementKind::Face).end());
    const std::size_t wallIndex = faceRec - snap.value().named->records(m::ElementKind::Face).begin();
    side.face = elementReference(snap.value(), m::ElementKind::Face, static_cast<std::uint32_t>(wallIndex)).value();
    side.def.toleranceField = "6H";
    ThreadNoteItem endNote;
    endNote.id = "TH2";
    endNote.view = "T";
    endNote.face = side.face;
    s.threadNotes = {side, endNote};
    doc.sheets().push_back(s);
    doc.setFont(font);
    ViewCache cache(8, 1);
    auto rep = doc.update(snap.value(), cache);
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    EXPECT_TRUE(rep.value().dangling.empty()) << rep.value().dangling.front();
    const ComputedSheet& cs = doc.computedSheets()[0];
    ASSERT_EQ(cs.views.size(), 3u);
    for (const ComputedView& cv : cs.views) EXPECT_EQ(cv.threads.size(), 2u) << cv.id;
    ASSERT_EQ(cs.threadNotes.size(), 2u);
    EXPECT_EQ(cs.threadNotes[0].text, "M10×1,25LH-6H");
    EXPECT_EQ(cs.threadNotes[1].text, "M10×1,25LH");
    EXPECT_NEAR(cs.threadNotes[0].value, 10.0, 1e-9);

    auto sheets = doc.render(font);
    ASSERT_TRUE(sheets.ok()) << sheets.error().message;
    const Drawing& d = sheets.value()[0];
    // Ожидаемые линии резьбы M10 разреза на листе (anchor + 2·p).
    const ComputedView& sec = cs.views[2];
    const Point2 l0 = sec.sheetView.toSheet({15, 20}), l1 = sec.sheetView.toSheet({15, 8});
    const Point2 b0 = sec.sheetView.toSheet({15, 8}), b1 = sec.sheetView.toSheet({25, 8});
    const ComputedView& tv = cs.views[1];
    const Point2 arcCenter = tv.sheetView.toSheet({20, 20});

    auto dxf = writeDxf(d);
    ASSERT_TRUE(dxf.ok()) << dxf.error().message;
    const std::string dxfPath = dv2test::outPath("threads_a3.dxf");
    ASSERT_TRUE(saveToFile(dxfPath, dxf.value()).ok());
    const auto file = dv2test::parseDxf(dxf.value());
    const auto* ents = file.section("ENTITIES");
    ASSERT_NE(ents, nullptr);
    bool thinLine = false, boundary = false, arc = false, hiddenThread = false;
    std::vector<std::string> texts;
    auto match = [](double x0, double y0, double x1, double y1, Point2 a, Point2 b) {
        const double t = 1e-6;
        return (std::abs(x0 - a.x) < t && std::abs(y0 - a.y) < t && std::abs(x1 - b.x) < t && std::abs(y1 - b.y) < t) ||
               (std::abs(x0 - b.x) < t && std::abs(y0 - b.y) < t && std::abs(x1 - a.x) < t && std::abs(y1 - a.y) < t);
    };
    const ComputedView& fvc = cs.views[0];
    const Point2 h0 = fvc.sheetView.toSheet({15, 20}), h1 = fvc.sheetView.toSheet({15, 8});
    for (const auto& e : *ents) {
        if (e.type == "TEXT") texts.push_back(dv2test::decodeDxfUnicode(e.first(1).value_or("")));
        if (e.type == "LINE") {
            const double x0 = e.real(10), y0 = e.real(20), x1 = e.real(11), y1 = e.real(21);
            const std::string layer = e.first(8).value_or("");
            if (layer == "THIN" && match(x0, y0, x1, y1, l0, l1)) {
                thinLine = true;
                EXPECT_EQ(e.real(370), 25);  // сплошная тонкая s/2
            }
            if (layer == "VISIBLE" && match(x0, y0, x1, y1, b0, b1)) {
                boundary = true;
                EXPECT_EQ(e.real(370), 50);  // сплошная основная s
            }
            if (layer == "HIDDEN" && match(x0, y0, x1, y1, h0, h1)) {
                hiddenThread = true;
                EXPECT_NE(e.first(6).value_or("").find("DASHED"), std::string::npos);
            }
        }
        if (e.type == "ARC" && std::abs(e.real(40) - 10.0) < 1e-9 && std::abs(e.real(10) - arcCenter.x) < 1e-6 &&
            std::abs(e.real(20) - arcCenter.y) < 1e-6) {
            arc = true;
            EXPECT_NEAR(e.real(50), 350.0, 1e-9);
            EXPECT_NEAR(e.real(51), 260.0, 1e-9);
            EXPECT_EQ(e.first(8).value_or(""), "THIN");
        }
    }
    EXPECT_TRUE(thinLine);
    EXPECT_TRUE(boundary);
    EXPECT_TRUE(arc);
    EXPECT_TRUE(hiddenThread);
    for (const char* text : {"M10×1,25LH-6H", "M10×1,25LH"})
        EXPECT_NE(std::find(texts.begin(), texts.end(), text), texts.end()) << text;

    PdfOptions po;
    po.font = font;
    auto pdf = writePdf(d, po);
    ASSERT_TRUE(pdf.ok()) << pdf.error().message;
    const std::string pdfPath = dv2test::outPath("threads_a3.pdf");
    ASSERT_TRUE(saveToFile(pdfPath, pdf.value()).ok());
    const auto pf = dv2test::parsePdf(pdf.value());
    // Граница резьбы в пунктах (мм · 72 / 25,4).
    const double pt = 72.0 / 25.4;
    bool pdfBoundary = false;
    for (const auto& o : pf.objects) {
        std::istringstream is(o.stream);
        std::string ln;
        while (std::getline(is, ln)) {
            double x0, y0, x1, y1;
            char m1[4], l1s[4];
            if (std::sscanf(ln.c_str(), "%lf %lf %3s %lf %lf %3s", &x0, &y0, m1, &x1, &y1, l1s) == 6 && std::string(m1) == "m" &&
                std::string(l1s) == "l" && match(x0 / pt, y0 / pt, x1 / pt, y1 / pt, b0, b1))
                pdfBoundary = true;
        }
    }
    EXPECT_TRUE(pdfBoundary);
#ifdef BENDCORE_PDFTOTEXT
    int code = 0;
    const std::string out = dv2test::runCommand(std::string(BENDCORE_PDFTOTEXT) + " -enc UTF-8 " + pdfPath + " - 2>&1", &code);
    EXPECT_EQ(code, 0) << out;
    EXPECT_NE(out.find("M10×1,25LH-6H"), std::string::npos) << out;
#endif

    // Сохранение и загрузка: размеры резьбы и флаг видов сохраняются.
    auto json = doc.toJson();
    ASSERT_TRUE(json.ok());
    auto loaded = DrawingDocument::fromJson(json.value());
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    ASSERT_EQ(loaded.value().sheets()[0].threadNotes.size(), 2u);
    EXPECT_EQ(loaded.value().sheets()[0].threadNotes[0].def.toleranceField, "6H");
    EXPECT_EQ(loaded.value().toJson().value(), json.value());

    // Подавленное отверстие: размер резьбы висячий с причиной.
    ASSERT_TRUE(part.doc.setSuppressed("Tap", true).ok());
    part.doc.rebuild();
    auto snap2 = snapshotPart(part.doc, {"РД.20.001", "Плита резьбовая", "Сталь 45", 7850.0});
    ASSERT_TRUE(snap2.ok());
    auto rep2 = doc.update(snap2.value(), cache);
    ASSERT_TRUE(rep2.ok()) << rep2.error().message;
    EXPECT_EQ(doc.computedSheets()[0].threadNotes[0].state, DimensionState::Dangling);
    EXPECT_EQ(rep2.value().dangling.size(), 2u);
    for (const ComputedView& cv : doc.computedSheets()[0].views) EXPECT_EQ(cv.threads.size(), 1u) << cv.id;
}
