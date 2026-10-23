#include "bendcore/bend_output.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <optional>
#include <sstream>

namespace bendcore::output {
namespace {

constexpr double kPi = std::numbers::pi;
double deg2rad(double d) { return d * kPi / 180.0; }
double rad2deg(double r) { return r * 180.0 / kPi; }

// Поворот вектора вокруг единичной оси (Родриг) — как в tubegeom.cpp.
Vec3 rot(const Vec3& v, const Vec3& axis, double angle_rad) {
    const double c = std::cos(angle_rad);
    const double s = std::sin(angle_rad);
    return add(add(mul(v, c), mul(cross(axis, v), s)), mul(axis, dot(axis, v) * (1.0 - c)));
}

// Угол (−180°, 180°].
double wrap(double a) {
    a = std::fmod(a, 360.0);
    if (a <= -180.0) a += 360.0;
    if (a > 180.0) a -= 360.0;
    return a;
}

// Чистка «−0» для вывода.
double clean(double v, int precision) {
    const double q = std::pow(10.0, -precision) / 2.0;
    return std::abs(v) < q ? 0.0 : v;
}

struct VertexGeom {
    Vec3 d_in, d_out, n;  // направления до и после вершины, нормаль плоскости гиба
    double B = 0.0;       // угол гиба, градусы ≥ 0
    double T = 0.0;       // касательная R·tan(B/2)
};

std::vector<VertexGeom> vertex_geometry(const SharpPolyline& poly) {
    const auto& P = poly.points;
    std::vector<VertexGeom> out;
    for (std::size_t i = 1; i + 1 < P.size(); ++i) {
        VertexGeom g;
        const Vec3 a = sub(P[i], P[i - 1]);
        const Vec3 b = sub(P[i + 1], P[i]);
        if (norm(a) < 1e-12 || norm(b) < 1e-12) throw OutputError("совпадающие вершины пути");
        g.d_in = unit(a);
        g.d_out = unit(b);
        const double c = std::clamp(dot(g.d_in, g.d_out), -1.0, 1.0);
        g.B = rad2deg(std::acos(c));
        const Vec3 nn = cross(g.d_in, g.d_out);
        if (norm(nn) < 1e-12) throw OutputError("угол гиба 0° или 180°: плоскость гиба не определена");
        g.n = unit(nn);
        g.T = poly.radius[i - 1] * std::tan(deg2rad(g.B) / 2.0);
        out.push_back(g);
    }
    return out;
}

SharpPolyline reversed(const SharpPolyline& p) {
    SharpPolyline r = p;
    std::reverse(r.points.begin(), r.points.end());
    std::reverse(r.radius.begin(), r.radius.end());
    std::reverse(r.bend_no.begin(), r.bend_no.end());
    return r;
}

// LRA с заданной исходной нормалью для первого поворота (nullopt — первый поворот 0).
std::vector<LraRow> lra_impl(const SharpPolyline& poly, std::optional<Vec3> first_plane) {
    const auto geom = vertex_geometry(poly);
    const auto& P = poly.points;
    std::vector<LraRow> rows;
    double T_prev = 0.0;
    std::optional<Vec3> n_prev = first_plane;
    for (std::size_t i = 0; i < geom.size(); ++i) {
        const auto& g = geom[i];
        LraRow r;
        r.bend_no = poly.bend_no[i];
        r.L = norm(sub(P[i + 1], P[i])) - T_prev - g.T;
        if (r.L < -1e-9) {
            throw OutputError("прямой участок перед гибом " + std::to_string(r.bend_no) +
                              " отрицательный: дуги перекрываются");
        }
        r.L = std::max(0.0, r.L);
        if (n_prev) {
            const double s = dot(cross(*n_prev, g.n), g.d_in);
            const double c = dot(*n_prev, g.n);
            r.rotation = wrap(rad2deg(std::atan2(s, c)));
        }
        r.angle = g.B;
        r.radius = poly.radius[i];
        rows.push_back(r);
        T_prev = g.T;
        n_prev = g.n;
    }
    LraRow end;
    end.is_end = true;
    end.L = norm(sub(P.back(), P[P.size() - 2])) - T_prev;
    if (end.L < -1e-9) throw OutputError("последний прямой участок отрицательный");
    end.L = std::max(0.0, end.L);
    rows.push_back(end);
    return rows;
}

std::string fmt(double v, int precision) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << std::fixed << std::setprecision(precision) << clean(v, precision);
    return os.str();
}

}  // namespace

Vec3 to_frame(const Vec3& p, const Frame& f) {
    const Vec3 d = sub(p, f.origin);
    return {dot(d, f.x), dot(d, f.y), dot(d, f.z)};
}

void validate_frame(const Frame& f) {
    const double tol = 1e-6;
    for (const Vec3* a : {&f.x, &f.y, &f.z}) {
        if (std::abs(norm(*a) - 1.0) > tol) throw OutputError("оси СК должны быть единичными");
    }
    if (std::abs(dot(f.x, f.y)) > tol || std::abs(dot(f.y, f.z)) > tol || std::abs(dot(f.x, f.z)) > tol) {
        throw OutputError("оси СК должны быть взаимно перпендикулярны");
    }
    if (dot(cross(f.x, f.y), f.z) < 0.0) throw OutputError("СК должна быть правой");
}

