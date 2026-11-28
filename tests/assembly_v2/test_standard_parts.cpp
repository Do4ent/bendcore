// Каталог подшипников ГОСТ 8338 / ISO 15 и шпонки ГОСТ 23360 (docs/assembly/FASTENERS.md,
// разделы 8, 9): размеры против таблиц стандартов (переписанных в тест независимо от кода),
// объёмы моделей против формул Паппа — Гульдена и элементарной геометрии, грани, атрибуты,
// постановка подшипника между корпусом и валом в документе сборки (степени свободы,
// пересечения), сохранение и загрузка атрибутов.

#include <bendcore/assembly_v2/bearings.hpp>
#include <bendcore/assembly_v2/interference.hpp>
#include <bendcore/assembly_v2/keys.hpp>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace bendcore::assembly_v2;
using namespace bendcore::assembly_v2::fasteners;
using bendcore::kernel::ErrorCode;

namespace {

constexpr double kPi = std::numbers::pi;

double volume(const TopoDS_Shape& s) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(s, p);
    return p.Mass();
}

int solids(const TopoDS_Shape& s) {
    int n = 0;
    for (TopExp_Explorer ex(s, TopAbs_SOLID); ex.More(); ex.Next()) ++n;
    return n;
}

// ISO 15 (ГОСТ 8338-75): d, D, B, r_s min по сериям 10, 02, 03 для обозначений xx00…xx12.
struct Iso15 {
    int number;
    double d, D, B, r;
};
const std::vector<Iso15> kIso15{
    {100, 10, 26, 8, 0.3},   {101, 12, 28, 8, 0.3},   {102, 15, 32, 9, 0.3},   {103, 17, 35, 10, 0.3},
    {104, 20, 42, 12, 0.6},  {105, 25, 47, 12, 0.6},  {106, 30, 55, 13, 1},    {107, 35, 62, 14, 1},
    {108, 40, 68, 15, 1},    {109, 45, 75, 16, 1},    {110, 50, 80, 16, 1},    {111, 55, 90, 18, 1.1},
    {112, 60, 95, 18, 1.1},  {200, 10, 30, 9, 0.6},   {201, 12, 32, 10, 0.6},  {202, 15, 35, 11, 0.6},
    {203, 17, 40, 12, 0.6},  {204, 20, 47, 14, 1},    {205, 25, 52, 15, 1},    {206, 30, 62, 16, 1},
    {207, 35, 72, 17, 1.1},  {208, 40, 80, 18, 1.1},  {209, 45, 85, 19, 1.1},  {210, 50, 90, 20, 1.1},
    {211, 55, 100, 21, 1.5}, {212, 60, 110, 22, 1.5}, {300, 10, 35, 11, 0.6},  {301, 12, 37, 12, 1},
    {302, 15, 42, 13, 1},    {303, 17, 47, 14, 1},    {304, 20, 52, 15, 1.1},  {305, 25, 62, 17, 1.1},
    {306, 30, 72, 19, 1.1},  {307, 35, 80, 21, 1.5},  {308, 40, 90, 23, 1.5},  {309, 45, 100, 25, 1.5},
    {310, 50, 110, 27, 2},   {311, 55, 120, 29, 2},   {312, 60, 130, 31, 2.1},
};

// Объём подшипника по формулам: кольца — теорема Паппа — Гульдена для профиля «прямоугольник
// минус два угла со скруглением r минус круговой сегмент желоба», шарики — Z·π·Dw³/6.
double bearingVolume(double d, double D, double B, double r, double Dw, double Dpw, int Z, double rg, double cl) {
    const double spArea = r * r * (1 - kPi / 4);                    // угол квадрата r×r без четверти круга
    const double spOff = (10 - 3 * kPi) / (12 - 3 * kPi) * r;       // центр тяжести от вершины угла
    const double h = 0.2 * Dw + cl / 2;                             // глубина желоба от борта
    const double theta = 2 * std::acos((rg - h) / rg);              // центральный угол сегмента
    const double segArea = rg * rg / 2 * (theta - std::sin(theta));
    const double segOff = 4 * rg * std::pow(std::sin(theta / 2), 3) / (3 * (theta - std::sin(theta)));
    // Внутреннее кольцо: r ∈ [d/2, ri], желоб — сегмент с центром окружности на rb + rg.
    const double ri = Dpw / 2 - 0.3 * Dw, rbi = Dpw / 2 - Dw / 2 - cl / 2;
    double M = (ri * ri - d * d / 4) / 2 * B;
    M -= 2 * spArea * (d / 2 + spOff);
    M -= segArea * (rbi + rg - segOff);
    // Наружное кольцо: r ∈ [ro, D/2].
    const double ro = Dpw / 2 + 0.3 * Dw, rbo = Dpw / 2 + Dw / 2 + cl / 2;
    double Mo = (D * D / 4 - ro * ro) / 2 * B;
    Mo -= 2 * spArea * (D / 2 - spOff);
    Mo -= segArea * (rbo - rg + segOff);
    return 2 * kPi * (M + Mo) + Z * kPi * Dw * Dw * Dw / 6;
}

