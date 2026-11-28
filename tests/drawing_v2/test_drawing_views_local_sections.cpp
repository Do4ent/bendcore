// Местный и ступенчатый разрезы (ГОСТ 2.305-2008, разд. 6), волнистая граница местного разреза
// (ГОСТ 2.303, табл. 1, п. 3), условности разрезов сборок (ГОСТ 2.305, разд. 9; ГОСТ 2.109) и
// разная штриховка смежных деталей (ГОСТ 2.306, п. 4). Эталон — площади и положения по формулам
// известной геометрии.

#include "test_drawing_views_support.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <numbers>

using namespace dvtest;
using bendcore::kernel::Shape;

namespace {

// Брусок 100×60×40 со сквозным вертикальным отверстием Ø20 в (50, 30); вид спереди (наблюдатель
// со стороны −Y) — передняя грань y = 0, глубина 30 — плоскость через ось отверстия.
Shape block() { return blockWithHole({100, 60, 40, 20, 50, 30}); }

double regionsArea(const SectionView& s) {
    double a = 0;
    for (const auto& region : s.regions)
        for (const auto& loop : region) a += std::abs(polygonArea(loop));
    return a;
}

double pathLength(const std::vector<Point2>& p) {
    double s = 0;
    for (std::size_t i = 1; i < p.size(); ++i) s += std::hypot(p[i].x - p[i - 1].x, p[i].y - p[i - 1].y);
    return s;
}

// Расстояние от точки до контура прямоугольника [x0, x1] × [y0, y1].
double rectDistance(Point2 p, double x0, double y0, double x1, double y1) {
    auto seg = [](Point2 q, Point2 a, Point2 b) {
        const double dx = b.x - a.x, dy = b.y - a.y, l2 = dx * dx + dy * dy;
        const double t = std::clamp(((q.x - a.x) * dx + (q.y - a.y) * dy) / l2, 0.0, 1.0);
        return std::hypot(q.x - a.x - t * dx, q.y - a.y - t * dy);
    };
    return std::min({seg(p, {x0, y0}, {x1, y0}), seg(p, {x1, y0}, {x1, y1}), seg(p, {x1, y1}, {x0, y1}),
                     seg(p, {x0, y1}, {x0, y0})});
}

// Суммарная длина видимых/невидимых вертикальных отрезков x = x0 в полосе y ∈ [y0, y1].
double verticalLength(const ProjectedView& v, Visibility vis, double x0, double y0, double y1) {
    double s = 0;
    for (const ViewEdge& e : v.edges) {
        if (e.visibility != vis) continue;
        const auto* l = std::get_if<LineSeg>(&e.curve);
        if (!l || std::abs(l->a.x - x0) > 1e-6 || std::abs(l->b.x - x0) > 1e-6) continue;
        const double a = std::max(y0, std::min(l->a.y, l->b.y)), b = std::min(y1, std::max(l->a.y, l->b.y));
        if (b > a) s += b - a;
    }
    return s;
}

LocalSection rectCut(double x0, double y0, double x1, double y1, double depth) {
    return LocalSection{{{{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, false}, depth};
}

}  // namespace

TEST(DrawingViewsLocalSections, FrontExtent) {
    auto f = frontExtent(block(), defaultFrontFrame());
    ASSERT_TRUE(f.ok()) << f.error().message;
    EXPECT_NEAR(f.value(), 0.0, 1e-6);  // передняя грань y = 0: max(P·toward) = max(−y) = 0
    auto r = frontExtent(block(), ViewFrame{{0, 1, 0}, {1, 0, 0}});
    ASSERT_TRUE(r.ok());
    EXPECT_NEAR(r.value(), 60.0, 1e-6);
}

TEST(DrawingViewsLocalSections, RectangleInsideTheFaceAreaAndWavyBoundary) {
    HlrOptions o;
    o.hidden = true;
    auto r = localSectionShape(block(), defaultFrontFrame(), {rectCut(30, 10, 70, 30, 30)}, o);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const SectionView& s = r.value();
    // Фигура сечения в плоскости y = 30: прямоугольник 40×20 минус отверстие (x ∈ [40, 60]) —
    // два прямоугольника 10×20, площадь 400 мм².
    EXPECT_NEAR(s.sectionArea, 400.0, 1e-6);
    EXPECT_NEAR(regionsArea(s), 400.0, 1e-6);
    ASSERT_EQ(s.regions.size(), 2u);

    // Граница целиком над материалом — один замкнутый участок по контуру прямоугольника, длина 120.
    ASSERT_EQ(s.breakLines.size(), 1u);
    const auto& bl = s.breakLines[0];
    EXPECT_NEAR(bl.front().x, bl.back().x, 1e-9);
    EXPECT_NEAR(bl.front().y, bl.back().y, 1e-9);
    EXPECT_NEAR(pathLength(bl), 120.0, 1e-6);
    for (const Point2& p : bl) EXPECT_LT(rectDistance(p, 30, 10, 70, 30), 1e-9);

    // Стенки выреза не изображаются: ни одного отрезка на границе прямоугольника.
    for (const ViewEdge& e : s.view.edges) {
        const auto* l = std::get_if<LineSeg>(&e.curve);
        if (!l) continue;
        const Point2 m{0.5 * (l->a.x + l->b.x), 0.5 * (l->a.y + l->b.y)};
        EXPECT_GT(rectDistance(m, 30, 10, 70, 30), 1e-6) << "edge on the boundary: " << m.x << ", " << m.y;
    }
    // Внутри области — стенки отверстия за плоскостью (видимые, x = 40 и 60, z ∈ [10, 30]);
    // снаружи — исходный вид: те же линии штриховые (z ∈ [0, 10] и [30, 40]) и контур бруска.
    EXPECT_NEAR(verticalLength(s.view, Visibility::Visible, 40, 10, 30), 20.0, 1e-3) << describe(s.view);
    EXPECT_NEAR(verticalLength(s.view, Visibility::Visible, 60, 10, 30), 20.0, 1e-3);
    EXPECT_NEAR(verticalLength(s.view, Visibility::Hidden, 40, 0, 40), 20.0, 1e-3);
    EXPECT_NEAR(verticalLength(s.view, Visibility::Hidden, 60, 0, 40), 20.0, 1e-3);
    EXPECT_NEAR(verticalLength(s.view, Visibility::Visible, 0, 0, 40), 40.0, 1e-6);
    EXPECT_NEAR(verticalLength(s.view, Visibility::Visible, 100, 0, 40), 40.0, 1e-6);
    // Линии внутри области — контур сечения с привязкой к цилиндру исходной модели.
    int cutEdges = 0;
    for (const ViewEdge& e : s.view.edges)
        if (e.cls == EdgeClass::SectionCut) ++cutEdges;
    EXPECT_GT(cutEdges, 0);

    // Лист 2:1: штриховка Σ длин · шаг = площадь на листе (400·4 = 1600 мм², ±2 %), волнистая
    // линия — на контуре области в пределах амплитуды и действительно волнистая.
    auto dr = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value());
    ASSERT_TRUE(dr.ok());
    Drawing d = std::move(dr).value();
    const SheetView sv{s.view, 2.0, {100, 100}};
    ASSERT_TRUE(renderView(d, sv).ok());
    auto spacing = renderSectionHatch(d, sv, s);
    ASSERT_TRUE(spacing.ok()) << spacing.error().message;
    ASSERT_TRUE(renderBreakLines(d, sv, s).ok());
    double hatchLen = 0, maxDev = 0;
    int wavy = 0;
    for (const Entity& e : d.entities()) {
        if (const auto* h = std::get_if<Hatch>(&e.geometry)) {
            EXPECT_DOUBLE_EQ(h->angleDeg, 45.0);
            const auto segs = hatchSegments(*h);
            ASSERT_TRUE(segs.ok());
            for (const LineSeg& l : segs.value()) hatchLen += std::hypot(l.b.x - l.a.x, l.b.y - l.a.y);
        }
        if (e.lineType == LineType::SolidWavy) {
            ++wavy;
            EXPECT_EQ(e.layer, "THIN");
            for (const Point2& p : std::get<Polyline>(e.geometry).points) {
                const double dev = rectDistance(p, 160, 120, 240, 160);
                EXPECT_LE(dev, 1.0 + 1e-9);
                maxDev = std::max(maxDev, dev);
            }
        }
    }
    EXPECT_EQ(wavy, 1);
    EXPECT_GT(maxDev, 0.9);
    EXPECT_NEAR(hatchLen * spacing.value(), 1600.0, 0.02 * 1600.0);
}

TEST(DrawingViewsLocalSections, RegionPartlyOutsideTheModel) {
    // Область x ∈ [80, 120] выходит за брусок (x ≤ 100): сечение 20×20, волнистая линия — только
    // над материалом: верх и низ по 20, левая сторона 20, всего 60.
    auto r = localSectionShape(block(), defaultFrontFrame(), {rectCut(80, 10, 120, 30, 30)}, {});
    ASSERT_TRUE(r.ok()) << r.error().message;
    const SectionView& s = r.value();
    EXPECT_NEAR(s.sectionArea, 400.0, 1e-6);
    double total = 0;
    for (const auto& bl : s.breakLines) {
        total += pathLength(bl);
        for (const Point2& p : bl) EXPECT_LE(p.x, 100.0 + 0.6);
    }
    EXPECT_NEAR(total, 60.0, 1.0);
    EXPECT_GE(s.breakLines.size(), 1u);
}

TEST(DrawingViewsLocalSections, SplineBoundaryIsNearlyACircle) {
    // Сплайн через 8 точек окружности R10 с центром (20, 20); вне отверстия: сечение — круг,
    // площадь πR² (сплайн отклоняется от окружности < 0,5 %), граница — длина 2πR.
    LocalSection c;
    for (int i = 0; i < 8; ++i) {
        const double a = i * std::numbers::pi / 4;
        c.boundary.points.push_back({20 + 10 * std::cos(a), 20 + 10 * std::sin(a)});
    }
    c.boundary.spline = true;
    c.depth = 10;
    auto r = localSectionShape(block(), defaultFrontFrame(), {c}, {});
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_NEAR(r.value().sectionArea, std::numbers::pi * 100, 0.005 * std::numbers::pi * 100);
    ASSERT_EQ(r.value().breakLines.size(), 1u);
    EXPECT_NEAR(pathLength(r.value().breakLines[0]), 2 * std::numbers::pi * 10, 0.01 * 2 * std::numbers::pi * 10);
    auto loop = localBoundaryLoop(c.boundary, 0.01);
    ASSERT_TRUE(loop.ok());
    EXPECT_NEAR(std::abs(polygonArea(loop.value())), std::numbers::pi * 100, 0.005 * std::numbers::pi * 100);
}

TEST(DrawingViewsLocalSections, Errors) {
    const ViewFrame f = defaultFrontFrame();
    auto two = localSectionShape(block(), f, {LocalSection{{{{0, 0}, {10, 10}}, false}, 10}}, {});
    EXPECT_FALSE(two.ok());
    auto bowtie = localSectionShape(block(), f, {LocalSection{{{{10, 10}, {30, 30}, {30, 10}, {10, 30}}, false}, 10}}, {});
    ASSERT_FALSE(bowtie.ok());
    EXPECT_NE(bowtie.error().message.find("intersects itself"), std::string::npos);
    EXPECT_FALSE(localSectionShape(block(), f, {rectCut(30, 10, 70, 30, 0)}, {}).ok());
    auto deep = localSectionShape(block(), f, {rectCut(30, 10, 70, 30, 60)}, {});
    ASSERT_FALSE(deep.ok());
    EXPECT_NE(deep.error().message.find("beyond"), std::string::npos);
    auto miss = localSectionShape(block(), f, {rectCut(200, 10, 220, 30, 10)}, {});
    ASSERT_FALSE(miss.ok());
    EXPECT_NE(miss.error().message.find("does not cut"), std::string::npos);
    EXPECT_FALSE(localSectionShape(block(), f, {}, {}).ok());
}

TEST(DrawingViewsLocalSections, SteppedSectionThroughTwoHoles) {
    // Брусок 100×60×40, отверстия Ø10: A (25, 20), B (75, 40). Плоскости y = 20 (x < 50) и y = 40
    // (x > 50): площадь 2 · (50·40 − 10·40) = 3200; линия уступа x = 50 не изображается.
    TopoDS_Shape box = BRepPrimAPI_MakeBox(100, 60, 40).Shape();
    TopoDS_Shape a = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(25, 20, -1), gp_Dir(0, 0, 1)), 5, 42).Shape();
    TopoDS_Shape b = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(75, 40, -1), gp_Dir(0, 0, 1)), 5, 42).Shape();
    const Shape part(BRepAlgoAPI_Cut(BRepAlgoAPI_Cut(box, a).Shape(), b).Shape());
    auto r = steppedSectionShape(part, defaultFrontFrame(), {{75, 40, 0}, {25, 20, 0}}, {1, 0, 0});
    ASSERT_TRUE(r.ok()) << r.error().message;
    const SectionView& s = r.value();
    EXPECT_NEAR(s.sectionArea, 3200.0, 1e-6);
    EXPECT_NEAR(regionsArea(s), 3200.0, 1e-6);
    for (double x : {20.0, 30.0, 70.0, 80.0}) EXPECT_NEAR(verticalLength(s.view, Visibility::Visible, x, 0, 40), 40.0, 1e-3) << x;
    EXPECT_NEAR(verticalLength(s.view, Visibility::Visible, 50, 0, 40), 0.0, 1e-9) << describe(s.view);
    for (const auto& p : hatchPatterns(s)) EXPECT_DOUBLE_EQ(p.angleDeg, 45.0);  // одна деталь — одна штриховка
    // Сечение (только фигура): контуры без линии уступа.
    auto sl = steppedSectionShape(part, defaultFrontFrame(), {{25, 20, 0}, {75, 40, 0}}, {1, 0, 0}, SectionKind::Slice);
    ASSERT_TRUE(sl.ok());
    EXPECT_NEAR(verticalLength(sl.value().view, Visibility::Visible, 50, 0, 40), 0.0, 1e-9);
    EXPECT_NEAR(verticalLength(sl.value().view, Visibility::Visible, 0, 0, 40), 40.0, 1e-6);
    // Ошибки: одна точка, точки на одной линии уступа, ось вдоль взгляда.
    EXPECT_FALSE(steppedSectionShape(part, defaultFrontFrame(), {{25, 20, 0}}, {1, 0, 0}).ok());
    EXPECT_FALSE(steppedSectionShape(part, defaultFrontFrame(), {{25, 20, 0}, {25, 40, 0}}, {1, 0, 0}).ok());
    EXPECT_FALSE(steppedSectionShape(part, defaultFrontFrame(), {{25, 20, 0}, {75, 40, 0}}, {0, 1, 0}).ok());
}

