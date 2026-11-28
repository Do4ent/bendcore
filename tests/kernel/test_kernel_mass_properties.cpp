// Mass properties against closed-form formulas (docs/kernel/ANALYSIS.md). Units: mm, kg,
// kg*mm^2. The roadmap requires mass within 0.1 %; the analytic bodies here are expected to
// match to ~1e-9 relative, so the bounds below are much tighter than that requirement.

#include <bendcore/kernel/mass_properties.hpp>

#include <gtest/gtest.h>

#include <cmath>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Pln.hxx>

using namespace bendcore::kernel;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kSteel = 7850.0;           // kg/m^3
constexpr double kAl = 2700.0;
constexpr double kPerMm3 = 1.0e-9;          // m^3 per mm^3
constexpr double kRel = 1.0e-7;             // relative tolerance for analytic bodies

void expectRel(double actual, double expected, double rel = kRel) {
    EXPECT_NEAR(actual, expected, std::abs(expected) * rel + 1.0e-12) << "expected " << expected;
}

void expectTensor(const Matrix3& actual, const Matrix3& expected, double scale, double rel = kRel) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            EXPECT_NEAR(actual[i][j], expected[i][j], scale * rel) << "component " << i << j;
}

Matrix3 diag(double a, double b, double c) { return Matrix3{{{a, 0, 0}, {0, b, 0}, {0, 0, c}}}; }

Shape box(double x, double y, double z, gp_Pnt corner = gp_Pnt(0, 0, 0)) {
    return Shape(BRepPrimAPI_MakeBox(corner, x, y, z).Shape());
}

Shape moved(const Shape& s, const gp_Trsf& t) { return Shape(BRepBuilderAPI_Transform(s.occt(), t, true).Shape()); }

// Mass within the roadmap bound (0.1 %) - stated explicitly for every body.
void expectMassWithinRoadmap(double mass, double expected) {
    EXPECT_LE(std::abs(mass - expected) / expected, 1.0e-3);
}

}  // namespace

TEST(MassProperties, BoxAgainstFormulas) {
    const double a = 100, b = 50, c = 20;
    auto r = computeMassProperties(box(a, b, c), kSteel);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const MassProperties& p = r.value();
    const double m = a * b * c * kSteel * kPerMm3;  // 0.785 kg
    expectMassWithinRoadmap(p.mass, m);
    expectRel(p.mass, m);
    expectRel(p.volume, a * b * c);
    expectRel(p.area, 2 * (a * b + b * c + c * a));
    EXPECT_NEAR(p.centerOfMass[0], a / 2, 1e-9);
    EXPECT_NEAR(p.centerOfMass[1], b / 2, 1e-9);
    EXPECT_NEAR(p.centerOfMass[2], c / 2, 1e-9);
    const double ixx = m * (b * b + c * c) / 12, iyy = m * (a * a + c * c) / 12, izz = m * (a * a + b * b) / 12;
    expectTensor(p.inertiaAtCenter, diag(ixx, iyy, izz), izz);
    EXPECT_EQ(p.bodyCount, 1u);

    // Principal moments ascending with axes along X, Y, Z.
    expectRel(p.principal.moments[0], ixx);
    expectRel(p.principal.moments[1], iyy);
    expectRel(p.principal.moments[2], izz);
    EXPECT_NEAR(std::abs(p.principal.axes[0][0]), 1.0, 1e-9);
    EXPECT_NEAR(std::abs(p.principal.axes[1][1]), 1.0, 1e-9);
    EXPECT_NEAR(std::abs(p.principal.axes[2][2]), 1.0, 1e-9);

    // About the corner (origin) frame: I = m/3 (b^2 + c^2), products -m a b / 4 etc.
    const Matrix3 corner = inertiaAboutFrame(p, gp_Ax3(gp::Origin(), gp::DZ(), gp::DX()));
    const Matrix3 expected{{{m * (b * b + c * c) / 3, -m * a * b / 4, -m * a * c / 4},
                            {-m * a * b / 4, m * (a * a + c * c) / 3, -m * b * c / 4},
                            {-m * a * c / 4, -m * b * c / 4, m * (a * a + b * b) / 3}}};
    expectTensor(corner, expected, expected[2][2]);
}

