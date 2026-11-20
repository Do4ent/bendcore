// Зубчатый венец: размеры против формул ГОСТ 16532, толщина зуба по делительной окружности по
// построенным рёбрам, объём против численного интеграла профиля × b (прямой и косой зуб,
// нарезание заготовки), подрезание при малом z, заострение, атрибуты.

#include "features_test_util.hpp"

#include <bendcore/modeling/gear.hpp>

#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRep_Tool.hxx>
#include <Geom_Curve.hxx>
#include <gtest/gtest.h>

#include <map>

using namespace ft;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

GearSpec spec(double m, int z, double x = 0.0, double b = 20.0, double betaDeg = 0.0) {
    GearSpec s;
    s.module = m;
    s.teeth = z;
    s.shift = x;
    s.width = b;
    s.helix = betaDeg * kDeg;
    return s;
}

// Площадь торцового профиля венца по формулам: 2z площадей сектора «центр — половина зуба»,
// контур — дуга впадины, переходная кривая, эвольвента, половина дуги вершины (формула Гаусса
// по точкам, взятым из аналитических кривых, а не из B-сплайнов).
double profileArea(const GearSpec& s, int samples = 4000) {
    auto side = toothSidePoints(s, samples);
    EXPECT_TRUE(side.ok()) << (side.ok() ? "" : side.error().message);
    if (!side.ok()) return 0.0;
    std::vector<std::pair<double, double>> poly{{0.0, 0.0}};
    for (const auto* part : {&side.value().root, &side.value().fillet, &side.value().involute, &side.value().tipHalf})
        poly.insert(poly.end(), part->begin(), part->end());
    double a = 0.0;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const auto& p = poly[i];
        const auto& q = poly[(i + 1) % poly.size()];
        a += p.first * q.second - q.first * p.second;
    }
    return 2.0 * s.teeth * std::abs(a) / 2.0;
}

std::map<std::string, TopoDS_Edge> edgesOf(const GearProfile& p) {
    std::map<std::string, TopoDS_Edge> out;
    for (const auto& [e, role] : p.roles) out[role] = e;
    return out;
}

// Точка ребра на окружности радиуса r вокруг оси Z (бисекция по параметру кривой).
gp_Pnt pointAtRadius(const TopoDS_Edge& e, double r) {
    double f, l;
    Handle(Geom_Curve) c = BRep_Tool::Curve(e, f, l);
    auto rad = [&](double t) {
        const gp_Pnt p = c->Value(t);
        return std::hypot(p.X(), p.Y()) - r;
    };
    double a = f, b = l;
    EXPECT_LT(rad(a) * rad(b), 0.0);
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (a + b);
        if (rad(a) * rad(mid) <= 0.0) b = mid; else a = mid;
    }
    return c->Value(0.5 * (a + b));
}

double radiusRange(const TopoDS_Edge& e, bool maximum) {
    double f, l;
    Handle(Geom_Curve) c = BRep_Tool::Curve(e, f, l);
    double v = maximum ? 0.0 : 1e300;
    for (int i = 0; i <= 200; ++i) {
        const gp_Pnt p = c->Value(f + (l - f) * i / 200);
        const double r = std::hypot(p.X(), p.Y());
        v = maximum ? std::max(v, r) : std::min(v, r);
    }
    return v;
}

}  // namespace

