// Вращение: объёмы по теореме Паппа — Гульдина, вырез вращением, ошибки.

#include "features_test_util.hpp"

#include <BRepPrimAPI_MakeCylinder.hxx>
#include <gtest/gtest.h>

using namespace ft;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Прямоугольник x ∈ [x0, x1], z ∈ [z0, z1] в плоскости y = 0.
Profile rectXZ(double x0, double z0, double x1, double z1) {
    BRepBuilderAPI_MakePolygon poly(gp_Pnt(x0, 0, z0), gp_Pnt(x1, 0, z0), gp_Pnt(x1, 0, z1), gp_Pnt(x0, 0, z1), true);
    return Profile{BRepBuilderAPI_MakeFace(poly.Wire(), true).Face(), gp_Ax3(gp::Origin(), gp::DY(), gp::DX())};
}

RevolveParams aroundZ(RevolveExtent e, double angle = 0.0) {
    RevolveParams p;
    p.axis = gp_Ax1(gp::Origin(), gp::DZ());
    p.extent = e;
    p.angle = angle;
    return p;
}

}  // namespace

TEST(Revolve, FullRevolutionPappusGuldinus) {
    // Кольцо: r ∈ [10, 15], h = 8. V = 2π · R̄ · A.
    auto r = revolve(rectXZ(10, 0, 15, 8), {}, aroundZ(RevolveExtent::Full));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(valid(r.value().shape));
    EXPECT_NEAR(volume(r.value().shape), 2 * kPi * 12.5 * 5 * 8, 1e-6);
    EXPECT_EQ(count(r.value().shape, TopAbs_SOLID), 1);
}

TEST(Revolve, TorusFromCircle) {
    BRepBuilderAPI_MakeWire w(BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(30, 0, 0), gp::DY()), 5.0)));
    const Profile p{BRepBuilderAPI_MakeFace(w.Wire()).Face(), gp_Ax3(gp::Origin(), gp::DY(), gp::DX())};
    auto r = revolve(p, {}, aroundZ(RevolveExtent::Full));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_NEAR(volume(r.value().shape), 2 * kPi * kPi * 30 * 25, 1e-4);
}

TEST(Revolve, PartialSymmetricAndTwoSided) {
    const Profile p = rectXZ(10, 0, 15, 8);
    const double full = 2 * kPi * 12.5 * 5 * 8;
    auto q = revolve(p, {}, aroundZ(RevolveExtent::Angle, kPi / 2));
    ASSERT_TRUE(q.ok()) << q.error().message;
    EXPECT_NEAR(volume(q.value().shape), full / 4, 1e-6);
    // Четверть в первом квадранте (вращение от +X к +Y): центр масс с y > 0.
    GProp_GProps g;
    BRepGProp::VolumeProperties(q.value().shape, g);
    EXPECT_GT(g.CentreOfMass().Y(), 1.0);

    auto s = revolve(p, {}, aroundZ(RevolveExtent::Symmetric, kPi / 2));
    ASSERT_TRUE(s.ok()) << s.error().message;
    EXPECT_NEAR(volume(s.value().shape), full / 4, 1e-6);
    GProp_GProps gs;
    BRepGProp::VolumeProperties(s.value().shape, gs);
    EXPECT_NEAR(gs.CentreOfMass().Y(), 0.0, 1e-6);

    RevolveParams two = aroundZ(RevolveExtent::TwoSided, kPi / 3);
    two.angle2 = kPi / 6;
    auto t = revolve(p, {}, two);
    ASSERT_TRUE(t.ok()) << t.error().message;
    EXPECT_NEAR(volume(t.value().shape), full / 4, 1e-6);
}

TEST(Revolve, CutGrooveInCylinder) {
    const TopoDS_Shape cyl = BRepPrimAPI_MakeCylinder(gp_Ax2(gp::Origin(), gp::DZ()), 20, 40).Shape();
    RevolveParams p = aroundZ(RevolveExtent::Full);
    p.mode = FeatureMode::Cut;
    auto r = revolve(rectXZ(15, 10, 25, 15), cyl, p);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(valid(r.value().shape));
    EXPECT_NEAR(volume(r.value().shape), kPi * 400 * 40 - kPi * (400 - 225) * 5, 1e-5);
}

TEST(Revolve, InvalidInputsAreRejected) {
    auto crossing = revolve(rectXZ(-5, 0, 5, 8), {}, aroundZ(RevolveExtent::Full));
    ASSERT_FALSE(crossing.ok());
    EXPECT_EQ(crossing.error().code, ErrorCode::InvalidInput);

    RevolveParams off = aroundZ(RevolveExtent::Full);
    off.axis = gp_Ax1(gp_Pnt(0, 5, 0), gp::DZ());  // ось вне плоскости профиля
    EXPECT_FALSE(revolve(rectXZ(10, 0, 15, 8), {}, off).ok());

    auto zero = revolve(rectXZ(10, 0, 15, 8), {}, aroundZ(RevolveExtent::Angle, 0.0));
    ASSERT_FALSE(zero.ok());
    EXPECT_EQ(zero.error().code, ErrorCode::InvalidInput);

    auto tooBig = revolve(rectXZ(10, 0, 15, 8), {}, aroundZ(RevolveExtent::Angle, 7.0));
    EXPECT_FALSE(tooBig.ok());
}
