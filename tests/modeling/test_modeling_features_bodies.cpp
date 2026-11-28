// Многотельная деталь: новое тело, булевы операции между телами, удаление, копии тел,
// вырез и бобышка на многотельной цели — объёмы против аналитики.

#include "features_test_util.hpp"

#include <gtest/gtest.h>

using namespace ft;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Тело A = [0,10]³ и новое тело B = [5,15]×[0,10]×[0,10] (выдавливание в режиме NewBody).
FeatureResult twoBodies() {
    ExtrudeParams p;
    p.mode = FeatureMode::NewBody;
    p.distance = 10;
    auto r = extrude(rect(5, 0, 15, 10), box(10, 10, 10), p);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    return r.value();
}

TopoDS_Shape bodyAt(const TopoDS_Shape& s, double x) {
    for (const auto& b : solidsOf(s)) {
        GProp_GProps g;
        BRepGProp::VolumeProperties(b, g);
        if (std::abs(g.CentreOfMass().X() - x) < 1e-6) return b;
    }
    return {};
}

}  // namespace

TEST(FeaturesBodies, NewBodyKeepsBodiesApart) {
    const FeatureResult r = twoBodies();
    EXPECT_EQ(count(r.shape, TopAbs_SOLID), 2);
    EXPECT_NEAR(volume(r.shape), 2000.0, 1e-9);
    EXPECT_TRUE(r.warnings.empty());
    ASSERT_FALSE(bodyAt(r.shape, 5).IsNull());
    ASSERT_FALSE(bodyAt(r.shape, 10).IsNull());
}

TEST(FeaturesBodies, BooleanBetweenBodies) {
    const FeatureResult r = twoBodies();
    const TopoDS_Shape a = bodyAt(r.shape, 5), b = bodyAt(r.shape, 10);
    struct Case {
        BodyBooleanType type;
        bool keep;
        int solids;
        double volume;
    };
    for (const Case c : {Case{BodyBooleanType::Union, false, 1, 1500.0}, Case{BodyBooleanType::Subtract, false, 1, 500.0},
                         Case{BodyBooleanType::Intersect, false, 1, 500.0}, Case{BodyBooleanType::Subtract, true, 2, 1500.0},
                         Case{BodyBooleanType::Intersect, true, 2, 1500.0}}) {
        auto res = bodyBoolean(r.shape, a, {b}, c.type, c.keep);
        ASSERT_TRUE(res.ok()) << res.error().message;
        EXPECT_TRUE(valid(res.value().shape));
        EXPECT_EQ(count(res.value().shape, TopAbs_SOLID), c.solids) << static_cast<int>(c.type) << c.keep;
        EXPECT_NEAR(volume(res.value().shape), c.volume, 1e-6) << static_cast<int>(c.type) << c.keep;
        if (c.keep) EXPECT_TRUE(contains(res.value().shape, b)) << "kept tool body is unchanged";
    }
    // История вычитания: грань B при x = 5 даёт стенку A (Generated при сохранённом инструменте).
    auto kept = bodyBoolean(r.shape, a, {b}, BodyBooleanType::Subtract, true);
    ASSERT_TRUE(kept.ok());
    const TopoDS_Face bLeft = planeFaceAt(b, 0, 5);
    ASSERT_FALSE(bLeft.IsNull());
    bool generated = false;
    for (const TopoDS_Shape& g : kept.value().history->Generated(bLeft))
        if (contains(bodyAt(kept.value().shape, 2.5), g)) generated = true;
    EXPECT_TRUE(generated);

    // Ошибки.
    EXPECT_FALSE(bodyBoolean(r.shape, a, {a}, BodyBooleanType::Union, false).ok());
    EXPECT_FALSE(bodyBoolean(r.shape, a, {}, BodyBooleanType::Union, false).ok());
    EXPECT_FALSE(bodyBoolean(r.shape, box(1, 1, 1), {b}, BodyBooleanType::Union, false).ok());
    ExtrudeParams far;
    far.mode = FeatureMode::NewBody;
    far.distance = 5;
    auto apart = extrude(rect(50, 50, 60, 60), box(10, 10, 10), far);
    ASSERT_TRUE(apart.ok());
    auto none = bodyBoolean(apart.value().shape, bodyAt(apart.value().shape, 5), {bodyAt(apart.value().shape, 55)},
                            BodyBooleanType::Intersect, false);
    ASSERT_FALSE(none.ok());
    EXPECT_NE(none.error().message.find("do not intersect"), std::string::npos);
}