namespace {

// Узел: вал R10 (z ∈ [−20, 60]), втулка R10…R20 (z ∈ [0, 40]), шайба R10…R30 (z ∈ [40, 45]) —
// компаунд в порядке вал, втулка, шайба.
Shape shaftUnit(bool withWasher) {
    auto tube = [](double r0, double r1, double z0, double h) {
        TopoDS_Shape o = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, z0), gp_Dir(0, 0, 1)), r1, h).Shape();
        TopoDS_Shape i = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, z0 - 1), gp_Dir(0, 0, 1)), r0, h + 2).Shape();
        return BRepAlgoAPI_Cut(o, i).Shape();
    };
    BRep_Builder bld;
    TopoDS_Compound c;
    bld.MakeCompound(c);
    bld.Add(c, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, -20), gp_Dir(0, 0, 1)), 10, 80).Shape());
    bld.Add(c, tube(10, 20, 0, 40));
    if (withWasher) bld.Add(c, tube(10, 30, 40, 5));
    return Shape(c);
}

}  // namespace

TEST(DrawingViewsLocalSections, AssemblyShaftIsNotCutInALongitudinalSection) {
    const CuttingPlane plane{{0, 0, 0}, {0, 1, 0}};
    SectionParts parts;
    parts.perChild = true;
    parts.keepWhole = {true, false};
    auto r = sectionShape(shaftUnit(false), defaultFrontFrame(), plane, SectionKind::Cut, {}, parts);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const SectionView& s = r.value();
    // Рассечена только втулка: два прямоугольника 10×40, площадь 800; номера областей — 1.
    EXPECT_NEAR(s.sectionArea, 800.0, 1e-6);
    ASSERT_EQ(s.regions.size(), 2u);
    for (int o : s.regionOwner) EXPECT_EQ(o, 1);
    // Вал — нерассечённым: очерк x = ±10 видим по всей длине 80 (на участке втулки — граница
    // вала и фигуры сечения).
    EXPECT_NEAR(verticalLength(s.view, Visibility::Visible, 10, -20, 60), 80.0, 1e-3) << describe(s.view);
    EXPECT_NEAR(verticalLength(s.view, Visibility::Visible, -10, -20, 60), 80.0, 1e-3);

    // Все рассечены: вал — третья фигура 20×80, у смежных деталей штриховка встречная.
    parts.keepWhole = {false, false};
    auto all = sectionShape(shaftUnit(false), defaultFrontFrame(), plane, SectionKind::Cut, {}, parts);
    ASSERT_TRUE(all.ok()) << all.error().message;
    EXPECT_NEAR(all.value().sectionArea, 800.0 + 1600.0, 1e-6);
    const auto pat = hatchPatterns(all.value());
    for (std::size_t i = 0; i < pat.size(); ++i) {
        const int o = all.value().regionOwner[i];
        EXPECT_DOUBLE_EQ(pat[i].angleDeg, o == 0 ? 45.0 : 135.0) << "owner " << o;
        EXPECT_DOUBLE_EQ(pat[i].spacingFactor, 1.0);
    }
    // Без деления на детали — одна штриховка (как разрез одной детали).
    auto single = sectionShape(shaftUnit(false), defaultFrontFrame(), plane);
    ASSERT_TRUE(single.ok());
    for (const auto& p : hatchPatterns(single.value())) EXPECT_DOUBLE_EQ(p.angleDeg, 45.0);
    // Неверное число флагов.
    parts.keepWhole = {true};
    EXPECT_FALSE(sectionShape(shaftUnit(false), defaultFrontFrame(), plane, SectionKind::Cut, {}, parts).ok());
}

