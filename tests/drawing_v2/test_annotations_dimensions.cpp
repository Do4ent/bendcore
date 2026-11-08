// Размеры на видах бруска с отверстием: значения против геометрии построения, геометрия
// размера на листе (масштаб, стрелки, выносные линии), пересчёт после изменения модели,
// «висячие» размеры с причиной, цепочки и размеры от базы, угловой размер, допуски.

#include "test_annotations_support.hpp"

#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <stdexcept>

using namespace annot_test;

namespace {

SheetView sheetView(const ModelBinding& m, const ViewFrame& f, double scale = 2.0, Point2 anchor = {100, 100}) {
    auto v = projectShape(m.shape(), f);
    if (!v) throw std::runtime_error(v.error().message);
    return SheetView{v.value(), scale, anchor};
}

ViewFrame topFrame() { return standardFrame(defaultFrontFrame(), StandardView::Top); }

bool hasLabel(const DimensionResult& r, const std::string& s) {
    for (const auto& l : r.geometry.labels)
        if (l.text == s) return true;
    return false;
}

struct BlockDims {
    DimensionDef length, width, diameter, holeX, radius;
};

// Размеры на виде сверху: длина по верхнему переднему ребру, ширина по левому, ⌀ отверстия,
// положение отверстия от левой передней вершины.
BlockDims makeDims(const ModelBinding& m, double L, double W, double H, double hx, double hy, double r) {
    BlockDims d;
    const auto eL = lineEdge(m, {0, 0, H}, {L, 0, H});
    const auto eW = lineEdge(m, {0, 0, H}, {0, W, H});
    const auto c = circleEdge(m, {hx, hy, H}, r);
    const auto v0 = vertexAt(m, {0, 0, H});
    if (!eL || !eW || !c || !v0) throw std::runtime_error("model element not found");
    d.length.type = DimensionType::Horizontal;
    d.length.anchors = {anchor(m, RefKind::Edge, *eL, AnchorKind::Start), anchor(m, RefKind::Edge, *eL, AnchorKind::End)};
    d.length.offset = -10;
    d.width.type = DimensionType::Vertical;
    d.width.anchors = {anchor(m, RefKind::Edge, *eW, AnchorKind::Start), anchor(m, RefKind::Edge, *eW, AnchorKind::End)};
    d.width.offset = 10;
    d.diameter.type = DimensionType::Diameter;
    d.diameter.anchors = {anchor(m, RefKind::Edge, *c, AnchorKind::Center)};
    d.holeX.type = DimensionType::Horizontal;
    d.holeX.anchors = {anchor(m, RefKind::Vertex, *v0, AnchorKind::Point), anchor(m, RefKind::Edge, *c, AnchorKind::Center)};
    d.holeX.offset = 10;
    d.radius.type = DimensionType::Radius;
    d.radius.anchors = {anchor(m, RefKind::Edge, *c, AnchorKind::Center)};
    d.radius.leaderAngleDeg = 135;
    return d;
}

}  // namespace