// ГОСТ 23360-78: d свыше…до, b, h, t1, t2, s min/max, l min/max.
struct Gost23360 {
    double over, upTo, b, h, t1, t2, smin, smax, lmin, lmax;
};
const std::vector<Gost23360> kKeys{
    {6, 8, 2, 2, 1.2, 1.0, 0.16, 0.25, 6, 20},       {8, 10, 3, 3, 1.8, 1.4, 0.16, 0.25, 6, 36},
    {10, 12, 4, 4, 2.5, 1.8, 0.16, 0.25, 8, 45},     {12, 17, 5, 5, 3.0, 2.3, 0.25, 0.4, 10, 56},
    {17, 22, 6, 6, 3.5, 2.8, 0.25, 0.4, 14, 70},     {22, 30, 8, 7, 4.0, 3.3, 0.25, 0.4, 18, 90},
    {30, 38, 10, 8, 5.0, 3.3, 0.4, 0.6, 22, 110},    {38, 44, 12, 8, 5.0, 3.3, 0.4, 0.6, 28, 140},
    {44, 50, 14, 9, 5.5, 3.8, 0.4, 0.6, 36, 160},    {50, 58, 16, 10, 6.0, 4.3, 0.4, 0.6, 45, 180},
    {58, 65, 18, 11, 7.0, 4.4, 0.4, 0.6, 50, 200},   {65, 75, 20, 12, 7.5, 4.9, 0.6, 0.8, 56, 220},
    {75, 85, 22, 14, 9.0, 5.4, 0.6, 0.8, 63, 250},   {85, 95, 25, 14, 9.0, 5.4, 0.6, 0.8, 70, 280},
    {95, 110, 28, 16, 10.0, 6.4, 0.6, 0.8, 80, 320},
};

// Объём шпонки: прямой участок — (b·h − 2s²)·Ls (четыре фаски s×45°), скруглённый конец —
// половина тела вращения профиля R×h без двух треугольников s×s: π·(h·R²/2 − s²(R − s/3)).
double keyVolume(KeyExecution e, double b, double h, double l, double s) {
    const double R = b / 2;
    const int rounded = e == KeyExecution::Rounded ? 2 : e == KeyExecution::OneRounded ? 1 : 0;
    const double straight = l - rounded * R;
    return (b * h - 2 * s * s) * straight + rounded * kPi * (h * R * R / 2 - s * s * (R - s / 3));
}

}  // namespace

// ---------------------------------------------------------------- подшипники

TEST(BearingCatalogue, RowsMatchIso15) {
    EXPECT_EQ(ballBearingNumbers().size(), kIso15.size());
    for (const Iso15& t : kIso15) {
        auto r = ballBearingRow(t.number);
        ASSERT_TRUE(r.ok()) << t.number;
        EXPECT_EQ(r.value().number, t.number);
        EXPECT_DOUBLE_EQ(r.value().d, t.d) << t.number;
        EXPECT_DOUBLE_EQ(r.value().D, t.D) << t.number;
        EXPECT_DOUBLE_EQ(r.value().B, t.B) << t.number;
        EXPECT_DOUBLE_EQ(r.value().r, t.r) << t.number;
        // ISO: 60xx / 62xx / 63xx с теми же двумя последними цифрами.
        const int series = t.number / 100;
        char iso[8];
        std::snprintf(iso, sizeof iso, "6%d%02d", series == 1 ? 0 : series, t.number % 100);
        EXPECT_EQ(r.value().iso, iso);
        auto bySize = ballBearingRow(static_cast<BearingSeries>(series), t.d);
        ASSERT_TRUE(bySize.ok());
        EXPECT_EQ(bySize.value().number, t.number);
    }
    for (int bad : {99, 113, 213, 400, 1205}) {
        auto r = ballBearingRow(bad);
        ASSERT_FALSE(r.ok()) << bad;
        EXPECT_EQ(r.error().code, ErrorCode::Unsupported);
    }
    EXPECT_FALSE(ballBearingRow(BearingSeries::Light, 22).ok());
    EXPECT_FALSE(bearingTableInfo().verified);
}

