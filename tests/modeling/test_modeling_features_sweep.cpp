// Кинематика (sweep): объёмы против аналитики (цилиндр, сектор тора, тор, труба с отверстием,
// тонкостенная), история, режимы ориентации, вырез, новое тело, ошибки.

#include "features_test_util.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <gtest/gtest.h>

using namespace ft;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Круг радиуса r с центром c в плоскости с нормалью n (X плоскости — x).
Profile circle(const gp_Pnt& c, const gp_Dir& n, const gp_Dir& x, double r, double holeR = 0.0) {
    const gp_Ax2 ax(c, n, x);
    TopoDS_Wire w = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(ax, r)).Edge()).Wire();
    BRepBuilderAPI_MakeFace mf(gp_Pln(gp_Ax3(ax)), w, true);
    if (holeR > 0) {
        TopoDS_Wire h = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(gp_Circ(ax, holeR)).Edge()).Wire();
        h.Reverse();
        mf.Add(h);
    }
    return Profile{mf.Face(), gp_Ax3(ax)};
}

TopoDS_Wire line(const gp_Pnt& a, const gp_Pnt& b) {
    return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(a, b).Edge()).Wire();
}

// Дуга окружности радиуса R в плоскости XY вокруг начала координат от угла 0 до angle.
TopoDS_Wire arc(double R, double angle) {
    const gp_Circ c(gp_Ax2(gp::Origin(), gp::DZ()), R);
    if (angle >= 2 * kPi - 1e-12) return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(c).Edge()).Wire();
    return BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(c, 0.0, angle).Edge()).Wire();
}

}  // namespace

TEST(FeaturesSweep, CircleAlongLineIsCylinder) {
    const double r = 2.0, h = 20.0;
    const Profile prof = circle(gp::Origin(), gp::DZ(), gp::DX(), r);
    auto res = sweep(prof, line(gp::Origin(), gp_Pnt(0, 0, h)), {}, SweepParams{});
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_TRUE(valid(res.value().shape));
    EXPECT_NEAR(volume(res.value().shape), kPi * r * r * h, 1e-6 * kPi * r * r * h);
    EXPECT_EQ(count(res.value().shape, TopAbs_FACE), 3);
    // История: стенка порождена ребром окружности, нижний торец — изменённая грань профиля,
    // верхний — порождённая.
    const auto& h0 = res.value().history;
    TopoDS_Shape circleEdge;
    for (TopExp_Explorer ex(prof.face, TopAbs_EDGE); ex.More(); ex.Next()) circleEdge = ex.Current();
    ASSERT_FALSE(h0->Generated(circleEdge).IsEmpty());
    BRepAdaptor_Surface wall(TopoDS::Face(h0->Generated(circleEdge).First()));
    EXPECT_EQ(wall.GetType(), GeomAbs_Cylinder);
    ASSERT_EQ(h0->Modified(prof.face).Extent(), 1);
    EXPECT_NEAR(faceCentroid(h0->Modified(prof.face).First()).Z(), 0.0, 1e-9);
    bool top = false;
    for (const TopoDS_Shape& g : h0->Generated(prof.face))
        if (g.ShapeType() == TopAbs_FACE && std::abs(faceCentroid(g).Z() - h) < 1e-9) top = true;
    EXPECT_TRUE(top);
}

TEST(FeaturesSweep, CircleAlongArcIsTorusSector) {
    const double R = 10.0, r = 2.0;
    // Профиль в плоскости XZ в точке (R, 0, 0), траектория — дуга в XY, касательная в начале +Y.
    const Profile prof = circle(gp_Pnt(R, 0, 0), gp::DY(), gp::DX(), r);
    for (SweepOrientation o : {SweepOrientation::CorrectedFrenet, SweepOrientation::Frenet}) {
        SweepParams p;
        p.orientation = o;
        auto quarter = sweep(prof, arc(R, kPi / 2), {}, p);
        ASSERT_TRUE(quarter.ok()) << quarter.error().message;
        EXPECT_TRUE(valid(quarter.value().shape));
        const double v = (kPi / 2) * R * kPi * r * r;  // Папп — Гульдин: угол · R · площадь
        EXPECT_NEAR(volume(quarter.value().shape), v, 1e-5 * v);
    }
    auto full = sweep(prof, arc(R, 2 * kPi), {}, SweepParams{});
    ASSERT_TRUE(full.ok()) << full.error().message;
    EXPECT_TRUE(valid(full.value().shape));
    const double torus = 2 * kPi * kPi * R * r * r;
    EXPECT_NEAR(volume(full.value().shape), torus, 1e-5 * torus);
}

