// Ориентация видов: ГОСТ 2.305 (основные виды, метод первого угла, ISO — метод третьего угла),
// ГОСТ 2.317 (изометрия, диметрия): направления и коэффициенты искажения против формул.

#include "test_drawing_views_support.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

using namespace dvtest;

namespace {

void expectVec(Vec3 a, Vec3 b, double tol = 1e-12) {
    EXPECT_NEAR(a.x, b.x, tol);
    EXPECT_NEAR(a.y, b.y, tol);
    EXPECT_NEAR(a.z, b.z, tol);
}

double deg(double r) { return r * 180.0 / std::numbers::pi; }

}  // namespace

TEST(DrawingViewsFrames, StandardViewsOfDefaultFront) {
    const ViewFrame f = defaultFrontFrame();
    // Вид спереди: наблюдатель на −Y, X вправо, Z вверх.
    expectVec(f.toward, {0, -1, 0});
    expectVec(f.up(), {0, 0, 1});
    const ViewFrame top = standardFrame(f, StandardView::Top);
    expectVec(top.toward, {0, 0, 1});
    expectVec(top.right, {1, 0, 0});
    expectVec(top.up(), {0, 1, 0});  // задняя сторона детали — вверху вида сверху
    const ViewFrame left = standardFrame(f, StandardView::Left);
    expectVec(left.toward, {-1, 0, 0});
    expectVec(left.right, {0, -1, 0});  // передняя сторона — справа на виде слева
    expectVec(left.up(), {0, 0, 1});
    const ViewFrame right = standardFrame(f, StandardView::Right);
    expectVec(right.toward, {1, 0, 0});
    expectVec(right.right, {0, 1, 0});
    const ViewFrame bottom = standardFrame(f, StandardView::Bottom);
    expectVec(bottom.toward, {0, 0, -1});
    expectVec(bottom.up(), {0, -1, 0});
    const ViewFrame rear = standardFrame(f, StandardView::Rear);
    expectVec(rear.toward, {0, 1, 0});
    expectVec(rear.right, {-1, 0, 0});
    // Каждая тройка правая и ортонормированная.
    for (auto v : {StandardView::Front, StandardView::Top, StandardView::Left, StandardView::Right,
                   StandardView::Bottom, StandardView::Rear}) {
        const ViewFrame g = standardFrame(f, v);
        EXPECT_NEAR(dot(g.toward, g.right), 0.0, 1e-12);
        EXPECT_NEAR(length(g.up()), 1.0, 1e-12);
        expectVec(cross(g.right, g.up()), g.toward);
    }
}

TEST(DrawingViewsFrames, FirstAndThirdAnglePlacement) {
    // ГОСТ 2.305, рис. 1: вид сверху — под главным, слева — справа, справа — слева, снизу —
    // над главным, сзади — справа от вида слева. ISO 128-30 метод A — зеркально.
    using P = ProjectionMethod;
    EXPECT_EQ(standardPlacement(StandardView::Top, P::FirstAngle), SheetSide::Below);
    EXPECT_EQ(standardPlacement(StandardView::Bottom, P::FirstAngle), SheetSide::Above);
    EXPECT_EQ(standardPlacement(StandardView::Left, P::FirstAngle), SheetSide::RightOf);
    EXPECT_EQ(standardPlacement(StandardView::Right, P::FirstAngle), SheetSide::LeftOf);
    EXPECT_EQ(standardPlacement(StandardView::Top, P::ThirdAngle), SheetSide::Above);
    EXPECT_EQ(standardPlacement(StandardView::Left, P::ThirdAngle), SheetSide::LeftOf);
    EXPECT_FALSE(standardPlacement(StandardView::Front, P::FirstAngle).has_value());

    const ViewFrame f = defaultFrontFrame();
    // Проекционный вид, поставленный на месте основного, совпадает с ним.
    for (auto m : {P::FirstAngle, P::ThirdAngle})
        for (auto v : {StandardView::Top, StandardView::Bottom, StandardView::Left, StandardView::Right}) {
            const ViewFrame g = projectedFrame(f, *standardPlacement(v, m), m);
            expectVec(g.toward, standardFrame(f, v).toward);
            expectVec(g.right, standardFrame(f, v).right);
        }
    // Вид сзади — проекционный от вида слева, справа от него (метод первого угла).
    const ViewFrame left = standardFrame(f, StandardView::Left);
    const ViewFrame rear = projectedFrame(left, SheetSide::RightOf, P::FirstAngle);
    expectVec(rear.toward, standardFrame(f, StandardView::Rear).toward);
    expectVec(rear.right, standardFrame(f, StandardView::Rear).right);
}