TEST(BearingCatalogue, ConventionalInternals) {
    auto in = ballBearingInternals(ballBearingRow(205).value()).value();
    EXPECT_NEAR(in.Dw, 0.3 * 27, 1e-12);
    EXPECT_NEAR(in.Dpw, 38.5, 1e-12);
    EXPECT_EQ(in.Z, 9);  // ⌊π·38,5 / (1,6·8,1)⌋ = ⌊9,33⌋
    EXPECT_NEAR(in.innerRaceway, 38.5 - 8.1 - 0.02, 1e-12);
    EXPECT_NEAR(in.outerRaceway, 38.5 + 8.1 + 0.02, 1e-12);
    // Шаг шариков по окружности центров больше их диаметра во всём каталоге.
    for (int n : ballBearingNumbers()) {
        auto i = ballBearingInternals(ballBearingRow(n).value());
        ASSERT_TRUE(i.ok()) << n;
        EXPECT_GT(i.value().Dpw * std::sin(kPi / i.value().Z), i.value().Dw) << n;
    }
}

TEST(BearingModel, VolumeMassAndBoundsForTheWholeCatalogue) {
    for (const Iso15& t : kIso15) {
        auto b = makeBallBearing(t.number);
        ASSERT_TRUE(b.ok()) << t.number << ": " << b.error().message;
        const Bearing& m = b.value();
        const BallBearingInternals& I = m.internals;
        EXPECT_EQ(solids(m.shape), 2 + I.Z) << t.number;
        for (TopExp_Explorer ex(m.shape, TopAbs_SOLID); ex.More(); ex.Next())
            EXPECT_TRUE(BRepCheck_Analyzer(ex.Current()).IsValid()) << t.number;
        const double f = bearingVolume(t.d, t.D, t.B, t.r, I.Dw, I.Dpw, I.Z, 0.52 * I.Dw, 0.02);
        EXPECT_NEAR(volume(m.shape), f, 1e-7 * f) << t.number;
        EXPECT_NEAR(m.properties.mass, f * 1e-9 * 7810.0, 1e-7 * f * 1e-9 * 7810.0) << t.number;
        Bnd_Box box;
        BRepBndLib::AddOptimal(m.shape, box, false, false);
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        EXPECT_NEAR(x1 - x0, t.D, 1e-6) << t.number;
        EXPECT_NEAR(z0, 0, 1e-6);
        EXPECT_NEAR(z1, t.B, 1e-6);
    }
    // 205: кольца и 9 шариков ⌀8,1 — масса модели без сепаратора.
    const Bearing b205 = makeBallBearing(205).value();
    std::printf("[bearing] 205: V = %.1f mm3, m = %.4f kg\n", b205.properties.volume, b205.properties.mass);
    EXPECT_EQ(b205.properties.designation, "Подшипник 205 ГОСТ 8338-75");
    EXPECT_EQ(b205.properties.equivalentDesignation, "Подшипник 6205 ISO 15");
    EXPECT_EQ(b205.properties.name, "Подшипник");
}