TEST(FeaturesGear, Gost16532Diameters) {
    // m = 2, z = 20, x = 0: d = 40, da = 44, df = 35; z = 60: d = 120, da = 124, df = 115.
    auto g1 = gearGeometry(spec(2, 20));
    ASSERT_TRUE(g1.ok()) << g1.error().message;
    EXPECT_NEAR(g1.value().d, 40.0, 1e-12);
    EXPECT_NEAR(g1.value().da, 44.0, 1e-12);
    EXPECT_NEAR(g1.value().df, 35.0, 1e-12);
    EXPECT_NEAR(g1.value().db, 40.0 * std::cos(20 * kDeg), 1e-12);
    EXPECT_NEAR(g1.value().s, kPi, 1e-12);  // π·m/2
    EXPECT_FALSE(g1.value().undercut);
    EXPECT_NEAR(g1.value().zMin, 2.0 / std::pow(std::sin(20 * kDeg), 2), 1e-12);  // 17,097
    auto g2 = gearGeometry(spec(2, 60));
    EXPECT_NEAR(g2.value().da, 124.0, 1e-12);
    EXPECT_NEAR(g2.value().df, 115.0, 1e-12);
    // Смещение x = 0,5, m = 3, z = 15: d = 45, da = 45 + 2·1,5·3 = 54, df = 45 − 2·0,75·3 = 40,5,
    // s = 3·(π/2 + tg 20°) = 5,8043.
    auto g3 = gearGeometry(spec(3, 15, 0.5));
    EXPECT_NEAR(g3.value().da, 54.0, 1e-12);
    EXPECT_NEAR(g3.value().df, 40.5, 1e-12);
    EXPECT_NEAR(g3.value().s, 3.0 * (kPi / 2 + std::tan(20 * kDeg)), 1e-12);
    // Косой зуб m = 2, z = 20, β = 15°: mt = 2,070552, d = 41,411047, tg αt = tg 20°/cos 15°,
    // da = d + 4, df = d − 5.
    auto g4 = gearGeometry(spec(2, 20, 0.0, 20, 15));
    EXPECT_NEAR(g4.value().mt, 2.0705523608201659, 1e-12);
    EXPECT_NEAR(g4.value().d, 41.411047216403318, 1e-11);
    EXPECT_NEAR(std::tan(g4.value().alphaT), std::tan(20 * kDeg) / std::cos(15 * kDeg), 1e-12);
    EXPECT_NEAR(g4.value().da, 45.411047216403318, 1e-11);
    EXPECT_NEAR(g4.value().df, 36.411047216403318, 1e-11);
    EXPECT_NEAR(g4.value().lead, kPi * 41.411047216403318 / std::tan(15 * kDeg), 1e-9);
}

TEST(FeaturesGear, ProfileDiametersAndToothThickness) {
    for (const GearSpec& s : {spec(2, 20), spec(3, 15, 0.5), spec(2, 20, 0.0, 20, 15), spec(2, 60, -0.3)}) {
        auto p = gearProfile(s);
        ASSERT_TRUE(p.ok()) << p.error().message;
        EXPECT_TRUE(valid(p.value().face));
        EXPECT_EQ(p.value().roles.size(), 6u * s.teeth);
        EXPECT_LE(p.value().maxDeviation, 1e-6);
        const GearGeometry g = gearGeometry(s).value();
        auto e = edgesOf(p.value());
        // Окружности вершин и впадин — по рёбрам.
        EXPECT_NEAR(radiusRange(e.at("T1.Tip"), true), g.da / 2, 1e-9);
        EXPECT_NEAR(radiusRange(e.at("T3.Root"), false), g.df / 2, 1e-9);
        // Наибольший радиус профиля — da/2, наименьший — df/2.
        double rMax = 0.0, rMin = 1e300;
        for (const auto& [edge, role] : p.value().roles) {
            rMax = std::max(rMax, radiusRange(edge, true));
            rMin = std::min(rMin, radiusRange(edge, false));
        }
        EXPECT_NEAR(rMax, g.da / 2, 1e-7);
        EXPECT_NEAR(rMin, g.df / 2, 1e-7);
        // Граничная точка — общая вершина переходной кривой и эвольвенты на диаметре dForm.
        EXPECT_NEAR(radiusRange(e.at("T1.FlankR"), false), g.dForm / 2, 1e-7);
        // Торцовая толщина по дуге делительной окружности: mt(π/2 + 2x tg α) (ГОСТ 16532).
        const double r = g.d / 2;
        const gp_Pnt a = pointAtRadius(e.at("T1.FlankR"), r), b = pointAtRadius(e.at("T1.FlankL"), r);
        const double arc = r * (std::atan2(b.Y(), b.X()) - std::atan2(a.Y(), a.X()));
        EXPECT_NEAR(arc, g.mt * (kPi / 2 + 2 * s.shift * std::tan(20 * kDeg)), 2e-5) << s.teeth;
        // Зуб № 1 — по оси X: середина зуба на угле 0.
        EXPECT_NEAR(std::atan2(a.Y(), a.X()) + std::atan2(b.Y(), b.X()), 0.0, 1e-6);
        // Толщина по вершинам sa (формула ГОСТ 16532) — по дуге вершины.
        double f, l;
        BRep_Tool::Curve(e.at("T2.Tip"), f, l);
        EXPECT_NEAR((l - f) * g.da / 2, g.sa, 1e-7);
    }
}