TEST(DrawingViewsFrames, AuxiliaryFrame) {
    // Взгляд вдоль (1, 1, 0)/√2 сверху вниз по экрану Z.
    auto f = auxiliaryFrame({1, 1, 0}, {0, 0, 1});
    ASSERT_TRUE(f.ok());
    const double k = 1 / std::sqrt(2.0);
    expectVec(f.value().toward, {-k, -k, 0});
    expectVec(f.value().up(), {0, 0, 1});
    // Точка (1, −1, 0) на дополнительном виде лежит на оси x на расстоянии √2.
    const Point2 p = f.value().project({1, -1, 0});
    EXPECT_NEAR(std::abs(p.x), std::sqrt(2.0), 1e-12);
    EXPECT_NEAR(p.y, 0.0, 1e-12);
    auto bad = auxiliaryFrame({0, 0, 1}, {0, 0, 2});
    ASSERT_FALSE(bad.ok());
    EXPECT_EQ(bad.error().code, ErrorCode::InvalidInput);
    EXPECT_FALSE(auxiliaryFrame({0, 0, 0}, {0, 0, 1}).ok());
    EXPECT_FALSE(makeFrame({0, 0, 1}, {0, 0, 3}).ok());
}

TEST(DrawingViewsFrames, IsometricAxesGost2317) {
    // ГОСТ 2.317, п. 5.1: оси x, y под 30° к горизонтали, z вертикально; коэффициент
    // искажения 0,82 (точно √(2/3)); приведённый — 1 при увеличении 1,22.
    // Оси аксонометрии в системе главного вида: x — вправо, y — к наблюдателю, z — вверх.
    const ViewFrame main = defaultFrontFrame();
    for (auto corner : {AxonometricCorner::FrontRightTop, AxonometricCorner::FrontLeftTop}) {
        const ViewFrame iso = axonometricFrame(main, Axonometry::Isometric, corner);
        // Ось x направлена к наблюдателю: +X при взгляде справа, −X при взгляде слева.
        const double sx = corner == AxonometricCorner::FrontRightTop ? 1.0 : -1.0;
        const Point2 x = iso.project({sx, 0, 0}), y = iso.project({0, -1, 0}), z = iso.project({0, 0, 1});
        for (Point2 a : {x, y}) {
            EXPECT_NEAR(std::hypot(a.x, a.y), std::sqrt(2.0 / 3.0), 1e-12);
            EXPECT_NEAR(std::abs(deg(std::atan(a.y / a.x))), 30.0, 1e-9);
            EXPECT_LT(a.y, 0.0);  // оси x и y уходят вниз от начала координат
        }
        EXPECT_NEAR(z.x, 0.0, 1e-12);
        EXPECT_NEAR(z.y, std::sqrt(2.0 / 3.0), 1e-12);
    }
    EXPECT_NEAR(axonometricFactor(Axonometry::Isometric, true), 1.2247448713915890, 1e-15);
    EXPECT_NEAR(axonometricFactor(Axonometry::Isometric, true) * std::sqrt(2.0 / 3.0), 1.0, 1e-14);
    EXPECT_EQ(axonometricFactor(Axonometry::Isometric, false), 1.0);
}

TEST(DrawingViewsFrames, DimetricAxesGost2317) {
    // ГОСТ 2.317, п. 5.2: ось x под 7°10′, ось y под 41°25′ к горизонтали, коэффициенты
    // 0,94 по x и z и 0,47 по y; приведённые 1 и 0,5 при увеличении 1,06.
    const ViewFrame main = defaultFrontFrame();
    const ViewFrame dim = axonometricFrame(main, Axonometry::Dimetric);
    const Point2 x = dim.project({1, 0, 0}), y = dim.project({0, -1, 0}), z = dim.project({0, 0, 1});
    EXPECT_NEAR(std::hypot(x.x, x.y), 0.94, 0.003);
    EXPECT_NEAR(std::hypot(z.x, z.y), 0.94, 0.003);
    EXPECT_NEAR(std::hypot(y.x, y.y), 0.47, 0.002);
    EXPECT_NEAR(std::abs(deg(std::atan(x.y / x.x))), 7.0 + 10.0 / 60.0, 1.0 / 60.0);
    EXPECT_NEAR(std::abs(deg(std::atan(y.y / y.x))), 41.0 + 25.0 / 60.0, 1.0 / 60.0);
    EXPECT_NEAR(z.x, 0.0, 1e-12);
    const double k = axonometricFactor(Axonometry::Dimetric, true);
    EXPECT_NEAR(k, 1.06, 0.001);
    EXPECT_NEAR(k * std::hypot(x.x, x.y), 1.0, 1e-12);
    EXPECT_NEAR(k * std::hypot(y.x, y.y), 0.5, 1e-12);
}
