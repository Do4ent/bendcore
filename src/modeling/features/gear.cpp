// Зубчатый венец: эвольвентный профиль, нарезанный исходным контуром ГОСТ 13755 (ISO 53),
// размеры по ГОСТ 16532, прямой и косой зуб (docs/modeling/FEATURES.md, «Зубчатый венец»).
//
// Торцовое сечение строится обкаткой торцового исходного контура (нормальный контур, у
// которого координаты вдоль рейки умножены на 1/cos β) по делительной окружности r = z·mt/2:
// точка рейки с нормалью n касается колеса, когда n проходит через полюс (условие
// зацепления), — так получаются переходная кривая (из скругления головки рейки) и,
// при подрезании, её продолжение (из участка прямой за предельной точкой N). Эвольвента
// берётся по формуле, обе кривые аппроксимируются B-сплайнами с контролем отклонения.

#include "feature_common.hpp"

#include <bendcore/modeling/gear.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRepLib.hxx>
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <Bnd_Box.hxx>
#include <GCE2d_MakeSegment.hxx>
#include <GProp_GProps.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <GeomAPI_ProjectPointOnCurve.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Circle.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Failure.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <optional>
#include <sstream>

namespace bendcore::modeling {

using namespace detail;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct P2 {
    double x = 0.0, y = 0.0;
};

double inv(double a) { return std::tan(a) - a; }

std::string number(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.15g", v);
    return buf;
}

// Все величины торцового сечения одного колеса.
struct Toothing {
    GearGeometry g;
    double m = 0.0, r = 0.0, rb = 0.0, ra = 0.0, rf = 0.0;
    double k = 1.0;        // 1 / cos β
    double xm = 0.0;       // смещение x·m
    double alpha = 0.0;    // нормальный угол профиля
    double psi = 0.0;      // половина угловой толщины на делительной окружности
    double rho = 0.0;      // радиус скругления головки рейки ρf*·m
    double h0 = 0.0;       // высота центра скругления от делительной прямой (< 0, к центру колеса)
    double s0n = 0.0;      // положение центра скругления вдоль рейки (нормальное сечение)
    double hT = 0.0;       // высота точки сопряжения скругления с прямой рейки
    double hN = 0.0;       // высота предельной точки N (касание линии зацепления с основной окружностью)
    double gamma = 0.0;    // угол конца дуги впадины от оси зуба
    double tA = 0.0;       // параметр развёртки эвольвенты на окружности вершин
    int z = 0;
};

// Точка торцового профиля, полученная обкаткой рейки: точка рейки (sn, h) нормального
// сечения с нормалью (nx, ny) нормального сечения. Координаты — ось зуба по +x.
P2 generated(const Toothing& T, double sn, double h, double nx, double ny) {
    const double st = sn * T.k;
    const double ntx = nx / T.k, nty = ny;  // нормаль торцового контура (координата s растянута в k раз)
    const double u = T.xm + h;              // высота над делительной окружностью
    const double X = u * ntx / nty;         // условие зацепления: нормаль проходит через полюс (0, r)
    const double Y = T.r + u;
    const double phi = (st - X) / T.r;
    // Точка в СК колеса, повёрнутого на φ (ось зуба по +y), затем поворот на −π/2.
    const double xo = X * std::cos(phi) + Y * std::sin(phi);
    const double yo = -X * std::sin(phi) + Y * std::cos(phi);
    return {yo, -xo};
}

// Переходная кривая по параметру w: [0, 1] — скругление головки рейки от вершины до
// сопряжения с прямой, (1, 2] — прямая рейки от точки сопряжения до N (только при подрезании).
P2 fillet(const Toothing& T, double w) {
    if (w <= 1.0) {
        const double th = 1.5 * kPi - w * (0.5 * kPi - T.alpha);
        return generated(T, T.s0n + T.rho * std::cos(th), T.h0 + T.rho * std::sin(th), std::cos(th), std::sin(th));
    }
    const double h = T.hT + (w - 1.0) * (T.hN - T.hT);
    return generated(T, 0.25 * kPi * T.m - h * std::tan(T.alpha), h, -std::cos(T.alpha), -std::sin(T.alpha));
}

// Эвольвента правой стороны зуба по параметру развёртки t: радиус rb·√(1 + t²), угол −θ(t).
P2 involute(const Toothing& T, double t) {
    const double rad = T.rb * std::sqrt(1.0 + t * t);
    const double th = T.psi + inv(T.g.alphaT) - (t - std::atan(t));
    return {rad * std::cos(th), -rad * std::sin(th)};
}

double angleOf(P2 p) { return std::atan2(p.y, p.x); }
double radiusOf(P2 p) { return std::hypot(p.x, p.y); }

// Пересечение отрезков ab и cd: параметры (s на ab, t на cd) или nullopt.
std::optional<std::pair<double, double>> crossing(P2 a, P2 b, P2 c, P2 d) {
    const double rx = b.x - a.x, ry = b.y - a.y, sx = d.x - c.x, sy = d.y - c.y;
    const double den = rx * sy - ry * sx;
    if (std::abs(den) < 1e-300) return std::nullopt;
    const double qx = c.x - a.x, qy = c.y - a.y;
    const double s = (qx * sy - qy * sx) / den, t = (qx * ry - qy * rx) / den;
    if (s < 0.0 || s > 1.0 || t < 0.0 || t > 1.0) return std::nullopt;
    return std::make_pair(s, t);
}

struct Sides {
    Toothing T;
    double wEnd = 1.0;    // конец переходной кривой
    double tStart = 0.0;  // начало эвольвенты
    bool undercut = false;
};

Result<Toothing> toothing(const GearSpec& s) {
    auto g = gearGeometry(s);
    if (!g) return g.error();
    Toothing T;
    T.g = g.value();
    T.m = s.module;
    T.z = s.teeth;
    T.k = 1.0 / std::cos(s.helix);
    T.r = T.g.d / 2.0;
    T.rb = T.g.db / 2.0;
    T.ra = T.g.da / 2.0;
    T.rf = T.g.df / 2.0;
    T.xm = s.shift * s.module;
    T.alpha = s.rack.alpha;
    T.psi = (0.5 * kPi + 2.0 * s.shift * std::tan(T.alpha)) / s.teeth;
    T.rho = s.rack.rhoStar * T.m;
    T.h0 = -(s.rack.haStar + s.rack.cStar - s.rack.rhoStar) * T.m;
    T.s0n = 0.25 * kPi * T.m - T.h0 * std::tan(T.alpha) + T.rho / std::cos(T.alpha);
    T.hT = T.h0 - T.rho * std::sin(T.alpha);
    const double sa = std::sin(T.g.alphaT);
    T.hN = -T.xm - T.r * sa * sa;
    T.gamma = T.s0n * T.k / T.r;
    T.tA = std::sqrt(T.ra * T.ra / (T.rb * T.rb) - 1.0);
    if (!(T.s0n < 0.5 * kPi * T.m))
        return Error{ErrorCode::InvalidInput, "basic rack: the tip fillet radius rho_f* is too large for the tooth"};
    if (T.hT < T.h0 - T.rho || T.rho <= 0.0)
        return Error{ErrorCode::InvalidInput, "basic rack: rho_f* must be in (0, c*/(1 - sin alpha)]"};
    return T;
}

Result<Sides> sides(const GearSpec& spec) {
    auto t = toothing(spec);
    if (!t) return t.error();
    Sides S;
    S.T = t.value();
    const Toothing& T = S.T;
    if (T.hT >= T.hN) {
        // Без подрезания: переходная кривая кончается в граничной точке эвольвенты.
        const P2 form = fillet(T, 1.0);
        const double rForm = radiusOf(form);
        S.wEnd = 1.0;
        S.tStart = std::sqrt(std::max(0.0, rForm * rForm / (T.rb * T.rb) - 1.0));
        S.undercut = false;
    } else {
        // Подрезание: переходная кривая (с продолжением от прямой рейки за точкой N) пересекает
        // эвольвенту; профиль — переходная кривая до первого пересечения, затем эвольвента.
        constexpr int n = 4000;
        std::vector<P2> f(n + 1), e(n + 1);
        for (int i = 0; i <= n; ++i) f[i] = fillet(T, 2.0 * i / n);
        for (int i = 0; i <= n; ++i) e[i] = involute(T, T.tA * i / n);
        const double cuspGap = 1e-6 * T.m;
        std::optional<std::pair<double, double>> best;  // (w, t)
        for (int i = 0; i < n && !best; ++i) {
            for (int j = 0; j < n; ++j) {
                auto c = crossing(f[i], f[i + 1], e[j], e[j + 1]);
                if (!c) continue;
                const double w = 2.0 * (i + c->first) / n, tt = T.tA * (j + c->second) / n;
                const P2 p = fillet(T, w);
                if (radiusOf(p) - T.rb < cuspGap) continue;  // касание в точке возврата на основной окружности
                best = std::make_pair(w, tt);
                break;
            }
        }
        if (!best)
            return Error{ErrorCode::OperationFailed, "gear profile: the undercut fillet does not cross the involute"};
        // Уточнение пересечения: f(w) = e(t), метод Ньютона по двум параметрам.
        double w = best->first, tt = best->second;
        for (int it = 0; it < 50; ++it) {
            const P2 a = fillet(T, w), b = involute(T, tt);
            const double fx = a.x - b.x, fy = a.y - b.y;
            if (std::hypot(fx, fy) < 1e-14 * T.m) break;
            const double dw = 1e-7, dt = 1e-7;
            const P2 aw = fillet(T, w + dw), bt = involute(T, tt + dt);
            const double j11 = (aw.x - a.x) / dw, j21 = (aw.y - a.y) / dw;
            const double j12 = -(bt.x - b.x) / dt, j22 = -(bt.y - b.y) / dt;
            const double det = j11 * j22 - j12 * j21;
            if (std::abs(det) < 1e-300) break;
            w -= (fx * j22 - fy * j12) / det;
            tt -= (j11 * fy - j21 * fx) / det;
        }
        S.wEnd = w;
        S.tStart = tt;
        S.undercut = true;
    }
    if (S.tStart >= T.tA)
        return Error{ErrorCode::InvalidInput, "gear profile: the involute starts above the tip circle (form diameter >= da)"};
    return S;
}

// Сторона зуба точками формул; проверка самопересечения и выхода за середину впадины.
Result<ToothSide> sidePoints(const Sides& S, int samples) {
    const Toothing& T = S.T;
    ToothSide out;
    const double tau = kPi / T.z;  // половина углового шага
    const int n = std::max(8, samples);
    for (int i = 0; i <= n; ++i) {
        const double a = -tau + (tau - T.gamma) * i / n;
        out.root.push_back({T.rf * std::cos(a), T.rf * std::sin(a)});
    }
    for (int i = 0; i <= n; ++i) {
        const P2 p = fillet(T, S.wEnd * i / n);
        out.fillet.push_back({p.x, p.y});
    }
    for (int i = 0; i <= n; ++i) {
        const double t = std::sqrt(S.tStart * S.tStart + (T.tA * T.tA - S.tStart * S.tStart) * i / n);
        const P2 p = involute(T, t);
        out.involute.push_back({p.x, p.y});
    }
    const double thA = -angleOf(involute(T, T.tA));
    for (int i = 0; i <= n; ++i) {
        const double a = -thA + thA * i / n;
        out.tipHalf.push_back({T.ra * std::cos(a), T.ra * std::sin(a)});
    }
    // Сторона — по часовой стрелке от оси зуба: углы точек переходной кривой и эвольвенты в
    // (−π/z, 0), полилиния стороны без самопересечений.
    std::vector<P2> poly;
    for (const auto* part : {&out.fillet, &out.involute})
        for (const auto& [x, y] : *part) poly.push_back({x, y});
    for (const P2& p : poly) {
        const double a = angleOf(p);
        if (!(a > -tau - 1e-12 && a < 1e-12))
            return Error{ErrorCode::OperationFailed, "gear profile: the tooth flank leaves its angular pitch (interference)"};
    }
    for (std::size_t i = 0; i + 1 < poly.size(); ++i)
        for (std::size_t j = i + 2; j + 1 < poly.size(); ++j) {
            if (std::hypot(poly[i + 1].x - poly[j].x, poly[i + 1].y - poly[j].y) < 1e-12) continue;  // общий узел
            if (crossing(poly[i], poly[i + 1], poly[j], poly[j + 1]))
                return Error{ErrorCode::OperationFailed, "gear profile: the tooth flank intersects itself"};
        }
    return out;
}

// B-сплайн через точки f(u), u ∈ [0, 1], с контролем отклонения в серединах интервалов.
Result<Handle(Geom_BSplineCurve)> fitCurve(const std::function<P2(double)>& f, double tol, double& deviation) {
    for (int n = 8; n <= 1024; n *= 2) {
        Handle(TColgp_HArray1OfPnt) pts = new TColgp_HArray1OfPnt(1, n + 1);
        for (int i = 0; i <= n; ++i) {
            const P2 p = f(static_cast<double>(i) / n);
            pts->SetValue(i + 1, gp_Pnt(p.x, p.y, 0.0));
        }
        GeomAPI_Interpolate interp(pts, false, 1e-12);
        interp.Perform();
        if (!interp.IsDone()) continue;
        Handle(Geom_BSplineCurve) c = interp.Curve();
        double dev = 0.0;
        for (int i = 0; i < n; ++i) {
            for (double q : {0.25, 0.5, 0.75}) {
                const P2 p = f((i + q) / n);
                GeomAPI_ProjectPointOnCurve proj(gp_Pnt(p.x, p.y, 0.0), c);
                if (proj.NbPoints() == 0) {
                    dev = 1e300;
                    break;
                }
                dev = std::max(dev, proj.LowerDistance());
            }
        }
        if (dev <= tol) {
            deviation = std::max(deviation, dev);
            return c;
        }
    }
    return Error{ErrorCode::OperationFailed, "gear profile: B-spline deviation exceeds the tolerance"};
}

gp_Ax2 frameOf(const GearSpec& s) {
    return gp_Ax2(s.axis.Location(), s.axis.Direction(), s.axis.XDirection());
}

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

std::string vec3(const gp_XYZ& v) { return number(v.X()) + " " + number(v.Y()) + " " + number(v.Z()); }

}  // namespace

