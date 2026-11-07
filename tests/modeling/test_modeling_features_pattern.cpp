// Массивы и зеркало: число граней, объём, история копий.

#include "features_test_util.hpp"

#include <gtest/gtest.h>

using namespace ft;

namespace {

constexpr double kPi = 3.14159265358979323846;

FeatureResult throughHole(const TopoDS_Shape& body, double topZ, double x, double y, double d) {
    HoleParams hp;
    hp.diameter = d;
    hp.end = HoleEnd::Through;
    auto r = hole(body, planeFaceAt(body, 2, topZ), gp_Pnt(x, y, topZ), hp);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    return r.ok() ? r.value() : FeatureResult{};
}

int cylinderFaces(const TopoDS_Shape& s) {
    int n = 0;
    for (TopExp_Explorer ex(s, TopAbs_FACE); ex.More(); ex.Next())
        if (BRepAdaptor_Surface(TopoDS::Face(ex.Current())).GetType() == GeomAbs_Cylinder) ++n;
    return n;
}

}  // namespace

TEST(Pattern, LinearTwoDirectionsOfHoles) {
    const TopoDS_Shape plate = box(100, 60, 10);
    const FeatureResult seed = throughHole(plate, 10, 10, 10, 6);
    ASSERT_FALSE(seed.shape.IsNull());
    LinearPatternParams lp;
    lp.direction1 = gp::DX();
    lp.spacing1 = 20;
    lp.count1 = 4;
    lp.direction2 = gp::DY();
    lp.spacing2 = 20;
    lp.count2 = 2;
    auto r = linearPattern(seed.shape, seed.tool, FeatureMode::Cut, lp);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(valid(r.value().shape));
    EXPECT_EQ(count(r.value().shape, TopAbs_FACE), 6 + 8);
    EXPECT_EQ(cylinderFaces(r.value().shape), 8);
    EXPECT_NEAR(volume(r.value().shape), 100 * 60 * 10 - 8 * kPi * 9 * 10, 1e-5);
    EXPECT_EQ(count(r.value().tool, TopAbs_SOLID), 7);

    // Цилиндрическая грань исходного инструмента порождает стенки семи копий.
    TopoDS_Face seedWall = findFace(seed.tool, [](const TopoDS_Face& f) {
        return BRepAdaptor_Surface(f).GetType() == GeomAbs_Cylinder;
    });
    ASSERT_FALSE(seedWall.IsNull());
    int walls = 0;
    for (const auto& g : r.value().history->Generated(seedWall))
        if (g.ShapeType() == TopAbs_FACE && contains(r.value().shape, g)) ++walls;
    EXPECT_EQ(walls, 7);
}

TEST(Pattern, CircularEqualAndStep) {
    const TopoDS_Shape plate = box(100, 100, 10);
    const FeatureResult seed = throughHole(plate, 10, 80, 50, 8);
    ASSERT_FALSE(seed.shape.IsNull());
    CircularPatternParams cp;
    cp.axis = gp_Ax1(gp_Pnt(50, 50, 0), gp::DZ());
    cp.count = 6;
    auto r = circularPattern(seed.shape, seed.tool, FeatureMode::Cut, cp);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(count(r.value().shape, TopAbs_FACE), 6 + 6);
    EXPECT_NEAR(volume(r.value().shape), 100 * 100 * 10 - 6 * kPi * 16 * 10, 1e-5);

    // Шаг 30°, 3 экземпляра: копии при 30° и 60°; копия при 60° в точке (65, 75.98).
    CircularPatternParams step = cp;
    step.count = 3;
    step.angle = kPi / 6;
    step.equalSpacing = false;
    auto s = circularPattern(seed.shape, seed.tool, FeatureMode::Cut, step);
    ASSERT_TRUE(s.ok()) << s.error().message;
    EXPECT_NEAR(volume(s.value().shape), 100 * 100 * 10 - 3 * kPi * 16 * 10, 1e-5);
    const TopoDS_Face at60 = findFace(s.value().shape, [](const TopoDS_Face& f) {
        if (BRepAdaptor_Surface(f).GetType() != GeomAbs_Cylinder) return false;
        const gp_Pnt c = faceCentroid(f);
        return std::abs(c.X() - (50 + 30 * std::cos(kPi / 3))) < 1e-6 &&
               std::abs(c.Y() - (50 + 30 * std::sin(kPi / 3))) < 1e-6;
    });
    EXPECT_FALSE(at60.IsNull());
}