TEST(BearingModel, NamedFacesAndAttributes) {
    const Bearing b = makeBallBearing(206).value();
    const auto& map = *b.elementMap;
    auto face = [&](const std::string& role) {
        auto i = map.find(bendcore::modeling::ElementKind::Face, bearingFaceName(role));
        EXPECT_TRUE(i.has_value()) << role;
        return BRepAdaptor_Surface(TopoDS::Face(map.element(bendcore::modeling::ElementKind::Face, *i)));
    };
    EXPECT_NEAR(face(bearing_roles::kBore).Cylinder().Radius(), 15, 1e-9);
    EXPECT_NEAR(face(bearing_roles::kOuter).Cylinder().Radius(), 31, 1e-9);
    EXPECT_NEAR(face("InnerFace1").Plane().Location().Z(), 0, 1e-9);
    EXPECT_NEAR(face("InnerFace2").Plane().Location().Z(), 16, 1e-9);
    EXPECT_NEAR(face("OuterFace2").Plane().Location().Z(), 16, 1e-9);
    EXPECT_NEAR(face(bearing_roles::kInnerRaceway).Torus().MinorRadius(), 0.52 * 9.6, 1e-9);
    EXPECT_NEAR(face("Ball1").Sphere().Location().X(), 23, 1e-9);
    EXPECT_NEAR(face("Ball" + std::to_string(b.internals.Z)).Sphere().Radius(), 4.8, 1e-9);
    // Все грани несут атрибуты «bearing.*».
    for (const auto& rec : map.records(bendcore::modeling::ElementKind::Face)) {
        auto a = bearingFromAttributes(rec.attributes);
        ASSERT_TRUE(a.ok()) << rec.name;
    }
    auto a = bearingOf(map);
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->designation, "Подшипник 206 ГОСТ 8338-75");
    EXPECT_EQ(a->number, "206");
    EXPECT_EQ(a->iso, "6206");
    EXPECT_EQ(a->type, "ball-radial");
    EXPECT_DOUBLE_EQ(a->d, 30);
    EXPECT_DOUBLE_EQ(a->D, 62);
    EXPECT_DOUBLE_EQ(a->B, 16);
    EXPECT_DOUBLE_EQ(a->r, 1);
    EXPECT_EQ(a->Z, b.internals.Z);
    auto missing = bearingFromAttributes({{"bearing.d", "30"}});
    ASSERT_FALSE(missing.ok());
    EXPECT_EQ(missing.error().code, ErrorCode::InvalidInput);
}

