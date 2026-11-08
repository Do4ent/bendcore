// Фоновый расчёт видов с кэшем: результат совпадает с прямым расчётом (рёбра и привязка к
// модели), повторный запрос — попадание в кэш, параллельные запросы, хэш формы (копии равны,
// сетка PolyAlgo не влияет), вытеснение LRU, выбор алгоритма.

#include <bendcore/drawing_v2/view_cache.hpp>

#include "test_drawing_views_support.hpp"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_NurbsConvert.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>

#include <gtest/gtest.h>

using namespace bendcore::drawing_v2;
using namespace bendcore::drawing_v2::views;

namespace {

bendcore::kernel::Shape plateWithHoles(int n) {
    TopoDS_Shape s = BRepPrimAPI_MakeBox(10.0 * n + 10, 10.0 * n + 10, 5).Shape();
    BRep_Builder b;
    TopoDS_Compound tools;
    b.MakeCompound(tools);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            b.Add(tools, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(10 + 10 * i, 10 + 10 * j, -1), gp_Dir(0, 0, 1)), 2, 7).Shape());
    return bendcore::kernel::Shape(BRepAlgoAPI_Cut(s, tools).Shape());
}

}  // namespace

TEST(ViewCache, SameResultAsDirectProjection) {
    const auto shape = dvtest::blockWithHole();
    const ViewFrame top = standardFrame(defaultFrontFrame(), StandardView::Top);
    ViewCache cache(8, 2);
    auto r = cache.get({shape, top, {}, false});
    ASSERT_TRUE(r && r->ok()) << r->error().message;
    auto direct = projectShape(shape, top);
    ASSERT_TRUE(direct.ok());
    ASSERT_EQ(r->value().edges.size(), direct.value().edges.size());
    for (std::size_t i = 0; i < direct.value().edges.size(); ++i) {
        EXPECT_EQ(r->value().edges[i].edgeIndex, direct.value().edges[i].edgeIndex) << i;
        EXPECT_EQ(r->value().edges[i].faceIndex, direct.value().edges[i].faceIndex) << i;
        EXPECT_NEAR(curveLength(r->value().edges[i].curve), curveLength(direct.value().edges[i].curve), 1e-9);
    }
    // Повторный запрос — тот же объект результата из кэша.
    auto again = cache.get({shape, top, {}, false});
    EXPECT_EQ(again.get(), r.get());
    // Копия формы — тот же хэш, тот же результат.
    const bendcore::kernel::Shape copy(BRepBuilderAPI_Copy(shape.occt()).Shape());
    EXPECT_EQ(shapeContentHash(copy).value(), shapeContentHash(shape).value());
    EXPECT_EQ(cache.get({copy, top, {}, false}).get(), r.get());
    const auto st = cache.stats();
    EXPECT_EQ(st.requests, 3u);
    EXPECT_EQ(st.computed, 1u);
    EXPECT_EQ(st.hits, 2u);
}

TEST(ViewCache, HashIgnoresMeshAndSeesChanges) {
    const auto shape = dvtest::blockWithHole();
    const auto h0 = shapeContentHash(shape).value();
    HlrOptions poly;
    poly.method = HlrMethod::Polygonal;
    ASSERT_TRUE(projectShape(shape, defaultFrontFrame(), poly).ok());  // строит сетку в форме
    EXPECT_EQ(shapeContentHash(shape).value(), h0);
    dvtest::BlockSpec other;
    other.d = 12;
    EXPECT_NE(shapeContentHash(dvtest::blockWithHole(other)).value(), h0);
    const ViewFrame f = defaultFrontFrame();
    HlrOptions a, b;
    b.hidden = false;
    EXPECT_NE(viewParametersHash(f, a), viewParametersHash(f, b));
    EXPECT_EQ(viewParametersHash(f, a), viewParametersHash(f, HlrOptions{}));
}

TEST(ViewCache, ParallelRequestsAndEviction) {
    const auto shape = plateWithHoles(4);
    const ViewFrame front = defaultFrontFrame();
    const std::vector<ViewFrame> frames = {front, standardFrame(front, StandardView::Top),
                                           standardFrame(front, StandardView::Left),
                                           axonometricFrame(front, Axonometry::Isometric)};
    ViewCache cache(3, 4);
    std::vector<std::shared_future<ViewResultPtr>> futures;
    for (const auto& f : frames) futures.push_back(cache.request({shape, f, {}, false}));
    // Тот же запрос, пока идёт расчёт, — присоединяется к нему.
    futures.push_back(cache.request({shape, frames[3], {}, false}));
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const auto& r = futures[i].get();
        ASSERT_TRUE(r->ok()) << r->error().message;
        const auto direct = projectShape(shape, frames[i]).value();
        EXPECT_EQ(r->value().edges.size(), direct.edges.size()) << i;
    }
    EXPECT_EQ(futures[4].get().get(), futures[3].get().get());
    const auto st = cache.stats();
    EXPECT_EQ(st.computed, 4u);
    EXPECT_EQ(st.hits + st.joined, 1u);
    // Ёмкость 3: готовые результаты вытесняются при следующей вставке.
    cache.get({shape, frames[0], HlrOptions{.hidden = false}, false});
    EXPECT_LE(cache.size(), 3u);
    EXPECT_GE(cache.stats().evictions, 1u);
}

TEST(ViewCache, AlgorithmChoice) {
    const ViewFrame front = defaultFrontFrame();
    const ViewFrame iso = axonometricFrame(front, Axonometry::Isometric);
    // Аналитическая геометрия — точный HLR в любом виде (замеры VIEWS.md).
    const auto plate = plateWithHoles(11);  // 121 отверстие
    const auto p = profileGeometry(plate, iso);
    EXPECT_EQ(p.obliqueCurved, 121u);
    EXPECT_EQ(p.quadric, 121u);
    EXPECT_EQ(p.freeform, 0u);
    EXPECT_EQ(profileGeometry(plate, front).obliqueCurved, 0u);
    EXPECT_EQ(chooseHlrMethod(plate, iso).method, HlrMethod::Exact);
    // Свободные поверхности (B-сплайн после NurbsConvert) — триангуляционный.
    const bendcore::kernel::Shape nurbs(BRepBuilderAPI_NurbsConvert(plateWithHoles(5).occt()).Shape());
    EXPECT_GT(profileGeometry(nurbs, front).freeform, 20u);
    EXPECT_EQ(chooseHlrMethod(nurbs, front).method, HlrMethod::Polygonal);
    EXPECT_EQ(chooseHlrMethod(nurbs, front, {.freeformThreshold = 1000}).method, HlrMethod::Exact);
    ViewCache cache(4, 1);
    auto r = cache.get({nurbs, iso, {}, true});
    ASSERT_TRUE(r->ok()) << r->error().message;
    // Polygonal: рёбра — ломаные и отрезки (дуг нет).
    EXPECT_EQ(dvtest::countCurves<Circle>(r->value(), Visibility::Visible) +
                  dvtest::countCurves<Arc>(r->value(), Visibility::Visible),
              0);
    EXPECT_GT(r->value().edges.size(), 20u);
}