TEST(DrawingViewsLocalSections, ThreeAdjacentPartsGetThreeHatches) {
    // Вал, втулка и шайба попарно касаются: 45°, 135°, затем 45° с шагом ×0,6 (ГОСТ 2.306, п. 4).
    SectionParts parts;
    parts.perChild = true;
    auto r = sectionShape(shaftUnit(true), defaultFrontFrame(), CuttingPlane{{0, 0, 0}, {0, 1, 0}}, SectionKind::Cut, {}, parts);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const auto pat = hatchPatterns(r.value());
    std::map<int, HatchPattern> byOwner;
    for (std::size_t i = 0; i < pat.size(); ++i) byOwner[r.value().regionOwner[i]] = pat[i];
    ASSERT_EQ(byOwner.size(), 3u);
    EXPECT_DOUBLE_EQ(byOwner[0].angleDeg, 45.0);
    EXPECT_DOUBLE_EQ(byOwner[1].angleDeg, 135.0);
    EXPECT_DOUBLE_EQ(byOwner[2].angleDeg, 45.0);
    EXPECT_DOUBLE_EQ(byOwner[2].spacingFactor, 0.6);
    // На листе: у шайбы шаг 0,6 базового (в пределах 1…10 мм).
    auto dr = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value());
    ASSERT_TRUE(dr.ok());
    Drawing d = std::move(dr).value();
    const SheetView sv{r.value().view, 2.0, {200, 150}};
    auto base = renderSectionHatch(d, sv, r.value(), 5.0);
    ASSERT_TRUE(base.ok());
    std::set<std::pair<double, double>> seen;
    for (const Entity& e : d.entities())
        if (const auto* h = std::get_if<Hatch>(&e.geometry)) seen.insert({h->angleDeg, h->spacing});
    EXPECT_EQ(seen, (std::set<std::pair<double, double>>{{45.0, 5.0}, {135.0, 5.0}, {45.0, 3.0}}));
}