// Вал-втулка с подшипником: корпус с расточкой ⌀52, подшипник 205 (соосность с расточкой,
// торец наружного кольца к уступу), вал ⌀25 (соосность с отверстием, заплечик к торцу
// внутреннего кольца). Положения вставки выполняют сопряжения; 2 степени свободы — поворот
// подшипника и вала; пересечений нет; после сохранения атрибуты на месте.
TEST(BearingAssembly, ShaftHousingBearing) {
    AssemblyDocument doc("Опора");
    TopoDS_Shape housing = BRepPrimAPI_MakeBox(gp_Pnt(-40, -40, -10), 80, 80, 30).Shape();
    housing = BRepAlgoAPI_Cut(housing, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, 0), gp::DZ()), 26, 21).Shape()).Shape();
    housing = BRepAlgoAPI_Cut(housing, BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, -11), gp::DZ()), 20, 12).Shape()).Shape();
    const DefinitionId hd = doc.addPart("Корпус", {housing, nullptr}, {"К-01", "Корпус", "СЧ 20", 7200}).value();
    const OccurrenceId ho = doc.addOccurrence(hd, Transform::identity()).value();
    ASSERT_TRUE(doc.setFixed(ho, true).ok());
    const Bearing brg = makeBallBearing(205).value();
    const DefinitionId bd = addBearingPart(doc, brg).value();
    const OccurrenceId bo = doc.addOccurrence(bd, Transform::identity()).value();
    // Вал ⌀25 z ∈ [0, 60] с заплечиком ⌀30 z ∈ [−8, 0] (опирается на торец внутреннего кольца).
    TopoDS_Shape shaft = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, 0), gp::DZ()), 12.5, 60).Shape();
    TopoDS_Shape collar = BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(0, 0, -8), gp::DZ()), 15, 8).Shape();
    shaft = BRepAlgoAPI_Fuse(shaft, collar).Shape();
    const DefinitionId sd = doc.addPart("Вал", {shaft, nullptr}, {"В-01", "Вал", "Сталь 45", 7850}).value();
    const OccurrenceId so = doc.addOccurrence(sd, Transform::identity()).value();

    auto housingFace = [&](auto pred) {
        const Definition& d = doc.definition(hd);
        for (TopExp_Explorer ex(d.shape, TopAbs_FACE); ex.More(); ex.Next())
            if (pred(BRepAdaptor_Surface(TopoDS::Face(ex.Current())), TopoDS::Face(ex.Current())))
                return GeometryRef{{ho}, ElementRef::face(*d.topology->faceIndex(ex.Current())), EntityUse::Auto};
        ADD_FAILURE() << "housing face not found";
        return GeometryRef{};
    };
    const GeometryRef bore = housingFace([](const BRepAdaptor_Surface& s, const TopoDS_Face&) {
        return s.GetType() == GeomAbs_Cylinder && std::abs(s.Cylinder().Radius() - 26) < 1e-9;
    });
    const GeometryRef step = housingFace([](const BRepAdaptor_Surface& s, const TopoDS_Face& f) {
        if (s.GetType() != GeomAbs_Plane || std::abs(s.Plane().Location().Z()) > 1e-9) return false;
        GProp_GProps p;
        BRepGProp::SurfaceProperties(f, p);
        return std::abs(p.Mass() - kPi * (26 * 26 - 20 * 20)) < 1e-6;
    });
    auto named = [](OccurrenceId o, const std::string& name) {
        return GeometryRef{{o}, ElementRef::named(bendcore::modeling::ElementKind::Face, name), EntityUse::Auto};
    };
    ASSERT_TRUE(doc.addMate(MateSpec::concentric(named(bo, bearingFaceName(bearing_roles::kOuter)), bore)).ok());
    ASSERT_TRUE(doc.addMate(MateSpec::coincident(named(bo, bearingFaceName("OuterFace1")), step, Alignment::AntiAligned)).ok());
    const Definition& sdef = doc.definition(sd);
    std::optional<GeometryRef> seat, shoulder;
    for (TopExp_Explorer ex(sdef.shape, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface s(TopoDS::Face(ex.Current()));
        const std::uint32_t i = *sdef.topology->faceIndex(ex.Current());
        if (s.GetType() == GeomAbs_Cylinder && std::abs(s.Cylinder().Radius() - 12.5) < 1e-9) seat = GeometryRef{{so}, ElementRef::face(i), EntityUse::Auto};
        if (s.GetType() == GeomAbs_Plane && std::abs(s.Plane().Location().Z()) < 1e-9) {
            GProp_GProps p;
            BRepGProp::SurfaceProperties(ex.Current(), p);
            if (std::abs(p.Mass() - kPi * (15 * 15 - 12.5 * 12.5)) < 1e-6) shoulder = GeometryRef{{so}, ElementRef::face(i), EntityUse::Auto};
        }
    }
    ASSERT_TRUE(seat && shoulder);
    ASSERT_TRUE(doc.addMate(MateSpec::concentric(*seat, named(bo, bearingFaceName(bearing_roles::kBore)))).ok());
    ASSERT_TRUE(doc.addMate(MateSpec::coincident(*shoulder, named(bo, bearingFaceName("InnerFace1")), Alignment::AntiAligned)).ok());
    auto solved = doc.solve();
    ASSERT_TRUE(solved.ok()) << solved.error().message;
    EXPECT_TRUE(solved.value().success);
    EXPECT_LT(solved.value().residualNorm, 1e-9);
    EXPECT_EQ(solved.value().totalDof, 2);
    for (OccurrenceId o : {bo, so}) EXPECT_NEAR(doc.findOccurrence(o)->placement.position.norm(), 0, 1e-9);

    auto inter = checkInterference(doc);
    ASSERT_TRUE(inter.ok()) << inter.error().message;
    int touching = 0;
    for (const InterferencePair& p : inter.value().pairs) {
        EXPECT_EQ(p.kind, ContactKind::Touching) << p.volume;
        ++touching;
    }
    EXPECT_EQ(touching, 2);  // подшипник–корпус, подшипник–вал (заплечик вала в выточке корпуса не касается)

    // Сохранение со встроенной геометрией и загрузка: атрибуты и имена тел сохраняются.
    auto json = doc.toJson();
    ASSERT_TRUE(json.ok()) << json.error().message;
    auto loaded = AssemblyDocument::fromJson(json.value(), {});
    ASSERT_TRUE(loaded.ok()) << loaded.error().message;
    AssemblyDocument& back = loaded.value().document;
    const Definition* bdef = nullptr;
    for (const Definition& d : back.definitions())
        if (d.name == "Подшипник 205 ГОСТ 8338-75") bdef = &d;
    ASSERT_NE(bdef, nullptr);
    ASSERT_TRUE(bdef->elementMap);
    auto a = bearingOf(*bdef->elementMap);
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->designation, "Подшипник 205 ГОСТ 8338-75");
    EXPECT_EQ(bdef->elementMap->bodyNames(), brg.elementMap->bodyNames());
    EXPECT_EQ(bdef->properties.sectionCut, std::optional<bool>(true));
    auto again = back.solve();
    ASSERT_TRUE(again.ok());
    EXPECT_EQ(again.value().totalDof, 2);
}