Result<GearGeometry> gearGeometry(const GearSpec& s) {
    const BasicRack& rk = s.rack;
    if (!(std::isfinite(s.module) && s.module > 0.0)) return Error{ErrorCode::InvalidInput, "gear module must be > 0"};
    if (s.teeth < 3 || s.teeth > 1000) return Error{ErrorCode::InvalidInput, "number of teeth must be in [3, 1000]"};
    if (!(std::isfinite(s.shift) && std::abs(s.shift) <= 2.0))
        return Error{ErrorCode::InvalidInput, "profile shift coefficient must be in [-2, 2]"};
    if (!(std::isfinite(s.width) && s.width > 0.0)) return Error{ErrorCode::InvalidInput, "face width must be > 0"};
    if (!(std::isfinite(s.helix) && s.helix >= 0.0 && s.helix <= kPi / 4 + 1e-12))
        return Error{ErrorCode::InvalidInput, "helix angle must be in [0, 45] degrees"};
    if (!(std::isfinite(rk.alpha) && rk.alpha > 0.0 && rk.alpha < kPi / 4))
        return Error{ErrorCode::InvalidInput, "pressure angle must be in (0, 45) degrees"};
    if (!(rk.haStar > 0.0 && rk.cStar >= 0.0 && rk.rhoStar > 0.0 && std::isfinite(rk.haStar + rk.cStar + rk.rhoStar)))
        return Error{ErrorCode::InvalidInput, "basic rack coefficients must be positive"};
    GearGeometry g;
    const double m = s.module, z = s.teeth, cb = std::cos(s.helix);
    g.mt = m / cb;
    g.alphaT = std::atan(std::tan(rk.alpha) / cb);
    g.d = z * g.mt;
    g.db = g.d * std::cos(g.alphaT);
    g.da = g.d + 2.0 * (rk.haStar + s.shift) * m;
    g.df = g.d - 2.0 * (rk.haStar + rk.cStar - s.shift) * m;
    g.pitch = kPi * m;
    g.s = m * (0.5 * kPi + 2.0 * s.shift * std::tan(rk.alpha));
    g.st = g.s / cb;
    const double sa2 = std::sin(g.alphaT) * std::sin(g.alphaT);
    g.zMin = 2.0 * (rk.haStar - s.shift) * cb / sa2;
    g.xMin = rk.haStar - z * sa2 / (2.0 * cb);
    g.undercut = z < g.zMin - 1e-12;
    // Торцовая толщина по окружности вершин: sa = da·(st/d + inv αt − inv αa).
    if (g.da <= g.db) return Error{ErrorCode::InvalidInput, "tip diameter is below the base diameter"};
    const double alphaA = std::acos(g.db / g.da);
    g.sa = g.da * (g.st / g.d + inv(g.alphaT) - inv(alphaA));
    if (!(g.sa > 0.0))
        return Error{ErrorCode::InvalidInput, "the teeth are pointed (tip thickness sa <= 0): reduce the profile shift"};
    if (g.df <= 0.0) return Error{ErrorCode::InvalidInput, "root diameter must be > 0"};
    // Граничная точка эвольвенты: при отсутствии подрезания — точка, нарезанная концом прямого
    // участка рейки (высота (ha* + c* − ρf*(1 − sin α))·m от делительной прямой).
    const double hT = -(rk.haStar + rk.cStar - rk.rhoStar * (1.0 - std::sin(rk.alpha))) * m;
    const double u = s.shift * m + hT, r = g.d / 2.0;
    if (!g.undercut) {
        const double X = u / std::tan(g.alphaT), Y = r + u;
        g.dForm = 2.0 * std::hypot(X, Y);
    } else {
        g.dForm = 0.0;  // уточняется построением профиля (пересечение переходной кривой с эвольвентой)
    }
    g.lead = s.helix > 0.0 ? kPi * g.d / std::tan(s.helix) : 0.0;
    g.twist = s.helix > 0.0 ? (s.hand == GearHand::Right ? 1.0 : -1.0) * 2.0 * s.width * std::tan(s.helix) / g.d : 0.0;
    return g;
}

