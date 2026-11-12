// Резьбовое отверстие: метрическая резьба ISO 261 / ГОСТ 8724, отверстие под резьбу и условная
// резьба как атрибут грани (docs/modeling/FEATURES.md, раздел «Резьбовое отверстие»).

#include "feature_common.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>

namespace bendcore::modeling {

using namespace detail;

namespace {

// ISO 261:1998, таблица 1 (= ГОСТ 8724-2002, ряды 1 и 2), номиналы 1–48 мм:
// крупный шаг и мелкие шаги по убыванию. Источник и статус сверки —
// docs/STANDARDS_VERIFICATION.md («Метрическая резьба»).
struct PitchRow {
    double nominal;
    double coarse;
    std::vector<double> fine;
};

const std::vector<PitchRow>& pitchTable() {
    static const std::vector<PitchRow> table = {
        {1.0, 0.25, {0.2}},          {1.2, 0.25, {0.2}},          {1.4, 0.3, {0.2}},
        {1.6, 0.35, {0.2}},          {1.8, 0.35, {0.2}},          {2.0, 0.4, {0.25}},
        {2.5, 0.45, {0.35}},         {3.0, 0.5, {0.35}},          {3.5, 0.6, {0.35}},
        {4.0, 0.7, {0.5}},           {5.0, 0.8, {0.5}},           {6.0, 1.0, {0.75}},
        {8.0, 1.25, {1.0, 0.75}},    {10.0, 1.5, {1.25, 1.0, 0.75}}, {12.0, 1.75, {1.5, 1.25, 1.0}},
        {14.0, 2.0, {1.5, 1.25, 1.0}}, {16.0, 2.0, {1.5, 1.0}},   {18.0, 2.5, {2.0, 1.5, 1.0}},
        {20.0, 2.5, {2.0, 1.5, 1.0}}, {22.0, 2.5, {2.0, 1.5, 1.0}}, {24.0, 3.0, {2.0, 1.5, 1.0}},
        {27.0, 3.0, {2.0, 1.5, 1.0}}, {30.0, 3.5, {3.0, 2.0, 1.5, 1.0}}, {33.0, 3.5, {3.0, 2.0, 1.5}},
        {36.0, 4.0, {3.0, 2.0, 1.5}}, {39.0, 4.0, {3.0, 2.0, 1.5}}, {42.0, 4.5, {4.0, 3.0, 2.0, 1.5}},
        {45.0, 4.5, {4.0, 3.0, 2.0, 1.5}}, {48.0, 5.0, {4.0, 3.0, 2.0, 1.5}},
    };
    return table;
}

bool near(double a, double b) { return std::abs(a - b) <= 1e-9 * std::max(1.0, std::abs(b)); }

const PitchRow* rowOf(double nominal) {
    for (const auto& r : pitchTable())
        if (near(r.nominal, nominal)) return &r;
    return nullptr;
}

std::string number(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.15g", v);
    return buf;
}

std::string vec3(double x, double y, double z) { return number(x) + " " + number(y) + " " + number(z); }

bool parseNumber(const std::string& text, double& out) {
    if (text.empty()) return false;
    char* end = nullptr;
    out = std::strtod(text.c_str(), &end);
    return end != nullptr && *end == '\0' && std::isfinite(out);
}

bool parseVec3(const std::string& text, double v[3]) {
    std::istringstream in(text);
    std::string a, b, c, extra;
    if (!(in >> a >> b >> c) || (in >> extra)) return false;
    return parseNumber(a, v[0]) && parseNumber(b, v[1]) && parseNumber(c, v[2]);
}

}  // namespace

std::vector<double> metricNominals() {
    std::vector<double> out;
    for (const auto& r : pitchTable()) out.push_back(r.nominal);
    return out;
}

Result<MetricThread> metricCoarse(double nominal) {
    const PitchRow* r = rowOf(nominal);
    if (!r) return Error{ErrorCode::InvalidInput, "no metric thread M" + number(nominal) + " in the ISO 261 table"};
    return MetricThread{r->nominal, r->coarse, ThreadSeries::Coarse};
}

Result<std::vector<double>> metricFinePitches(double nominal) {
    const PitchRow* r = rowOf(nominal);
    if (!r) return Error{ErrorCode::InvalidInput, "no metric thread M" + number(nominal) + " in the ISO 261 table"};
    return r->fine;
}

Result<MetricThread> metricThread(double nominal, double pitch) {
    const PitchRow* r = rowOf(nominal);
    if (!r) return Error{ErrorCode::InvalidInput, "no metric thread M" + number(nominal) + " in the ISO 261 table"};
    if (near(r->coarse, pitch)) return MetricThread{r->nominal, r->coarse, ThreadSeries::Coarse};
    for (double f : r->fine)
        if (near(f, pitch)) return MetricThread{r->nominal, f, ThreadSeries::Fine};
    return Error{ErrorCode::InvalidInput, "pitch " + number(pitch) + " is not an ISO 261 pitch of M" + number(nominal)};
}

double tapDrillDiameter(const MetricThread& t) { return t.nominal - t.pitch; }

double minorDiameter(const MetricThread& t) { return t.nominal - 5.0 * std::sqrt(3.0) / 8.0 * t.pitch; }

std::string threadDesignation(const MetricThread& t, ThreadHand hand) {
    std::string s = "M" + number(t.nominal);
    if (t.series == ThreadSeries::Fine) s += "x" + number(t.pitch);
    if (hand == ThreadHand::Left) s += "LH";
    return s;
}

std::vector<std::pair<std::string, std::string>> threadAttributes(const ThreadSpec& t) {
    return {
        {"thread.standard", t.standard},
        {"thread.designation", t.designation},
        {"thread.nominal", number(t.nominal)},
        {"thread.pitch", number(t.pitch)},
        {"thread.minor", number(t.minor)},
        {"thread.drill", number(t.drill)},
        {"thread.length", number(t.length)},
        {"thread.hand", t.hand == ThreadHand::Left ? "left" : "right"},
        {"thread.kind", t.internal ? "internal" : "external"},
        {"thread.entry", vec3(t.entry.X(), t.entry.Y(), t.entry.Z())},
        {"thread.direction", vec3(t.direction.X(), t.direction.Y(), t.direction.Z())},
    };
}

Result<ThreadSpec> threadFromAttributes(const std::vector<std::pair<std::string, std::string>>& attributes) {
    std::map<std::string, std::string> a(attributes.begin(), attributes.end());
    auto get = [&](const char* key) -> Result<std::string> {
        auto it = a.find(key);
        if (it == a.end()) return Error{ErrorCode::InvalidInput, std::string("thread attribute '") + key + "' is missing"};
        return it->second;
    };
    ThreadSpec t;
    for (const char* key : {"thread.standard", "thread.designation", "thread.hand", "thread.kind"}) {
        auto v = get(key);
        if (!v) return v.error();
    }
    t.standard = a["thread.standard"];
    t.designation = a["thread.designation"];
    const std::string hand = a["thread.hand"], kind = a["thread.kind"];
    if (hand != "right" && hand != "left") return Error{ErrorCode::InvalidInput, "thread.hand must be right or left"};
    if (kind != "internal" && kind != "external")
        return Error{ErrorCode::InvalidInput, "thread.kind must be internal or external"};
    t.hand = hand == "left" ? ThreadHand::Left : ThreadHand::Right;
    t.internal = kind == "internal";
    struct NumKey {
        const char* key;
        double* value;
    };
    for (NumKey k : {NumKey{"thread.nominal", &t.nominal}, NumKey{"thread.pitch", &t.pitch},
                     NumKey{"thread.minor", &t.minor}, NumKey{"thread.drill", &t.drill},
                     NumKey{"thread.length", &t.length}}) {
        auto v = get(k.key);
        if (!v) return v.error();
        if (!parseNumber(v.value(), *k.value))
            return Error{ErrorCode::InvalidInput, std::string(k.key) + " is not a number: '" + v.value() + "'"};
    }
    double e[3], d[3];
    auto ev = get("thread.entry");
    if (!ev) return ev.error();
    auto dv = get("thread.direction");
    if (!dv) return dv.error();
    if (!parseVec3(ev.value(), e)) return Error{ErrorCode::InvalidInput, "thread.entry must be three numbers"};
    if (!parseVec3(dv.value(), d) || std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) < 1e-12)
        return Error{ErrorCode::InvalidInput, "thread.direction must be a non-zero vector"};
    t.entry = gp_Pnt(e[0], e[1], e[2]);
    t.direction = gp_Dir(d[0], d[1], d[2]);
    return t;
}

