// Отверстие: удалённый объём простого, глухого с конусом 118°, цековки, зенковки.

#include "features_test_util.hpp"

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <gp_Trsf.hxx>

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

namespace {

// Внешняя нормаль плоской грани, вычисленная независимо от hole(): от центра масс тела к
// центру грани (для выпуклого тела направлена из материала).
gp_Dir outwardFromCentroid(const TopoDS_Shape& body, const TopoDS_Face& f) {
    GProp_GProps g;
    BRepGProp::VolumeProperties(body, g);
    BRepAdaptor_Surface a(f, false);
    gp_Dir n = a.Plane().Axis().Direction();
    if (gp_Vec(g.CentreOfMass(), faceCentroid(f)).Dot(gp_Vec(n)) < 0) n.Reverse();
    return n;
}

// Глухое отверстие без конуса на каждой плоской грани тела: удалённый объём π r² h (сверлит
// внутрь, а не наружу, где удалённый объём был бы 0) и явное направление «внутрь» принимается,
// «наружу» — отклоняется. Возвращает число граней с левой СК плоскости.
int checkEveryPlanarFace(const TopoDS_Shape& body) {
    const double v0 = volume(body);
    HoleParams hp;
    hp.diameter = 6;
    hp.depth = 8;
    hp.drillPoint = false;
    int leftHanded = 0;
    for (TopExp_Explorer ex(body, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face f = TopoDS::Face(ex.Current());
        BRepAdaptor_Surface a(f, false);
        if (a.GetType() != GeomAbs_Plane) continue;
        if (!a.Plane().Position().Direct()) ++leftHanded;
        const gp_Pnt c = faceCentroid(f);
        auto r = hole(body, f, c, hp);
        EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
        if (!r.ok()) continue;
        EXPECT_TRUE(valid(r.value().shape));
        EXPECT_NEAR(v0 - volume(r.value().shape), kPi * 9 * 8, 1e-6) << "face centre " << c.X() << " " << c.Y() << " " << c.Z();
        const gp_Dir out = outwardFromCentroid(body, f);
        auto explicitIn = hole(body, f, c, hp, out.Reversed());
        EXPECT_TRUE(explicitIn.ok()) << (explicitIn.ok() ? "" : explicitIn.error().message);
        if (explicitIn.ok()) EXPECT_NEAR(v0 - volume(explicitIn.value().shape), kPi * 9 * 8, 1e-6);
        auto explicitOut = hole(body, f, c, hp, out);
        EXPECT_FALSE(explicitOut.ok());
    }
    return leftHanded;
}

}  // namespace

TEST(Hole, DrillsInwardOnLeftHandedPlaneFaces) {
    // Зеркальное отражение коробки: плоскости граней получают левую СК (Du x Dv = -ось);
    // раньше нормаль бралась по оси gp_Pln, и отверстие строилось снаружи тела.
    gp_Trsf mirror;
    mirror.SetMirror(gp_Ax2(gp::Origin(), gp::DX()));
    const TopoDS_Shape mirrored = BRepBuilderAPI_Transform(box(100, 100, 50), mirror, true).Shape();
    ASSERT_TRUE(valid(mirrored));
    EXPECT_EQ(checkEveryPlanarFace(mirrored), 6);
    // Без отражения (правая СК, часть граней REVERSED) — то же.
    EXPECT_EQ(checkEveryPlanarFace(box(100, 100, 50)), 0);
}

TEST(Hole, DrillsInwardOnRevolvedEndFaces) {
    // Цилиндр вращением прямоугольника XZ вокруг Z: торцы — плоскости, построенные вращением.
    const TopoDS_Face profile =
        BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakePolygon(gp_Pnt(0, 0, 0), gp_Pnt(30, 0, 0), gp_Pnt(30, 0, 40),
                                                           gp_Pnt(0, 0, 40), true)
                                    .Wire(),
                                true)
            .Face();
    const TopoDS_Shape cyl = BRepPrimAPI_MakeRevol(profile, gp_Ax1(gp::Origin(), gp::DZ())).Shape();
    ASSERT_TRUE(valid(cyl));
    checkEveryPlanarFace(cyl);
    // Сквозное отверстие с торца: удалён цилиндр на всю высоту.
    const TopoDS_Face top = planeFaceAt(cyl, 2, 40);
    ASSERT_FALSE(top.IsNull());
    HoleParams hp;
    hp.diameter = 10;
    hp.end = HoleEnd::Through;
    auto r = hole(cyl, top, gp_Pnt(12, 0, 40), hp);
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_NEAR(volume(cyl) - volume(r.value().shape), kPi * 25 * 40, 1e-5);
}

TEST(Hole, ThreadedHoleDrillsInwardOnLeftHandedFace) {
    gp_Trsf mirror;
    mirror.SetMirror(gp_Ax2(gp::Origin(), gp::DZ()));
    const TopoDS_Shape body = BRepBuilderAPI_Transform(box(40, 40, 20), mirror, true).Shape();  // z ∈ [-20, 0]
    const TopoDS_Face top = planeFaceAt(body, 2, 0.0);
    ASSERT_FALSE(top.IsNull());
    ASSERT_FALSE(BRepAdaptor_Surface(top, false).Plane().Position().Direct());
    ThreadedHoleParams tp;
    auto th = metricCoarse(6);
    ASSERT_TRUE(th.ok());
    tp.thread = th.value();
    tp.end = ThreadEnd::Blind;
    tp.threadLength = 10;
    tp.drillDepth = 12;
    tp.drillPoint = false;
    auto r = threadedHole(body, top, gp_Pnt(20, 20, 0), tp);
    ASSERT_TRUE(r.ok()) << r.error().message;
    const double d = tapDrillDiameter(tp.thread);  // 5,0 для М6
    EXPECT_NEAR(volume(body) - volume(r.value().shape), kPi * d * d / 4 * 12, 1e-6);
    ASSERT_FALSE(r.value().threads.empty());
    EXPECT_NEAR(r.value().threads.front().thread.direction.Z(), -1.0, 1e-9);
}
