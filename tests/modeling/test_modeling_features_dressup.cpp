// Скругление, фаска, оболочка, уклон граней — объёмы против аналитики и диагностика.

#include "features_test_util.hpp"

#include <gtest/gtest.h>

using namespace ft;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double a = 40, b = 30, c = 20;

// Ребро вдоль Z в углу (x, y).
TopoDS_Edge verticalEdge(const TopoDS_Shape& s, double x, double y) {
    return edgeThrough(s, gp_Pnt(x, y, 0), gp_Pnt(x, y, c));
}

}  // namespace

TEST(Fillet, SingleBoxEdgeVolume) {
    const TopoDS_Shape bx = box(a, b, c);
    const TopoDS_Edge e = verticalEdge(bx, a, b);
    ASSERT_FALSE(e.IsNull());
    const double r = 5;
    auto f = fillet(bx, {e}, r);
    ASSERT_TRUE(f.ok()) << f.error().message;
    EXPECT_TRUE(valid(f.value().shape));
    EXPECT_NEAR(volume(f.value().shape), a * b * c - (1 - kPi / 4) * r * r * c, 1e-5);
    EXPECT_EQ(count(f.value().shape, TopAbs_FACE), 7);
    // История: скругляющая грань построена из ребра, ребро удалено.
    const auto& gen = f.value().history->Generated(e);
    ASSERT_EQ(gen.Size(), 1);
    EXPECT_EQ(BRepAdaptor_Surface(TopoDS::Face(gen.First())).GetType(), GeomAbs_Cylinder);
    EXPECT_TRUE(f.value().history->IsRemoved(e));
}

TEST(Fillet, FourParallelEdges) {
    const TopoDS_Shape bx = box(a, b, c);
    std::vector<TopoDS_Shape> edges = {verticalEdge(bx, 0, 0), verticalEdge(bx, a, 0), verticalEdge(bx, a, b),
                                       verticalEdge(bx, 0, b)};
    const double r = 3;
    auto f = fillet(bx, edges, r);
    ASSERT_TRUE(f.ok()) << f.error().message;
    EXPECT_NEAR(volume(f.value().shape), a * b * c - 4 * (1 - kPi / 4) * r * r * c, 1e-5);
}

TEST(Fillet, TooLargeRadiusIsReportedWithEdgeIndex) {
    const TopoDS_Shape bx = box(a, b, c);
    std::vector<TopoDS_Shape> edges = {verticalEdge(bx, 0, 0), edgeThrough(bx, gp_Pnt(0, 0, c), gp_Pnt(a, 0, c))};
    auto f = fillet(bx, edges, 25.0);  // больше высоты 20 у второго ребра и половины ширины
    ASSERT_FALSE(f.ok());
    EXPECT_EQ(f.error().code, ErrorCode::OperationFailed);
    EXPECT_NE(f.error().message.find("failed edges"), std::string::npos) << f.error().message;

    auto diag = filletFailures(bx, edges, 25.0);
    ASSERT_TRUE(diag.ok()) << diag.error().message;
    EXPECT_FALSE(diag.value().empty());
    for (const auto& d : diag.value()) EXPECT_LT(d.index, edges.size());

    auto good = filletFailures(bx, {edges[0]}, 2.0);
    ASSERT_TRUE(good.ok());
    EXPECT_TRUE(good.value().empty());
}

TEST(Fillet, InvalidInputsAreRejected) {
    const TopoDS_Shape bx = box(a, b, c);
    const TopoDS_Shape other = box(5, 5, 5);
    auto foreign = fillet(bx, {edgeThrough(other, gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 5))}, 1.0);
    ASSERT_FALSE(foreign.ok());
    EXPECT_EQ(foreign.error().code, ErrorCode::InvalidInput);
    auto zero = fillet(bx, {verticalEdge(bx, 0, 0)}, 0.0);
    ASSERT_FALSE(zero.ok());
    EXPECT_EQ(zero.error().code, ErrorCode::InvalidInput);
    EXPECT_FALSE(fillet(bx, {}, 1.0).ok());
}

