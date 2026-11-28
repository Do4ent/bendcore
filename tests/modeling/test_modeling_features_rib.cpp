// Ребро жёсткости: треугольное ребро в углу уголка (параллельно эскизу) и ребро до верхней грани
// плиты (перпендикулярно эскизу) — объём по формуле; роли граней; ошибки.

#include "features_test_util.hpp"

#include <BRepAlgoAPI_Fuse.hxx>
#include <gtest/gtest.h>

using namespace ft;

namespace {

// Уголок: полка [0,40]×[0,20]×[0,5] и стенка [0,5]×[0,20]×[0,40].
TopoDS_Shape bracket() {
    BRepAlgoAPI_Fuse f(BRepPrimAPI_MakeBox(40, 20, 5).Shape(), BRepPrimAPI_MakeBox(5, 20, 40).Shape());
    f.SimplifyResult();
    return f.Shape();
}

TopoDS_Wire segment(const gp_Pnt& a, const gp_Pnt& b) {
    return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(a, b).Edge()).Wire();
}

const double kBracket = 40.0 * 20 * 5 + 5.0 * 20 * 35;

}  // namespace

TEST(FeaturesRib, ParallelToSketchFillsTheCorner) {
    const TopoDS_Shape b = bracket();
    ASSERT_NEAR(volume(b), kBracket, 1e-6);
    // Эскиз в средней плоскости y = 10 (нормаль +Y): отрезок от стенки (x = 5, z = 25) до полки
    // (x = 25, z = 5). Ребро — треугольник с катетами 20 и 20, толщина 2: объём 400.
    const gp_Ax3 plane(gp_Pnt(0, 10, 0), gp::DY(), gp::DX());
    for (RibThickness side : {RibThickness::Symmetric, RibThickness::FirstSide, RibThickness::SecondSide}) {
        RibParams p;
        p.thickness = 2.0;
        p.side = side;
        auto res = rib(segment(gp_Pnt(5, 10, 25), gp_Pnt(25, 10, 5)), plane, b, p);
        ASSERT_TRUE(res.ok()) << res.error().message;
        EXPECT_TRUE(valid(res.value().shape));
        EXPECT_EQ(count(res.value().shape, TopAbs_SOLID), 1);
        EXPECT_NEAR(volume(res.value().shape), kBracket + 400.0, 1e-6 * kBracket);
        int sides = 0;
        for (const auto& [f, role] : res.value().roles) {
            if (role != "SideA" && role != "SideB") continue;
            ++sides;
            EXPECT_NEAR(area(f), 200.0, 1e-6) << role;
            const double y = faceCentroid(f).Y();
            const double expected = side == RibThickness::Symmetric ? (role == "SideA" ? 11.0 : 9.0)
                                    : side == RibThickness::FirstSide ? (role == "SideA" ? 12.0 : 10.0)
                                                                      : (role == "SideA" ? 10.0 : 8.0);
            EXPECT_NEAR(y, expected, 1e-9) << role;
        }
        EXPECT_EQ(sides, 2);
    }
    // Сторона материала +n = нормаль эскиза × хорда = (−1, 0, −1)/√2 смотрит в угол; −n (flip)
    // уходит от угла в пустоту — ребро не упирается в тело.
    RibParams away;
    away.flip = true;
    auto res = rib(segment(gp_Pnt(5, 10, 25), gp_Pnt(25, 10, 5)), plane, b, away);
    ASSERT_FALSE(res.ok());
    EXPECT_EQ(res.error().code, ErrorCode::OperationFailed);
    EXPECT_NE(res.error().message.find("chosen side"), std::string::npos) << res.error().message;
    RibParams toward;
    toward.flip = false;
    auto ok = rib(segment(gp_Pnt(5, 10, 25), gp_Pnt(25, 10, 5)), plane, b, toward);
    ASSERT_TRUE(ok.ok()) << ok.error().message;
    EXPECT_NEAR(volume(ok.value().shape), kBracket + 400.0, 1e-6 * kBracket);
}

