// Выдавливание: объёмы против аналитики, условия окончания, уклон, тонкостенное, ошибки.

#include "features_test_util.hpp"

#include <BRep_Builder.hxx>
#include <Geom_Plane.hxx>
#include <gtest/gtest.h>

using namespace ft;

namespace {

constexpr double kPi = 3.14159265358979323846;

}  // namespace

TEST(Extrude, BlindBossWithHoleVolumeAndTopology) {
    const Profile p = rect(0, 0, 40, 30, 0, 5, 20, 15);
    ExtrudeParams ep;
    ep.distance = 10;
    auto r = extrude(p, {}, ep);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(valid(r.value().shape));
    EXPECT_NEAR(volume(r.value().shape), (40 * 30 - kPi * 25) * 10, 1e-6);
    EXPECT_EQ(count(r.value().shape, TopAbs_FACE), 7);  // 4 стороны, 2 торца, цилиндр
    EXPECT_EQ(count(r.value().shape, TopAbs_SOLID), 1);
    EXPECT_FALSE(r.value().tool.IsNull());
}

TEST(Extrude, SymmetricAndTwoSidedExtents) {
    const Profile p = rect(0, 0, 10, 20);
    ExtrudeParams sym;
    sym.extent = ExtentType::Symmetric;
    sym.distance = 10;
    auto s = extrude(p, {}, sym);
    ASSERT_TRUE(s.ok()) << s.error().message;
    EXPECT_NEAR(volume(s.value().shape), 2000.0, 1e-6);
    EXPECT_FALSE(planeFaceAt(s.value().shape, 2, -5).IsNull());
    EXPECT_FALSE(planeFaceAt(s.value().shape, 2, 5).IsNull());

    ExtrudeParams two;
    two.extent = ExtentType::TwoSided;
    two.distance = 6;
    two.distance2 = 4;
    auto t = extrude(p, {}, two);
    ASSERT_TRUE(t.ok()) << t.error().message;
    EXPECT_NEAR(volume(t.value().shape), 2000.0, 1e-6);
    EXPECT_FALSE(planeFaceAt(t.value().shape, 2, -4).IsNull());
    EXPECT_FALSE(planeFaceAt(t.value().shape, 2, 6).IsNull());

    ExtrudeParams rev;
    rev.distance = 7;
    rev.reversed = true;
    auto rv = extrude(p, {}, rev);
    ASSERT_TRUE(rv.ok()) << rv.error().message;
    EXPECT_FALSE(planeFaceAt(rv.value().shape, 2, -7).IsNull());
}

TEST(Extrude, DraftGivesFrustumVolume) {
    const double a = 20, h = 10, angle = 5.0 * kPi / 180.0;
    const Profile p = rect(0, 0, a, a);
    for (DraftDirection d : {DraftDirection::Inward, DraftDirection::Outward}) {
        ExtrudeParams ep;
        ep.distance = h;
        ep.draftAngle = angle;
        ep.draftDirection = d;
        auto r = extrude(p, {}, ep);
        ASSERT_TRUE(r.ok()) << r.error().message;
        EXPECT_TRUE(valid(r.value().shape));
        const double top = d == DraftDirection::Inward ? a - 2 * h * std::tan(angle) : a + 2 * h * std::tan(angle);
        const double a1 = a * a, a2 = top * top;
        EXPECT_NEAR(volume(r.value().shape), h / 3 * (a1 + a2 + std::sqrt(a1 * a2)), 1e-5);
        const TopoDS_Face topFace = planeFaceAt(r.value().shape, 2, h);
        ASSERT_FALSE(topFace.IsNull());
        EXPECT_NEAR(area(topFace), a2, 1e-6);
    }
}

