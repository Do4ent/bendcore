// Нагрузка: вид HLR детали около 1000 граней через кэш видов (docs/drawing/VIEWS.md, ViewCache).
//
// Плита 300 × 270 × 20 с сеткой 33 × 30 сквозных отверстий R2 (шаг 9): 990 цилиндрических
// граней + 6 плоских = 996 граней. Эталон по построению: на виде сверху видимы 990 окружностей
// верхних кромок отверстий (нижние кромки под ними совпадают и скрыты) и контур плиты
// 300 × 270; габарит вида — 300 × 270. Повторный запрос того же вида — попадание в кэш
// (без пересчёта, на порядки быстрее), запрос после изменения модели — пересчёт.

#include "stress_util.hpp"

#include <bendcore/drawing_v2/view_cache.hpp>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>

#include <cmath>
#include <variant>

using namespace bendcore::drawing_v2::views;
using bendcore::qa::stress::timed;

namespace {

constexpr int kCols = 33, kRows = 30, kHoles = kCols * kRows;
constexpr double kPitch = 9.0, kR = 2.0, kL = 300.0, kW = 270.0, kT = 20.0;

TopoDS_Shape plate(double thickness) {
    BRep_Builder bb;
    TopoDS_Compound tools;
    bb.MakeCompound(tools);
    for (int i = 0; i < kCols; ++i)
        for (int j = 0; j < kRows; ++j)
            bb.Add(tools, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(6 + kPitch * i, 4.5 + kPitch * j, -1), gp::DZ()), kR,
                                                   thickness + 2)
                              .Shape());
    return BRepAlgoAPI_Cut(BRepPrimAPI_MakeBox(kL, kW, thickness).Shape(), tools).Shape();
}

std::size_t visibleCircles(const ProjectedView& v, double r) {
    std::size_t n = 0;
    for (const ViewEdge& e : v.edges) {
        if (e.visibility != Visibility::Visible) continue;
        if (const auto* c = std::get_if<bendcore::drawing_v2::Circle>(&e.curve)) n += std::abs(c->radius - r) < 1e-6;
    }
    return n;
}

}  // namespace

TEST(StressHlr, ThousandFaceTopViewThroughCache) {
    const bendcore::kernel::Shape model(plate(kT));
    ASSERT_EQ(model.counts().faces, std::size_t(6 + kHoles));
    const ViewFrame top = standardFrame(defaultFrontFrame(), StandardView::Top);

    ViewCache cache(16, 2);
    const ViewRequest req{model, top, {}, false};  // точный HLRBRep_Algo
    ViewResultPtr first;
    timed("hlr_1000_faces", "top_view_exact_cold", 300, [&] { first = cache.get(req); },
          "\"faces\":" + std::to_string(6 + kHoles));
    ASSERT_TRUE(first && first->ok()) << (first ? first->error().message : "null");
    const ProjectedView& v = first->value();
    EXPECT_EQ(visibleCircles(v, kR), std::size_t(kHoles));
    EXPECT_NEAR(v.bounds.width(), kL, 1e-6);
    EXPECT_NEAR(v.bounds.height(), kW, 1e-6);

    ViewResultPtr again;
    const double hit = timed("hlr_1000_faces", "top_view_cache_hit", 5, [&] { again = cache.get(req); });
    EXPECT_EQ(again.get(), first.get()) << "the cached result object is returned";
    auto st = cache.stats();
    EXPECT_EQ(st.computed, 1u);
    EXPECT_EQ(st.hits, 1u);
    EXPECT_LT(hit, 0.5 * v.stats.milliseconds / 1000.0 + 0.5) << "a hit must not recompute HLR";

    // Изменённая модель (толщина 25) — другой ключ кэша: пересчёт, тот же вид сверху.
    const bendcore::kernel::Shape thicker(plate(25));
    ViewResultPtr changed;
    timed("hlr_1000_faces", "top_view_after_model_change", 300,
          [&] { changed = cache.get({thicker, top, {}, false}); });
    ASSERT_TRUE(changed && changed->ok());
    EXPECT_EQ(visibleCircles(changed->value(), kR), std::size_t(kHoles));
    st = cache.stats();
    EXPECT_EQ(st.computed, 2u);

    // Изометрия с автоматическим выбором алгоритма (PolyAlgo для крупных моделей).
    ViewResultPtr iso;
    timed("hlr_1000_faces", "isometric_auto_method", 300,
          [&] { iso = cache.get({model, axonometricFrame(defaultFrontFrame(), Axonometry::Isometric), {}, true}); });
    ASSERT_TRUE(iso && iso->ok()) << (iso ? iso->error().message : "null");
    EXPECT_FALSE(iso->value().edges.empty());
}
