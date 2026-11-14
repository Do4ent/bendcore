// Параметрические твердотельные модели крепежа (docs/assembly/FASTENERS.md, раздел 3):
// тела вращения по профилю (BRepPrimAPI_MakeRevol), шестигранники (BRepPrimAPI_MakePrism),
// булевы операции OCCT; роли граней определяются по геометрии готового тела и записываются
// в карту элементов (modeling::buildNamedShape).

#include <bendcore/assembly_v2/fasteners.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <GProp_GProps.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax1.hxx>
#include <gp_Cone.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <numbers>
#include <sstream>

namespace bendcore::assembly_v2::fasteners {

using kernel::ErrorCode;

namespace {

constexpr double kTan30 = 0.57735026918962576;  // tan 30°
// Условная фаска головки и гайки: конус 30° к торцу, начинается на окружности 0,95·s
// (стандарты задают форму фаски диапазоном; значение выбрано внутри него и не табличное).
constexpr double kChamferStartRatio = 0.95;
// Условная фаска верхней кромки головки винта ISO 4762: 45° × 0,1·d (форма кромки
// стандартом не нормируется размером).
constexpr double kSocketHeadChamferRatio = 0.1;

struct Failure {
    ErrorCode code;
    std::string message;
};

[[noreturn]] void fail(ErrorCode code, std::string message) { throw Failure{code, std::move(message)}; }

std::string num(double v) {
    std::ostringstream os;
    os << std::setprecision(10) << v;
    return os.str();
}

// Число для обозначения: без лишних нулей, десятичный разделитель — запятая (ГОСТ) или точка (ISO).
std::string designationNumber(double v, bool gost) {
    std::ostringstream os;
    os << std::setprecision(6) << v;
    std::string s = os.str();
    if (gost) std::replace(s.begin(), s.end(), '.', ',');
    return s;
}

// Тело вращения вокруг Z замкнутого профиля (r, z) в плоскости XZ.
TopoDS_Shape revolve(const std::vector<std::pair<double, double>>& profile) {
    BRepBuilderAPI_MakePolygon poly;
    for (const auto& [r, z] : profile) poly.Add(gp_Pnt(r, 0, z));
    poly.Close();
    if (!poly.IsDone()) fail(ErrorCode::OperationFailed, "fastener profile polygon failed");
    BRepBuilderAPI_MakeFace face(poly.Wire(), true);
    if (!face.IsDone()) fail(ErrorCode::OperationFailed, "fastener profile face failed");
    BRepPrimAPI_MakeRevol rev(face.Face(), gp_Ax1(gp::Origin(), gp::DZ()), 2 * std::numbers::pi);
    rev.Build();
    if (!rev.IsDone()) fail(ErrorCode::OperationFailed, "fastener revolution failed");
    return rev.Shape();
}

// Правильная шестигранная призма: размер под ключ s, грани с нормалями под углами 0°, 60°, …
TopoDS_Shape hexPrism(double s, double z0, double height) {
    const double R = s / std::sqrt(3.0);  // радиус описанной окружности = e/2
    BRepBuilderAPI_MakePolygon poly;
    for (int i = 0; i < 6; ++i) {
        const double a = std::numbers::pi / 6 + i * std::numbers::pi / 3;
        poly.Add(gp_Pnt(R * std::cos(a), R * std::sin(a), z0));
    }
    poly.Close();
    BRepBuilderAPI_MakeFace face(poly.Wire(), true);
    if (!face.IsDone()) fail(ErrorCode::OperationFailed, "hexagon face failed");
    BRepPrimAPI_MakePrism prism(face.Face(), gp_Vec(0, 0, height));
    prism.Build();
    if (!prism.IsDone()) fail(ErrorCode::OperationFailed, "hexagon prism failed");
    return prism.Shape();
}

template <typename Op>
TopoDS_Shape boolean(const TopoDS_Shape& a, const TopoDS_Shape& b, const char* what) {
    Op op(a, b);
    if (!op.IsDone() || op.HasErrors()) fail(ErrorCode::OperationFailed, std::string("fastener ") + what + " failed");
    return op.Shape();
}

// Объединение граней одной поверхности и единственное тело результата.
TopoDS_Shape finish(const TopoDS_Shape& shape) {
    ShapeUpgrade_UnifySameDomain unify(shape, true, true, false);
    unify.Build();
    TopoDS_Shape out = unify.Shape();
    TopoDS_Shape solid;
    int solids = 0;
    for (TopExp_Explorer ex(out, TopAbs_SOLID); ex.More(); ex.Next()) {
        solid = ex.Current();
        ++solids;
    }
    if (solids != 1) fail(ErrorCode::OperationFailed, "fastener body has " + std::to_string(solids) + " solids");
    BRepCheck_Analyzer check(solid);
    if (!check.IsValid()) fail(ErrorCode::OperationFailed, "fastener body is not valid (BRepCheck)");
    return solid;
}

// Грань с геометрией для определения роли.
struct FaceGeom {
    TopoDS_Face face;
    GeomAbs_SurfaceType type = GeomAbs_OtherSurface;
    gp_Pnt centroid;
    gp_Dir normal;        // плоскость: наружная нормаль
    double radius = 0.0;  // цилиндр
};

std::vector<FaceGeom> facesOf(const TopoDS_Shape& shape) {
    std::vector<FaceGeom> out;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        FaceGeom g;
        g.face = TopoDS::Face(ex.Current());
        BRepAdaptor_Surface s(g.face, true);
        g.type = s.GetType();
        GProp_GProps props;
        BRepGProp::SurfaceProperties(g.face, props);
        g.centroid = props.CentreOfMass();
        if (g.type == GeomAbs_Plane) {
            gp_Dir n = s.Plane().Axis().Direction();
            if (!s.Plane().Position().Direct()) n.Reverse();
            if (g.face.Orientation() == TopAbs_REVERSED) n.Reverse();
            g.normal = n;
        } else if (g.type == GeomAbs_Cylinder) {
            g.radius = s.Cylinder().Radius();
        }
        out.push_back(g);
    }
    return out;
}

bool near(double a, double b) { return std::abs(a - b) <= 1e-6 * std::max(1.0, std::abs(b)); }
bool axial(const FaceGeom& g) { return g.type == GeomAbs_Plane && std::abs(g.normal.Z()) > 1 - 1e-9; }
bool lateral(const FaceGeom& g) { return g.type == GeomAbs_Plane && std::abs(g.normal.Z()) < 1e-9; }
double polarAngle(const gp_Dir& n) {
    double a = std::atan2(n.Y(), n.X());
    if (a < -1e-9) a += 2 * std::numbers::pi;
    return a;
}

// Назначает роли граням: classify возвращает роль или пусто; грани сторон шестигранника
// и разреза нумеруются по углу/координате. Роль, которую получили две грани, — ошибка.
std::shared_ptr<const modeling::NamedShape> nameFaces(
    const TopoDS_Shape& shape, const std::function<std::string(const FaceGeom&)>& classify,
    const std::vector<std::string>& required) {
    std::vector<FaceGeom> faces = facesOf(shape);
    std::vector<modeling::ElementRole> roles;
    std::map<std::string, std::vector<const FaceGeom*>> numbered;
    std::map<std::string, int> seen;
    for (const FaceGeom& g : faces) {
        std::string role = classify(g);
        if (role.empty()) continue;
        if (role == roles::kFlat || role == roles::kSocket || role == roles::kSlot || role == roles::kEnd ||
            role == roles::kEndChamfer) {
            numbered[role].push_back(&g);
            continue;
        }
        if (++seen[role] > 1) fail(ErrorCode::OperationFailed, "fastener face role '" + role + "' is not unique");
        roles.push_back({g.face, role});
    }
    for (auto& [role, list] : numbered) {
        std::sort(list.begin(), list.end(), [&](const FaceGeom* a, const FaceGeom* b) {
            if (role == roles::kSlot) return a->centroid.Y() < b->centroid.Y();
            if (role == roles::kEnd || role == roles::kEndChamfer) return a->centroid.Z() < b->centroid.Z();
            return polarAngle(a->normal) < polarAngle(b->normal);
        });
        for (std::size_t i = 0; i < list.size(); ++i) {
            roles.push_back({list[i]->face, role + std::to_string(i + 1)});
            seen[role + std::to_string(i + 1)] = 1;
        }
    }
    for (const std::string& r : required) {
        if (seen.count(r) == 0) fail(ErrorCode::OperationFailed, "fastener face role '" + r + "' not found");
    }
    modeling::NamingRequest req;
    req.featureId = roles::kFeature;
    req.result = shape;
    req.roles = std::move(roles);
    auto named = modeling::buildNamedShape(req);
    if (!named) fail(named.error().code, "fastener naming: " + named.error().message);
    return std::make_shared<const modeling::NamedShape>(std::move(named).value());
}

template <typename T>
T unwrap(kernel::Result<T> r) {
    if (!r) fail(r.error().code, r.error().message);
    return std::move(r).value();
}

// Шестигранник s высотой h от z0 с фасками 30° на выбранных торцах; внутреннее отверстие
// радиуса bore (0 — без отверстия).
TopoDS_Shape chamferedHex(double s, double z0, double h, bool chamferBottom, bool chamferTop, double bore) {
    const double Rb = s / std::sqrt(3.0) * 1.02;  // за пределами углов шестигранника
    const double rt = kChamferStartRatio * s / 2;
    const double dz = (Rb - rt) * kTan30;
    if (2 * dz >= h) fail(ErrorCode::InvalidInput, "hex height " + num(h) + " is too small for the 30° chamfer");
    std::vector<std::pair<double, double>> p;
    p.emplace_back(bore, z0);
    if (chamferBottom) {
        p.emplace_back(rt, z0);
        p.emplace_back(Rb, z0 + dz);
    } else {
        p.emplace_back(Rb, z0);
    }
    if (chamferTop) {
        p.emplace_back(Rb, z0 + h - dz);
        p.emplace_back(rt, z0 + h);
    } else {
        p.emplace_back(Rb, z0 + h);
    }
    p.emplace_back(bore, z0 + h);
    return boolean<BRepAlgoAPI_Common>(hexPrism(s, z0, h), revolve(p), "hex chamfer");
}

// Стержень радиуса d/2 от z = −l до 0 с фаской 45° до внутреннего диаметра резьбы на конце.
TopoDS_Shape shank(double d, double d3, double l) {
    const double c = (d - d3) / 2;
    return revolve({{0, -l}, {d3 / 2, -l}, {d / 2, -l + c}, {d / 2, 0}, {0, 0}});
}

double volumeOf(const TopoDS_Shape& s) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(s, props);
    return props.Mass();
}