Result<ToothSide> toothSidePoints(const GearSpec& spec, int samples) {
    auto S = sides(spec);
    if (!S) return S.error();
    return sidePoints(S.value(), samples);
}

Result<GearProfile> gearProfile(const GearSpec& spec, double tolerance) {
    if (!(std::isfinite(tolerance) && tolerance > 0.0)) return Error{ErrorCode::InvalidInput, "gear profile tolerance must be > 0"};
    auto Sr = sides(spec);
    if (!Sr) return Sr.error();
    const Sides& S = Sr.value();
    const Toothing& T = S.T;
    auto check = sidePoints(S, 200);
    if (!check) return check.error();
    GearProfile out;
    if (S.undercut)
        out.warnings.push_back("gear: undercut (z = " + std::to_string(spec.teeth) + " < z_min = " + number(T.g.zMin) +
                               " for x = " + number(spec.shift) + "): the fillet cuts into the involute");
    if (T.g.sa < 0.25 * T.m)
        out.warnings.push_back("gear: small tip thickness sa = " + number(T.g.sa) + " mm < 0.25 m");
    try {
        double dev = 0.0;
        auto fil = fitCurve([&](double u) { return fillet(T, u * S.wEnd); }, tolerance, dev);
        if (!fil) return fil.error();
        auto evo = fitCurve(
            [&](double u) {
                return involute(T, std::sqrt(S.tStart * S.tStart + (T.tA * T.tA - S.tStart * S.tStart) * u));
            },
            tolerance, dev);
        if (!evo) return evo.error();
        out.maxDeviation = dev;
        const gp_Ax2 frame = frameOf(spec);
        gp_Trsf toWorld;
        toWorld.SetDisplacement(gp_Ax3(), gp_Ax3(frame));
        gp_Trsf mirror;
        mirror.SetMirror(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(1, 0, 0)));
        const double tau = 2.0 * kPi / spec.teeth;
        const double thA = -angleOf(involute(T, T.tA));
        const P2 pForm = fillet(T, S.wEnd);
        const P2 pRoot = fillet(T, 0.0);
        const P2 pTip = involute(T, T.tA);
        auto rot = [&](P2 p, double a, bool mirrored) {
            const double y = mirrored ? -p.y : p.y;
            gp_Pnt q(p.x * std::cos(a) - y * std::sin(a), p.x * std::sin(a) + y * std::cos(a), 0.0);
            return q.Transformed(toWorld);
        };
        const int z = spec.teeth;
        // Вершины: на каждый зуб — впадина справа, граничная точка, вершина (по стороне).
        std::vector<std::array<TopoDS_Vertex, 6>> v(z);
        for (int k = 0; k < z; ++k) {
            const double a = k * tau;
            v[k][0] = BRepBuilderAPI_MakeVertex(rot(pRoot, a, false));
            v[k][1] = BRepBuilderAPI_MakeVertex(rot(pForm, a, false));
            v[k][2] = BRepBuilderAPI_MakeVertex(rot(pTip, a, false));
            v[k][3] = BRepBuilderAPI_MakeVertex(rot(pTip, a, true));
            v[k][4] = BRepBuilderAPI_MakeVertex(rot(pForm, a, true));
            v[k][5] = BRepBuilderAPI_MakeVertex(rot(pRoot, a, true));
        }
        const gp_Circ tipCircle(frame, T.ra), rootCircle(frame, T.rf);
        BRepBuilderAPI_MakeWire wire;
        auto curveOf = [&](const Handle(Geom_BSplineCurve)& c, double a, bool mirrored) {
            Handle(Geom_Curve) cc = Handle(Geom_Curve)::DownCast(c->Copy());
            if (mirrored) cc->Transform(mirror);
            gp_Trsf r;
            r.SetRotation(gp_Ax1(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), a);
            cc->Transform(r);
            cc->Transform(toWorld);
            return cc;
        };
        auto add = [&](const TopoDS_Edge& e, const std::string& role) {
            wire.Add(e);
            if (!wire.IsDone()) throw Standard_Failure(("gear profile: edge " + role + " does not connect").c_str());
            out.roles.emplace_back(wire.Edge(), role);
        };
        for (int k = 0; k < z; ++k) {
            const double a = k * tau;
            const std::string tk = "T" + std::to_string(k + 1) + ".";
            BRepBuilderAPI_MakeEdge fr(curveOf(fil.value(), a, false), v[k][0], v[k][1]);
            BRepBuilderAPI_MakeEdge er(curveOf(evo.value(), a, false), v[k][1], v[k][2]);
            Handle(Geom_Circle) tc = new Geom_Circle(tipCircle);
            BRepBuilderAPI_MakeEdge tip(tc, v[k][2], v[k][3], a - thA, a + thA);
            BRepBuilderAPI_MakeEdge el(curveOf(evo.value(), a, true), v[k][4], v[k][3]);
            BRepBuilderAPI_MakeEdge fl(curveOf(fil.value(), a, true), v[k][5], v[k][4]);
            Handle(Geom_Circle) rc = new Geom_Circle(rootCircle);
            BRepBuilderAPI_MakeEdge root(rc, v[k][5], v[(k + 1) % z][0], a + T.gamma, a + tau - T.gamma);
            for (auto* e : {&fr, &er, &tip, &el, &fl, &root})
                if (!e->IsDone()) return Error{ErrorCode::OperationFailed, "gear profile: edge construction failed"};
            add(fr.Edge(), tk + "FilletR");
            add(er.Edge(), tk + "FlankR");
            add(tip.Edge(), tk + "Tip");
            add(TopoDS::Edge(el.Edge().Reversed()), tk + "FlankL");
            add(TopoDS::Edge(fl.Edge().Reversed()), tk + "FilletL");
            add(root.Edge(), tk + "Root");
        }
        out.wire = wire.Wire();
        if (!out.wire.Closed()) return Error{ErrorCode::OperationFailed, "gear profile: the contour is not closed"};
        BRepBuilderAPI_MakeFace mf(gp_Pln(gp_Ax3(frame)), out.wire, true);
        if (!mf.IsDone()) return Error{ErrorCode::OperationFailed, "gear profile: face construction failed"};
        out.face = mf.Face();
        if (!BRepCheck_Analyzer(out.face).IsValid())
            return Error{ErrorCode::OperationFailed, "gear profile: the profile face is not valid"};
        // Роли — рёбра грани (тот же TShape, что в контуре).
        return out;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("gearProfile", e.what())};
    }
}