TEST(FeaturesBodies, DeleteAndCopyBodies) {
    const FeatureResult r = twoBodies();
    const TopoDS_Shape a = bodyAt(r.shape, 5), b = bodyAt(r.shape, 10);
    auto del = deleteBodies(r.shape, {b});
    ASSERT_TRUE(del.ok()) << del.error().message;
    EXPECT_EQ(count(del.value().shape, TopAbs_SOLID), 1);
    EXPECT_NEAR(volume(del.value().shape), 1000.0, 1e-9);
    const TopoDS_Face bRight = planeFaceAt(b, 0, 15);
    EXPECT_TRUE(del.value().history->IsRemoved(bRight));
    EXPECT_FALSE(deleteBodies(r.shape, {a, b}).ok());

    // Массив тела A: 3 копии с шагом 20 по Y — новые тела.
    std::vector<gp_Trsf> moves;
    for (int i = 1; i <= 3; ++i) {
        gp_Trsf t;
        t.SetTranslation(gp_Vec(0, 20.0 * i, 0));
        moves.push_back(t);
    }
    auto copies = copyBodies(r.shape, {a}, moves, false);
    ASSERT_TRUE(copies.ok()) << copies.error().message;
    EXPECT_EQ(count(copies.value().shape, TopAbs_SOLID), 5);
    EXPECT_NEAR(volume(copies.value().shape), 5000.0, 1e-9);
    const TopoDS_Face aTop = planeFaceAt(a, 2, 10);
    EXPECT_EQ(copies.value().history->Generated(aTop).Extent(), 3);
    // Зеркало тела B относительно плоскости x = 15 с объединением: брусок 20×10×10.
    gp_Trsf m;
    m.SetMirror(gp_Ax2(gp_Pnt(15, 0, 0), gp::DX()));
    auto mirrored = copyBodies(r.shape, {b}, {m}, true);
    ASSERT_TRUE(mirrored.ok()) << mirrored.error().message;
    EXPECT_EQ(count(mirrored.value().shape, TopAbs_SOLID), 2);
    EXPECT_NEAR(volume(mirrored.value().shape), 3000.0, 1e-6);
}

TEST(FeaturesBodies, CutAndBossOnSeveralBodies) {
    const FeatureResult r = twoBodies();
    // Сквозной вырез круга r = 2 в зоне перекрытия (x = 7,5): каждое тело теряет π·4·10, тела
    // остаются отдельными (не сливаются).
    ExtrudeParams cut;
    cut.mode = FeatureMode::Cut;
    cut.extent = ExtentType::ThroughAll;
    BRepBuilderAPI_MakeFace mf(gp_Pln(gp::XOY()),
                               BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(7.5, 5, 0), gp::DZ()), 2)).Edge()).Wire(),
                               true);
    auto res = extrude(Profile{mf.Face(), gp_Ax3(gp::XOY())}, r.shape, cut);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_EQ(count(res.value().shape, TopAbs_SOLID), 2);
    EXPECT_NEAR(volume(res.value().shape), 2000.0 - 2 * kPi * 4 * 10, 1e-6 * 2000);
    // Бобышка у дальнего края B (x ∈ [15, 20]) объединяется только с B.
    ExtrudeParams boss;
    boss.distance = 10;
    auto grown = extrude(rect(15, 0, 20, 10), res.value().shape, boss);
    ASSERT_TRUE(grown.ok()) << grown.error().message;
    EXPECT_EQ(count(grown.value().shape, TopAbs_SOLID), 2);
    EXPECT_NEAR(volume(grown.value().shape), 2500.0 - 2 * kPi * 4 * 10, 1e-6 * 2500);
    EXPECT_TRUE(grown.value().warnings.empty());
    // Вырез мимо всех тел — ошибка.
    auto miss = extrude(rect(50, 50, 60, 60), r.shape, cut);
    ASSERT_FALSE(miss.ok());
    EXPECT_NE(miss.error().message.find("does not intersect"), std::string::npos);
}