TEST(Extrude, DraftedCylinderIsConeFrustum) {
    BRepBuilderAPI_MakeWire w(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp::Origin(), gp::DZ()), 10.0)));
    const Profile p{BRepBuilderAPI_MakeFace(w.Wire()).Face(), gp_Ax3(gp::Origin(), gp::DZ(), gp::DX())};
    const double h = 20, angle = 10.0 * kPi / 180.0;
    ExtrudeParams ep;
    ep.distance = h;
    ep.draftAngle = angle;
    auto r = extrude(p, {}, ep);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const double R = 10, rr = 10 - h * std::tan(angle);
    EXPECT_NEAR(volume(r.value().shape), kPi * h / 3 * (R * R + R * rr + rr * rr), 1e-4);
}

TEST(Extrude, ThinWallInsideAndOutside) {
    const Profile p = rect(0, 0, 20, 20);
    ExtrudeParams ep;
    ep.distance = 10;
    ep.thin = ThinWall{2.0, ThinSide::Inside};
    auto in = extrude(p, {}, ep);
    ASSERT_TRUE(in.ok()) << in.error().message;
    EXPECT_TRUE(valid(in.value().shape));
    EXPECT_NEAR(volume(in.value().shape), (400.0 - 256.0) * 10, 1e-5);
    ep.thin = ThinWall{2.0, ThinSide::Outside};
    auto out = extrude(p, {}, ep);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_NEAR(volume(out.value().shape), (576.0 - 400.0) * 10, 1e-5);
}

TEST(Extrude, BlindCutFromBox) {
    const TopoDS_Shape b = box(100, 60, 20);
    ExtrudeParams ep;
    ep.mode = FeatureMode::Cut;
    ep.distance = 5;
    ep.reversed = true;
    auto r = extrude(rect(10, 10, 40, 30, 20), b, ep);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(valid(r.value().shape));
    EXPECT_NEAR(volume(r.value().shape), 100 * 60 * 20 - 30 * 20 * 5, 1e-6);
    EXPECT_FALSE(planeFaceAt(r.value().shape, 2, 15).IsNull());  // дно кармана
}

TEST(Extrude, ThroughAllCut) {
    const TopoDS_Shape b = box(100, 60, 20);
    ExtrudeParams ep;
    ep.mode = FeatureMode::Cut;
    ep.extent = ExtentType::ThroughAll;
    ep.reversed = true;
    // Профиль выше тела: вырез проходит всю толщину.
    auto r = extrude(rect(10, 10, 20, 20, 35), b, ep);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_NEAR(volume(r.value().shape), 100 * 60 * 20 - 100 * 20, 1e-6);
    EXPECT_EQ(count(r.value().shape, TopAbs_FACE), 10);

    // Через всё в обе стороны из средней плоскости.
    ExtrudeParams both;
    both.mode = FeatureMode::Cut;
    both.extent = ExtentType::ThroughAllBoth;
    auto r2 = extrude(rect(50, 10, 60, 20, 10), b, both);
    ASSERT_TRUE(r2.ok()) << r2.error().message;
    EXPECT_NEAR(volume(r2.value().shape), 100 * 60 * 20 - 100 * 20, 1e-6);

    // Тело позади плоскости эскиза — явная ошибка.
    ExtrudeParams behind;
    behind.mode = FeatureMode::Cut;
    behind.extent = ExtentType::ThroughAll;
    auto r3 = extrude(rect(10, 10, 20, 20, 35), b, behind);
    ASSERT_FALSE(r3.ok());
    EXPECT_EQ(r3.error().code, ErrorCode::InvalidInput);
}

TEST(Extrude, UpToFaceOfTarget) {
    const TopoDS_Shape b = box(50, 50, 10);
    const TopoDS_Face top = planeFaceAt(b, 2, 10);
    ASSERT_FALSE(top.IsNull());
    ExtrudeParams ep;
    ep.extent = ExtentType::UpToFace;
    ep.limit = top;
    ep.reversed = true;
    auto r = extrude(rect(10, 10, 20, 20, 40), b, ep);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(valid(r.value().shape));
    EXPECT_NEAR(volume(r.value().shape), 50 * 50 * 10 + 100 * 30, 1e-6);
    EXPECT_EQ(count(r.value().shape, TopAbs_SOLID), 1);
}

