// Масштабы ГОСТ 2.302, проекционная связь видов, типы линий ГОСТ 2.303 на листе, осевые
// линии и центровые метки (выход за контур 2…5 мм, пересечение штрихами).

#include "test_drawing_views_support.hpp"

#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Tool.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <gtest/gtest.h>

#include <algorithm>

#include <cmath>

using namespace dvtest;
using bendcore::kernel::Shape;

namespace {

ProjectedView stdView(const Shape& s, StandardView v) {
    auto r = projectShape(s, standardFrame(defaultFrontFrame(), v));
    EXPECT_TRUE(r.ok());
    return r.ok() ? r.value() : ProjectedView{};
}

Drawing a3() {
    return Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
}

std::vector<LineSeg> layerLines(const Drawing& d, const std::string& layer, LineType t) {
    std::vector<LineSeg> out;
    for (const Entity& e : d.entities())
        if (e.layer == layer && e.lineType == t)
            if (auto* l = std::get_if<LineSeg>(&e.geometry)) out.push_back(*l);
    return out;
}

}  // namespace

TEST(DrawingViewsSheet, GostScaleSeries) {
    // ГОСТ 2.302-68, п. 2: 17 масштабов уменьшения, натуральная величина, 9 увеличения.
    const auto& s = gostScales();
    ASSERT_EQ(s.size(), 27u);
    for (std::size_t i = 1; i < s.size(); ++i) EXPECT_GT(s[i - 1].value(), s[i].value());
    std::vector<std::string> labels;
    for (const ScaleRatio& r : s) labels.push_back(r.label());
    for (const char* l : {"1:2", "1:2,5", "1:4", "1:5", "1:10", "1:15", "1:20", "1:25", "1:40", "1:50",
                          "1:75", "1:100", "1:200", "1:400", "1:500", "1:800", "1:1000", "1:1", "2:1",
                          "2,5:1", "4:1", "5:1", "10:1", "20:1", "40:1", "50:1", "100:1"})
        EXPECT_NE(std::find(labels.begin(), labels.end(), l), labels.end()) << l;
    EXPECT_EQ(chooseGostScale(200, 100, 150, 150).value().label(), "1:2");
    EXPECT_EQ(chooseGostScale(10, 5, 300, 300).value().label(), "20:1");
    EXPECT_EQ(chooseGostScale(100, 10, 250, 250).value().label(), "2,5:1");
    EXPECT_FALSE(chooseGostScale(1e7, 1, 100, 100).ok());
    EXPECT_FALSE(chooseGostScale(10, 10, 0, 100).ok());
}

TEST(DrawingViewsSheet, LayoutKeepsProjectionLink) {
    // Главный вид, вид сверху под ним, вид слева справа (метод первого угла). Область
    // 300×200, зазор 20: при 2,5:1 ширина (60 + 40)·2,5 + 20 = 270, высота (20 + 40)·2,5 + 20
    // = 170 помещаются; при 4:1 — нет.
    const Shape s = blockWithHole();
    const ProjectedView front = stdView(s, StandardView::Front);
    const ProjectedView top = stdView(s, StandardView::Top);
    const ProjectedView left = stdView(s, StandardView::Left);
    Box2 area;
    area.add({50, 50});
    area.add({350, 250});
    auto lay = layoutViews({{&front, -1, SheetSide::Below},
                            {&top, 0, *standardPlacement(StandardView::Top, ProjectionMethod::FirstAngle)},
                            {&left, 0, *standardPlacement(StandardView::Left, ProjectionMethod::FirstAngle)}},
                           area, 20);
    ASSERT_TRUE(lay.ok()) << lay.error().message;
    EXPECT_EQ(lay.value().scale.label(), "2,5:1");
    const double k = 2.5;
    const auto& an = lay.value().anchors;
    // Выравнивание: вид сверху — по x главного вида, вид слева — по y.
    EXPECT_NEAR(an[1].x, an[0].x, 1e-12);
    EXPECT_NEAR(an[2].y, an[0].y, 1e-12);
    const SheetView F{front, k, an[0]}, T{top, k, an[1]}, L{left, k, an[2]};
    EXPECT_NEAR(F.sheetBounds().minY - T.sheetBounds().maxY, 20, 1e-9);
    EXPECT_NEAR(L.sheetBounds().minX - F.sheetBounds().maxX, 20, 1e-9);
    // Одна и та же точка модели (угол x = 60) — на одной вертикали на виде спереди и сверху,
    // верх z = 20 — на одной горизонтали на виде спереди и слева.
    EXPECT_NEAR(F.toSheet({60, 0}).x, T.toSheet({60, 0}).x, 1e-12);
    EXPECT_NEAR(F.toSheet({0, 20}).y, L.toSheet({0, 20}).y, 1e-12);
    // Компоновка центрирована в области.
    EXPECT_NEAR(lay.value().bounds.width(), 270, 1e-9);
    EXPECT_NEAR(lay.value().bounds.height(), 170, 1e-9);
    EXPECT_NEAR(lay.value().bounds.minX, 50 + 15, 1e-9);
    EXPECT_NEAR(lay.value().bounds.minY, 50 + 15, 1e-9);
    // Ошибки: родитель после ребёнка, пустая область.
    EXPECT_FALSE(layoutViews({{&front, -1, SheetSide::Below}, {&top, 1, SheetSide::Below}}, area, 20).ok());
    EXPECT_FALSE(layoutViews({{&front, -1, SheetSide::Below}}, Box2{}, 20).ok());
}

