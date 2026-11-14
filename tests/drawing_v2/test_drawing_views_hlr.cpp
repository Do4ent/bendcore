// HLR: виды бруска с отверстием против аналитики (ГОСТ 2.305), привязка 2D-рёбер к
// TopologyIndex, изометрия против формулы ГОСТ 2.317, линии перехода на скруглении.

#include "test_drawing_views_support.hpp"

#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Tool.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <gtest/gtest.h>

#include <cmath>
#include <map>

using namespace dvtest;
using bendcore::kernel::Shape;
using bendcore::kernel::TopologyIndex;

namespace {

const HlrMethod kMethods[] = {HlrMethod::Exact, HlrMethod::Polygonal};

ProjectedView view(const Shape& s, StandardView v, HlrMethod m, bool hidden = true) {
    HlrOptions o;
    o.method = m;
    o.hidden = hidden;
    auto r = projectShape(s, standardFrame(defaultFrontFrame(), v), o);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    return r.ok() ? r.value() : ProjectedView{};
}

// Сумма длин видимых/невидимых отрезков на вертикали x = x0 (для PolyAlgo отрезки могут
// дробиться).
double verticalLength(const ProjectedView& v, Visibility vis, double x0, double tol = 1e-3) {
    double s = 0;
    for (const LineSeg& l : segments(v, vis))
        if (std::abs(l.a.x - x0) < tol && std::abs(l.b.x - x0) < tol) s += std::abs(l.b.y - l.a.y);
    return s;
}

Point2 vertexPoint(const TopoDS_Vertex& v, const ViewFrame& f) {
    const gp_Pnt p = BRep_Tool::Pnt(v);
    return f.project({p.X(), p.Y(), p.Z()});
}

}  // namespace

TEST(DrawingViewsHlr, FrontViewOfBlockWithHole) {
    // Брусок 60×40×20 с отверстием Ø10 в (30, 20): на виде спереди контур 60×20 и две
    // штриховые линии очерка отверстия x = 25 и x = 35 от z = 0 до z = 20.
    const Shape s = blockWithHole();
    const ProjectedView v = view(s, StandardView::Front, HlrMethod::Exact);
    const auto vis = segments(v, Visibility::Visible);
    const auto hid = segments(v, Visibility::Hidden);
    ASSERT_EQ(vis.size(), 4u) << describe(v);
    ASSERT_EQ(hid.size(), 2u) << describe(v);
    EXPECT_EQ(v.edges.size(), 6u) << describe(v);
    EXPECT_TRUE(hasSegment(vis, {0, 0}, {60, 0}));
    EXPECT_TRUE(hasSegment(vis, {0, 20}, {60, 20}));
    EXPECT_TRUE(hasSegment(vis, {0, 0}, {0, 20}));
    EXPECT_TRUE(hasSegment(vis, {60, 0}, {60, 20}));
    EXPECT_TRUE(hasSegment(hid, {25, 0}, {25, 20}));
    EXPECT_TRUE(hasSegment(hid, {35, 0}, {35, 20}));
    EXPECT_NEAR(v.bounds.minX, 0, 1e-9);
    EXPECT_NEAR(v.bounds.maxX, 60, 1e-9);
    EXPECT_NEAR(v.bounds.maxY, 20, 1e-9);
    for (const ViewEdge& e : v.edges)
        if (e.visibility == Visibility::Hidden) EXPECT_EQ(e.cls, EdgeClass::Silhouette);
}

TEST(DrawingViewsHlr, FrontViewPolygonalMatchesExact) {
    const Shape s = blockWithHole();
    const ProjectedView v = view(s, StandardView::Front, HlrMethod::Polygonal);
    const auto vis = segments(v, Visibility::Visible);
    ASSERT_EQ(vis.size(), 4u) << describe(v);
    EXPECT_TRUE(hasSegment(vis, {0, 0}, {60, 0}, 1e-6));
    EXPECT_TRUE(hasSegment(vis, {60, 0}, {60, 20}, 1e-6));
    EXPECT_NEAR(verticalLength(v, Visibility::Hidden, 25), 20.0, 1e-6) << describe(v);
    EXPECT_NEAR(verticalLength(v, Visibility::Hidden, 35), 20.0, 1e-6) << describe(v);
    EXPECT_EQ(segments(v, Visibility::Hidden).size(), 2u) << describe(v);
}