TEST(FeaturesGear, SpurVolumeAgainstProfileIntegral) {
    GearParams p;
    p.spec = spec(2, 20, 0.0, 16.0);
    p.mode = FeatureMode::NewBody;
    auto res = gearTeeth(TopoDS_Shape(), p);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_TRUE(valid(res.value().shape));
    const double A = profileArea(p.spec);
    // Площадь лежит между кругами впадин и вершин.
    EXPECT_GT(A, kPi * 17.5 * 17.5);
    EXPECT_LT(A, kPi * 22.0 * 22.0);
    EXPECT_NEAR(volume(res.value().shape), A * 16.0, 3e-6 * A * 16.0);
    // Грани ролей: 6 на зуб + 2 торца.
    EXPECT_EQ(res.value().roles.size(), 6u * 20 + 2);
    EXPECT_EQ(count(res.value().shape, TopAbs_FACE), 6 * 20 + 2);
}

TEST(FeaturesGear, CutOnBlank) {
    // Заготовка — цилиндр ⌀da × 30, венец шириной 20 со смещением 5 от торца.
    GearParams p;
    p.spec = spec(2, 24, 0.2, 20.0);
    const GearGeometry g = gearGeometry(p.spec).value();
    TopoDS_Shape blank = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, -5), gp_Dir(0, 0, 1)), g.da / 2, 30).Shape();
    auto res = gearTeeth(blank, p);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_TRUE(valid(res.value().shape));
    const double A = profileArea(p.spec);
    const double removed = (kPi * g.da * g.da / 4 - A) * 20.0;
    EXPECT_NEAR(volume(blank) - volume(res.value().shape), removed, 3e-6 * removed);
    // Роли — грани инструмента (без вершин зубьев: они остаются гранью заготовки).
    EXPECT_EQ(res.value().roles.size(), 5u * 24);
    for (const auto& [f, role] : res.value().roles) EXPECT_EQ(f.ShapeType(), TopAbs_FACE) << role;
}

TEST(FeaturesGear, HelicalVolumeAndTwist) {
    GearParams p;
    p.spec = spec(2, 18, 0.0, 15.0, 12.0);
    p.spec.hand = GearHand::Left;
    p.mode = FeatureMode::Boss;
    auto res = gearTeeth(TopoDS_Shape(), p);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_TRUE(valid(res.value().shape));
    const double A = profileArea(p.spec);
    // Принцип Кавальери: объём косозубого венца равен площади торцового сечения × b.
    EXPECT_NEAR(volume(res.value().shape), A * 15.0, 3e-5 * A * 15.0);
    // Торец End повёрнут на twist = −2b·tg β/d (левый зуб): центр масс торца — на оси, поэтому
    // поворот проверяется по вершине зуба № 1 (середина дуги вершины).
    const GearGeometry g = gearGeometry(p.spec).value();
    EXPECT_NEAR(g.twist, -2.0 * 15.0 * std::tan(12 * kDeg) / g.d, 1e-15);
    TopoDS_Shape end;
    for (const auto& [f, role] : res.value().roles)
        if (role == "End") end = f;
    ASSERT_FALSE(end.IsNull());
    double best = 1e300;
    const gp_Pnt expected(g.da / 2 * std::cos(g.twist), g.da / 2 * std::sin(g.twist), 15.0);
    for (TopExp_Explorer ex(end, TopAbs_EDGE); ex.More(); ex.Next()) {
        double f, l;
        Handle(Geom_Curve) c = BRep_Tool::Curve(TopoDS::Edge(ex.Current()), f, l);
        if (c.IsNull()) continue;
        best = std::min(best, c->Value(0.5 * (f + l)).Distance(expected));
    }
    EXPECT_LT(best, 1e-4);
}

