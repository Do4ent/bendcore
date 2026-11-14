// Отверстие: удалённый объём простого, глухого с конусом 118°, цековки, зенковки.

#include "features_test_util.hpp"

#include <gtest/gtest.h>

using namespace ft;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kBox = 100.0 * 100.0 * 50.0;

struct Plate {
    TopoDS_Shape body = box(100, 100, 50);
    TopoDS_Face top = planeFaceAt(body, 2, 50);
};

double removed(const HoleParams& hp, const gp_Pnt& at = gp_Pnt(50, 50, 50)) {
    Plate p;
    auto r = hole(p.body, p.top, at, hp);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    if (!r.ok()) return -1.0;
    EXPECT_TRUE(valid(r.value().shape));
    return kBox - volume(r.value().shape);
}

double tipHeight(double r) { return r / std::tan(118.0 / 2 * kPi / 180); }

}  // namespace

TEST(Hole, SimpleBlindWithDrillPoint) {
    HoleParams hp;
    hp.diameter = 10;
    hp.depth = 20;
    EXPECT_NEAR(removed(hp), kPi * 25 * 20 + kPi * 25 * tipHeight(5) / 3, 1e-5);
    hp.drillPoint = false;
    EXPECT_NEAR(removed(hp), kPi * 25 * 20, 1e-5);
}

TEST(Hole, SimpleThrough) {
    Plate p;
    HoleParams hp;
    hp.diameter = 10;
    hp.end = HoleEnd::Through;
    auto r = hole(p.body, p.top, gp_Pnt(30, 40, 50), hp);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_NEAR(kBox - volume(r.value().shape), kPi * 25 * 50, 1e-5);
    EXPECT_EQ(count(r.value().shape, TopAbs_FACE), 7);
    // Стенка отверстия построена из грани размещения.
    int cylinders = 0;
    for (const auto& g : r.value().history->Generated(p.top))
        if (BRepAdaptor_Surface(TopoDS::Face(g)).GetType() == GeomAbs_Cylinder) ++cylinders;
    EXPECT_EQ(cylinders, 1);
    EXPECT_FALSE(r.value().history->IsRemoved(p.top));
    EXPECT_EQ(r.value().history->Modified(p.top).Size(), 1);
}

TEST(Hole, CounterboreRemovedVolume) {
    HoleParams hp;
    hp.type = HoleType::Counterbore;
    hp.end = HoleEnd::Through;
    hp.diameter = 10;
    hp.counterboreDiameter = 18;
    hp.counterboreDepth = 6;
    EXPECT_NEAR(removed(hp), kPi * 81 * 6 + kPi * 25 * 44, 1e-5);
    hp.end = HoleEnd::Blind;
    hp.depth = 30;
    EXPECT_NEAR(removed(hp), kPi * 81 * 6 + kPi * 25 * 24 + kPi * 25 * tipHeight(5) / 3, 1e-5);
}

TEST(Hole, CountersinkRemovedVolume) {
    HoleParams hp;
    hp.type = HoleType::Countersink;
    hp.end = HoleEnd::Through;
    hp.diameter = 10;
    hp.countersinkDiameter = 20;
    hp.countersinkAngle = kPi / 2;  // 90°: глубина конуса (20 - 10) / 2 = 5
    const double cone = kPi * 5 / 3 * (100 + 50 + 25);
    EXPECT_NEAR(removed(hp), cone + kPi * 25 * 45, 1e-5);
}

TEST(Hole, InclinedDirection) {
    Plate p;
    HoleParams hp;
    hp.diameter = 6;
    hp.depth = 10;
    hp.drillPoint = false;
    const double tilt = 0.3;
    const gp_Dir d(0.0, std::sin(tilt), -std::cos(tilt));
    auto r = hole(p.body, p.top, gp_Pnt(50, 50, 50), hp, d);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(valid(r.value().shape));
    // Начальный торец инструмента перпендикулярен оси и наклонён к грани: внутри тела нет
    // клина цилиндра над плоскостью грани, его объём (2/3) r³ tg θ.
    const double rr = 3.0;
    EXPECT_NEAR(kBox - volume(r.value().shape), kPi * rr * rr * 10 - 2.0 / 3.0 * rr * rr * rr * std::tan(tilt), 1e-4);
}

TEST(Hole, InvalidInputsAreRejected) {
    Plate p;
    HoleParams hp;
    auto outside = hole(p.body, p.top, gp_Pnt(150, 50, 50), hp);
    ASSERT_FALSE(outside.ok());
    EXPECT_EQ(outside.error().code, ErrorCode::InvalidInput);

    HoleParams zero;
    zero.depth = 0.0;
    EXPECT_FALSE(hole(p.body, p.top, gp_Pnt(50, 50, 50), zero).ok());

    HoleParams cb;
    cb.type = HoleType::Counterbore;
    cb.counterboreDiameter = 3;  // меньше диаметра отверстия
    cb.counterboreDepth = 2;
    EXPECT_FALSE(hole(p.body, p.top, gp_Pnt(50, 50, 50), cb).ok());

    auto outward = hole(p.body, p.top, gp_Pnt(50, 50, 50), hp, gp::DZ());
    ASSERT_FALSE(outward.ok());
    EXPECT_EQ(outward.error().code, ErrorCode::InvalidInput);

    const TopoDS_Shape other = box(10, 10, 10);
    const TopoDS_Face foreign = planeFaceAt(other, 2, 10);
    EXPECT_FALSE(hole(p.body, foreign, gp_Pnt(5, 5, 10), hp).ok());
}