namespace {

// Сектор фаски зуба у торца t0 (t0 = 0 — начало венца, t0 = b — конец): тело вращения
// треугольника (ra − c, t0), (R, t0), (R, t0 ± (R − ra + c)) вокруг оси венца на угол sweep от
// угла a0 (от зуба № 1) — снимает материал, где r − (ra − c) > |t − t0| (фаска c × 45° по кромке
// вершин, R = ra + c/2). sweep = 2π — кольцо на все зубья.
TopoDS_Shape chamferSector(const gp_Ax2& frame, double ra, double c, double t0, double sign, double a0, double sweep) {
    const double R = ra + 0.5 * c;
    gp_Dir x = frame.XDirection();
    x.Rotate(gp_Ax1(frame.Location(), frame.Direction()), a0);
    auto at = [&](double rho, double t) {
        return frame.Location().Translated(gp_Vec(x) * rho + gp_Vec(frame.Direction()) * t);
    };
    BRepBuilderAPI_MakePolygon poly(at(ra - c, t0), at(R, t0), at(R, t0 + sign * (R - ra + c)), true);
    BRepBuilderAPI_MakeFace face(poly.Wire(), true);
    BRepPrimAPI_MakeRevol rev(face.Face(), gp_Ax1(frame.Location(), frame.Direction()), sweep);
    rev.Build();
    if (!rev.IsDone()) throw Standard_Failure("tooth chamfer revolution failed");
    return rev.Shape();
}

// Номер зуба (1…z), к которому относится точка: угол вокруг оси от направления зуба № 1,
// повёрнутого на долю twist по ширине (косой зуб).
int toothOf(const gp_Ax2& frame, const GearGeometry& g, int z, double width, const gp_Pnt& p) {
    const gp_Vec v(frame.Location(), p);
    const double t = v.Dot(gp_Vec(frame.Direction()));
    const double a = std::atan2(v.Dot(gp_Vec(frame.YDirection())), v.Dot(gp_Vec(frame.XDirection()))) - g.twist * t / width;
    const double step = 2 * kPi / z;
    int k = static_cast<int>(std::lround(a / step)) % z;
    if (k < 0) k += z;
    return k + 1;
}

}  // namespace