TEST(DrawingViewsLocalSections, WavyPath) {
    // Открытая ломаная длиной 40, волна 8: 10 полуволн, концы на ломаной, отклонение ≤ A.
    auto w = makeWavyPath({{0, 0}, {40, 0}}, 1.0, 8.0);
    ASSERT_TRUE(w.ok());
    const auto& p = w.value().points;
    EXPECT_NEAR(p.front().y, 0.0, 1e-9);
    EXPECT_NEAR(p.back().x, 40.0, 1e-9);
    EXPECT_NEAR(p.back().y, 0.0, 1e-9);
    double mx = 0;
    int signChanges = 0, lastSign = 0;
    for (std::size_t i = 1; i < p.size(); ++i) {
        mx = std::max(mx, std::abs(p[i].y));
        const int sg = p[i].y > 1e-6 ? 1 : p[i].y < -1e-6 ? -1 : 0;
        if (sg != 0 && lastSign != 0 && sg != lastSign) ++signChanges;
        if (sg != 0) lastSign = sg;
    }
    EXPECT_NEAR(mx, 1.0, 1e-3);
    EXPECT_GE(signChanges, 8);
    EXPECT_FALSE(makeWavyPath({{0, 0}}, 1.0, 8.0).ok());
    EXPECT_FALSE(makeWavyPath({{0, 0}, {1, 0}}, 0.0, 8.0).ok());
}