TEST(AnnotationsDimensions, ValuesOnTopViewMatchGeometry) {
    BlockPart part;
    const ModelBinding m = part.binding();
    ASSERT_TRUE(m.hasHistoryNames());
    const SheetView sv = sheetView(m, topFrame());
    const BlockDims d = makeDims(m, 60, 40, 20, 30, 20, 5);

    auto L = evaluateDimension(d.length, m, sv);
    ASSERT_TRUE(L.ok()) << L.error().message;
    ASSERT_EQ(L.value().state, DimensionState::Ok) << L.value().reason;
    EXPECT_NEAR(L.value().value, 60.0, 1e-9);
    EXPECT_EQ(L.value().text, "60");
    // Размерная линия на 10 мм ниже нижнего края вида (y = 100) и длиной 60·2 = 120 мм листа.
    ASSERT_EQ(L.value().geometry.dimensionLines.size(), 1u);
    const LineSeg dl = L.value().geometry.dimensionLines[0];
    EXPECT_NEAR(dl.a.y, 90.0, 1e-9);
    EXPECT_NEAR(std::abs(dl.b.x - dl.a.x), 120.0, 1e-9);
    EXPECT_NEAR(std::min(dl.a.x, dl.b.x), 100.0, 1e-9);
    // Выносные линии от точек контура до 2 мм за размерную линию.
    ASSERT_EQ(L.value().geometry.extensionLines.size(), 2u);
    for (const auto& e : L.value().geometry.extensionLines) {
        EXPECT_NEAR(e.a.y, 100.0, 1e-9);
        EXPECT_NEAR(e.b.y, 88.0, 1e-9);
    }
    // Две стрелки остриями в концах размерной линии, внутрь между выносными линиями.
    ASSERT_EQ(L.value().geometry.arrows.size(), 2u);
    for (const auto& a : L.value().geometry.arrows) {
        const Point2 tip = a.points[0];
        EXPECT_TRUE(std::abs(tip.x - 100) < 1e-9 || std::abs(tip.x - 220) < 1e-9);
        const double baseX = (a.points[1].x + a.points[2].x) / 2;
        EXPECT_NEAR(std::abs(baseX - tip.x), 4.0, 1e-9);
        EXPECT_TRUE(baseX > 100 && baseX < 220);  // основание стрелки — внутри размерной линии
    }
    // Размерное число над линией, по центру.
    ASSERT_EQ(L.value().geometry.labels.size(), 1u);
    EXPECT_GT(L.value().geometry.labels[0].position.y, 90.0);

    auto W = evaluateDimension(d.width, m, sv);
    ASSERT_EQ(W.value().state, DimensionState::Ok) << W.value().reason;
    EXPECT_NEAR(W.value().value, 40.0, 1e-9);
    EXPECT_NEAR(W.value().geometry.labels[0].rotationDeg, 90.0, 1e-9);  // читается снизу вверх
    EXPECT_NEAR(W.value().geometry.dimensionLines[0].a.x, 90.0, 1e-9);  // слева от вида на 10 мм

    auto D = evaluateDimension(d.diameter, m, sv);
    ASSERT_EQ(D.value().state, DimensionState::Ok) << D.value().reason;
    EXPECT_NEAR(D.value().value, 10.0, 1e-9);
    EXPECT_EQ(D.value().text, "⌀10");
    // Стрелки — на окружности R = 5·2 = 10 мм листа с центром (160, 140).
    for (const auto& a : D.value().geometry.arrows) {
        EXPECT_NEAR(std::hypot(a.points[0].x - 160, a.points[0].y - 140), 10.0, 1e-9);
    }

    auto X = evaluateDimension(d.holeX, m, sv);
    ASSERT_EQ(X.value().state, DimensionState::Ok) << X.value().reason;
    EXPECT_NEAR(X.value().value, 30.0, 1e-9);

    auto R = evaluateDimension(d.radius, m, sv);
    ASSERT_EQ(R.value().state, DimensionState::Ok);
    EXPECT_EQ(R.value().text, "R5");
    ASSERT_EQ(R.value().geometry.arrows.size(), 1u);
}

TEST(AnnotationsDimensions, DiameterOfCylinderSeenFromSide) {
    BlockPart part;
    const ModelBinding m = part.binding();
    const SheetView sv = sheetView(m, defaultFrontFrame(), 1.0, {50, 50});
    const auto cyl = cylinderFace(m, 5);
    ASSERT_TRUE(cyl);
    DimensionDef d;
    d.type = DimensionType::Diameter;
    d.anchors = {anchor(m, RefKind::Face, *cyl, AnchorKind::Center)};
    d.offset = 8;
    auto r = evaluateDimension(d, m, sv);
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ(r.value().state, DimensionState::Ok) << r.value().reason;
    EXPECT_NEAR(r.value().value, 10.0, 1e-6);  // между штриховыми очерками x = 25 и 35
    // Выносные линии продолжают очерк вверх за верхнюю грань (z = 20) на 8 мм.
    ASSERT_EQ(r.value().geometry.dimensionLines.size(), 1u);
    EXPECT_NEAR(r.value().geometry.dimensionLines[0].a.y, 50 + 20 + 8, 1e-6);
    // Радиус цилиндра сбоку не проставляется — висячий с причиной.
    d.type = DimensionType::Radius;
    auto rr = evaluateDimension(d, m, sv);
    ASSERT_TRUE(rr.ok());
    EXPECT_EQ(rr.value().state, DimensionState::Dangling);
    EXPECT_NE(rr.value().reason.find("diameter"), std::string::npos);
}