// ---------------------------------------------------------------- шпонки

TEST(KeyTable, RowsDepthsAndLengths) {
    ASSERT_EQ(keyTable().size(), kKeys.size());
    for (const Gost23360& t : kKeys) {
        for (double d : {t.over + 0.5, t.upTo}) {
            auto r = keyRowForShaft(d);
            ASSERT_TRUE(r.ok()) << d;
            EXPECT_DOUBLE_EQ(r.value().b, t.b) << d;
            EXPECT_DOUBLE_EQ(r.value().h, t.h) << d;
            EXPECT_DOUBLE_EQ(r.value().t1, t.t1) << d;
            EXPECT_DOUBLE_EQ(r.value().t2, t.t2) << d;
            EXPECT_DOUBLE_EQ(r.value().sMin, t.smin) << d;
            EXPECT_DOUBLE_EQ(r.value().sMax, t.smax) << d;
            EXPECT_DOUBLE_EQ(r.value().lMin, t.lmin) << d;
            EXPECT_DOUBLE_EQ(r.value().lMax, t.lmax) << d;
            auto depth = keySeatDepths(d).value();
            EXPECT_DOUBLE_EQ(depth.t1, t.t1);
            EXPECT_DOUBLE_EQ(depth.t2, t.t2);
            EXPECT_DOUBLE_EQ(depth.b, t.b);
        }
        // Паз вала и втулки вместе глубже высоты шпонки — радиальный зазор над шпонкой.
        EXPECT_GT(t.t1 + t.t2, t.h);
        EXPECT_TRUE(keyRow(t.b, t.h).ok());
    }
    // Граница диапазона: 22 — ещё 6×6, свыше 22 — 8×7.
    EXPECT_DOUBLE_EQ(keyRowForShaft(22).value().b, 6);
    EXPECT_DOUBLE_EQ(keyRowForShaft(22.01).value().b, 8);
    EXPECT_DOUBLE_EQ(keySeatDepths(35).value().t1, 5.0);
    EXPECT_DOUBLE_EQ(keySeatDepths(35).value().t2, 3.3);
    for (double d : {6.0, 5.0, 110.5}) {
        auto r = keyRowForShaft(d);
        ASSERT_FALSE(r.ok()) << d;
        EXPECT_EQ(r.error().code, ErrorCode::Unsupported);
    }
    EXPECT_FALSE(keyRow(10, 7).ok());
    const std::vector<double> series{6,  8,  10, 12, 14, 16,  18,  20,  22,  25,  28,  32,  36,  40,  45,  50,  56,  63,
                                     70, 80, 90, 100, 110, 125, 140, 160, 180, 200, 220, 250, 280, 320, 360, 400, 450, 500};
    EXPECT_EQ(keyLengthSeries(), series);
    for (const Gost23360& t : kKeys) {
        EXPECT_NE(std::find(series.begin(), series.end(), t.lmin), series.end());
        EXPECT_NE(std::find(series.begin(), series.end(), t.lmax), series.end());
    }
    EXPECT_FALSE(keyTableInfo().verified);
}