TEST(MassProperties, RotatedBoxTensorFollowsRotation) {
    // A box rotated by 30 degrees about Z: I = R diag(Ixx, Iyy, Izz) R^T. This pins the sign
    // convention of the products of inertia to the tensor definition (Ixy = -sum x y dm).
    const double a = 80, b = 20, c = 10;
    const double phi = kPi / 6;
    gp_Trsf rot;
    rot.SetRotation(gp_Ax1(gp::Origin(), gp::DZ()), phi);
    auto r = computeMassProperties(moved(box(a, b, c, gp_Pnt(-a / 2, -b / 2, -c / 2)), rot), kSteel);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const double m = a * b * c * kSteel * kPerMm3;
    const double i1 = m * (b * b + c * c) / 12, i2 = m * (a * a + c * c) / 12, i3 = m * (a * a + b * b) / 12;
    const double cs = std::cos(phi), sn = std::sin(phi);
    const Matrix3 expected{{{i1 * cs * cs + i2 * sn * sn, (i1 - i2) * cs * sn, 0},
                            {(i1 - i2) * cs * sn, i1 * sn * sn + i2 * cs * cs, 0},
                            {0, 0, i3}}};
    expectTensor(r.value().inertiaAtCenter, expected, i3);
    // Smallest principal moment belongs to the long edge direction (cos phi, sin phi, 0).
    const Vector3& ax = r.value().principal.axes[0];
    EXPECT_NEAR(std::abs(ax[0] * cs + ax[1] * sn), 1.0, 1e-9);
    expectRel(r.value().principal.moments[0], i1);
}

TEST(MassProperties, Cylinder) {
    const double rad = 10, h = 40;
    auto r = computeMassProperties(Shape(BRepPrimAPI_MakeCylinder(rad, h).Shape()), kAl);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const double v = kPi * rad * rad * h;
    const double m = v * kAl * kPerMm3;
    expectMassWithinRoadmap(r.value().mass, m);
    expectRel(r.value().volume, v);
    expectRel(r.value().area, 2 * kPi * rad * h + 2 * kPi * rad * rad);
    EXPECT_NEAR(r.value().centerOfMass[2], h / 2, 1e-9);
    const double ixx = m * (3 * rad * rad + h * h) / 12, izz = m * rad * rad / 2;
    expectTensor(r.value().inertiaAtCenter, diag(ixx, ixx, izz), ixx);
}

TEST(MassProperties, HollowCylinder) {
    const double ro = 20, ri = 12, h = 30;
    BRepAlgoAPI_Cut cut(BRepPrimAPI_MakeCylinder(ro, h).Shape(), BRepPrimAPI_MakeCylinder(ri, h).Shape());
    ASSERT_TRUE(cut.IsDone());
    auto r = computeMassProperties(Shape(cut.Shape()), kSteel);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const double v = kPi * (ro * ro - ri * ri) * h;
    const double m = v * kSteel * kPerMm3;
    expectMassWithinRoadmap(r.value().mass, m);
    expectRel(r.value().volume, v);
    expectRel(r.value().area, 2 * kPi * (ro + ri) * h + 2 * kPi * (ro * ro - ri * ri));
    const double izz = m * (ro * ro + ri * ri) / 2;
    const double ixx = m * (3 * (ro * ro + ri * ri) + h * h) / 12;
    expectTensor(r.value().inertiaAtCenter, diag(ixx, ixx, izz), izz);
}

TEST(MassProperties, Sphere) {
    const double rad = 25;
    auto r = computeMassProperties(Shape(BRepPrimAPI_MakeSphere(gp_Pnt(5, -7, 9), rad).Shape()), kSteel);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const double v = 4.0 / 3.0 * kPi * rad * rad * rad;
    const double m = v * kSteel * kPerMm3;
    expectMassWithinRoadmap(r.value().mass, m);
    expectRel(r.value().volume, v);
    expectRel(r.value().area, 4 * kPi * rad * rad);
    EXPECT_NEAR(r.value().centerOfMass[0], 5, 1e-7);
    EXPECT_NEAR(r.value().centerOfMass[1], -7, 1e-7);
    EXPECT_NEAR(r.value().centerOfMass[2], 9, 1e-7);
    const double i = 0.4 * m * rad * rad;
    expectTensor(r.value().inertiaAtCenter, diag(i, i, i), i);
}

