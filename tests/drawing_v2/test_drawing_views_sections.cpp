// Разрезы и сечения (ГОСТ 2.305, разделы 6–7), штриховка (ГОСТ 2.306), след секущей
// плоскости, выносной элемент (раздел 8).

#include "test_drawing_views_support.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

using namespace dvtest;
using bendcore::kernel::Shape;
using bendcore::kernel::TopologyIndex;

namespace {

// Фронтальный разрез через ось отверстия: плоскость y = 20, взгляд вдоль +Y.
const CuttingPlane kPlane{{0, 20, 0}, {0, 1, 0}};

SectionView frontSection(SectionKind kind, bool hidden = false) {
    HlrOptions o;
    o.hidden = hidden;
    auto r = sectionShape(blockWithHole(), defaultFrontFrame(), kPlane, kind, o);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    return r.ok() ? r.value() : SectionView{};
}

}  // namespace

TEST(DrawingViewsSections, CutThroughHoleAreaAndLines) {
    // Фигура сечения — два прямоугольника 25×20 (брусок 60 минус отверстие 10 по ширине):
    // площадь 2·25·20 = 1000 мм².
    const SectionView s = frontSection(SectionKind::Cut);
    EXPECT_NEAR(s.sectionArea, 1000.0, 1e-6);
    ASSERT_EQ(s.regions.size(), 2u);
    double area = 0;
    for (const auto& region : s.regions)
        for (const auto& loop : region) area += std::abs(polygonArea(loop));
    EXPECT_NEAR(area, 1000.0, 1e-6);

    // Контур сечения: стенки отверстия x = 25 и x = 35 — линии сечения, ссылка на
    // цилиндрическую грань исходной модели.
    auto idx = TopologyIndex::build(blockWithHole());
    ASSERT_TRUE(idx.ok());
    const auto vis = segments(s.view, Visibility::Visible);
    EXPECT_TRUE(hasSegment(vis, {25, 0}, {25, 20})) << describe(s.view);
    EXPECT_TRUE(hasSegment(vis, {35, 0}, {35, 20})) << describe(s.view);
    EXPECT_TRUE(hasSegment(vis, {0, 0}, {0, 20})) << describe(s.view);
    EXPECT_TRUE(hasSegment(vis, {60, 0}, {60, 20})) << describe(s.view);
    int wall = 0;
    for (const ViewEdge& e : s.view.edges) {
        EXPECT_EQ(e.visibility, Visibility::Visible);
        const auto* l = std::get_if<LineSeg>(&e.curve);
        if (!l || std::abs(l->a.x - l->b.x) > 1e-9) continue;
        if (std::abs(l->a.x - 25) > 1e-9 && std::abs(l->a.x - 35) > 1e-9) continue;
        ++wall;
        EXPECT_EQ(e.cls, EdgeClass::SectionCut);
        // Линия пересечения цилиндра с плоскостью — ссылка на цилиндр; шов цилиндра,
        // попавший в плоскость, — ссылка на само ребро шва.
        if (e.faceIndex) {
            EXPECT_EQ(idx.value().faces()[*e.faceIndex].surface, bendcore::kernel::SurfaceType::Cylinder);
        } else {
            ASSERT_TRUE(e.edgeIndex.has_value()) << describe(s.view);
            EXPECT_EQ(idx.value().edges()[*e.edgeIndex].curve, bendcore::kernel::CurveType::Line);
        }
    }
    EXPECT_EQ(wall, 2) << describe(s.view);
    // Верхняя линия: [0, 25] и [35, 60] — контур сечения, между ними — задняя половина
    // окружности отверстия на верхней грани (видна ребром): всего 60.
    double topLen = 0, topCut = 0;
    for (const ViewEdge& e : s.view.edges)
        if (auto* l = std::get_if<LineSeg>(&e.curve))
            if (std::abs(l->a.y - 20) < 1e-9 && std::abs(l->b.y - 20) < 1e-9) {
                topLen += curveLength(*l);
                if (e.cls == EdgeClass::SectionCut) topCut += curveLength(*l);
            }
    EXPECT_NEAR(topCut, 50.0, 1e-6) << describe(s.view);
    EXPECT_NEAR(topLen, 60.0, 1e-3) << describe(s.view);
}