TEST(KeyModel, VolumesOfAllExecutions) {
    for (const Gost23360& t : kKeys) {
        for (KeyExecution e : {KeyExecution::Rounded, KeyExecution::Flat, KeyExecution::OneRounded}) {
            for (double s : {t.smin, t.smax}) {
                auto k = makeKey({e, t.b, t.h, t.lmin, s});
                ASSERT_TRUE(k.ok()) << t.b << " exec " << int(e) << ": " << k.error().message;
                const double f = keyVolume(e, t.b, t.h, t.lmin, s);
                EXPECT_NEAR(volume(k.value().shape), f, 1e-9 * f) << t.b << "x" << t.h << " exec " << int(e);
                EXPECT_NEAR(k.value().properties.mass, f * 1e-9 * 7850.0, 1e-9 * f * 1e-9 * 7850.0);
                EXPECT_EQ(solids(k.value().shape), 1);
                Bnd_Box box;
                BRepBndLib::AddOptimal(k.value().shape, box, false, false);
                double x0, y0, z0, x1, y1, z1;
                box.Get(x0, y0, z0, x1, y1, z1);
                EXPECT_NEAR(x1 - x0, t.lmin, 1e-6);
                EXPECT_NEAR(y1 - y0, t.b, 1e-6);
                EXPECT_NEAR(z1 - z0, t.h, 1e-6);
            }
        }
    }
}

TEST(KeyModel, FacesDesignationAndErrors) {
    auto k = makeKey({KeyExecution::Rounded, 10, 8, 32, std::nullopt});
    ASSERT_TRUE(k.ok()) << k.error().message;
    EXPECT_EQ(k.value().properties.designation, "Шпонка 10×8×32 ГОСТ 23360-78");
    EXPECT_DOUBLE_EQ(k.value().s, 0.4);  // наименьшая фаска сечения 10×8
    EXPECT_DOUBLE_EQ(*k.value().dimension("t1"), 5.0);
    const auto& map = *k.value().elementMap;
    auto surf = [&](const std::string& role) {
        auto i = map.find(bendcore::modeling::ElementKind::Face, keyFaceName(role));
        EXPECT_TRUE(i.has_value()) << role;
        return BRepAdaptor_Surface(TopoDS::Face(map.element(bendcore::modeling::ElementKind::Face, *i)));
    };
    EXPECT_NEAR(surf(key_roles::kTop).Plane().Location().Z(), 8, 1e-9);
    EXPECT_NEAR(surf(key_roles::kBottom).Plane().Location().Z(), 0, 1e-9);
    EXPECT_NEAR(std::abs(surf("Side1").Plane().Location().Y()), 5, 1e-9);
    EXPECT_EQ(surf("End1").GetType(), GeomAbs_Cylinder);
    EXPECT_NEAR(surf("End2").Cylinder().Radius(), 5, 1e-9);
    for (const auto& rec : map.records(bendcore::modeling::ElementKind::Face)) {
        bool has = false;
        for (const auto& [key, v] : rec.attributes) has = has || (key == "key.designation" && v == "Шпонка 10×8×32 ГОСТ 23360-78");
        EXPECT_TRUE(has) << rec.name;
    }
    auto flat = makeKey({KeyExecution::Flat, 8, 7, 32, 0.3});
    ASSERT_TRUE(flat.ok());
    EXPECT_EQ(flat.value().properties.designation, "Шпонка 2-8×7×32 ГОСТ 23360-78");
    auto fm = flat.value().elementMap;
    EXPECT_EQ(BRepAdaptor_Surface(TopoDS::Face(fm->element(bendcore::modeling::ElementKind::Face,
                                                            *fm->find(bendcore::modeling::ElementKind::Face, keyFaceName("End1")))))
                  .GetType(),
              GeomAbs_Plane);
    EXPECT_EQ(makeKey({KeyExecution::OneRounded, 6, 6, 20, std::nullopt}).value().properties.designation, "Шпонка 3-6×6×20 ГОСТ 23360-78");
    // Ошибки: длина не из ряда, длина вне диапазона сечения, фаска вне диапазона, нет сечения.
    EXPECT_EQ(makeKey({KeyExecution::Rounded, 10, 8, 33, std::nullopt}).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(makeKey({KeyExecution::Rounded, 10, 8, 20, std::nullopt}).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(makeKey({KeyExecution::Rounded, 10, 8, 32, 0.8}).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(makeKey({KeyExecution::Rounded, 10, 7, 32, std::nullopt}).error().code, ErrorCode::Unsupported);
}