TEST(Pattern, BossPatternFusesCopies) {
    const TopoDS_Shape plate = box(100, 40, 10);
    // Бобышка 10×10×5 на верхней грани.
    ExtrudeParams ep;
    ep.distance = 5;
    auto seed = extrude(rect(5, 15, 15, 25, 10), plate, ep);
    ASSERT_TRUE(seed.ok()) << seed.error().message;
    LinearPatternParams lp;
    lp.direction1 = gp::DX();
    lp.spacing1 = 30;
    lp.count1 = 3;
    auto r = linearPattern(seed.value().shape, seed.value().tool, FeatureMode::Boss, lp);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(count(r.value().shape, TopAbs_SOLID), 1);
    EXPECT_NEAR(volume(r.value().shape), 100 * 40 * 10 + 3 * 100 * 5, 1e-6);
}

TEST(Mirror, FeatureAndBody) {
    const TopoDS_Shape plate = box(100, 60, 10);
    const FeatureResult seed = throughHole(plate, 10, 20, 30, 10);
    auto m = mirrorFeature(seed.shape, seed.tool, FeatureMode::Cut, gp_Ax2(gp_Pnt(50, 0, 0), gp::DX()));
    ASSERT_TRUE(m.ok()) << m.error().message;
    EXPECT_TRUE(valid(m.value().shape));
    EXPECT_NEAR(volume(m.value().shape), 100 * 60 * 10 - 2 * kPi * 25 * 10, 1e-5);
    const TopoDS_Face mirrored = findFace(m.value().shape, [](const TopoDS_Face& f) {
        return BRepAdaptor_Surface(f).GetType() == GeomAbs_Cylinder && std::abs(faceCentroid(f).X() - 80) < 1e-6;
    });
    EXPECT_FALSE(mirrored.IsNull());

    const TopoDS_Shape half = box(10, 20, 5);
    auto merged = mirrorBody(half, gp_Ax2(gp::Origin(), gp::DX()), true);
    ASSERT_TRUE(merged.ok()) << merged.error().message;
    EXPECT_TRUE(valid(merged.value().shape));
    EXPECT_EQ(count(merged.value().shape, TopAbs_SOLID), 1);
    EXPECT_NEAR(volume(merged.value().shape), 2000.0, 1e-6);
    EXPECT_EQ(count(merged.value().shape, TopAbs_FACE), 6);
    // Грань x = 0 исчезла при объединении.
    EXPECT_TRUE(merged.value().history->IsRemoved(planeFaceAt(half, 0, 0)));

    auto separate = mirrorBody(half, gp_Ax2(gp_Pnt(-5, 0, 0), gp::DX()), false);
    ASSERT_TRUE(separate.ok()) << separate.error().message;
    EXPECT_EQ(count(separate.value().shape, TopAbs_SOLID), 2);
    EXPECT_NEAR(volume(separate.value().shape), 2000.0, 1e-6);
    const TopoDS_Face far = planeFaceAt(half, 0, 10);
    const auto& img = separate.value().history->Generated(far);
    ASSERT_EQ(img.Size(), 1);
    EXPECT_NEAR(faceCentroid(img.First()).X(), -20.0, 1e-9);
}

TEST(Pattern, InvalidInputsAreRejected) {
    const TopoDS_Shape plate = box(100, 60, 10);
    const FeatureResult seed = throughHole(plate, 10, 10, 10, 6);
    LinearPatternParams single;
    single.direction1 = gp::DX();
    single.count1 = 1;
    auto r = linearPattern(seed.shape, seed.tool, FeatureMode::Cut, single);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::InvalidInput);

    LinearPatternParams away;
    away.direction1 = gp::DZ();
    away.spacing1 = 100;
    away.count1 = 2;
    auto r2 = linearPattern(seed.shape, seed.tool, FeatureMode::Cut, away);
    ASSERT_FALSE(r2.ok());
    EXPECT_EQ(r2.error().code, ErrorCode::OperationFailed);

    CircularPatternParams one;
    one.count = 1;
    EXPECT_FALSE(circularPattern(seed.shape, seed.tool, FeatureMode::Cut, one).ok());
}