TEST(FeaturesGear, UndercutSmallTeeth) {
    // z = 8 < zmin = 17 при x = 0: подрезание — предупреждение, профиль без самопересечений.
    GearParams p;
    p.spec = spec(2, 8, 0.0, 10.0);
    p.mode = FeatureMode::NewBody;
    const GearGeometry g = gearGeometry(p.spec).value();
    EXPECT_TRUE(g.undercut);
    EXPECT_NEAR(g.xMin, 1.0 - 8 * std::pow(std::sin(20 * kDeg), 2) / 2, 1e-12);
    auto side = toothSidePoints(p.spec, 400);
    ASSERT_TRUE(side.ok()) << side.error().message;
    auto res = gearTeeth(TopoDS_Shape(), p);
    ASSERT_TRUE(res.ok()) << res.error().message;
    EXPECT_TRUE(valid(res.value().shape));
    bool warned = false;
    for (const auto& w : res.value().warnings) warned = warned || w.find("undercut") != std::string::npos;
    EXPECT_TRUE(warned);
    const double A = profileArea(p.spec);
    EXPECT_NEAR(volume(res.value().shape), A * 10.0, 3e-6 * A * 10.0);
    // Подрезанная ножка тоньше: переходная кривая уходит внутрь основной окружности.
    double rMin = 1e300;
    for (const auto& [x, y] : side.value().fillet) rMin = std::min(rMin, std::hypot(x, y));
    EXPECT_LT(rMin, g.db / 2);
    // z = 10, x = 0,45 > xmin = 0,415: подрезания нет (при z = 8 зуб без подрезания заострён).
    p.spec = spec(2, 10, 0.45, 10.0);
    EXPECT_FALSE(gearGeometry(p.spec).value().undercut);
    auto res2 = gearTeeth(TopoDS_Shape(), p);
    ASSERT_TRUE(res2.ok()) << res2.error().message;
    for (const auto& w : res2.value().warnings) EXPECT_EQ(w.find("undercut"), std::string::npos) << w;
    // Без подрезания эвольвента начинается не ниже основной окружности.
    EXPECT_GE(gearGeometry(p.spec).value().dForm, gearGeometry(p.spec).value().db - 1e-9);
    // Очень малое z тоже строится без самопересечений.
    for (int z : {5, 6, 10, 12}) {
        auto s = toothSidePoints(spec(1, z), 300);
        EXPECT_TRUE(s.ok()) << z << ": " << (s.ok() ? "" : s.error().message);
        auto pr = gearProfile(spec(1, z));
        EXPECT_TRUE(pr.ok() && valid(pr.value().face)) << z;
    }
}

TEST(FeaturesGear, Errors) {
    EXPECT_FALSE(gearGeometry(spec(0, 20)).ok());
    EXPECT_FALSE(gearGeometry(spec(2, 2)).ok());
    EXPECT_FALSE(gearGeometry(spec(2, 20, 0, -1)).ok());
    EXPECT_FALSE(gearGeometry(spec(2, 20, 0, 10, 50)).ok());
    // z = 8, x = 1,5: зуб заострён.
    auto pointed = gearGeometry(spec(2, 8, 1.5));
    ASSERT_FALSE(pointed.ok());
    EXPECT_NE(pointed.error().message.find("pointed"), std::string::npos);
    GearParams p;
    p.spec = spec(2, 20);
    EXPECT_FALSE(gearTeeth(TopoDS_Shape(), p).ok());  // нарезание без заготовки
}

TEST(FeaturesGear, Attributes) {
    GearSpec s = spec(2.5, 33, -0.25, 18, 10);
    s.hand = GearHand::Left;
    s.accuracy = "8-B";
    s.axis = gp_Ax3(gp_Pnt(1, 2, 3), gp_Dir(0, 1, 0), gp_Dir(0, 0, 1));
    auto back = gearFromAttributes(gearAttributes(s));
    ASSERT_TRUE(back.ok()) << back.error().message;
    EXPECT_DOUBLE_EQ(back.value().module, 2.5);
    EXPECT_EQ(back.value().teeth, 33);
    EXPECT_DOUBLE_EQ(back.value().shift, -0.25);
    EXPECT_DOUBLE_EQ(back.value().helix, s.helix);
    EXPECT_EQ(back.value().hand, GearHand::Left);
    EXPECT_EQ(back.value().accuracy, "8-B");
    EXPECT_TRUE(back.value().axis.Direction().IsEqual(gp_Dir(0, 1, 0), 1e-15));
    EXPECT_NEAR(back.value().axis.Location().Distance(gp_Pnt(1, 2, 3)), 0.0, 1e-15);
    auto attrs = gearAttributes(s);
    attrs.erase(attrs.begin() + 2);
    EXPECT_FALSE(gearFromAttributes(attrs).ok());
}