SharpPolyline sharp_polyline(const Path& path, double E_A, double E_B) {
    if (E_A < 0.0 || E_B < 0.0) throw OutputError("глубина вставки E не может быть отрицательной");
    SharpPolyline out;
    Vec3 pos{0.0, 0.0, 0.0};
    Vec3 heading{1.0, 0.0, 0.0};
    Vec3 up{0.0, 0.0, 1.0};
    out.points.push_back(sub(pos, mul(heading, E_A)));
    int bend_no = 0;
    for (const auto& it : path) {
        if (const auto* s = std::get_if<Straight>(&it)) {
            if (s->S < 0.0) throw OutputError("S < 0: геометрическая ошибка");
            pos = add(pos, mul(heading, s->S));
        } else if (const auto* b = std::get_if<Bend>(&it)) {
            if (std::abs(b->B) < kDegenerateBDeg) continue;  // вырожденный гиб — не гиб (2.4)
            if (std::abs(b->B) >= 180.0 - 1e-9) throw OutputError("угол гиба 180°: вершина не определена");
            up = rot(up, heading, deg2rad(b->A));
            const double sgn = b->B >= 0 ? 1.0 : -1.0;
            const double T = tangent_length(b->R, std::abs(b->B));
            const Vec3 vertex = add(pos, mul(heading, T));
            out.points.push_back(vertex);
            out.radius.push_back(b->R);
            out.bend_no.push_back(++bend_no);
            heading = rot(heading, up, sgn * deg2rad(std::abs(b->B)));
            pos = add(vertex, mul(heading, T));
        } else {
            throw OutputError("LRA не описывает спираль");
        }
    }
    out.points.push_back(add(pos, mul(heading, E_B)));
    return out;
}

std::vector<LraRow> lra_from_polyline(const SharpPolyline& poly, Feed feed) {
    if (poly.points.size() < 2) throw OutputError("путь пуст");
    return lra_impl(feed == Feed::A ? poly : reversed(poly), std::nullopt);
}

std::vector<LraRow> lra(const Path& path, Feed feed, double E_A, double E_B) {
    return lra_from_polyline(sharp_polyline(path, E_A, E_B), feed);
}

Path path_from_lra(const std::vector<LraRow>& rows) {
    Path p;
    for (const auto& r : rows) {
        p.push_back(Straight{r.L});
        if (!r.is_end) p.push_back(Bend{r.radius, r.angle, r.rotation});
    }
    return p;
}

std::vector<XyzRow> xyz(const Path& path, Feed feed, double E_A, double E_B, const Frame& frame) {
    validate_frame(frame);
    SharpPolyline poly = sharp_polyline(path, E_A, E_B);
    const bool rev = feed == Feed::B;
    if (rev) poly = reversed(poly);
    std::vector<XyzRow> rows;
    const std::size_t n = poly.points.size();
    for (std::size_t i = 0; i < n; ++i) {
        XyzRow r;
        r.p = to_frame(poly.points[i], frame);
        if (i == 0) {
            r.label = rev ? "Б" : "А";
        } else if (i + 1 == n) {
            r.label = rev ? "А" : "Б";
        } else {
            r.bend_no = poly.bend_no[i - 1];
            r.label = std::to_string(r.bend_no);
            r.radius = poly.radius[i - 1];
        }
        rows.push_back(r);
    }
    return rows;
}

BendTable bend_table(const Path& path, Feed feed, Profile direction, bool minimize) {
    SharpPolyline poly = sharp_polyline(path);
    // В порядке по пути первый поворот отсчитывается от исходной плоскости эскиза (3.1):
    // нормаль исходной плоскости — «верх» +Z ядра. С конца Б исходной плоскости нет: A₁ = 0.
    std::optional<Vec3> first_plane;
    if (feed == Feed::A) {
        first_plane = Vec3{0.0, 0.0, 1.0};
    } else {
        poly = reversed(poly);
    }
    const auto rows = lra_impl(poly, first_plane);
    BendTable t;
    for (const auto& r : rows) {
        if (r.is_end) {
            t.last_S = r.L;
            t.finished_length += r.L;
            continue;
        }
        const TableRow tr = table_row(Bend{r.radius, r.angle, r.rotation}, direction, minimize);
        BendTableRow row;
        row.bend_no = r.bend_no;
        row.S = r.L;
        row.A = tr.A;
        row.B = tr.B;
        row.side = tr.side;
        row.R = r.radius;
        row.arc = r.radius * deg2rad(r.angle);
        t.finished_length += row.S + row.arc;
        t.rows.push_back(row);
    }
    return t;
}

std::string xyz_csv(const std::vector<XyzRow>& rows, const CsvOptions& opt) {
    const char d = opt.delimiter;
    std::ostringstream os;
    os << "Точка" << d << "X" << d << "Y" << d << "Z" << d << "R\n";
    for (const auto& r : rows) {
        os << r.label << d << fmt(r.p[0], opt.precision) << d << fmt(r.p[1], opt.precision) << d
           << fmt(r.p[2], opt.precision) << d;
        if (r.radius) os << fmt(*r.radius, opt.precision);
        os << "\n";
    }
    return os.str();
}

std::string lra_csv(const std::vector<LraRow>& rows, const CsvOptions& opt) {
    const char d = opt.delimiter;
    std::ostringstream os;
    os << "Гиб" << d << "L" << d << "R" << d << "A" << d << "Радиус\n";
    for (const auto& r : rows) {
        if (r.is_end) {
            os << "Конец" << d << fmt(r.L, opt.precision) << d << d << d << "\n";
        } else {
            os << r.bend_no << d << fmt(r.L, opt.precision) << d << fmt(r.rotation, opt.precision) << d
               << fmt(r.angle, opt.precision) << d << fmt(r.radius, opt.precision) << "\n";
        }
    }
    return os.str();
}

}  // namespace bendcore::output