void setThreadExternal(Fastener& f, double d, double pitch, double length) {
    f.thread.present = true;
    f.thread.internal = false;
    f.thread.nominalDiameter = d;
    f.thread.pitch = pitch;
    f.thread.minorDiameter = externalMinorDiameter(d, pitch);
    f.thread.length = length;
    f.thread.designation = "M" + designationNumber(d, false);
}

// Длина резьбы b болта ISO 4014 / ГОСТ 7798 по длине l (ISO 4014: 2d+6 при l ≤ 125,
// 2d+12 при l ≤ 200, 2d+25 свыше; требует сверки).
double boltThreadLength(int d, double l) {
    if (l <= 125) return 2.0 * d + 6;
    if (l <= 200) return 2.0 * d + 12;
    return 2.0 * d + 25;
}

std::string boltDesignation(FastenerStandard st, int d, double l) {
    if (isGost(st)) {
        return "Болт М" + std::to_string(d) + "×" + designationNumber(l, true) + " " + toString(st);
    }
    return std::string("Болт ") + toString(st) + " - M" + std::to_string(d) + " x " + designationNumber(l, false);
}

std::string screwDesignation(FastenerStandard st, int d, double l) {
    if (isGost(st)) {
        return "Винт М" + std::to_string(d) + "×" + designationNumber(l, true) + " " + toString(st);
    }
    return std::string("Винт ") + toString(st) + " - M" + std::to_string(d) + " x " + designationNumber(l, false);
}