TEST(Extrude, UpToInclinedSurface) {
    // Плоскость z = 20 + 0.5 x, задана маленькой гранью вдали от профиля.
    const gp_Dir n(-0.5, 0.0, 1.0);
    const gp_Pln pln(gp_Pnt(0, 0, 20), n);
    const TopoDS_Face small = BRepBuilderAPI_MakeFace(pln, 200, 210, 200, 210).Face();
    ExtrudeParams ep;
    ep.extent = ExtentType::UpToSurface;
    ep.limit = small;
    auto r = extrude(rect(0, 0, 10, 10), {}, ep);
    ASSERT_TRUE(r.ok()) << r.error().message;
    // V = ∫∫ (20 + 0.5x) dx dy по квадрату 10×10.
    EXPECT_NEAR(volume(r.value().shape), 10 * (200 + 25), 1e-5);

    // До грани (без продолжения): грань не перекрывает профиль — ошибка.
    ep.extent = ExtentType::UpToFace;
    auto bad = extrude(rect(0, 0, 10, 10), {}, ep);
    ASSERT_FALSE(bad.ok());
    EXPECT_EQ(bad.error().code, ErrorCode::InvalidInput);

    // Грань, перекрывающая профиль, — до грани работает так же.
    ep.limit = BRepBuilderAPI_MakeFace(pln, -50, 50, -50, 50).Face();
    auto ok = extrude(rect(0, 0, 10, 10), {}, ep);
    ASSERT_TRUE(ok.ok()) << ok.error().message;
    EXPECT_NEAR(volume(ok.value().shape), 2250.0, 1e-5);
}

TEST(Extrude, InvalidInputsAreRejected) {
    const Profile p = rect(0, 0, 10, 10);
    ExtrudeParams zero;
    zero.distance = 0.0;
    auto r0 = extrude(p, {}, zero);
    ASSERT_FALSE(r0.ok());
    EXPECT_EQ(r0.error().code, ErrorCode::InvalidInput);

    // Незамкнутый контур: три стороны квадрата.
    BRepBuilderAPI_MakePolygon open(gp_Pnt(0, 0, 0), gp_Pnt(10, 0, 0), gp_Pnt(10, 10, 0), gp_Pnt(0, 10, 0), false);
    TopoDS_Face face;
    BRep_Builder bb;
    bb.MakeFace(face, new Geom_Plane(gp_Pln(gp::Origin(), gp::DZ())), 1e-7);
    bb.Add(face, open.Wire());
    auto r1 = extrude(Profile{face, gp_Ax3(gp::Origin(), gp::DZ(), gp::DX())}, {}, ExtrudeParams{});
    ASSERT_FALSE(r1.ok());
    EXPECT_EQ(r1.error().code, ErrorCode::InvalidInput);
    EXPECT_NE(r1.error().message.find("not closed"), std::string::npos);

    // Плоскость эскиза не совпадает с гранью.
    Profile shifted = p;
    shifted.placement = gp_Ax3(gp_Pnt(0, 0, 3), gp::DZ(), gp::DX());
    EXPECT_FALSE(extrude(shifted, {}, ExtrudeParams{}).ok());

    ExtrudeParams cut;
    cut.mode = FeatureMode::Cut;
    auto r2 = extrude(p, {}, cut);
    ASSERT_FALSE(r2.ok());
    EXPECT_EQ(r2.error().code, ErrorCode::InvalidInput);

    // Вырез, не задевающий тело.
    auto r3 = extrude(rect(200, 200, 210, 210), box(10, 10, 10), cut);
    ASSERT_FALSE(r3.ok());
    EXPECT_EQ(r3.error().code, ErrorCode::OperationFailed);

    ExtrudeParams symDraft;
    symDraft.extent = ExtentType::Symmetric;
    symDraft.draftAngle = 0.1;
    auto r4 = extrude(p, {}, symDraft);
    ASSERT_FALSE(r4.ok());
    EXPECT_EQ(r4.error().code, ErrorCode::Unsupported);
}