TEST(DrawingViewsSections, HatchAreaEqualsSectionArea) {
    // Штриховка: Σ длин линий · шаг = площадь фигуры на листе (масштаб 2:1 → 4000 мм²).
    const SectionView s = frontSection(SectionKind::Cut);
    auto d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value());
    ASSERT_TRUE(d.ok());
    SheetView sv{s.view, 2.0, {100, 100}};
    auto spacing = renderSectionHatch(d.value(), sv, s, 1.0);
    ASSERT_TRUE(spacing.ok()) << spacing.error().message;
    double total = 0;
    int hatches = 0;
    for (const Entity& e : d.value().entities())
        if (auto* h = std::get_if<Hatch>(&e.geometry)) {
            ++hatches;
            EXPECT_EQ(e.lineType, LineType::SolidThin);
            EXPECT_EQ(h->angleDeg, 45.0);
            auto segs = hatchSegments(*h);
            ASSERT_TRUE(segs.ok());
            for (const LineSeg& l : segs.value()) total += curveLength(l);
        }
    EXPECT_EQ(hatches, 2);
    EXPECT_NEAR(total * 1.0, 4000.0, 4000.0 * 0.01);
    // Шаг по площади: √4000/10 = 6,32 → 6,5 мм; пределы ГОСТ 2.306 — 1…10 мм.
    auto d2 = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value());
    auto autoSpacing = renderSectionHatch(d2.value(), sv, s);
    ASSERT_TRUE(autoSpacing.ok());
    EXPECT_DOUBLE_EQ(autoSpacing.value(), 6.5);
    EXPECT_DOUBLE_EQ(gostHatchSpacing(1.0), 1.0);
    EXPECT_DOUBLE_EQ(gostHatchSpacing(1e6), 10.0);
    EXPECT_DOUBLE_EQ(gostHatchSpacing(900.0), 3.0);
}

TEST(DrawingViewsSections, SliceShowsOnlyTheSectionFigure) {
    // Сечение: только контур фигуры в плоскости — два прямоугольника 25×20, периметр 180.
    const SectionView s = frontSection(SectionKind::Slice);
    double len = 0;
    for (const ViewEdge& e : s.view.edges) {
        EXPECT_EQ(e.cls, EdgeClass::SectionCut);
        EXPECT_EQ(e.visibility, Visibility::Visible);
        len += curveLength(e.curve);
    }
    EXPECT_NEAR(len, 180.0, 1e-6) << describe(s.view);
    EXPECT_NEAR(s.sectionArea, 1000.0, 1e-6);
}

TEST(DrawingViewsSections, CutWithHiddenLines) {
    // За плоскостью разреза скрытых элементов нет (отверстие рассечено) — невидимых линий
    // нет и при включённом расчёте.
    const SectionView s = frontSection(SectionKind::Cut, true);
    for (const ViewEdge& e : s.view.edges) EXPECT_EQ(e.visibility, Visibility::Visible) << describe(s.view);
}

TEST(DrawingViewsSections, Errors) {
    const Shape b = blockWithHole();
    auto miss = sectionShape(b, defaultFrontFrame(), {{0, 100, 0}, {0, 1, 0}});
    ASSERT_FALSE(miss.ok());
    EXPECT_EQ(miss.error().code, ErrorCode::InvalidInput);
    auto wrongView = sectionShape(b, standardFrame(defaultFrontFrame(), StandardView::Top), kPlane);
    ASSERT_FALSE(wrongView.ok());
    EXPECT_EQ(wrongView.error().code, ErrorCode::InvalidInput);
    EXPECT_FALSE(sectionShape(Shape(), defaultFrontFrame(), kPlane).ok());
}

TEST(DrawingViewsSections, CuttingTraceOnTopView) {
    // На виде сверху след плоскости y = 20 — горизонталь y = 20 через весь габарит
    // (x от 0 до 60); направление взгляда — вверх по виду (к задней стороне детали).
    auto top = projectShape(blockWithHole(), standardFrame(defaultFrontFrame(), StandardView::Top));
    ASSERT_TRUE(top.ok());
    auto tr = cuttingTrace(top.value().frame, top.value().bounds, kPlane);
    ASSERT_TRUE(tr.ok()) << tr.error().message;
    const double ax = std::min(tr.value().a.x, tr.value().b.x), bx = std::max(tr.value().a.x, tr.value().b.x);
    EXPECT_NEAR(ax, 0, 1e-9);
    EXPECT_NEAR(bx, 60, 1e-9);
    EXPECT_NEAR(tr.value().a.y, 20, 1e-9);
    EXPECT_NEAR(tr.value().b.y, 20, 1e-9);
    EXPECT_NEAR(tr.value().sight2d.x, 0, 1e-12);
    EXPECT_NEAR(tr.value().sight2d.y, 1, 1e-12);
    // Плоскость, параллельная плоскости вида, следа не даёт.
    EXPECT_FALSE(cuttingTrace(top.value().frame, top.value().bounds, {{0, 0, 10}, {0, 0, 1}}).ok());
}