std::string nutDesignation(FastenerStandard st, int d) {
    if (isGost(st)) return "Гайка М" + std::to_string(d) + " " + toString(st);
    return std::string("Гайка ") + toString(st) + " - M" + std::to_string(d);
}

std::string washerDesignation(FastenerStandard st, int d) {
    if (st == FastenerStandard::Gost6402) return "Шайба " + std::to_string(d) + " 65Г " + toString(st);
    if (isGost(st)) return "Шайба " + std::to_string(d) + " " + toString(st);
    return std::string("Шайба ") + toString(st) + " - " + std::to_string(d);
}

std::string pinDesignation(FastenerStandard st, int d, double l) {
    if (isGost(st)) return "Штифт " + std::to_string(d) + " m6×" + designationNumber(l, true) + " " + toString(st);
    return std::string("Штифт ") + toString(st) + " - " + std::to_string(d) + " m6 x " + designationNumber(l, false);
}

bool same(double a, double b) { return std::abs(a - b) < 1e-9; }

// ---- построители

Fastener buildHexBolt(const FastenerSpec& spec) {
    const HexBoltRow row = unwrap(hexBoltRow(spec.standard, spec.size));
    const double d = row.d, l = spec.length, P = row.pitch;
    const double d3 = externalMinorDiameter(d, P);
    if (!(l > d - d3)) fail(ErrorCode::InvalidInput, "bolt length " + num(l) + " mm is too short");
    double b = boltThreadLength(row.d, l);
    if (spec.standard == FastenerStandard::Iso4017) {
        b = l;
    } else if (spec.standard == FastenerStandard::Iso4014) {
        if (l <= b) {
            fail(ErrorCode::InvalidInput, "ISO 4014 M" + std::to_string(row.d) + " needs l > b = " + num(b) +
                                              " mm; use ISO 4017 (fully threaded) for l = " + num(l));
        }
    } else {
        b = std::min(b, l);
    }

    Fastener f;
    f.spec = spec;
    f.kind = FastenerKind::HexBolt;
    TopoDS_Shape head = chamferedHex(row.s, 0, row.k, false, true, 0);
    f.shape = finish(boolean<BRepAlgoAPI_Fuse>(head, shank(d, d3, l), "bolt fuse"));
    const double k = row.k;
    f.elementMap = nameFaces(
        f.shape,
        [&](const FaceGeom& g) -> std::string {
            if (axial(g)) {
                if (near(g.centroid.Z(), 0)) return roles::kBearing;
                if (near(g.centroid.Z(), k)) return roles::kTop;
                if (near(g.centroid.Z(), -l)) return roles::kTip;
            }
            if (lateral(g)) return roles::kFlat;
            if (g.type == GeomAbs_Cylinder && near(g.radius, d / 2)) return roles::kShank;
            if (g.type == GeomAbs_Cone) return g.centroid.Z() > 0 ? roles::kHeadChamfer : roles::kTipChamfer;
            return {};
        },
        {roles::kBearing, roles::kTop, roles::kTip, roles::kShank, roles::kHeadChamfer, roles::kTipChamfer,
         "Flat1", "Flat2", "Flat3", "Flat4", "Flat5", "Flat6"});
    setThreadExternal(f, d, P, b);
    f.properties.name = "Болт";
    f.properties.designation = boltDesignation(spec.standard, row.d, l);
    // Соответствие: тот же размер под ключ и высота головки в таблице другого стандарта.
    if (auto cp = counterpart(spec.standard)) {
        FastenerStandard other = *cp;
        if (other == FastenerStandard::Iso4014 && b >= l) other = FastenerStandard::Iso4017;
        auto orow = hexBoltRow(other, row.d);
        if (orow && same(orow.value().s, row.s) && same(orow.value().k, row.k)) {
            f.properties.equivalentDesignation = boltDesignation(other, row.d, l);
        }
    }
    f.dimensions = {{"d", d}, {"P", P}, {"s", row.s}, {"e", 2 * row.s / std::sqrt(3.0)}, {"k", k}, {"l", l}, {"b", b}};
    if (row.eMin) f.dimensions.emplace_back("e_min", *row.eMin);
    return f;
}