TEST(DrawingViewsHlr, TopViewShowsHoleCircleAndNoHiddenLines) {
    const Shape s = blockWithHole();
    for (HlrMethod m : kMethods) {
        const ProjectedView v = view(s, StandardView::Top, m);
        const auto vis = segments(v, Visibility::Visible);
        ASSERT_EQ(vis.size(), 4u) << describe(v);
        EXPECT_TRUE(hasSegment(vis, {0, 0}, {60, 0}));
        EXPECT_TRUE(hasSegment(vis, {0, 40}, {60, 40}));
        EXPECT_TRUE(hasSegment(vis, {0, 0}, {0, 40}));
        EXPECT_TRUE(hasSegment(vis, {60, 0}, {60, 40}));
        // Нижняя окружность отверстия совпадает с верхней и удалена; невидимых линий нет.
        int hidden = 0;
        for (const ViewEdge& e : v.edges) hidden += e.visibility == Visibility::Hidden;
        EXPECT_EQ(hidden, 0) << describe(v);
        if (m == HlrMethod::Exact) {
            ASSERT_EQ(countCurves<Circle>(v, Visibility::Visible), 1) << describe(v);
            for (const ViewEdge& e : v.edges)
                if (auto* c = std::get_if<Circle>(&e.curve)) {
                    EXPECT_NEAR(c->center.x, 30, 1e-9);
                    EXPECT_NEAR(c->center.y, 20, 1e-9);
                    EXPECT_NEAR(c->radius, 5, 1e-9);
                }
        } else {
            // PolyAlgo: окружность — замкнутая ломаная, вершины на окружности r = 5.
            int polys = 0;
            for (const ViewEdge& e : v.edges)
                if (auto* p = std::get_if<Polyline>(&e.curve)) {
                    ++polys;
                    for (const Point2& q : p->points) EXPECT_NEAR(std::hypot(q.x - 30, q.y - 20), 5.0, 1e-6);
                }
            EXPECT_EQ(polys, 1) << describe(v);
        }
    }
}

TEST(DrawingViewsHlr, LeftViewFirstAngle) {
    // Вид слева: ось x вида = −Y модели; очерк отверстия при y = 15 и y = 25 → x = −15, −25.
    const Shape s = blockWithHole();
    for (HlrMethod m : kMethods) {
        const ProjectedView v = view(s, StandardView::Left, m);
        const auto vis = segments(v, Visibility::Visible);
        ASSERT_EQ(vis.size(), 4u) << describe(v);
        EXPECT_TRUE(hasSegment(vis, {-40, 0}, {0, 0}));
        EXPECT_TRUE(hasSegment(vis, {-40, 20}, {0, 20}));
        EXPECT_NEAR(verticalLength(v, Visibility::Hidden, -15), 20.0, 1e-6);
        EXPECT_NEAR(verticalLength(v, Visibility::Hidden, -25), 20.0, 1e-6);
    }
}

TEST(DrawingViewsHlr, HiddenLinesCanBeSwitchedOff) {
    const Shape s = blockWithHole();
    for (HlrMethod m : kMethods) {
        const ProjectedView v = view(s, StandardView::Front, m, false);
        for (const ViewEdge& e : v.edges) EXPECT_EQ(e.visibility, Visibility::Visible);
        EXPECT_EQ(segments(v, Visibility::Visible).size(), 4u);
    }
}

TEST(DrawingViewsHlr, EdgesAreBoundToTopologyIndex) {
    // Каждое видимое ребро вида спереди ссылается на ребро модели, концы которого
    // проецируются в концы 2D-отрезка; очерк отверстия ссылается на цилиндрическую грань.
    const Shape s = blockWithHole();
    auto idx = TopologyIndex::build(s);
    ASSERT_TRUE(idx.ok());
    const ViewFrame f = standardFrame(defaultFrontFrame(), StandardView::Front);
    for (HlrMethod m : kMethods) {
        const ProjectedView v = view(s, StandardView::Front, m);
        for (const ViewEdge& e : v.edges) {
            if (e.cls == EdgeClass::Silhouette && e.faceIndex) {
                EXPECT_EQ(idx.value().faces()[*e.faceIndex].surface,
                          bendcore::kernel::SurfaceType::Cylinder);
                continue;
            }
            ASSERT_TRUE(e.edgeIndex.has_value()) << describe(v);
            ASSERT_LT(*e.edgeIndex, idx.value().edges().size());
            const TopoDS_Edge& me = idx.value().edges()[*e.edgeIndex].edge;
            TopoDS_Vertex v1, v2;
            TopExp::Vertices(me, v1, v2);
            const auto* l = std::get_if<LineSeg>(&e.curve);
            ASSERT_NE(l, nullptr);
            if (e.cls == EdgeClass::Silhouette) {
                // Шов цилиндра, совпадающий с очерком: ребро модели — прямая шва.
                EXPECT_EQ(idx.value().edges()[*e.edgeIndex].curve, bendcore::kernel::CurveType::Line);
                continue;
            }
            EXPECT_TRUE(hasSegment({*l}, vertexPoint(v1, f), vertexPoint(v2, f), 1e-6)) << describe(v);
        }
    }
}