TEST(DrawingViewsSections, CuttingPlaneMarkGeometry) {
    // ГОСТ 2.305, разд. 6: разомкнутые штрихи 8…20 мм вне контура, стрелки
    // перпендикулярно штрихам в направлении взгляда, у стрелок — буквы.
    auto top = projectShape(blockWithHole(), standardFrame(defaultFrontFrame(), StandardView::Top));
    ASSERT_TRUE(top.ok());
    auto d = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value());
    SheetView sv{top.value(), 2.0, {100, 50}};
    auto g = renderCuttingPlaneMark(d.value(), sv, kPlane, "А", 12.0);
    ASSERT_TRUE(g.ok()) << g.error().message;
    const Box2 b = sv.sheetBounds();  // x 100…220, y 50…130; след на y = 90
    for (int i = 0; i < 2; ++i) {
        const LineSeg& st = g.value().strokes[i];
        EXPECT_NEAR(curveLength(st), 12.0, 1e-9);
        EXPECT_NEAR(st.a.y, 90, 1e-9);
        EXPECT_NEAR(st.b.y, 90, 1e-9);
        EXPECT_TRUE(std::max(st.a.x, st.b.x) <= b.minX || std::min(st.a.x, st.b.x) >= b.maxX);
        const LineSeg& sh = g.value().arrowShafts[i];
        EXPECT_NEAR(sh.a.x, sh.b.x, 1e-9);  // перпендикулярно следу
        EXPECT_LT(sh.a.y, sh.b.y);          // направлена вверх по листу — по направлению взгляда
        // Острие стрелки на штрихе в 2,5 мм от его внешнего конца.
        const Point2 tip = g.value().arrowHeads[i].points[0];
        EXPECT_NEAR(tip.y, 90, 1e-9);
        const Point2 outer = st.b;
        EXPECT_NEAR(std::abs(tip.x - outer.x), 2.5, 1e-9);
    }
    int open = 0, texts = 0;
    for (const Entity& e : d.value().entities()) {
        open += e.lineType == LineType::Open;
        texts += std::holds_alternative<Text>(e.geometry);
    }
    EXPECT_EQ(open, 2);
    EXPECT_EQ(texts, 2);
    EXPECT_FALSE(renderCuttingPlaneMark(d.value(), sv, kPlane, "А", 25.0).ok());
}

TEST(DrawingViewsSections, DetailViewClipsToCircle) {
    auto top = projectShape(blockWithHole(), standardFrame(defaultFrontFrame(), StandardView::Top));
    ASSERT_TRUE(top.ok());
    // Окружность r = 8 вокруг отверстия: внутри только окружность отверстия.
    auto d1 = detailView(top.value(), {30, 20}, 8);
    ASSERT_TRUE(d1.ok());
    ASSERT_EQ(d1.value().edges.size(), 1u) << describe(d1.value());
    EXPECT_TRUE(std::holds_alternative<Circle>(d1.value().edges[0].curve));
    EXPECT_NEAR(d1.value().bounds.minX, 22, 1e-12);  // габарит — окружность ограничения
    EXPECT_NEAR(d1.value().bounds.maxY, 28, 1e-12);
    // Угол (0, 0), r = 10: два отрезка длиной 10 по сторонам угла.
    auto d2 = detailView(top.value(), {0, 0}, 10);
    ASSERT_TRUE(d2.ok());
    const auto segs = segments(d2.value(), Visibility::Visible);
    ASSERT_EQ(segs.size(), 2u) << describe(d2.value());
    EXPECT_TRUE(hasSegment(segs, {0, 0}, {10, 0}, 1e-9));
    EXPECT_TRUE(hasSegment(segs, {0, 0}, {0, 10}, 1e-9));
    // Окружность r = 3 с центром (35, 20) на краю отверстия: дуга окружности r = 5,
    // концы на обеих окружностях, длина = 5·2·acos((25 + 25 − 9)/(2·5·5))… через
    // полуугол: cos φ = (5² + 5² − 3²)/(2·5·5).
    auto d3 = detailView(top.value(), {35, 20}, 3);
    ASSERT_TRUE(d3.ok());
    double arcLen = 0;
    for (const ViewEdge& e : d3.value().edges) {
        const auto* a = std::get_if<Arc>(&e.curve);
        ASSERT_NE(a, nullptr) << describe(d3.value());
        arcLen += curveLength(*a);
        for (double ang : {a->startDeg, a->endDeg}) {
            const double t = ang * std::numbers::pi / 180;
            const Point2 p{a->center.x + a->radius * std::cos(t), a->center.y + a->radius * std::sin(t)};
            EXPECT_NEAR(std::hypot(p.x - 35, p.y - 20), 3.0, 1e-9);
        }
    }
    const double phi = std::acos((25.0 + 25.0 - 9.0) / 50.0);
    EXPECT_NEAR(arcLen, 5.0 * 2 * phi, 1e-9);
    EXPECT_FALSE(detailView(top.value(), {0, 0}, 0).ok());
}