Fastener buildSocketScrew(const FastenerSpec& spec) {
    const SocketScrewRow row = unwrap(socketScrewRow(spec.standard, spec.size));
    const double d = row.d, l = spec.length, P = row.pitch, k = row.k;
    const double d3 = externalMinorDiameter(d, P);
    if (!(l > d - d3)) fail(ErrorCode::InvalidInput, "screw length " + num(l) + " mm is too short");
    const double b = std::min(l, 2.0 * row.d + 12);  // ISO 4762: b ref = 2d + 12
    const double c = kSocketHeadChamferRatio * d;
    const double r = row.dk / 2;
    TopoDS_Shape head = revolve({{0, 0}, {r, 0}, {r, k - c}, {r - c, k}, {0, k}});
    TopoDS_Shape body = boolean<BRepAlgoAPI_Fuse>(head, shank(d, d3, l), "screw fuse");
    body = boolean<BRepAlgoAPI_Cut>(body, hexPrism(row.s, k - row.t, row.t + 1), "socket cut");

    Fastener f;
    f.spec = spec;
    f.kind = FastenerKind::SocketHeadScrew;
    f.shape = finish(body);
    const double socketBottom = k - row.t;
    f.elementMap = nameFaces(
        f.shape,
        [&](const FaceGeom& g) -> std::string {
            if (axial(g)) {
                if (near(g.centroid.Z(), 0)) return roles::kBearing;
                if (near(g.centroid.Z(), k)) return roles::kTop;
                if (near(g.centroid.Z(), -l)) return roles::kTip;
                if (near(g.centroid.Z(), socketBottom)) return roles::kSocketBottom;
            }
            if (lateral(g)) return roles::kSocket;
            if (g.type == GeomAbs_Cylinder) {
                if (near(g.radius, d / 2)) return roles::kShank;
                if (near(g.radius, r)) return roles::kOuter;
            }
            if (g.type == GeomAbs_Cone) return g.centroid.Z() > 0 ? roles::kHeadChamfer : roles::kTipChamfer;
            return {};
        },
        {roles::kBearing, roles::kTop, roles::kTip, roles::kSocketBottom, roles::kShank, roles::kOuter,
         roles::kHeadChamfer, roles::kTipChamfer, "Socket1", "Socket6"});
    setThreadExternal(f, d, P, b);
    f.properties.name = "Винт";
    f.properties.designation = screwDesignation(spec.standard, row.d, l);
    if (auto cp = counterpart(spec.standard)) {
        auto orow = socketScrewRow(*cp, row.d);
        if (orow && same(orow.value().dk, row.dk) && same(orow.value().k, row.k) && same(orow.value().s, row.s) &&
            same(orow.value().t, row.t)) {
            f.properties.equivalentDesignation = screwDesignation(*cp, row.d, l);
        }
    }
    f.dimensions = {{"d", d}, {"P", P}, {"dk", row.dk}, {"k", k}, {"s", row.s}, {"t", row.t},
                    {"c", c}, {"l", l}, {"b", b}};
    return f;
}