TEST(DrawingViewsSheet, RenderUsesGost2303LineTypes) {
    // Вид спереди 2:1 с привязкой (100, 100): видимые — сплошная толстая на VISIBLE,
    // очерк отверстия — штриховая на HIDDEN в x = 100 + 2·25 и 100 + 2·35.
    const Shape s = blockWithHole();
    Drawing d = a3();
    const SheetView v{stdView(s, StandardView::Front), 2.0, {100, 100}};
    ASSERT_TRUE(renderView(d, v).ok());
    const auto vis = layerLines(d, "VISIBLE", LineType::SolidThick);
    const auto hid = layerLines(d, "HIDDEN", LineType::Dashed);
    ASSERT_EQ(vis.size(), 4u);
    ASSERT_EQ(hid.size(), 2u);
    EXPECT_TRUE(hasSegment(vis, {100, 100}, {220, 100}));
    EXPECT_TRUE(hasSegment(vis, {100, 140}, {220, 140}));
    EXPECT_TRUE(hasSegment(hid, {150, 100}, {150, 140}));
    EXPECT_TRUE(hasSegment(hid, {170, 100}, {170, 140}));
    EXPECT_EQ(d.entities().size(), 6u);
    // Без невидимых линий.
    Drawing d2 = a3();
    RenderOptions o;
    o.hidden = false;
    ASSERT_TRUE(renderView(d2, v, o).ok());
    EXPECT_TRUE(layerLines(d2, "HIDDEN", LineType::Dashed).empty());
    // Вид сверху: окружность отверстия радиусом 2·5.
    Drawing d3 = a3();
    ASSERT_TRUE(renderView(d3, SheetView{stdView(s, StandardView::Top), 2.0, {100, 100}}).ok());
    int circles = 0;
    for (const Entity& e : d3.entities())
        if (auto* c = std::get_if<Circle>(&e.geometry)) {
            ++circles;
            EXPECT_NEAR(c->center.x, 160, 1e-9);
            EXPECT_NEAR(c->center.y, 140, 1e-9);
            EXPECT_NEAR(c->radius, 10, 1e-9);
            EXPECT_EQ(e.lineType, LineType::SolidThick);
        }
    EXPECT_EQ(circles, 1);
}

TEST(DrawingViewsSheet, CenterGeometryOfHole) {
    const Shape s = blockWithHole();
    // Вид сверху: центровая метка в (30, 20), r = 5.
    auto top = findCenterGeometry(s, standardFrame(defaultFrontFrame(), StandardView::Top));
    ASSERT_TRUE(top.ok());
    ASSERT_EQ(top.value().marks.size(), 1u);
    EXPECT_TRUE(top.value().axes.empty());
    EXPECT_NEAR(top.value().marks[0].center.x, 30, 1e-9);
    EXPECT_NEAR(top.value().marks[0].center.y, 20, 1e-9);
    EXPECT_NEAR(top.value().marks[0].radius, 5, 1e-9);
    // Вид спереди: осевая линия x = 30 от z = 0 до z = 20 (по торцам отверстия).
    auto front = findCenterGeometry(s, defaultFrontFrame());
    ASSERT_TRUE(front.ok());
    ASSERT_EQ(front.value().axes.size(), 1u);
    EXPECT_TRUE(front.value().marks.empty());
    const AxisLine& a = front.value().axes[0];
    EXPECT_NEAR(a.a.x, 30, 1e-9);
    EXPECT_NEAR(a.b.x, 30, 1e-9);
    EXPECT_NEAR(std::min(a.a.y, a.b.y), 0, 1e-9);
    EXPECT_NEAR(std::max(a.a.y, a.b.y), 20, 1e-9);
}

TEST(DrawingViewsSheet, CenterMarkSmallCircleIsSolidThin) {
    // ГОСТ 2.303: при диаметре на чертеже < 12 мм центровые — сплошные тонкие. Ø10 при 1:1:
    // две линии длиной 2·(5 + 3), вылет 3 мм за окружность.
    const Shape s = blockWithHole();
    const ViewFrame f = standardFrame(defaultFrontFrame(), StandardView::Top);
    auto g = findCenterGeometry(s, f);
    ASSERT_TRUE(g.ok());
    Drawing d = a3();
    const SheetView v{stdView(s, StandardView::Top), 1.0, {100, 100}};
    ASSERT_TRUE(renderCenterLines(d, v, g.value()).ok());
    const auto lines = layerLines(d, "CENTER", LineType::SolidThin);
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_TRUE(hasSegment(lines, {122, 120}, {138, 120}));
    EXPECT_TRUE(hasSegment(lines, {130, 112}, {130, 128}));
}