TEST(FeaturesSweep, FixedAndBinormalOrientationKeepProfile) {
    const Profile prof = rect(-2, -1, 2, 1);
    // Наклонная прямая: при постоянном трёхграннике профиль не поворачивается — призма с
    // основанием 8 и высотой 10 (объём = площадь · проекция траектории на нормаль).
    SweepParams fixed;
    fixed.orientation = SweepOrientation::Fixed;
    auto res = sweep(prof, line(gp::Origin(), gp_Pnt(3, 0, 10)), {}, fixed);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_NEAR(volume(res.value().shape), 8.0 * 10.0, 1e-6 * 80);
    SweepParams bin;
    bin.orientation = SweepOrientation::Binormal;
    bin.binormal = gp::DY();
    auto res2 = sweep(prof, line(gp::Origin(), gp_Pnt(0, 0, 12)), {}, bin);
    ASSERT_TRUE(res2.ok()) << res2.error().message;
    EXPECT_NEAR(volume(res2.value().shape), 8.0 * 12.0, 1e-6 * 96);
}

TEST(FeaturesSweep, GuideCurve) {
    // Направляющая параллельна траектории: профиль переносится без поворота — цилиндр.
    const Profile prof = circle(gp::Origin(), gp::DZ(), gp::DX(), 1.5);
    SweepParams p;
    p.orientation = SweepOrientation::Guide;
    p.guide = line(gp_Pnt(5, 0, 0), gp_Pnt(5, 0, 8));
    auto res = sweep(prof, line(gp::Origin(), gp_Pnt(0, 0, 8)), {}, p);
    ASSERT_TRUE(res.ok()) << res.error().message;
    // С направляющей MakePipeShell аппроксимирует поверхность (BSpline): допуск 2e-4.
    EXPECT_NEAR(volume(res.value().shape), kPi * 1.5 * 1.5 * 8, 2e-4 * kPi * 18);
}

TEST(FeaturesSweep, ProfileWithHoleAndThinWall) {
    const double h = 15.0;
    const Profile tube = rect(-5, -5, 5, 5, 0.0, 2.0, 0.0, 0.0);
    auto res = sweep(tube, line(gp::Origin(), gp_Pnt(0, 0, h)), {}, SweepParams{});
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_TRUE(valid(res.value().shape));
    EXPECT_NEAR(volume(res.value().shape), (100.0 - kPi * 4.0) * h, 1e-6 * 1500);
    EXPECT_EQ(count(res.value().shape, TopAbs_FACE), 7);

    SweepParams thin;
    thin.thin = ThinWall{0.5, ThinSide::Inside};
    const double r = 3.0;
    auto wall = sweep(circle(gp::Origin(), gp::DZ(), gp::DX(), r), line(gp::Origin(), gp_Pnt(0, 0, h)), {}, thin);
    ASSERT_TRUE(wall.ok()) << wall.error().message;
    const double v = kPi * (r * r - 2.5 * 2.5) * h;
    EXPECT_NEAR(volume(wall.value().shape), v, 1e-5 * v);
    // Замкнутая траектория без торцов — тонкостенная не поддерживается.
    auto closed = sweep(circle(gp_Pnt(10, 0, 0), gp::DY(), gp::DX(), 1.0), arc(10, 2 * kPi), {}, thin);
    ASSERT_FALSE(closed.ok());
    EXPECT_EQ(closed.error().code, ErrorCode::Unsupported);
}