TEST(AnnotationsDimensions, RecomputedAfterModelChange) {
    BlockPart part;
    ModelBinding m = part.binding();
    DimensionSet set;
    const BlockDims d = makeDims(m, 60, 40, 20, 30, 20, 5);
    set.add(d.length);
    set.add(d.width);
    set.add(d.diameter);
    set.add(d.holeX);
    ASSERT_TRUE(set.evaluate(m, sheetView(m, topFrame())).ok());
    for (const auto& r : set.results()) ASSERT_EQ(r.state, DimensionState::Ok) << r.reason;

    // Длина 60 → 80, отверстие x 30 → 50, радиус 5 → 6.
    part.set("L", 80);
    part.set("hx", 50);
    part.set("r", 6);
    m = part.binding();
    auto rep = set.update(m, sheetView(m, topFrame()));
    ASSERT_TRUE(rep.ok()) << rep.error().message;
    const double expected[] = {80, 40, 12, 50};
    const double old[] = {60, 40, 10, 30};
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(rep.value()[i].state, DimensionState::Ok) << i << ": " << rep.value()[i].reason;
        EXPECT_NEAR(rep.value()[i].newValue, expected[i], 1e-9) << i;
        ASSERT_TRUE(rep.value()[i].oldValue);
        EXPECT_NEAR(*rep.value()[i].oldValue, old[i], 1e-9);
    }
    EXPECT_EQ(set.results()[0].text, "80");
    EXPECT_EQ(set.results()[2].text, "⌀12");

    // Отверстие подавлено — размеры, ссылающиеся на него, висячие с причиной; остальные живы.
    ASSERT_TRUE(part.doc.setSuppressed("Hole", true).ok());
    part.doc.rebuild();
    m = part.binding();
    rep = set.update(m, sheetView(m, topFrame()));
    ASSERT_TRUE(rep.ok());
    EXPECT_EQ(rep.value()[0].state, DimensionState::Ok);
    EXPECT_NEAR(rep.value()[0].newValue, 80, 1e-9);
    EXPECT_EQ(rep.value()[2].state, DimensionState::Dangling);
    EXPECT_FALSE(rep.value()[2].reason.empty());
    EXPECT_EQ(rep.value()[3].state, DimensionState::Dangling);
    Drawing dr = Drawing::create(gostSheet(GostFormat::A3, 1, Orientation::Landscape).value()).value();
    auto n = set.render(dr);
    ASSERT_TRUE(n.ok());
    EXPECT_EQ(n.value(), 2u);  // висячие не выводятся
    EXPECT_FALSE(renderDimension(dr, set.results()[2]).ok());

    // Отверстие вернули — размеры снова разрешаются.
    ASSERT_TRUE(part.doc.setSuppressed("Hole", false).ok());
    part.doc.rebuild();
    m = part.binding();
    rep = set.update(m, sheetView(m, topFrame()));
    ASSERT_TRUE(rep.ok());
    EXPECT_EQ(rep.value()[2].state, DimensionState::Ok) << rep.value()[2].reason;
    EXPECT_NEAR(rep.value()[2].newValue, 12, 1e-9);
}