Result<FeatureResult> gearTeeth(const TopoDS_Shape& target, const GearParams& params) {
    const GearSpec& spec = params.spec;
    auto prof = gearProfile(spec, params.tolerance);
    if (!prof) return prof.error();
    GearProfile& P = prof.value();
    auto geo = gearGeometry(spec);
    const GearGeometry& g = geo.value();
    try {
        const gp_Ax2 frame = frameOf(spec);
        const gp_Vec along = gp_Vec(frame.Direction()) * spec.width;
        History toolHistory = newHistory();
        std::vector<std::pair<TopoDS_Shape, std::string>> roles;
        TopoDS_Shape tool;
        const bool helical = spec.helix > 0.0;
        // Тело венца (диск с зубьями): призма или заметание с поворотом.
        TopoDS_Shape gearSolid;
        std::vector<std::pair<TopoDS_Shape, std::string>> gearRoles;
        double outerRadius = 0.0;
        if (params.mode == FeatureMode::Cut) {
            if (target.IsNull()) return Error{ErrorCode::InvalidInput, "gear cut needs a blank body"};
            Bnd_Box box;
            BRepBndLib::Add(target, box);
            double x0, y0, z0, x1, y1, z1;
            box.Get(x0, y0, z0, x1, y1, z1);
            const gp_Lin axisLine(frame.Location(), frame.Direction());
            for (double x : {x0, x1})
                for (double y : {y0, y1})
                    for (double zz : {z0, z1}) outerRadius = std::max(outerRadius, axisLine.Distance(gp_Pnt(x, y, zz)));
            outerRadius = std::max(outerRadius, g.da / 2.0) + spec.module;
        }
        if (!helical) {
            if (params.mode == FeatureMode::Cut) {
                // Кольцо впадин: круг radius outerRadius с профилем венца как отверстием.
                Handle(Geom_Circle) c = new Geom_Circle(gp_Circ(frame, outerRadius));
                TopoDS_Wire outer = BRepBuilderAPI_MakeWire(BRepBuilderAPI_MakeEdge(c)).Wire();
                BRepBuilderAPI_MakeFace mf(gp_Pln(gp_Ax3(frame)), outer, true);
                mf.Add(TopoDS::Wire(P.wire.Reversed()));
                if (!mf.IsDone()) return Error{ErrorCode::OperationFailed, "gear: ring face construction failed"};
                TopoDS_Face ring = mf.Face();
                if (!BRepCheck_Analyzer(ring).IsValid()) return Error{ErrorCode::OperationFailed, "gear: ring face is not valid"};
                BRepPrimAPI_MakePrism prism(ring, along, false);
                prism.Build();
                if (!prism.IsDone()) return Error{ErrorCode::OperationFailed, "gear: prism failed"};
                tool = prism.Shape();
                for (const auto& [e, role] : P.roles) {
                    if (role.size() > 4 && role.compare(role.size() - 4, 4, ".Tip") == 0) continue;
                    for (const TopoDS_Shape& f : prism.Generated(e))
                        if (f.ShapeType() == TopAbs_FACE) roles.emplace_back(f, role);
                }
            } else {
                BRepPrimAPI_MakePrism prism(P.face, along, false);
                prism.Build();
                if (!prism.IsDone()) return Error{ErrorCode::OperationFailed, "gear: prism failed"};
                tool = prism.Shape();
                for (const auto& [e, role] : P.roles)
                    for (const TopoDS_Shape& f : prism.Generated(e))
                        if (f.ShapeType() == TopAbs_FACE) roles.emplace_back(f, role);
                roles.emplace_back(prism.FirstShape(), "Start");
                roles.emplace_back(prism.LastShape(), "End");
            }
        } else {
            // Косой зуб: заметание профиля вдоль оси со вспомогательной винтовой линией на
            // цилиндре радиуса 1 (поворот на twist за ширину b).
            TopoDS_Wire spine = BRepBuilderAPI_MakeWire(
                BRepBuilderAPI_MakeEdge(frame.Location(), frame.Location().Translated(along))).Wire();
            Handle(Geom_CylindricalSurface) cyl = new Geom_CylindricalSurface(gp_Ax3(frame), 1.0);
            Handle(Geom2d_TrimmedCurve) line = GCE2d_MakeSegment(gp_Pnt2d(0, 0), gp_Pnt2d(g.twist, spec.width)).Value();
            TopoDS_Edge helixEdge = BRepBuilderAPI_MakeEdge(line, cyl).Edge();
            BRepLib::BuildCurves3d(helixEdge, 1e-9);
            TopoDS_Wire aux = BRepBuilderAPI_MakeWire(helixEdge).Wire();
            BRepOffsetAPI_MakePipeShell pipe(spine);
            pipe.SetMode(aux, false);
            pipe.SetTolerance(1e-7, 1e-7, 1e-6);
            pipe.Add(P.wire, false, false);
            pipe.Build();
            if (!pipe.IsDone() || !pipe.MakeSolid()) return Error{ErrorCode::OperationFailed, "gear: helical sweep failed"};
            gearSolid = pipe.Shape();
            if (gearSolid.ShapeType() != TopAbs_SOLID) {
                TopExp_Explorer ex(gearSolid, TopAbs_SOLID);
                if (!ex.More()) return Error{ErrorCode::OperationFailed, "gear: helical sweep gave no solid"};
                gearSolid = ex.Current();
            }
            GProp_GProps vp;
            BRepGProp::VolumeProperties(gearSolid, vp);
            if (vp.Mass() < 0) gearSolid.Reverse();
            for (const auto& [e, role] : P.roles)
                for (const TopoDS_Shape& f : pipe.Generated(e))
                    if (f.ShapeType() == TopAbs_FACE) gearRoles.emplace_back(f, role);
            if (params.mode == FeatureMode::Cut) {
                TopoDS_Shape c = BRepPrimAPI_MakeCylinder(frame, outerRadius, spec.width).Shape();
                auto ring = booleanWithHistory(kernel::BooleanType::Cut, {c}, {gearSolid}, false, true);
                if (!ring) return ring.error();
                tool = ring.value().shape;
                for (const auto& [f, role] : gearRoles) {
                    if (role.size() > 4 && role.compare(role.size() - 4, 4, ".Tip") == 0) continue;
                    const auto& mods = ring.value().history->Modified(f);
                    if (!mods.IsEmpty()) {
                        for (const TopoDS_Shape& m : mods) roles.emplace_back(m, role);
                    } else if (containsSubShape(tool, f)) {
                        roles.emplace_back(f, role);
                    }
                }
            } else {
                tool = gearSolid;
                roles = gearRoles;
                roles.emplace_back(pipe.FirstShape(), "Start");
                roles.emplace_back(pipe.LastShape(), "End");
            }
        }
        // Фаски c × 45° по кромкам вершин на торцах венца: кольца фасок объединяются с
        // инструментом нарезания (Cut) или вычитаются из тела венца (Boss, NewBody); роли граней
        // переносятся по истории, конусы фасок — роли «T<k>.ChamferStart» / «T<k>.ChamferEnd».
        const double c = params.chamfer * spec.module;
        if (!(std::isfinite(params.chamfer) && params.chamfer >= 0.0))
            return Error{ErrorCode::InvalidInput, "gear tooth chamfer must be >= 0"};
        // Косой зуб: пересечение конуса фаски с заметёнными гранями зубьев в OCCT занимает минуты
        // на венец — фаска не строится (предупреждение), венец остаётся без фасок.
        if (c > 0.0 && helical) {
            P.warnings.push_back("gear tooth chamfer is not built on helical teeth");
        } else if (c > 0.0) {
            if (!(c < (g.da - g.dForm) / 2.0) || !(3.0 * c < spec.width))
                return Error{ErrorCode::InvalidInput, "gear tooth chamfer " + number(c) + " mm is too large: it must be below " +
                                                          number((g.da - g.dForm) / 2.0) + " mm (tip to form circle) and " +
                                                          number(spec.width / 3.0) + " mm (face width / 3)"};
            const double ra = g.da / 2.0;
            // По сектору на зуб (угол 1,6π/z вокруг оси зуба): габарит сектора пересекается только с
            // гранями своего зуба, и булева операция не пересекает конус со всеми гранями венца.
            // Сектор должен накрывать вершину зуба на всей глубине фаски с учётом поворота косого зуба.
            const double step = 2 * kPi / spec.teeth, half = 0.8 * kPi / spec.teeth;
            const double toothHalf = (kPi / 2 + 2 * spec.shift * std::tan(spec.rack.alpha)) / spec.teeth +
                                     (std::tan(g.alphaT) - g.alphaT) -
                                     (std::tan(std::acos(g.db / (2 * (ra - c)))) - std::acos(g.db / (2 * (ra - c))));
            const double drift = std::abs(g.twist) * 0.75 * c / spec.width;
            const bool sectors = toothHalf + drift < 0.9 * half;
            std::vector<TopoDS_Shape> rings;
            std::vector<std::string> ringRoles;
            for (int end = 0; end < 2; ++end) {
                const double t0 = end == 0 ? 0.0 : spec.width, sign = end == 0 ? 1.0 : -1.0;
                // Нарезание: заготовка, продолжающаяся за торец венца у вершин, фаску не получает.
                if (params.mode == FeatureMode::Cut) {
                    bool beyond = false;
                    for (int k = 0; k < 8 && !beyond; ++k) {
                        const double a = 2 * kPi * k / 8;
                        const gp_Pnt q = frame.Location().Translated(
                            (gp_Vec(frame.XDirection()) * std::cos(a) + gp_Vec(frame.YDirection()) * std::sin(a)) * (ra - 0.5 * c) +
                            gp_Vec(frame.Direction()) * (t0 - sign * 1e-3 * spec.module));
                        BRepClass3d_SolidClassifier cls(target, q, 1e-9);
                        beyond = cls.State() == TopAbs_IN;
                    }
                    if (beyond) {
                        P.warnings.push_back(std::string("gear tooth chamfer is not built at the ") + (end == 0 ? "start" : "end") +
                                             " face: the blank continues beyond the rim");
                        continue;
                    }
                }
                const std::string role = end == 0 ? "ChamferStart" : "ChamferEnd";
                if (!sectors) {
                    rings.push_back(chamferSector(frame, ra, c, t0, sign, 0.0, 2 * kPi));
                    ringRoles.push_back(role);
                    continue;
                }
                // Ось зуба k на середине глубины фаски (косой зуб поворачивается на twist по ширине).
                const double tm = t0 + sign * 0.75 * c;
                for (int k = 0; k < spec.teeth; ++k) {
                    const double axis = k * step + g.twist * tm / spec.width;
                    rings.push_back(chamferSector(frame, ra, c, t0, sign, axis - half, 2 * half));
                    ringRoles.push_back("T" + std::to_string(k + 1) + "." + role);
                }
            }
            if (!rings.empty()) {
                const kernel::BooleanType op = params.mode == FeatureMode::Cut ? kernel::BooleanType::Fuse : kernel::BooleanType::Cut;
                // Тело венца построено из контура, проверенного на самопересечения: повторный
                // BRepAlgoAPI_Check его сплайновых граней не нужен (см. ниже о booleanReady).
                if (!isValidShape(tool)) return Error{ErrorCode::OperationFailed, "gear: the tool body is not valid"};
                ShapeFacts ready;
                ready.valid = true;
                ready.booleanReady = true;
                rememberFacts(tool, ready);
                auto step = booleanWithHistory(op, {tool}, rings, true, true);
                if (!step) return step.error();
                const TopoDS_Shape chamfered = step.value().shape;
                const History& h = step.value().history;
                std::vector<std::pair<TopoDS_Shape, std::string>> mapped;
                for (const auto& [f, role] : roles) {
                    const auto& mods = h->Modified(f);
                    if (!mods.IsEmpty()) {
                        for (const TopoDS_Shape& m : mods) mapped.emplace_back(m, role);
                    } else if (containsSubShape(chamfered, f)) {
                        mapped.emplace_back(f, role);
                    }
                }
                // Конические грани колец фасок в инструменте: по куску на зуб.
                for (std::size_t i = 0; i < rings.size(); ++i) {
                    for (TopExp_Explorer ex(rings[i], TopAbs_FACE); ex.More(); ex.Next()) {
                        BRepAdaptor_Surface sf(TopoDS::Face(ex.Current()), false);
                        if (sf.GetType() != GeomAbs_Cone) continue;
                        std::vector<TopoDS_Shape> images;
                        const auto& mods = h->Modified(ex.Current());
                        for (const TopoDS_Shape& m : mods) images.push_back(m);
                        if (mods.IsEmpty() && containsSubShape(chamfered, ex.Current())) images.push_back(ex.Current());
                        for (const TopoDS_Shape& im : images) {
                            if (im.ShapeType() != TopAbs_FACE) continue;
                            GProp_GProps fp;
                            BRepGProp::SurfaceProperties(im, fp);
                            if (ringRoles[i].front() == 'T') {
                                mapped.emplace_back(im, ringRoles[i]);
                            } else {
                                const int k = toothOf(frame, g, spec.teeth, spec.width, fp.CentreOfMass());
                                mapped.emplace_back(im, "T" + std::to_string(k) + "." + ringRoles[i]);
                            }
                        }
                    }
                }
                roles = std::move(mapped);
                tool = chamfered;
                P.warnings.insert(P.warnings.end(), step.value().warnings.begin(), step.value().warnings.end());
            }
        }
        // Инструмент — призма или заметание контура, проверенного на самопересечения
        // (sidePoints), вокруг оси: самопересечений у него нет, BRepAlgoAPI_Check не повторяется
        // (на 120–360 сплайновых гранях он дольше самой булевой операции).
        if (!isValidShape(tool)) return Error{ErrorCode::OperationFailed, "gear: the tool body is not valid"};
        ShapeFacts facts;
        facts.valid = true;
        facts.booleanReady = true;
        rememberFacts(tool, facts);
        auto res = applyTool(params.mode, target, tool, toolHistory, P.warnings);
        if (!res) return res.error();
        FeatureResult out = std::move(res).value();
        out.roles = std::move(roles);
        return out;
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, occtMessage("gearTeeth", e.what())};
    }
}