TEST(Chamfer, EqualTwoDistancesAndDistanceAngle) {
    const TopoDS_Shape bx = box(a, b, c);
    const TopoDS_Edge e = verticalEdge(bx, a, b);
    // Опорная грань x = a (ширина по y = b).
    const TopoDS_Face ref = planeFaceAt(bx, 0, a);
    ASSERT_FALSE(ref.IsNull());

    ChamferParams eq;
    eq.distance = 4;
    auto r1 = chamfer(bx, {e}, eq);
    ASSERT_TRUE(r1.ok()) << r1.error().message;
    EXPECT_NEAR(volume(r1.value().shape), a * b * c - 4.0 * 4.0 / 2 * c, 1e-6);

    ChamferParams two;
    two.type = ChamferType::TwoDistances;
    two.distance = 3;
    two.distance2 = 6;
    two.referenceFace = ref;
    auto r2 = chamfer(bx, {e}, two);
    ASSERT_TRUE(r2.ok()) << r2.error().message;
    EXPECT_NEAR(volume(r2.value().shape), a * b * c - 3.0 * 6.0 / 2 * c, 1e-6);
    // На опорной грани срезано distance: её ширина b - 3.
    EXPECT_NEAR(area(planeFaceAt(r2.value().shape, 0, a)), (b - 3) * c, 1e-6);

    ChamferParams da;
    da.type = ChamferType::DistanceAngle;
    da.distance = 3;
    da.angle = 60.0 * kPi / 180;
    da.referenceFace = ref;
    auto r3 = chamfer(bx, {e}, da);
    ASSERT_TRUE(r3.ok()) << r3.error().message;
    const double other = 3 * std::tan(da.angle);
    EXPECT_NEAR(volume(r3.value().shape), a * b * c - 3 * other / 2 * c, 1e-5);
    EXPECT_NEAR(area(planeFaceAt(r3.value().shape, 0, a)), (b - 3) * c, 1e-6);

    ChamferParams bad = two;
    bad.referenceFace = planeFaceAt(bx, 0, 0);  // не примыкает к ребру
    auto r4 = chamfer(bx, {e}, bad);
    ASSERT_FALSE(r4.ok());
    EXPECT_EQ(r4.error().code, ErrorCode::InvalidInput);
}

TEST(Shell, InwardAndOutwardThickness) {
    const TopoDS_Shape bx = box(a, b, c);
    const TopoDS_Face top = planeFaceAt(bx, 2, c);
    const double t = 2;
    auto in = shell(bx, {top}, t, ShellDirection::Inward);
    ASSERT_TRUE(in.ok()) << in.error().message;
    EXPECT_TRUE(valid(in.value().shape));
    EXPECT_NEAR(volume(in.value().shape), a * b * c - (a - 2 * t) * (b - 2 * t) * (c - t), 1e-4);
    // Удалённая грань переходит в торец стенок (кольцо ab - (a-2t)(b-2t)).
    const auto& rim = in.value().history->Modified(top);
    ASSERT_EQ(rim.Size(), 1);
    EXPECT_NEAR(area(rim.First()), a * b - (a - 2 * t) * (b - 2 * t), 1e-6);
    EXPECT_FALSE(in.value().history->IsRemoved(top));

    auto out = shell(bx, {top}, t, ShellDirection::Outward);
    ASSERT_TRUE(out.ok()) << out.error().message;
    EXPECT_NEAR(volume(out.value().shape), (a + 2 * t) * (b + 2 * t) * (c + t) - a * b * c, 1e-4);

    auto zero = shell(bx, {top}, 0.0, ShellDirection::Inward);
    EXPECT_FALSE(zero.ok());
}

TEST(Draft, SideFacesGiveFrustum) {
    const double s = 20, h = 10, angle = 5.0 * kPi / 180;
    const TopoDS_Shape bx = box(s, s, h);
    std::vector<TopoDS_Shape> sides = {planeFaceAt(bx, 0, 0), planeFaceAt(bx, 0, s), planeFaceAt(bx, 1, 0),
                                       planeFaceAt(bx, 1, s)};
    auto r = draftFaces(bx, sides, gp::DZ(), angle, gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()));
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_TRUE(valid(r.value().shape));
    const double top = s - 2 * h * std::tan(angle);
    const double a1 = s * s, a2 = top * top;
    EXPECT_NEAR(volume(r.value().shape), h / 3 * (a1 + a2 + std::sqrt(a1 * a2)), 1e-5);
    EXPECT_EQ(r.value().history->Modified(sides[0]).Size(), 1);

    auto zero = draftFaces(bx, sides, gp::DZ(), 0.0, gp_Ax3());
    EXPECT_FALSE(zero.ok());
}