TEST(FeaturesSweep, CutAndNewBody) {
    const TopoDS_Shape b = box(20, 20, 10);
    const double r = 3.0;
    SweepParams cut;
    cut.mode = FeatureMode::Cut;
    // Круг в плоскости z = 0 под коробкой, траектория вверх через всю коробку.
    auto res = sweep(circle(gp_Pnt(10, 10, -1), gp::DZ(), gp::DX(), r), line(gp_Pnt(10, 10, -1), gp_Pnt(10, 10, 12)), b, cut);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_NEAR(volume(res.value().shape), 4000.0 - kPi * r * r * 10, 1e-6 * 4000);

    SweepParams nb;
    nb.mode = FeatureMode::NewBody;
    auto two = sweep(circle(gp_Pnt(10, 10, 5), gp::DZ(), gp::DX(), r), line(gp_Pnt(10, 10, 5), gp_Pnt(10, 10, 30)), b, nb);
    ASSERT_TRUE(two.ok()) << two.error().message;
    EXPECT_EQ(count(two.value().shape, TopAbs_SOLID), 2);
    // Тела не сливаются: объём — сумма, включая перекрытие.
    EXPECT_NEAR(volume(two.value().shape), 4000.0 + kPi * r * r * 25, 1e-6 * 5000);
}

TEST(FeaturesSweep, Errors) {
    const Profile prof = circle(gp::Origin(), gp::DZ(), gp::DX(), 1.0);
    auto off = sweep(prof, line(gp_Pnt(0, 0, 1), gp_Pnt(0, 0, 5)), {}, SweepParams{});
    ASSERT_FALSE(off.ok());
    EXPECT_EQ(off.error().code, ErrorCode::InvalidInput);
    EXPECT_NE(off.error().message.find("profile plane"), std::string::npos) << off.error().message;
    auto empty = sweep(prof, TopoDS_Wire(), {}, SweepParams{});
    ASSERT_FALSE(empty.ok());
    EXPECT_EQ(empty.error().code, ErrorCode::InvalidInput);
    SweepParams guide;
    guide.orientation = SweepOrientation::Guide;
    auto noGuide = sweep(prof, line(gp::Origin(), gp_Pnt(0, 0, 5)), {}, guide);
    ASSERT_FALSE(noGuide.ok());
    EXPECT_EQ(noGuide.error().code, ErrorCode::InvalidInput);
    SweepParams cut;
    cut.mode = FeatureMode::Cut;
    auto noTarget = sweep(prof, line(gp::Origin(), gp_Pnt(0, 0, 5)), {}, cut);
    ASSERT_FALSE(noTarget.ok());
    auto miss = sweep(circle(gp_Pnt(50, 50, 0), gp::DZ(), gp::DX(), 1.0), line(gp_Pnt(50, 50, 0), gp_Pnt(50, 50, 5)),
                      box(1, 1, 1), cut);
    ASSERT_FALSE(miss.ok());
    EXPECT_EQ(miss.error().code, ErrorCode::OperationFailed);
    EXPECT_NE(miss.error().message.find("does not intersect"), std::string::npos) << miss.error().message;
}

TEST(FeaturesSweep, ChainEdges) {
    const gp_Pnt a(0, 0, 0), b(10, 0, 0), c(10, 10, 0), d(10, 10, 10);
    const TopoDS_Edge ab = BRepBuilderAPI_MakeEdge(a, b), bc = BRepBuilderAPI_MakeEdge(b, c), cd = BRepBuilderAPI_MakeEdge(c, d);
    auto w = chainEdges({cd, ab, bc});
    ASSERT_TRUE(w.ok()) << w.error().message;
    EXPECT_EQ(count(w.value(), TopAbs_EDGE), 3);
    GProp_GProps g;
    BRepGProp::LinearProperties(w.value(), g);
    EXPECT_NEAR(g.Mass(), 30.0, 1e-9);
    // Круг по ломаной с углом 90° (стык под углом — по биссекторной плоскости): объём равен
    // площади, умноженной на длину осевой линии, πr²(10 + 10).
    auto res = sweep(circle(a, gp::DX(), gp::DY(), 1.0), BRepBuilderAPI_MakeWire(ab, bc).Wire(), {}, SweepParams{});
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_TRUE(valid(res.value().shape));
    EXPECT_NEAR(volume(res.value().shape), kPi * 20.0, 1e-4 * kPi * 20.0);

    auto branch = chainEdges({ab, bc, BRepBuilderAPI_MakeEdge(b, gp_Pnt(10, -5, 0)).Edge()});
    ASSERT_FALSE(branch.ok());
    EXPECT_NE(branch.error().message.find("branches"), std::string::npos);
    auto gap = chainEdges({ab, cd});
    ASSERT_FALSE(gap.ok());
    EXPECT_NE(gap.error().message.find("not connected"), std::string::npos);
    auto none = chainEdges({});
    EXPECT_FALSE(none.ok());
}