TEST(MassProperties, TwoBodiesOfDifferentMaterials) {
    // Steel cube 20^3 at the origin, aluminium cube 20^3 at x = 40.
    const double s = 20;
    std::vector<Body> bodies{{box(s, s, s), kSteel}, {box(s, s, s, gp_Pnt(40, 0, 0)), kAl}};
    auto r = computeMassProperties(bodies);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const double m1 = s * s * s * kSteel * kPerMm3, m2 = s * s * s * kAl * kPerMm3;
    const double mt = m1 + m2;
    expectMassWithinRoadmap(r.value().mass, mt);
    expectRel(r.value().mass, mt);
    expectRel(r.value().volume, 2 * s * s * s);
    expectRel(r.value().area, 12 * s * s);
    EXPECT_EQ(r.value().bodyCount, 2u);
    const double xc = (m1 * 10 + m2 * 50) / mt;
    EXPECT_NEAR(r.value().centerOfMass[0], xc, 1e-9);
    EXPECT_NEAR(r.value().centerOfMass[1], 10, 1e-9);
    // Steiner: own I = m s^2 / 6 for a cube; offsets along X only.
    const double d1 = 10 - xc, d2 = 50 - xc;
    const double iown = (m1 + m2) * s * s / 6;
    const double ixx = iown;
    const double iyy = iown + m1 * d1 * d1 + m2 * d2 * d2;
    expectTensor(r.value().inertiaAtCenter, diag(ixx, iyy, iyy), iyy);
}

TEST(MassProperties, CompoundOfTwoSolidsWithOneDensity) {
    BRep_Builder builder;
    TopoDS_Compound c;
    builder.MakeCompound(c);
    builder.Add(c, BRepPrimAPI_MakeBox(10, 10, 10).Shape());
    builder.Add(c, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(50, 0, 0), gp::DZ()), 5, 10).Shape());
    auto r = computeMassProperties(Shape(c), kSteel);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const double v = 1000 + kPi * 25 * 10;
    expectRel(r.value().volume, v);
    expectRel(r.value().mass, v * kSteel * kPerMm3);
    EXPECT_EQ(r.value().bodyCount, 2u);
    // Centre of mass along X: (1000*5 + Vc*50) / V.
    EXPECT_NEAR(r.value().centerOfMass[0], (1000 * 5 + kPi * 250 * 50) / v, 1e-9);
}

TEST(MassProperties, AssemblyOfTwoOffsetOccurrencesFollowsSteiner) {
    // One part (box 40 x 20 x 10 centred at its own origin), placed twice: translated by
    // (100, 0, 0) and rotated 90 degrees about Z then translated by (0, 60, 0).
    const double a = 40, b = 20, c = 10;
    auto part = computeMassProperties(box(a, b, c, gp_Pnt(-a / 2, -b / 2, -c / 2)), kSteel);
    ASSERT_TRUE(part.ok());
    gp_Trsf t1;
    t1.SetTranslation(gp_Vec(100, 0, 0));
    gp_Trsf rot, shift;
    rot.SetRotation(gp_Ax1(gp::Origin(), gp::DZ()), kPi / 2);
    shift.SetTranslation(gp_Vec(0, 60, 0));
    const gp_Trsf t2 = shift * rot;
    auto total = combine(std::vector<Occurrence>{{part.value(), t1}, {part.value(), t2}});
    ASSERT_TRUE(total.ok()) << total.error().message;

    const double m = a * b * c * kSteel * kPerMm3;
    expectMassWithinRoadmap(total.value().mass, 2 * m);
    EXPECT_NEAR(total.value().centerOfMass[0], 50, 1e-9);
    EXPECT_NEAR(total.value().centerOfMass[1], 30, 1e-9);
    EXPECT_NEAR(total.value().centerOfMass[2], 0, 1e-9);
    // Own tensors: first as is, second with a and b swapped. Offsets d1 = (50,-30,0),
    // d2 = (-50,30,0): Steiner adds m (d^2 E - d d^T) for each.
    const double ixx1 = m * (b * b + c * c) / 12, iyy1 = m * (a * a + c * c) / 12, izz = m * (a * a + b * b) / 12;
    const double dx = 50, dy = 30;
    const Matrix3 expected{{{ixx1 + iyy1 + 2 * m * dy * dy, 2 * m * dx * dy, 0},
                            {2 * m * dx * dy, iyy1 + ixx1 + 2 * m * dx * dx, 0},
                            {0, 0, 2 * izz + 2 * m * (dx * dx + dy * dy)}}};
    expectTensor(total.value().inertiaAtCenter, expected, expected[2][2]);

    // The same assembly from moved shapes through BRepGProp directly.
    BRep_Builder builder;
    TopoDS_Compound comp;
    builder.MakeCompound(comp);
    const Shape base = box(a, b, c, gp_Pnt(-a / 2, -b / 2, -c / 2));
    builder.Add(comp, moved(base, t1).occt());
    builder.Add(comp, moved(base, t2).occt());
    GProp_GProps g;
    BRepGProp::VolumeProperties(comp, g);
    const gp_Mat gm = g.MatrixOfInertia();
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            EXPECT_NEAR(total.value().inertiaAtCenter[i][j], gm.Value(i + 1, j + 1) * kSteel * kPerMm3,
                        expected[2][2] * kRel);
}