Fastener buildHexNut(const FastenerSpec& spec) {
    const HexNutRow row = unwrap(hexNutRow(spec.standard, spec.size));
    const double d = row.d, m = row.m, P = row.pitch;
    Fastener f;
    f.spec = spec;
    f.kind = FastenerKind::HexNut;
    f.shape = finish(chamferedHex(row.s, 0, m, true, true, d / 2));
    f.elementMap = nameFaces(
        f.shape,
        [&](const FaceGeom& g) -> std::string {
            if (axial(g)) {
                if (near(g.centroid.Z(), 0)) return roles::kBearing;
                if (near(g.centroid.Z(), m)) return roles::kTop;
            }
            if (lateral(g)) return roles::kFlat;
            if (g.type == GeomAbs_Cylinder && near(g.radius, d / 2)) return roles::kBore;
            if (g.type == GeomAbs_Cone) return g.centroid.Z() < m / 2 ? roles::kBearingChamfer : roles::kTopChamfer;
            return {};
        },
        {roles::kBearing, roles::kTop, roles::kBore, roles::kBearingChamfer, roles::kTopChamfer, "Flat1", "Flat6"});
    f.thread.present = true;
    f.thread.internal = true;
    f.thread.nominalDiameter = d;
    f.thread.pitch = P;
    f.thread.minorDiameter = d - 1.082532 * P;  // D1 = D − 1,082532·P (ISO 724)
    f.thread.length = m;
    f.thread.designation = "M" + std::to_string(row.d);
    f.properties.name = "Гайка";
    f.properties.designation = nutDesignation(spec.standard, row.d);
    if (auto cp = counterpart(spec.standard)) {
        auto orow = hexNutRow(*cp, row.d);
        if (orow && same(orow.value().s, row.s) && same(orow.value().m, row.m)) {
            f.properties.equivalentDesignation = nutDesignation(*cp, row.d);
        }
    }
    f.dimensions = {{"d", d}, {"P", P}, {"s", row.s}, {"e", 2 * row.s / std::sqrt(3.0)}, {"m", m}};
    if (row.eMin) f.dimensions.emplace_back("e_min", *row.eMin);
    return f;
}