TEST(DrawingViewsHlr, IsometricLengthsFollowGost2317) {
    // Брусок 60×40×20 в изометрии: 9 видимых и 3 невидимых ребра; проекции рёбер,
    // параллельных осям, имеют длину L·√(2/3) (коэффициент 0,82), с приведённым
    // коэффициентом (увеличение 1,22) — L.
    const Shape box(BRepPrimAPI_MakeBox(60, 40, 20).Shape());
    const ViewFrame iso = axonometricFrame(defaultFrontFrame(), Axonometry::Isometric);
    for (HlrMethod m : kMethods) {
        HlrOptions o;
        o.method = m;
        auto r = projectShape(box, iso, o);
        ASSERT_TRUE(r.ok());
        const auto vis = segments(r.value(), Visibility::Visible);
        const auto hid = segments(r.value(), Visibility::Hidden);
        ASSERT_EQ(vis.size(), 9u) << describe(r.value());
        ASSERT_EQ(hid.size(), 3u) << describe(r.value());
        const double k = std::sqrt(2.0 / 3.0);
        const double factor = axonometricFactor(Axonometry::Isometric, true);
        std::map<long, int> byLength;
        for (const auto* list : {&vis, &hid})
            for (const LineSeg& l : *list) {
                const double len = curveLength(l);
                bool matched = false;
                for (double L : {60.0, 40.0, 20.0})
                    if (std::abs(len - L * k) < 1e-6) {
                        EXPECT_NEAR(len * factor, L, 1e-6);
                        ++byLength[long(L)];
                        matched = true;
                    }
                EXPECT_TRUE(matched) << len;
            }
        EXPECT_EQ(byLength[60], 4);
        EXPECT_EQ(byLength[40], 4);
        EXPECT_EQ(byLength[20], 4);
    }
}

TEST(DrawingViewsHlr, TransitionLineOfFillet) {
    // Скругление R5 ребра x = 60, z = 20 (вдоль Y). На виде сверху линия перехода
    // скругления к верхней грани — x = 55 длиной 40 (класс Smooth, сплошная тонкая); на
    // виде спереди скругление — дуга R5 с центром (55, 15).
    TopoDS_Shape box = BRepPrimAPI_MakeBox(60, 40, 20).Shape();
    BRepFilletAPI_MakeFillet fillet(box);
    for (TopExp_Explorer ex(box, TopAbs_EDGE); ex.More(); ex.Next()) {
        TopoDS_Vertex a, b;
        TopExp::Vertices(TopoDS::Edge(ex.Current()), a, b);
        const gp_Pnt pa = BRep_Tool::Pnt(a), pb = BRep_Tool::Pnt(b);
        if (std::abs(pa.X() - 60) < 1e-9 && std::abs(pb.X() - 60) < 1e-9 && std::abs(pa.Z() - 20) < 1e-9 &&
            std::abs(pb.Z() - 20) < 1e-9)
            fillet.Add(5.0, TopoDS::Edge(ex.Current()));
    }
    const Shape s(fillet.Shape());
    auto idx = TopologyIndex::build(s);
    ASSERT_TRUE(idx.ok());

    const ProjectedView top = view(s, StandardView::Top, HlrMethod::Exact);
    int smooth = 0;
    for (const ViewEdge& e : top.edges)
        if (e.cls == EdgeClass::Smooth && e.visibility == Visibility::Visible) {
            ++smooth;
            const auto* l = std::get_if<LineSeg>(&e.curve);
            ASSERT_NE(l, nullptr);
            EXPECT_NEAR(l->a.x, 55, 1e-9);
            EXPECT_NEAR(l->b.x, 55, 1e-9);
            EXPECT_NEAR(std::abs(l->b.y - l->a.y), 40, 1e-9);
            ASSERT_TRUE(e.edgeIndex.has_value());
            EXPECT_EQ(idx.value().edges()[*e.edgeIndex].curve, bendcore::kernel::CurveType::Line);
        }
    EXPECT_EQ(smooth, 1) << describe(top);
    // Без линий перехода.
    HlrOptions o;
    o.smooth = false;
    auto noSmooth = projectShape(s, standardFrame(defaultFrontFrame(), StandardView::Top), o);
    ASSERT_TRUE(noSmooth.ok());
    for (const ViewEdge& e : noSmooth.value().edges) EXPECT_NE(e.cls, EdgeClass::Smooth);

    const ProjectedView front = view(s, StandardView::Front, HlrMethod::Exact);
    int arcs = 0;
    for (const ViewEdge& e : front.edges)
        if (auto* a = std::get_if<Arc>(&e.curve); a && e.visibility == Visibility::Visible) {
            ++arcs;
            EXPECT_NEAR(a->center.x, 55, 1e-9);
            EXPECT_NEAR(a->center.y, 15, 1e-9);
            EXPECT_NEAR(a->radius, 5, 1e-9);
            EXPECT_NEAR(curveLength(*a), 5 * 3.14159265358979 / 2, 1e-6);
        }
    EXPECT_EQ(arcs, 1) << describe(front);
}

TEST(DrawingViewsHlr, Errors) {
    auto r = projectShape(Shape(), defaultFrontFrame());
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::InvalidInput);
    HlrOptions o;
    o.method = HlrMethod::Polygonal;
    o.linearDeflection = 0;
    auto r2 = projectShape(blockWithHole(), defaultFrontFrame(), o);
    ASSERT_FALSE(r2.ok());
    EXPECT_EQ(r2.error().code, ErrorCode::InvalidInput);
}