TEST(DrawingViewsSheet, CenterMarkDashDotCrossesWithDashes) {
    // Ø10 при 2:1 (Ø20 на листе): штрихпунктирная тонкая, четыре половины от центра
    // (пересечение штрихами), вылет 3 мм: (10 + 3) mod 18 = 13 ≤ 15 — линия кончается штрихом.
    const Shape s = blockWithHole();
    auto g = findCenterGeometry(s, standardFrame(defaultFrontFrame(), StandardView::Top));
    ASSERT_TRUE(g.ok());
    Drawing d = a3();
    const SheetView v{stdView(s, StandardView::Top), 2.0, {100, 100}};
    ASSERT_TRUE(renderCenterLines(d, v, g.value()).ok());
    const auto lines = layerLines(d, "CENTER", LineType::DashDotThin);
    ASSERT_EQ(lines.size(), 4u);
    const Point2 c{160, 140};
    for (const LineSeg& l : lines) {
        EXPECT_NEAR(l.a.x, c.x, 1e-12);
        EXPECT_NEAR(l.a.y, c.y, 1e-12);
        EXPECT_NEAR(curveLength(l), 13.0, 1e-9);  // радиус 10 + вылет 3
    }
    // r = 6,5 при 2:1 (13 мм на листе): при вылете 3 конец попал бы в промежуток
    // (16 mod 18 = 16 > 15), выбирается вылет 2 мм (15 — конец штриха).
    const BlockSpec spec{60, 40, 20, 13, 30, 20};
    const Shape s2 = blockWithHole(spec);
    auto g2 = findCenterGeometry(s2, standardFrame(defaultFrontFrame(), StandardView::Top));
    ASSERT_TRUE(g2.ok());
    Drawing d2 = a3();
    ASSERT_TRUE(renderCenterLines(d2, SheetView{stdView(s2, StandardView::Top), 2.0, {100, 100}}, g2.value()).ok());
    for (const LineSeg& l : layerLines(d2, "CENTER", LineType::DashDotThin)) {
        const double over = curveLength(l) - 13.0;
        EXPECT_GE(over, 2.0 - 1e-9);
        EXPECT_LE(over, 5.0 + 1e-9);
        EXPECT_NEAR(over, 2.0, 1e-9);
    }
    // Осевая линия на виде спереди 2:1: от z = 0 до 20 → 40 мм, половины 20 + 3.
    auto ga = findCenterGeometry(s, defaultFrontFrame());
    Drawing d3 = a3();
    ASSERT_TRUE(renderCenterLines(d3, SheetView{stdView(s, StandardView::Front), 2.0, {100, 100}}, ga.value()).ok());
    const auto axis = layerLines(d3, "CENTER", LineType::DashDotThin);
    ASSERT_EQ(axis.size(), 2u);
    EXPECT_TRUE(hasSegment(axis, {160, 120}, {160, 143}));
    EXPECT_TRUE(hasSegment(axis, {160, 120}, {160, 97}));
}

TEST(DrawingViewsSheet, FilletDoesNotGetCenterMark) {
    // Скругление R5 вертикального ребра (охват 90°) на виде сверху метки не получает;
    // при пороге охвата < 90° — метка в центре скругления (55, 35).
    TopoDS_Shape box = BRepPrimAPI_MakeBox(60, 40, 20).Shape();
    BRepFilletAPI_MakeFillet fillet(box);
    for (TopExp_Explorer ex(box, TopAbs_EDGE); ex.More(); ex.Next()) {
        TopoDS_Vertex a, b;
        TopExp::Vertices(TopoDS::Edge(ex.Current()), a, b);
        const gp_Pnt pa = BRep_Tool::Pnt(a), pb = BRep_Tool::Pnt(b);
        if (std::abs(pa.X() - 60) < 1e-9 && std::abs(pb.X() - 60) < 1e-9 && std::abs(pa.Y() - 40) < 1e-9 &&
            std::abs(pb.Y() - 40) < 1e-9)
            fillet.Add(5.0, TopoDS::Edge(ex.Current()));
    }
    const Shape s(fillet.Shape());
    const ViewFrame top = standardFrame(defaultFrontFrame(), StandardView::Top);
    auto g = findCenterGeometry(s, top);
    ASSERT_TRUE(g.ok());
    EXPECT_TRUE(g.value().marks.empty());
    auto g2 = findCenterGeometry(s, top, 1.5);
    ASSERT_TRUE(g2.ok());
    ASSERT_EQ(g2.value().marks.size(), 1u);
    EXPECT_NEAR(g2.value().marks[0].center.x, 55, 1e-9);
    EXPECT_NEAR(g2.value().marks[0].center.y, 35, 1e-9);
}

TEST(DrawingViewsSheet, DesignationLetters) {
    EXPECT_EQ(designationLetter(0), "А");
    EXPECT_EQ(designationLetter(6), "Ж");
    EXPECT_EQ(designationLetter(7), "И");  // З пропущена (ГОСТ 2.316)
    EXPECT_EQ(designationLetter(12), "П");  // О пропущена
    EXPECT_EQ(designationLetter(25), "А1");
}