Fastener buildPlainWasher(const FastenerSpec& spec) {
    const WasherRow row = unwrap(washerRow(spec.standard, spec.size));
    const double r1 = row.d1 / 2, r2 = row.d2 / 2, h = row.h;
    Fastener f;
    f.spec = spec;
    f.kind = FastenerKind::PlainWasher;
    f.shape = finish(revolve({{r1, 0}, {r2, 0}, {r2, h}, {r1, h}}));
    f.elementMap = nameFaces(
        f.shape,
        [&](const FaceGeom& g) -> std::string {
            if (axial(g)) return near(g.centroid.Z(), 0) ? roles::kBearing : roles::kTop;
            if (g.type == GeomAbs_Cylinder) return near(g.radius, r1) ? roles::kBore : roles::kOuter;
            return {};
        },
        {roles::kBearing, roles::kTop, roles::kBore, roles::kOuter});
    f.properties.name = "Шайба";
    f.properties.designation = washerDesignation(spec.standard, row.d);
    if (auto cp = counterpart(spec.standard)) {
        auto orow = washerRow(*cp, row.d);
        if (orow && same(orow.value().d1, row.d1) && same(orow.value().d2, row.d2) && same(orow.value().h, row.h)) {
            f.properties.equivalentDesignation = washerDesignation(*cp, row.d);
        }
    }
    f.dimensions = {{"d", static_cast<double>(row.d)}, {"d1", row.d1}, {"d2", row.d2}, {"h", h}};
    return f;
}

Fastener buildSpringWasher(const FastenerSpec& spec) {
    const SpringWasherRow row = unwrap(springWasherRow(spec.standard, spec.size));
    const double r1 = row.d1 / 2, r2 = row.d1 / 2 + row.b, s = row.s;
    // Условное изображение: плоское кольцо с радиальным разрезом шириной s (развод концов
    // и винтовая форма не моделируются; ширина разреза не табличная).
    const double gap = s;
    TopoDS_Shape ring = revolve({{r1, 0}, {r2, 0}, {r2, s}, {r1, s}});
    TopoDS_Shape slot = BRepPrimAPI_MakeBox(gp_Pnt(0, -gap / 2, -1), r2 + 1, gap, s + 2).Shape();
    Fastener f;
    f.spec = spec;
    f.kind = FastenerKind::SpringWasher;
    f.shape = finish(boolean<BRepAlgoAPI_Cut>(ring, slot, "spring washer slot"));
    f.elementMap = nameFaces(
        f.shape,
        [&](const FaceGeom& g) -> std::string {
            if (axial(g)) return near(g.centroid.Z(), 0) ? roles::kBearing : roles::kTop;
            if (lateral(g)) return roles::kSlot;
            if (g.type == GeomAbs_Cylinder) return near(g.radius, r1) ? roles::kBore : roles::kOuter;
            return {};
        },
        {roles::kBearing, roles::kTop, roles::kBore, roles::kOuter, "Slot1", "Slot2"});
    f.properties.name = "Шайба пружинная";
    f.properties.material = "Сталь 65Г";
    f.properties.designation = washerDesignation(spec.standard, row.d);
    f.dimensions = {{"d", static_cast<double>(row.d)}, {"d1", row.d1}, {"s", s}, {"b", row.b}, {"gap", gap}};
    return f;
}

