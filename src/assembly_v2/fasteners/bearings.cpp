// Каталог шариковых радиальных однорядных подшипников ГОСТ 8338-75 / ISO 15 и их модели
// (docs/assembly/FASTENERS.md, раздел 8): кольца — тела вращения профиля с желобом дорожки
// качения и скруглениями r монтажных кромок, шарики — сферы на окружности Dpw.

#include <bendcore/assembly_v2/bearings.hpp>

#include "part_model.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax1.hxx>

#include <cmath>
#include <map>
#include <numbers>

namespace bendcore::assembly_v2::fasteners {

using kernel::ErrorCode;
using namespace detail;

namespace {

// Таблица ISO 15 (d, D, B, r_s min) по серии; номер — две последние цифры обозначения.
struct CatalogueRow {
    int code;  // 0…12
    double D, B, r;
};

// Серия 10 (ГОСТ 8338: особо лёгкая 100), 02 (лёгкая 200), 03 (средняя 300).
const std::map<BearingSeries, std::vector<CatalogueRow>>& catalogue() {
    static const std::map<BearingSeries, std::vector<CatalogueRow>> t{
        {BearingSeries::ExtraLight,
         {{0, 26, 8, 0.3}, {1, 28, 8, 0.3}, {2, 32, 9, 0.3}, {3, 35, 10, 0.3}, {4, 42, 12, 0.6},
          {5, 47, 12, 0.6}, {6, 55, 13, 1}, {7, 62, 14, 1}, {8, 68, 15, 1}, {9, 75, 16, 1},
          {10, 80, 16, 1}, {11, 90, 18, 1.1}, {12, 95, 18, 1.1}}},
        {BearingSeries::Light,
         {{0, 30, 9, 0.6}, {1, 32, 10, 0.6}, {2, 35, 11, 0.6}, {3, 40, 12, 0.6}, {4, 47, 14, 1},
          {5, 52, 15, 1}, {6, 62, 16, 1}, {7, 72, 17, 1.1}, {8, 80, 18, 1.1}, {9, 85, 19, 1.1},
          {10, 90, 20, 1.1}, {11, 100, 21, 1.5}, {12, 110, 22, 1.5}}},
        {BearingSeries::Medium,
         {{0, 35, 11, 0.6}, {1, 37, 12, 1}, {2, 42, 13, 1}, {3, 47, 14, 1}, {4, 52, 15, 1.1},
          {5, 62, 17, 1.1}, {6, 72, 19, 1.1}, {7, 80, 21, 1.5}, {8, 90, 23, 1.5}, {9, 100, 25, 1.5},
          {10, 110, 27, 2}, {11, 120, 29, 2}, {12, 130, 31, 2.1}}},
    };
    return t;
}

// Диаметр отверстия по двум последним цифрам обозначения (ГОСТ 3189: 00 — 10, 01 — 12,
// 02 — 15, 03 — 17, от 04 — 5·n).
double boreOf(int code) {
    switch (code) {
        case 0: return 10;
        case 1: return 12;
        case 2: return 15;
        case 3: return 17;
        default: return 5.0 * code;
    }
}

std::string numbersText() {
    std::string s;
    for (int n : ballBearingNumbers()) s += (s.empty() ? "" : ", ") + std::to_string(n);
    return s;
}

BallBearingRow rowOf(BearingSeries series, const CatalogueRow& c) {
    BallBearingRow r;
    r.series = series;
    r.number = static_cast<int>(series) * 100 + c.code;
    const char* isoSeries = series == BearingSeries::ExtraLight ? "60" : series == BearingSeries::Light ? "62" : "63";
    char buf[16];
    std::snprintf(buf, sizeof buf, "%s%02d", isoSeries, c.code);
    r.iso = buf;
    r.d = boreOf(c.code);
    r.D = c.D;
    r.B = c.B;
    r.r = c.r;
    return r;
}

constexpr double kBallRatio = 0.3;        // Dw = 0,3·(D − d)
constexpr double kBallSpacing = 1.6;      // шаг шариков по окружности Dpw не меньше 1,6·Dw
constexpr double kGrooveRatio = 0.52;     // радиус желоба 0,52·Dw
constexpr double kShoulderRatio = 0.3;    // борта на Dpw/2 ∓ 0,3·Dw
constexpr double kClearance = 0.02;       // радиальный зазор, мм (сумма по двум дорожкам)

// Ребро профиля в плоскости XZ (x — радиус, z — ось).
TopoDS_Edge segment(double r0, double z0, double r1, double z1) {
    BRepBuilderAPI_MakeEdge e(gp_Pnt(r0, 0, z0), gp_Pnt(r1, 0, z1));
    if (!e.IsDone()) partFail(ErrorCode::OperationFailed, "bearing profile edge failed");
    return e.Edge();
}

TopoDS_Edge arc(double r0, double z0, double rm, double zm, double r1, double z1) {
    GC_MakeArcOfCircle a(gp_Pnt(r0, 0, z0), gp_Pnt(rm, 0, zm), gp_Pnt(r1, 0, z1));
    if (!a.IsDone()) partFail(ErrorCode::OperationFailed, "bearing profile arc failed");
    BRepBuilderAPI_MakeEdge e(a.Value());
    if (!e.IsDone()) partFail(ErrorCode::OperationFailed, "bearing profile edge failed");
    return e.Edge();
}

TopoDS_Shape revolveEdges(const std::vector<TopoDS_Edge>& edges) {
    BRepBuilderAPI_MakeWire w;
    for (const TopoDS_Edge& e : edges) w.Add(e);
    if (!w.IsDone()) partFail(ErrorCode::OperationFailed, "bearing profile wire failed");
    BRepBuilderAPI_MakeFace f(w.Wire(), true);
    if (!f.IsDone()) partFail(ErrorCode::OperationFailed, "bearing profile face failed");
    BRepPrimAPI_MakeRevol rev(f.Face(), gp_Ax1(gp::Origin(), gp::DZ()), 2 * std::numbers::pi);
    rev.Build();
    if (!rev.IsDone()) partFail(ErrorCode::OperationFailed, "bearing ring revolution failed");
    requireValid(rev.Shape(), "bearing ring");
    return rev.Shape();
}

// Внутреннее кольцо: ⌀d … ⌀innerShoulder, желоб — дуга радиуса grooveRadius с дном на
// innerRaceway/2 посередине ширины, скругления r кромок отверстия.
TopoDS_Shape innerRing(const BallBearingRow& row, const BallBearingInternals& in) {
    const double a = row.d / 2, rs = in.innerShoulder / 2, rb = in.innerRaceway / 2, B = row.B, r = row.r;
    const double c = rb + in.grooveRadius;  // центр желоба
    const double w = std::sqrt(in.grooveRadius * in.grooveRadius - (c - rs) * (c - rs));
    const double k = r * (1 - std::numbers::sqrt2 / 2);
    return revolveEdges({segment(a + r, 0, rs, 0), segment(rs, 0, rs, B / 2 - w), arc(rs, B / 2 - w, rb, B / 2, rs, B / 2 + w),
                         segment(rs, B / 2 + w, rs, B), segment(rs, B, a + r, B), arc(a + r, B, a + k, B - k, a, B - r),
                         segment(a, B - r, a, r), arc(a, r, a + k, k, a + r, 0)});
}

// Наружное кольцо: ⌀outerShoulder … ⌀D, желоб с дном на outerRaceway/2, скругления r наружных кромок.
TopoDS_Shape outerRing(const BallBearingRow& row, const BallBearingInternals& in) {
    const double A = row.D / 2, rs = in.outerShoulder / 2, rb = in.outerRaceway / 2, B = row.B, r = row.r;
    const double c = rb - in.grooveRadius;
    const double w = std::sqrt(in.grooveRadius * in.grooveRadius - (rs - c) * (rs - c));
    const double k = r * (1 - std::numbers::sqrt2 / 2);
    return revolveEdges({segment(rs, 0, A - r, 0), arc(A - r, 0, A - k, k, A, r), segment(A, r, A, B - r),
                         arc(A, B - r, A - k, B - k, A - r, B), segment(A - r, B, rs, B), segment(rs, B, rs, B / 2 + w),
                         arc(rs, B / 2 + w, rb, B / 2, rs, B / 2 - w), segment(rs, B / 2 - w, rs, 0)});
}

std::string designationOf(const BallBearingRow& row) { return "Подшипник " + std::to_string(row.number) + " ГОСТ 8338-75"; }

bool near(double a, double b) { return std::abs(a - b) <= 1e-6 * std::max(1.0, std::abs(b)); }

double polarAngle(const gp_Pnt& p) {
    double a = std::atan2(p.Y(), p.X());
    if (a < -1e-9) a += 2 * std::numbers::pi;
    return a;
}

bool parseNumber(const std::string& s, double& out) {
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return !s.empty() && end == s.c_str() + s.size() && std::isfinite(out);
}

}  // namespace

std::vector<int> ballBearingNumbers() {
    std::vector<int> out;
    for (const auto& [series, rows] : catalogue())
        for (const CatalogueRow& c : rows) out.push_back(static_cast<int>(series) * 100 + c.code);
    return out;
}

kernel::Result<BallBearingRow> ballBearingRow(int number) {
    const int s = number / 100, code = number % 100;
    for (const auto& [series, rows] : catalogue()) {
        if (static_cast<int>(series) != s) continue;
        for (const CatalogueRow& c : rows)
            if (c.code == code) return rowOf(series, c);
    }
    return kernel::Result<BallBearingRow>::failure(
        ErrorCode::Unsupported, "ball bearing " + std::to_string(number) + " is not in the catalogue (GOST 8338: " + numbersText() + ")");
}

kernel::Result<BallBearingRow> ballBearingRow(BearingSeries series, double d) {
    auto it = catalogue().find(series);
    if (it != catalogue().end())
        for (const CatalogueRow& c : it->second)
            if (std::abs(boreOf(c.code) - d) < 1e-9) return rowOf(series, c);
    return kernel::Result<BallBearingRow>::failure(ErrorCode::Unsupported,
                                                   "ball bearing with bore " + partNum(d) + " mm is not in the catalogue series " +
                                                       std::to_string(static_cast<int>(series) * 100) + " (GOST 8338: " +
                                                       numbersText() + ")");
}

BearingTableInfo bearingTableInfo() {
    return {"ГОСТ 8338-75 / ISO 15",
            "ISO 15 (серии размеров 10, 02, 03; d, D, B, r_s min) по памяти; ГОСТ 8338-75 — те же d, D, B",
            "требует сверки с официальным изданием (ГОСТ 8338-75 приводит r по своей таблице — сверить с r_s min ISO 15)",
            false};
}

kernel::Result<BallBearingInternals> ballBearingInternals(const BallBearingRow& row) {
    BallBearingInternals in;
    in.Dw = kBallRatio * (row.D - row.d);
    in.Dpw = (row.d + row.D) / 2;
    in.Z = static_cast<int>(std::floor(std::numbers::pi * in.Dpw / (kBallSpacing * in.Dw)));
    in.grooveRadius = kGrooveRatio * in.Dw;
    in.clearance = kClearance;
    in.innerShoulder = in.Dpw - 2 * kShoulderRatio * in.Dw;
    in.outerShoulder = in.Dpw + 2 * kShoulderRatio * in.Dw;
    in.innerRaceway = in.Dpw - in.Dw - in.clearance;
    in.outerRaceway = in.Dpw + in.Dw + in.clearance;
    // Желоб не доходит до скруглений r, кольца не тоньше r под желобом.
    const double depth = (in.innerShoulder - in.innerRaceway) / 2;
    const double halfWidth = std::sqrt(in.grooveRadius * in.grooveRadius - (in.grooveRadius - depth) * (in.grooveRadius - depth));
    if (!(halfWidth + row.r < row.B / 2) || !(in.innerRaceway / 2 - row.d / 2 > row.r) || !(row.D / 2 - in.outerRaceway / 2 > row.r) ||
        in.Z < 3)
        return kernel::Result<BallBearingInternals>::failure(ErrorCode::InvalidInput,
                                                             "bearing " + std::to_string(row.number) + ": rings are too thin for the raceways");
    return in;
}

std::string bearingFaceName(const std::string& role) { return std::string(bearing_roles::kFeature) + ":" + role; }

modeling::ElementAttributes bearingAttributes(const BearingAttributes& a) {
    return {{"bearing.type", a.type},       {"bearing.standard", a.standard}, {"bearing.designation", a.designation},
            {"bearing.number", a.number},   {"bearing.iso", a.iso},           {"bearing.d", partNum(a.d)},
            {"bearing.D", partNum(a.D)},    {"bearing.B", partNum(a.B)},      {"bearing.r", partNum(a.r)},
            {"bearing.Dw", partNum(a.Dw)},  {"bearing.Dpw", partNum(a.Dpw)},  {"bearing.Z", std::to_string(a.Z)}};
}

kernel::Result<BearingAttributes> bearingFromAttributes(const modeling::ElementAttributes& attributes) {
    std::map<std::string, std::string> m(attributes.begin(), attributes.end());
    BearingAttributes a;
    for (const char* key : {"bearing.type", "bearing.standard", "bearing.designation", "bearing.number", "bearing.iso", "bearing.d",
                            "bearing.D", "bearing.B", "bearing.r", "bearing.Dw", "bearing.Dpw", "bearing.Z"})
        if (!m.count(key)) return kernel::Result<BearingAttributes>::failure(ErrorCode::InvalidInput, std::string("bearing attribute '") + key + "' is missing");
    a.type = m["bearing.type"];
    a.standard = m["bearing.standard"];
    a.designation = m["bearing.designation"];
    a.number = m["bearing.number"];
    a.iso = m["bearing.iso"];
    double z = 0;
    struct NumKey {
        const char* key;
        double* value;
    };
    for (NumKey k : {NumKey{"bearing.d", &a.d}, NumKey{"bearing.D", &a.D}, NumKey{"bearing.B", &a.B}, NumKey{"bearing.r", &a.r},
                     NumKey{"bearing.Dw", &a.Dw}, NumKey{"bearing.Dpw", &a.Dpw}, NumKey{"bearing.Z", &z}})
        if (!parseNumber(m[k.key], *k.value))
            return kernel::Result<BearingAttributes>::failure(ErrorCode::InvalidInput,
                                                              std::string(k.key) + " is not a number: '" + m[k.key] + "'");
    a.Z = static_cast<int>(z);
    return a;
}

std::optional<BearingAttributes> bearingOf(const modeling::NamedShape& shape) {
    for (const modeling::ElementRecord& r : shape.records(modeling::ElementKind::Face)) {
        bool has = false;
        for (const auto& [k, v] : r.attributes) has = has || k.rfind("bearing.", 0) == 0;
        if (!has) continue;
        auto a = bearingFromAttributes(r.attributes);
        if (a) return a.value();
    }
    return std::nullopt;
}

std::optional<double> Bearing::dimension(const std::string& key) const { return findDimension(dimensions, key); }

kernel::Result<Bearing> makeBallBearing(int number) {
    auto row = ballBearingRow(number);
    if (!row) return row.error();
    auto in = ballBearingInternals(row.value());
    if (!in) return in.error();
    try {
        Bearing b;
        b.row = row.value();
        b.internals = in.value();
        const BallBearingRow& R = b.row;
        const BallBearingInternals& I = b.internals;
        BRep_Builder builder;
        TopoDS_Compound c;
        builder.MakeCompound(c);
        builder.Add(c, outerRing(R, I));
        builder.Add(c, innerRing(R, I));
        for (int k = 0; k < I.Z; ++k) {
            const double a = 2 * std::numbers::pi * k / I.Z;
            BRepPrimAPI_MakeSphere sphere(gp_Pnt(I.Dpw / 2 * std::cos(a), I.Dpw / 2 * std::sin(a), R.B / 2), I.Dw / 2);
            sphere.Build();
            if (!sphere.IsDone()) partFail(ErrorCode::OperationFailed, "bearing ball failed");
            builder.Add(c, sphere.Shape());
        }
        b.shape = c;
        b.attributes = {"ball-radial", "ГОСТ 8338-75", designationOf(R), std::to_string(R.number), R.iso, R.d, R.D, R.B, R.r,
                        I.Dw, I.Dpw, I.Z};
        b.elementMap = namePartFaces(
            b.shape, bearing_roles::kFeature,
            [&](const PartFace& g) -> FaceRole {
                const bool inner = g.body == 1;  // тела: 0 — наружное кольцо, 1 — внутреннее, далее шарики
                switch (g.type) {
                    case GeomAbs_Plane:
                        if (std::abs(g.normal.Z()) < 1 - 1e-9) return {};
                        return {inner ? bearing_roles::kInnerFace : bearing_roles::kOuterFace, true, g.centroid.Z()};
                    case GeomAbs_Cylinder:
                        if (near(g.radius, R.d / 2)) return {bearing_roles::kBore};
                        if (near(g.radius, R.D / 2)) return {bearing_roles::kOuter};
                        if (near(g.radius, I.innerShoulder / 2)) return {bearing_roles::kInnerShoulder, true, g.centroid.Z()};
                        if (near(g.radius, I.outerShoulder / 2)) return {bearing_roles::kOuterShoulder, true, g.centroid.Z()};
                        return {};
                    case GeomAbs_Torus:
                        if (near(g.radius, I.grooveRadius)) return {inner ? bearing_roles::kInnerRaceway : bearing_roles::kOuterRaceway};
                        if (near(g.radius, R.r)) return {inner ? bearing_roles::kBoreRound : bearing_roles::kOuterRound, true, g.centroid.Z()};
                        return {};
                    case GeomAbs_Sphere: return {bearing_roles::kBall, true, polarAngle(g.centroid)};
                    default: return {};
                }
            },
            {bearing_roles::kBore, bearing_roles::kOuter, "InnerFace1", "InnerFace2", "OuterFace1", "OuterFace2",
             bearing_roles::kInnerRaceway, bearing_roles::kOuterRaceway, "InnerShoulder1", "InnerShoulder2", "OuterShoulder1",
             "OuterShoulder2", "BoreRound1", "BoreRound2", "OuterRound1", "OuterRound2", "Ball1", "Ball" + std::to_string(I.Z)},
            bearingAttributes(b.attributes));
        b.properties.name = "Подшипник";
        b.properties.designation = designationOf(R);
        b.properties.equivalentDesignation = "Подшипник " + R.iso + " ISO 15";
        b.properties.material = "Сталь ШХ15 ГОСТ 801-78";
        b.properties.density = kBearingSteelDensity;
        b.properties.volume = shapeVolume(b.shape);
        b.properties.mass = b.properties.volume * 1e-9 * b.properties.density;
        b.dimensions = {{"d", R.d},   {"D", R.D},     {"B", R.B}, {"r", R.r},
                        {"Dw", I.Dw}, {"Dpw", I.Dpw}, {"Z", static_cast<double>(I.Z)}};
        return b;
    } catch (const PartFailure& e) {
        return kernel::Result<Bearing>::failure(e.code, e.message);
    } catch (const Standard_Failure& e) {
        return kernel::Result<Bearing>::failure(ErrorCode::OperationFailed, std::string("OCCT failure while building bearing: ") +
                                                                                (e.GetMessageString() ? e.GetMessageString() : ""));
    }
}

PartData toPartData(const Bearing& bearing) { return PartData{bearing.shape, bearing.elementMap}; }

PartProperties toPartProperties(const Bearing& bearing) {
    PartProperties p;
    p.name = bearing.properties.designation;
    p.material = bearing.properties.material;
    p.density = bearing.properties.density;
    p.sectionCut = true;
    return p;
}

kernel::Result<DefinitionId> addBearingPart(AssemblyDocument& document, const Bearing& bearing) {
    return document.addPart(bearing.properties.designation, toPartData(bearing), toPartProperties(bearing));
}

}  // namespace bendcore::assembly_v2::fasteners