TEST(FeaturesRib, PerpendicularToSketchDownToTheFace) {
    const TopoDS_Shape plate = box(40, 20, 5);
    // Эскиз на высоте z = 15, отрезок длиной 20: ребро толщиной 2 до верхней грани (z = 5),
    // объём 20·2·10.
    const gp_Ax3 plane(gp_Pnt(0, 0, 15), gp::DZ(), gp::DX());
    RibParams p;
    p.thickness = 2.0;
    p.direction = RibDirection::PerpendicularToSketch;
    auto res = rib(segment(gp_Pnt(10, 10, 15), gp_Pnt(30, 10, 15)), plane, plate, p);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_TRUE(valid(res.value().shape));
    EXPECT_NEAR(volume(res.value().shape), 4000.0 + 400.0, 1e-6 * 4400);
    bool top = false;
    for (const auto& [f, role] : res.value().roles)
        if (role == "Top") {
            top = true;
            EXPECT_NEAR(area(f), 40.0, 1e-9);
            EXPECT_NEAR(faceCentroid(f).Z(), 15.0, 1e-9);
        }
    EXPECT_TRUE(top);
    // Ломаная из двух отрезков: стенки 20 и 10 и круглый стык диаметра t.
    BRepBuilderAPI_MakeWire mw;
    mw.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(10, 5, 15), gp_Pnt(30, 5, 15)).Edge());
    mw.Add(BRepBuilderAPI_MakeEdge(gp_Pnt(30, 5, 15), gp_Pnt(30, 15, 15)).Edge());
    auto bent = rib(mw.Wire(), plane, plate, p);
    ASSERT_TRUE(bent.ok()) << bent.error().message;
    // Сечение — объединение прямоугольников [10,30]×[4,6] и [29,31]×[5,15] (перекрытие 1×1) и
    // круга r = 1 в вершине (30, 5), из которого вне прямоугольников остаётся четверть:
    // 40 + 20 − 1 + π/4.
    const double kPi = 3.14159265358979323846;
    double topArea = 0;
    for (const auto& [f, role] : bent.value().roles)
        if (role == "Top") topArea += area(f);
    EXPECT_NEAR(topArea, 59.0 + kPi / 4, 1e-6) << "top area";
    EXPECT_NEAR(volume(bent.value().shape), 4000.0 + (59.0 + kPi / 4) * 10.0, 1e-6 * 4600);
}

TEST(FeaturesRib, Errors) {
    const TopoDS_Shape b = bracket();
    const gp_Ax3 plane(gp_Pnt(0, 10, 0), gp::DY(), gp::DX());
    RibParams p;
    p.thickness = 0.0;
    auto zero = rib(segment(gp_Pnt(5, 10, 25), gp_Pnt(25, 10, 5)), plane, b, p);
    ASSERT_FALSE(zero.ok());
    EXPECT_EQ(zero.error().code, ErrorCode::InvalidInput);
    p.thickness = 2.0;
    BRepBuilderAPI_MakePolygon tri(gp_Pnt(10, 10, 10), gp_Pnt(20, 10, 10), gp_Pnt(10, 10, 20), true);
    auto closed = rib(tri.Wire(), plane, b, p);
    ASSERT_FALSE(closed.ok());
    EXPECT_NE(closed.error().message.find("open"), std::string::npos) << closed.error().message;
    auto offPlane = rib(segment(gp_Pnt(5, 12, 25), gp_Pnt(25, 10, 5)), plane, b, p);
    ASSERT_FALSE(offPlane.ok());
    EXPECT_NE(offPlane.error().message.find("plane"), std::string::npos) << offPlane.error().message;
    // Отрезок далеко над деталью, параллельно полке: с обеих сторон ребро не упирается в тело
    // (одна сторона уходит в пустоту, другая — бесконечная полоса между линией и полкой вдоль x).
    auto nowhere = rib(segment(gp_Pnt(100, 10, 100), gp_Pnt(120, 10, 100)), plane, b, p);
    ASSERT_FALSE(nowhere.ok());
    EXPECT_EQ(nowhere.error().code, ErrorCode::OperationFailed);
}