TEST(AnnotationsDimensions, PlainShapeRebindsBySignature) {
    auto m0 = ModelBinding::fromShape(bendcore::kernel::Shape(BRepPrimAPI_MakePrism(
        BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakePolygon(gp_Pnt(0, 0, 0), gp_Pnt(60, 0, 0), gp_Pnt(60, 40, 0),
                                                           gp_Pnt(0, 40, 0), true).Wire()).Face(),
        gp_Vec(0, 0, 20)).Shape()));
    ASSERT_TRUE(m0.ok());
    EXPECT_FALSE(m0.value().hasHistoryNames());
    const auto e = lineEdge(m0.value(), {0, 0, 20}, {60, 0, 20});
    ASSERT_TRUE(e);
    DimensionSet set;
    DimensionDef d;
    d.type = DimensionType::Horizontal;
    d.anchors = {anchor(m0.value(), RefKind::Edge, *e, AnchorKind::Start), anchor(m0.value(), RefKind::Edge, *e, AnchorKind::End)};
    EXPECT_TRUE(d.anchors[0].ref.persistent.name.empty());
    set.add(d);
    ASSERT_TRUE(set.evaluate(m0.value(), sheetView(m0.value(), topFrame())).ok());
    EXPECT_NEAR(set.results()[0].value, 60, 1e-9);
    // Длина 60 → 64: ребро находится по сигнатуре.
    auto m1 = ModelBinding::fromShape(bendcore::kernel::Shape(BRepPrimAPI_MakePrism(
        BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakePolygon(gp_Pnt(0, 0, 0), gp_Pnt(64, 0, 0), gp_Pnt(64, 40, 0),
                                                           gp_Pnt(0, 40, 0), true).Wire()).Face(),
        gp_Vec(0, 0, 20)).Shape()));
    auto rep = set.update(m1.value(), sheetView(m1.value(), topFrame()));
    ASSERT_TRUE(rep.ok());
    ASSERT_EQ(rep.value()[0].state, DimensionState::Ok) << rep.value()[0].reason;
    EXPECT_NEAR(rep.value()[0].newValue, 64, 1e-9);
}

TEST(AnnotationsDimensions, ElementNotShownMakesDangling) {
    BlockPart part;
    const ModelBinding m = part.binding();
    // Вертикальное ребро x = 0, y = 0 на виде сверху проецируется в точку.
    const auto e = lineEdge(m, {0, 0, 0}, {0, 0, 20});
    ASSERT_TRUE(e);
    DimensionDef d;
    d.type = DimensionType::Aligned;
    d.anchors = {anchor(m, RefKind::Edge, *e, AnchorKind::Start), anchor(m, RefKind::Edge, *e, AnchorKind::End)};
    auto r = evaluateDimension(d, m, sheetView(m, topFrame()));
    ASSERT_TRUE(r.ok());
    EXPECT_EQ(r.value().state, DimensionState::Dangling);
    EXPECT_NE(r.value().reason.find("not shown"), std::string::npos) << r.value().reason;
    // Неверное определение — ошибка, а не висячий размер.
    d.anchors.pop_back();
    EXPECT_FALSE(evaluateDimension(d, m, sheetView(m, topFrame())).ok());
}

TEST(AnnotationsDimensions, ChainAndBaseline) {
    BlockPart part;
    const ModelBinding m = part.binding();
    const SheetView sv = sheetView(m, topFrame(), 1.0, {50, 50});
    const auto v0 = vertexAt(m, {0, 40, 20}), v1 = vertexAt(m, {60, 40, 20});
    const auto c = circleEdge(m, {30, 20, 20}, 5);
    ASSERT_TRUE(v0 && v1 && c);
    const Anchor a0 = anchor(m, RefKind::Vertex, *v0, AnchorKind::Point);
    const Anchor a1 = anchor(m, RefKind::Vertex, *v1, AnchorKind::Point);
    const Anchor ac = anchor(m, RefKind::Edge, *c, AnchorKind::Center);
    auto chain = chainDimensions(DimensionType::Horizontal, {a1, ac, a0}, m, sv, 10);
    ASSERT_TRUE(chain.ok()) << chain.error().message;
    ASSERT_EQ(chain.value().size(), 2u);
    double lineY[2];
    for (int i = 0; i < 2; ++i) {
        auto r = evaluateDimension(chain.value()[i], m, sv);
        ASSERT_EQ(r.value().state, DimensionState::Ok) << r.value().reason;
        EXPECT_NEAR(r.value().value, 30, 1e-9);
        lineY[i] = r.value().geometry.dimensionLines[0].a.y;
    }
    EXPECT_NEAR(lineY[0], 50 + 40 + 10, 1e-9);  // общая размерная линия на 10 мм над видом
    EXPECT_NEAR(lineY[1], lineY[0], 1e-9);

    auto base = baselineDimensions(DimensionType::Horizontal, a0, {a1, ac}, m, sv, 10, 7);
    ASSERT_TRUE(base.ok());
    ASSERT_EQ(base.value().size(), 2u);
    auto b0 = evaluateDimension(base.value()[0], m, sv).value();
    auto b1 = evaluateDimension(base.value()[1], m, sv).value();
    EXPECT_NEAR(b0.value, 30, 1e-9);  // меньший размер ближе к контуру
    EXPECT_NEAR(b1.value, 60, 1e-9);
    EXPECT_NEAR(b1.geometry.dimensionLines[0].a.y - b0.geometry.dimensionLines[0].a.y, 7.0, 1e-9);
}