TEST(MassProperties, InertiaAboutRotatedFrame) {
    // Cylinder axis Z; frame with origin at the base centre and Z' along global X.
    const double rad = 10, h = 40;
    auto r = computeMassProperties(Shape(BRepPrimAPI_MakeCylinder(rad, h).Shape()), kSteel);
    ASSERT_TRUE(r.ok());
    const double m = kPi * rad * rad * h * kSteel * kPerMm3;
    const Matrix3 t = inertiaAboutFrame(r.value(), gp_Ax3(gp::Origin(), gp::DX(), gp::DY()));
    // Frame axes: X' = global Y, Y' = global Z, Z' = global X.
    const double iBase = m * (3 * rad * rad + h * h) / 12 + m * (h / 2) * (h / 2);  // about a base diameter
    expectRel(t[0][0], iBase);           // about global Y through the base centre
    expectRel(t[1][1], m * rad * rad / 2);  // about the cylinder axis
    expectRel(t[2][2], iBase);
    EXPECT_NEAR(t[0][1], 0.0, iBase * kRel);
}

TEST(MassProperties, MaterialOverloadUsesDensity) {
    const Material* s235 = MaterialLibrary::withBuiltIns().find("steel_s235");
    ASSERT_NE(s235, nullptr);
    auto r = computeMassProperties(box(10, 10, 10), *s235);
    ASSERT_TRUE(r.ok());
    expectRel(r.value().mass, 1000 * 7850 * kPerMm3);
}

TEST(MassProperties, Errors) {
    EXPECT_EQ(computeMassProperties(Shape(), kSteel).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(computeMassProperties(box(1, 1, 1), 0.0).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(computeMassProperties(box(1, 1, 1), -5.0).error().code, ErrorCode::InvalidInput);
    const TopoDS_Shape face = BRepBuilderAPI_MakeFace(gp_Pln(), 0, 1, 0, 1).Shape();
    EXPECT_EQ(computeMassProperties(Shape(face), kSteel).error().code, ErrorCode::InvalidInput);
    // Inside-out solid: negative volume.
    auto reversed = computeMassProperties(Shape(box(1, 1, 1).occt().Reversed()), kSteel);
    ASSERT_FALSE(reversed.ok());
    EXPECT_EQ(reversed.error().code, ErrorCode::InvalidShape);
    // Scaled placement changes the mass and is refused.
    auto part = computeMassProperties(box(1, 1, 1), kSteel);
    gp_Trsf scale;
    scale.SetScale(gp::Origin(), 2.0);
    EXPECT_EQ(transformed(part.value(), scale).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(combine(std::vector<Occurrence>{}).error().code, ErrorCode::InvalidInput);
}