std::vector<std::pair<std::string, std::string>> gearAttributes(const GearSpec& s) {
    const gp_Ax3& a = s.axis;
    return {
        {"gear.standard", s.rack.standard},
        {"gear.kind", "external"},
        {"gear.module", number(s.module)},
        {"gear.teeth", std::to_string(s.teeth)},
        {"gear.shift", number(s.shift)},
        {"gear.width", number(s.width)},
        {"gear.helix", number(s.helix)},
        {"gear.hand", s.hand == GearHand::Left ? "left" : "right"},
        {"gear.alpha", number(s.rack.alpha)},
        {"gear.haStar", number(s.rack.haStar)},
        {"gear.cStar", number(s.rack.cStar)},
        {"gear.rhoStar", number(s.rack.rhoStar)},
        {"gear.origin", vec3(a.Location().XYZ())},
        {"gear.axis", vec3(a.Direction().XYZ())},
        {"gear.xdir", vec3(a.XDirection().XYZ())},
        {"gear.accuracy", s.accuracy},
    };
}

Result<GearSpec> gearFromAttributes(const std::vector<std::pair<std::string, std::string>>& attributes) {
    std::map<std::string, std::string> a(attributes.begin(), attributes.end());
    for (const char* key : {"gear.standard", "gear.kind", "gear.module", "gear.teeth", "gear.shift", "gear.width",
                            "gear.helix", "gear.hand", "gear.alpha", "gear.haStar", "gear.cStar", "gear.rhoStar",
                            "gear.origin", "gear.axis", "gear.xdir"})
        if (!a.count(key)) return Error{ErrorCode::InvalidInput, std::string("gear attribute '") + key + "' is missing"};
    if (a["gear.kind"] != "external") return Error{ErrorCode::Unsupported, "only external gears are supported"};
    GearSpec s;
    s.rack.standard = a["gear.standard"];
    s.accuracy = a.count("gear.accuracy") ? a["gear.accuracy"] : "";
    const std::string hand = a["gear.hand"];
    if (hand != "right" && hand != "left") return Error{ErrorCode::InvalidInput, "gear.hand must be right or left"};
    s.hand = hand == "left" ? GearHand::Left : GearHand::Right;
    double teeth = 0.0;
    struct NumKey {
        const char* key;
        double* value;
    };
    for (NumKey k : {NumKey{"gear.module", &s.module}, NumKey{"gear.teeth", &teeth}, NumKey{"gear.shift", &s.shift},
                     NumKey{"gear.width", &s.width}, NumKey{"gear.helix", &s.helix}, NumKey{"gear.alpha", &s.rack.alpha},
                     NumKey{"gear.haStar", &s.rack.haStar}, NumKey{"gear.cStar", &s.rack.cStar},
                     NumKey{"gear.rhoStar", &s.rack.rhoStar}})
        if (!parseNumber(a[k.key], *k.value))
            return Error{ErrorCode::InvalidInput, std::string(k.key) + " is not a number: '" + a[k.key] + "'"};
    if (teeth != std::floor(teeth)) return Error{ErrorCode::InvalidInput, "gear.teeth must be an integer"};
    s.teeth = static_cast<int>(teeth);
    double o[3], d[3], x[3];
    if (!parseVec3(a["gear.origin"], o) || !parseVec3(a["gear.axis"], d) || !parseVec3(a["gear.xdir"], x))
        return Error{ErrorCode::InvalidInput, "gear.origin, gear.axis and gear.xdir must be three numbers"};
    try {
        s.axis = gp_Ax3(gp_Pnt(o[0], o[1], o[2]), gp_Dir(d[0], d[1], d[2]), gp_Dir(x[0], x[1], x[2]));
    } catch (const Standard_Failure&) {
        return Error{ErrorCode::InvalidInput, "gear.axis and gear.xdir must be non-zero, non-parallel vectors"};
    }
    auto g = gearGeometry(s);
    if (!g) return g.error();
    return s;
}

}  // namespace bendcore::modeling