Fastener buildPin(const FastenerSpec& spec) {
    const PinRow row = unwrap(pinRow(spec.standard, spec.size));
    const double r = row.d / 2.0, c = row.c, l = spec.length;
    if (!(l > 2 * c)) fail(ErrorCode::InvalidInput, "pin length " + num(l) + " mm must exceed 2c = " + num(2 * c));
    Fastener f;
    f.spec = spec;
    f.kind = FastenerKind::ParallelPin;
    f.shape = finish(revolve({{0, 0}, {r - c, 0}, {r, c}, {r, l - c}, {r - c, l}, {0, l}}));
    f.elementMap = nameFaces(
        f.shape,
        [&](const FaceGeom& g) -> std::string {
            if (axial(g)) return roles::kEnd;
            if (g.type == GeomAbs_Cylinder) return roles::kShank;
            if (g.type == GeomAbs_Cone) return roles::kEndChamfer;
            return {};
        },
        {roles::kShank, "End1", "End2", "EndChamfer1", "EndChamfer2"});
    f.properties.name = "Штифт";
    f.properties.designation = pinDesignation(spec.standard, row.d, l);
    if (auto cp = counterpart(spec.standard)) {
        auto orow = pinRow(*cp, row.d);
        if (orow && same(orow.value().c, row.c)) f.properties.equivalentDesignation = pinDesignation(*cp, row.d, l);
    }
    f.dimensions = {{"d", static_cast<double>(row.d)}, {"c", c}, {"l", l}};
    return f;
}

}  // namespace

std::string faceName(const std::string& role) { return std::string(roles::kFeature) + ":" + role; }

std::optional<double> Fastener::dimension(const std::string& key) const {
    for (const auto& [k, v] : dimensions) {
        if (k == key) return v;
    }
    return std::nullopt;
}

kernel::Result<Fastener> makeFastener(const FastenerSpec& spec) {
    try {
        Fastener f;
        switch (kindOf(spec.standard)) {
            case FastenerKind::HexBolt: f = buildHexBolt(spec); break;
            case FastenerKind::SocketHeadScrew: f = buildSocketScrew(spec); break;
            case FastenerKind::HexNut: f = buildHexNut(spec); break;
            case FastenerKind::PlainWasher: f = buildPlainWasher(spec); break;
            case FastenerKind::SpringWasher: f = buildSpringWasher(spec); break;
            case FastenerKind::ParallelPin: f = buildPin(spec); break;
        }
        f.properties.volume = volumeOf(f.shape);
        f.properties.mass = f.properties.volume * 1e-9 * f.properties.density;
        return f;
    } catch (const Failure& e) {
        return kernel::Result<Fastener>::failure(e.code, e.message);
    } catch (const Standard_Failure& e) {
        return kernel::Result<Fastener>::failure(ErrorCode::OperationFailed,
                                                 std::string("OCCT failure while building fastener: ") +
                                                     (e.GetMessageString() ? e.GetMessageString() : ""));
    }
}

PartData toPartData(const Fastener& fastener) { return PartData{fastener.shape, fastener.elementMap}; }

PartProperties toPartProperties(const Fastener& fastener) {
    PartProperties p;
    p.name = fastener.properties.designation;
    p.material = fastener.properties.material;
    p.density = fastener.properties.density;
    return p;
}

kernel::Result<DefinitionId> addFastenerPart(AssemblyDocument& document, const Fastener& fastener) {
    return document.addPart(fastener.properties.designation, toPartData(fastener), toPartProperties(fastener));
}

}  // namespace bendcore::assembly_v2::fasteners
