// Шпонки призматические ГОСТ 23360-78 (docs/assembly/FASTENERS.md, раздел 9): таблица сечений
// с глубинами пазов, ряд длин, модели исполнений 1–3 с фасками s × 45° по продольным кромкам.
// Модель — прямой участок (призма сечения с четырьмя фасками) и скруглённые концы (половина
// тела вращения того же сечения вокруг вертикальной оси через центр скругления), объединённые
// булевой операцией.

#include <bendcore/assembly_v2/keys.hpp>

#include "part_model.hpp"

#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <Standard_Failure.hxx>
#include <gp_Ax1.hxx>

#include <cmath>
#include <numbers>

namespace bendcore::assembly_v2::fasteners {

using kernel::ErrorCode;
using namespace detail;

namespace {

TopoDS_Face polygonFace(const std::vector<gp_Pnt>& pts) {
    BRepBuilderAPI_MakePolygon poly;
    for (const gp_Pnt& p : pts) poly.Add(p);
    poly.Close();
    if (!poly.IsDone()) partFail(ErrorCode::OperationFailed, "key profile polygon failed");
    BRepBuilderAPI_MakeFace f(poly.Wire(), true);
    if (!f.IsDone()) partFail(ErrorCode::OperationFailed, "key profile face failed");
    return f.Face();
}

// Прямой участок x ∈ [x0, x1]: призма сечения b×h с фасками s по четырём кромкам.
TopoDS_Shape straightPart(double b, double h, double s, double x0, double x1) {
    const double y = b / 2;
    const TopoDS_Face f = polygonFace({gp_Pnt(x0, -y + s, 0), gp_Pnt(x0, y - s, 0), gp_Pnt(x0, y, s), gp_Pnt(x0, y, h - s),
                                       gp_Pnt(x0, y - s, h), gp_Pnt(x0, -y + s, h), gp_Pnt(x0, -y, h - s), gp_Pnt(x0, -y, s)});
    BRepPrimAPI_MakePrism prism(f, gp_Vec(x1 - x0, 0, 0));
    prism.Build();
    if (!prism.IsDone()) partFail(ErrorCode::OperationFailed, "key prism failed");
    return prism.Shape();
}

// Скруглённый конец: половина тела вращения профиля (ρ, z) радиуса R = b/2 с фасками s вокруг
// вертикальной оси через (xc, 0); side = +1 — половина x ≥ xc, −1 — x ≤ xc.
TopoDS_Shape roundEnd(double b, double h, double s, double xc, int side) {
    const double R = b / 2;
    const TopoDS_Face f = polygonFace({gp_Pnt(xc, 0, 0), gp_Pnt(xc, R - s, 0), gp_Pnt(xc, R, s), gp_Pnt(xc, R, h - s),
                                       gp_Pnt(xc, R - s, h), gp_Pnt(xc, 0, h)});
    // Профиль в плоскости x = xc по +Y: поворот на −π проходит через +X, на +π — через −X.
    BRepPrimAPI_MakeRevol rev(f, gp_Ax1(gp_Pnt(xc, 0, 0), gp::DZ()), side > 0 ? -std::numbers::pi : std::numbers::pi);
    rev.Build();
    if (!rev.IsDone()) partFail(ErrorCode::OperationFailed, "key end revolution failed");
    return rev.Shape();
}

TopoDS_Shape fuse(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    BRepAlgoAPI_Fuse op(a, b);
    if (!op.IsDone() || op.HasErrors()) partFail(ErrorCode::OperationFailed, "key fuse failed");
    return op.Shape();
}

std::string designationOf(const KeySpec& s) {
    std::string prefix = s.execution == KeyExecution::Rounded ? "" : std::to_string(static_cast<int>(s.execution)) + "-";
    return "Шпонка " + prefix + gostNumber(s.b) + "×" + gostNumber(s.h) + "×" + gostNumber(s.l) + " ГОСТ 23360-78";
}

std::string seriesText() {
    std::string out;
    for (double l : keyLengthSeries()) out += (out.empty() ? "" : ", ") + partNum(l);
    return out;
}

}  // namespace

const std::vector<KeyRow>& keyTable() {
    // d свыше … до, b, h, t1, t2, s min, s max, l min, l max (ГОСТ 23360-78, табл. 1, по памяти).
    static const std::vector<KeyRow> t{
        {6, 8, 2, 2, 1.2, 1.0, 0.16, 0.25, 6, 20},       {8, 10, 3, 3, 1.8, 1.4, 0.16, 0.25, 6, 36},
        {10, 12, 4, 4, 2.5, 1.8, 0.16, 0.25, 8, 45},     {12, 17, 5, 5, 3.0, 2.3, 0.25, 0.40, 10, 56},
        {17, 22, 6, 6, 3.5, 2.8, 0.25, 0.40, 14, 70},    {22, 30, 8, 7, 4.0, 3.3, 0.25, 0.40, 18, 90},
        {30, 38, 10, 8, 5.0, 3.3, 0.40, 0.60, 22, 110},  {38, 44, 12, 8, 5.0, 3.3, 0.40, 0.60, 28, 140},
        {44, 50, 14, 9, 5.5, 3.8, 0.40, 0.60, 36, 160},  {50, 58, 16, 10, 6.0, 4.3, 0.40, 0.60, 45, 180},
        {58, 65, 18, 11, 7.0, 4.4, 0.40, 0.60, 50, 200}, {65, 75, 20, 12, 7.5, 4.9, 0.60, 0.80, 56, 220},
        {75, 85, 22, 14, 9.0, 5.4, 0.60, 0.80, 63, 250}, {85, 95, 25, 14, 9.0, 5.4, 0.60, 0.80, 70, 280},
        {95, 110, 28, 16, 10.0, 6.4, 0.60, 0.80, 80, 320},
    };
    return t;
}

const std::vector<double>& keyLengthSeries() {
    static const std::vector<double> l{6,  8,  10, 12, 14, 16,  18,  20,  22,  25,  28,  32,  36,  40,  45,  50,  56,  63,
                                       70, 80, 90, 100, 110, 125, 140, 160, 180, 200, 220, 250, 280, 320, 360, 400, 450, 500};
    return l;
}

KeyTableInfo keyTableInfo() {
    return {"ГОСТ 23360-78", "ГОСТ 23360-78 (сечения b×h, t1, t2, s, диапазоны длин, ряд длин) по памяти",
            "требует сверки с официальным изданием (действующая редакция с изменениями; диапазоны диаметров валов — справочные)",
            false};
}

kernel::Result<KeyRow> keyRowForShaft(double d) {
    for (const KeyRow& r : keyTable())
        if (d > r.dOver && d <= r.dUpTo) return r;
    return kernel::Result<KeyRow>::failure(ErrorCode::Unsupported, "no GOST 23360 key for shaft diameter " + partNum(d) +
                                                                       " mm (the table covers over 6 up to 110 mm)");
}

kernel::Result<KeyRow> keyRow(double b, double h) {
    std::string list;
    for (const KeyRow& r : keyTable()) {
        if (std::abs(r.b - b) < 1e-9 && std::abs(r.h - h) < 1e-9) return r;
        list += (list.empty() ? "" : ", ") + partNum(r.b) + "x" + partNum(r.h);
    }
    return kernel::Result<KeyRow>::failure(ErrorCode::Unsupported,
                                           "key section " + partNum(b) + "x" + partNum(h) + " is not in GOST 23360 (" + list + ")");
}

kernel::Result<KeySeatDepths> keySeatDepths(double d) {
    auto r = keyRowForShaft(d);
    if (!r) return r.error();
    return KeySeatDepths{r.value().b, r.value().h, r.value().t1, r.value().t2};
}

std::string keyFaceName(const std::string& role) { return std::string(key_roles::kFeature) + ":" + role; }

modeling::ElementAttributes keyAttributes(const KeySpec& spec, const KeyRow& row, double s, const std::string& designation) {
    return {{"key.standard", "ГОСТ 23360-78"},
            {"key.designation", designation},
            {"key.execution", std::to_string(static_cast<int>(spec.execution))},
            {"key.b", partNum(spec.b)},
            {"key.h", partNum(spec.h)},
            {"key.l", partNum(spec.l)},
            {"key.s", partNum(s)},
            {"key.t1", partNum(row.t1)},
            {"key.t2", partNum(row.t2)}};
}

std::optional<double> Key::dimension(const std::string& key) const { return findDimension(dimensions, key); }

kernel::Result<Key> makeKey(const KeySpec& spec) {
    auto row = keyRow(spec.b, spec.h);
    if (!row) return row.error();
    const KeyRow& R = row.value();
    const int exec = static_cast<int>(spec.execution);
    if (exec < 1 || exec > 3) return kernel::Result<Key>::failure(ErrorCode::InvalidInput, "key execution must be 1, 2 or 3");
    bool inSeries = false;
    for (double l : keyLengthSeries()) inSeries = inSeries || std::abs(l - spec.l) < 1e-9;
    if (!inSeries)
        return kernel::Result<Key>::failure(ErrorCode::InvalidInput, "key length " + partNum(spec.l) +
                                                                         " mm is not in the GOST 23360 series (" + seriesText() + ")");
    if (spec.l < R.lMin - 1e-9 || spec.l > R.lMax + 1e-9)
        return kernel::Result<Key>::failure(ErrorCode::InvalidInput, "key " + partNum(R.b) + "x" + partNum(R.h) + " length must be in [" +
                                                                         partNum(R.lMin) + ", " + partNum(R.lMax) + "] mm, is " +
                                                                         partNum(spec.l));
    const double s = spec.chamfer.value_or(R.sMin);
    if (s < R.sMin - 1e-12 || s > R.sMax + 1e-12)
        return kernel::Result<Key>::failure(ErrorCode::InvalidInput, "key chamfer s must be in [" + partNum(R.sMin) + ", " +
                                                                         partNum(R.sMax) + "] mm, is " + partNum(s));
    try {
        Key k;
        k.spec = spec;
        k.spec.chamfer = s;
        k.row = R;
        k.s = s;
        const double b = R.b, h = R.h, l = spec.l, half = l / 2, rr = b / 2;
        TopoDS_Shape body;
        switch (spec.execution) {
            case KeyExecution::Rounded:
                body = fuse(fuse(straightPart(b, h, s, -half + rr, half - rr), roundEnd(b, h, s, half - rr, 1)),
                            roundEnd(b, h, s, -half + rr, -1));
                break;
            case KeyExecution::Flat: body = straightPart(b, h, s, -half, half); break;
            case KeyExecution::OneRounded:
                body = fuse(straightPart(b, h, s, -half + rr, half), roundEnd(b, h, s, -half + rr, -1));
                break;
        }
        ShapeUpgrade_UnifySameDomain unify(body, true, true, false);
        unify.Build();
        TopoDS_Shape solid;
        int solids = 0;
        for (TopExp_Explorer ex(unify.Shape(), TopAbs_SOLID); ex.More(); ex.Next(), ++solids) solid = ex.Current();
        if (solids != 1) partFail(ErrorCode::OperationFailed, "key body has " + std::to_string(solids) + " solids");
        requireValid(solid, "key");
        k.shape = solid;
        const std::string designation = designationOf(k.spec);
        k.elementMap = namePartFaces(
            k.shape, key_roles::kFeature,
            [&](const PartFace& g) -> FaceRole {
                if (g.type == GeomAbs_Plane) {
                    if (g.normal.Z() < -1 + 1e-9) return {key_roles::kBottom};
                    if (g.normal.Z() > 1 - 1e-9) return {key_roles::kTop};
                    if (std::abs(g.normal.Y()) > 1 - 1e-9) return {key_roles::kSide, true, g.centroid.Y()};
                    if (std::abs(g.normal.X()) > 1 - 1e-9) return {key_roles::kEnd, true, g.centroid.X()};
                    return {};
                }
                if (g.type == GeomAbs_Cylinder && std::abs(g.radius - rr) < 1e-6) return {key_roles::kEnd, true, g.centroid.X()};
                return {};
            },
            {key_roles::kBottom, key_roles::kTop, "Side1", "Side2", "End1", "End2"}, keyAttributes(k.spec, R, s, designation));
        k.properties.name = "Шпонка";
        k.properties.designation = designation;
        k.properties.material = "Сталь 45 ГОСТ 1050-2013";
        k.properties.volume = shapeVolume(k.shape);
        k.properties.mass = k.properties.volume * 1e-9 * k.properties.density;
        k.dimensions = {{"b", b}, {"h", h}, {"l", l}, {"s", s}, {"t1", R.t1}, {"t2", R.t2}};
        return k;
    } catch (const PartFailure& e) {
        return kernel::Result<Key>::failure(e.code, e.message);
    } catch (const Standard_Failure& e) {
        return kernel::Result<Key>::failure(ErrorCode::OperationFailed, std::string("OCCT failure while building key: ") +
                                                                            (e.GetMessageString() ? e.GetMessageString() : ""));
    }
}

PartData toPartData(const Key& key) { return PartData{key.shape, key.elementMap}; }

PartProperties toPartProperties(const Key& key) {
    PartProperties p;
    p.name = key.properties.designation;
    p.material = key.properties.material;
    p.density = key.properties.density;
    return p;
}

kernel::Result<DefinitionId> addKeyPart(AssemblyDocument& document, const Key& key) {
    return document.addPart(key.properties.designation, toPartData(key), toPartProperties(key));
}

}  // namespace bendcore::assembly_v2::fasteners