TEST(AnnotationsDimensions, AngleBetweenEdges) {
    const double h = 40 * std::tan(std::numbers::pi / 6.0);
    auto m = ModelBinding::fromShape(bendcore::kernel::Shape(BRepPrimAPI_MakePrism(
        BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakePolygon(gp_Pnt(0, 0, 0), gp_Pnt(40, 0, 0), gp_Pnt(0, h, 0), true).Wire())
            .Face(),
        gp_Vec(0, 0, 10)).Shape()));
    ASSERT_TRUE(m.ok());
    const auto base = lineEdge(m.value(), {0, 0, 10}, {40, 0, 10});
    const auto hyp = lineEdge(m.value(), {40, 0, 10}, {0, h, 10});
    ASSERT_TRUE(base && hyp);
    DimensionDef d;
    d.type = DimensionType::Angle;
    d.anchors = {anchor(m.value(), RefKind::Edge, *base, AnchorKind::Start), anchor(m.value(), RefKind::Edge, *hyp, AnchorKind::Start)};
    d.offset = 20;
    const SheetView sv = sheetView(m.value(), topFrame(), 1.0, {0, 0});
    auto r = evaluateDimension(d, m.value(), sv);
    ASSERT_TRUE(r.ok()) << r.error().message;
    ASSERT_EQ(r.value().state, DimensionState::Ok) << r.value().reason;
    EXPECT_NEAR(r.value().value, 30.0, 1e-6);
    EXPECT_EQ(r.value().text, "30°");
    ASSERT_EQ(r.value().geometry.dimensionArcs.size(), 1u);
    const Arc a = r.value().geometry.dimensionArcs[0];
    EXPECT_NEAR(a.center.x, 40, 1e-6);  // вершина угла
    EXPECT_NEAR(a.center.y, 0, 1e-6);
    EXPECT_NEAR(a.radius, 20, 1e-12);
    double sweep = std::fmod(a.endDeg - a.startDeg + 720, 360);
    EXPECT_NEAR(sweep, 30, 1e-6);
    EXPECT_EQ(formatAngle(12.5), "12°30′");
}

TEST(AnnotationsDimensions, ToleranceAndOverride) {
    BlockPart part;
    const ModelBinding m = part.binding();
    const SheetView sv = sheetView(m, topFrame());
    BlockDims d = makeDims(m, 60, 40, 20, 30, 20, 5);
    d.diameter.tolerance = parseToleranceClass("H7").value();
    auto r = evaluateDimension(d.diameter, m, sv).value();
    EXPECT_EQ(r.text, "⌀10H7");
    ASSERT_TRUE(r.deviations);
    EXPECT_DOUBLE_EQ(r.deviations->upperUm, 15);  // ⌀10 H7: +0,015 / 0
    d.diameter.toleranceDisplay = ToleranceDisplay::Deviations;
    r = evaluateDimension(d.diameter, m, sv).value();
    EXPECT_TRUE(hasLabel(r, "⌀10"));
    EXPECT_TRUE(hasLabel(r, "+0,015"));
    EXPECT_FALSE(hasLabel(r, "0"));  // нулевое нижнее отклонение не пишется
    // Отклонение — шрифтом на ступень меньше, выше основания числа.
    for (const auto& l : r.geometry.labels) {
        if (l.text == "+0,015") EXPECT_DOUBLE_EQ(l.height, 2.5);
    }
    d.length.textOverride = "<>*";
    r = evaluateDimension(d.length, m, sv).value();
    EXPECT_EQ(r.text, "60*");
    EXPECT_FALSE(r.overridden);
    d.length.textOverride = "L";
    r = evaluateDimension(d.length, m, sv).value();
    EXPECT_EQ(r.text, "L");
    EXPECT_TRUE(r.overridden);
    EXPECT_NEAR(r.value, 60, 1e-9);  // значение остаётся вычисленным
}