Result<FeatureResult> threadedHole(const TopoDS_Shape& target, const TopoDS_Face& face, const gp_Pnt& point,
                                   const ThreadedHoleParams& p, const std::optional<gp_Dir>& direction) {
    const MetricThread& th = p.thread;
    if (!(std::isfinite(th.nominal) && th.nominal > 0.0)) return Error{ErrorCode::InvalidInput, "thread nominal diameter must be > 0"};
    if (!(std::isfinite(th.pitch) && th.pitch > 0.0 && th.pitch < th.nominal / 2))
        return Error{ErrorCode::InvalidInput, "thread pitch must be in (0, D/2)"};
    const double drill = p.drillDiameter ? *p.drillDiameter : tapDrillDiameter(th);
    if (!(std::isfinite(drill) && drill > 0.0 && drill < th.nominal))
        return Error{ErrorCode::InvalidInput, "tap drill diameter must be in (0, D)"};
    HoleParams hp;
    hp.type = HoleType::Simple;
    hp.diameter = drill;
    hp.drillPoint = p.drillPoint;
    double threadLength = p.threadLength;
    if (p.end == ThreadEnd::Blind) {
        if (!(std::isfinite(threadLength) && threadLength > 0.0)) return Error{ErrorCode::InvalidInput, "thread length must be > 0"};
        const double depth = p.drillDepth > 0.0 ? p.drillDepth : threadLength + 3.0 * th.pitch;
        if (!std::isfinite(depth)) return Error{ErrorCode::InvalidInput, "drill depth must be finite"};
        if (threadLength > depth + 1e-9)
            return Error{ErrorCode::InvalidInput, "thread length exceeds the drill depth"};
        hp.end = HoleEnd::Blind;
        hp.depth = depth;
    } else {
        if (!(std::isfinite(threadLength) && threadLength >= 0.0))
            return Error{ErrorCode::InvalidInput, "thread length must be >= 0 (0: through the whole hole)"};
        hp.end = HoleEnd::Through;
    }
    auto made = hole(target, face, point, hp, direction);
    if (!made) return made.error();
    FeatureResult out = std::move(made).value();
    try {
        // Стенка отверстия: цилиндр радиуса drill/2 среди граней, порождённых гранью размещения.
        const TopoDS_Face* placed = nullptr;
        TopoDS_Face placedFace;
        for (TopExp_Explorer ex(target, TopAbs_FACE); ex.More(); ex.Next())
            if (ex.Current().IsSame(face)) { placedFace = TopoDS::Face(ex.Current()); placed = &placedFace; break; }
        if (!placed) return Error{ErrorCode::InvalidInput, "hole face does not belong to the target body"};
        std::vector<TopoDS_Face> walls;
        gp_Dir axisDir;
        for (const TopoDS_Shape& g : out.history->Generated(*placed)) {
            if (g.ShapeType() != TopAbs_FACE) continue;
            BRepAdaptor_Surface s(TopoDS::Face(g), false);
            if (s.GetType() != GeomAbs_Cylinder || std::abs(s.Cylinder().Radius() - drill / 2) > 1e-7) continue;
            walls.push_back(TopoDS::Face(g));
            axisDir = s.Cylinder().Axis().Direction();
        }
        if (walls.empty()) return Error{ErrorCode::OperationFailed, "threaded hole: the hole wall was not found"};
        // Направление резьбы — от входа в материал (ось сверления).
        gp_Dir dir = direction ? *direction : axisDir;
        if (!direction) {
            GProp_GProps g;
            BRepGProp::SurfaceProperties(walls.front(), g);
            if (gp_Vec(point, g.CentreOfMass()).Dot(gp_Vec(dir)) < 0) dir.Reverse();
        }
        // Длина стенки вдоль оси — по вершинам её граничных окружностей (точно, без габарита).
        double wallLength = 0.0;
        for (const auto& w : walls)
            for (TopExp_Explorer vx(w, TopAbs_VERTEX); vx.More(); vx.Next())
                wallLength = std::max(wallLength, gp_Vec(point, BRep_Tool::Pnt(TopoDS::Vertex(vx.Current()))).Dot(gp_Vec(dir)));
        if (p.end == ThreadEnd::Through) {
            if (threadLength == 0.0) threadLength = wallLength;
            if (threadLength > wallLength + 1e-6)
                return Error{ErrorCode::InvalidInput, "thread length exceeds the hole length " + number(wallLength)};
        }
        ThreadSpec spec;
        spec.standard = "ISO 261";
        spec.designation = threadDesignation(th, p.hand);
        spec.nominal = th.nominal;
        spec.pitch = th.pitch;
        spec.minor = minorDiameter(th);
        spec.drill = drill;
        spec.length = threadLength;
        spec.hand = p.hand;
        spec.internal = true;
        spec.entry = point;
        spec.direction = dir;
        for (const auto& w : walls) out.threads.push_back(ThreadedFace{w, spec});
        return out;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("threadedHole", e.what())};
    }
}

}  // namespace bendcore::modeling
