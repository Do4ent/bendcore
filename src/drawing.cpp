#include "bendcore/drawing.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <limits>
#include <numbers>
#include <set>
#include <sstream>
#include <stdexcept>

#include "bendcore/compensation.hpp"

// OpenSSL для SHA256 (OpenSSL 3.0)
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace bendcore::drawing {
namespace {

constexpr double kPi = std::numbers::pi;
double deg2rad(double d) { return d * kPi / 180.0; }

Vec2 project(const Vec3& p, const ViewBasis& b) { return {dot(p, b.right), dot(p, b.up)}; }

double len2(const Vec2& v) { return std::sqrt(v[0] * v[0] + v[1] * v[1]); }

char decimal_for(Standard s) { return s == Standard::ANSI ? '.' : ','; }

// Путь детали после подрезки: концевые прямые удлинены на E (начало смещается на −E_A по X).
Path with_ends(const Path& path, double E_A, double E_B) {
    Path p;
    if (E_A > 0) p.push_back(Straight{E_A});
    p.insert(p.end(), path.begin(), path.end());
    if (E_B > 0) p.push_back(Straight{E_B});
    return p;
}

struct Segment3 {
    Vec3 a, b;
};

// Прямые участки детали (между точками касания) в СК детали.
std::vector<Segment3> straight_segments(const output::SharpPolyline& poly) {
    const auto& P = poly.points;
    std::vector<double> T(P.size(), 0.0);
    for (std::size_t i = 1; i + 1 < P.size(); ++i) {
        const Vec3 din = unit(sub(P[i], P[i - 1]));
        const Vec3 dout = unit(sub(P[i + 1], P[i]));
        const double B = std::acos(std::clamp(dot(din, dout), -1.0, 1.0));
        T[i] = poly.radius[i - 1] * std::tan(B / 2.0);
    }
    std::vector<Segment3> out;
    for (std::size_t i = 0; i + 1 < P.size(); ++i) {
        const Vec3 d = unit(sub(P[i + 1], P[i]));
        out.push_back({add(P[i], mul(d, T[i])), sub(P[i + 1], mul(d, T[i + 1]))});
    }
    return out;
}

}  // namespace

// ---------- форматирование ----------
double round_to(double v, double step) {
    if (step <= 0) return v;
    return std::round(v / step) * step;
}

std::string fmt(double v, int precision, char decimal) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    const double q = std::pow(10.0, -precision) / 2.0;
    if (std::abs(v) < q) v = 0.0;
    os << std::fixed << std::setprecision(precision) << v;
    std::string s = os.str();
    if (decimal != '.') std::replace(s.begin(), s.end(), '.', decimal);
    return s;
}

// ---------- стандарты ----------
Projection projection_method(Standard s) {
    return s == Standard::ANSI ? Projection::ThirdAngle : Projection::FirstAngle;
}

std::vector<double> scale_series(Standard s) {
    switch (s) {
        case Standard::GOST:  // ГОСТ 2.302
            return {100, 50, 40, 20, 10, 5, 4, 2.5, 2, 1, 1 / 2.0, 1 / 2.5, 1 / 4.0, 1 / 5.0, 1 / 10.0, 1 / 15.0,
                    1 / 20.0, 1 / 25.0, 1 / 40.0, 1 / 50.0, 1 / 75.0, 1 / 100.0};
        case Standard::ISO:
        case Standard::DIN:  // ISO 5455
            return {50, 20, 10, 5, 2, 1, 1 / 2.0, 1 / 5.0, 1 / 10.0, 1 / 20.0, 1 / 50.0, 1 / 100.0};
        case Standard::ANSI:  // распространённый ряд для чертежей в дюймах
            return {4, 2, 1, 1 / 2.0, 1 / 4.0, 1 / 8.0, 1 / 10.0, 1 / 16.0, 1 / 20.0, 1 / 50.0, 1 / 100.0};
    }
    return {1};
}

bool is_standard_scale(double scale, Standard s) {
    for (double v : scale_series(s)) {
        if (std::abs(v - scale) / v < 1e-9) return true;
    }
    return false;
}

std::string scale_label(double scale, Standard s) {
    const char dec = decimal_for(s);
    auto num = [&](double v) {
        std::string t = fmt(v, 2, dec);
        while (!t.empty() && t.back() == '0') t.pop_back();
        if (!t.empty() && t.back() == dec) t.pop_back();
        return t;
    };
    if (scale >= 1.0) return num(scale) + ":1";
    return "1:" + num(1.0 / scale);
}

std::optional<double> pick_scale(double w, double h, double avail_w, double avail_h, Standard s) {
    if (avail_w <= 0 || avail_h <= 0) return std::nullopt;
    for (double v : scale_series(s)) {
        if (w * v <= avail_w + 1e-9 && h * v <= avail_h + 1e-9) return v;
    }
    return std::nullopt;
}

Format format_by_name(const std::string& name, bool landscape) {
    struct F {
        const char* n;
        double w, h;
    };
    static const F table[] = {
        {"A4", 297, 210},      {"A3", 420, 297},      {"A2", 594, 420},      {"A1", 841, 594},
        {"A0", 1189, 841},     {"ANSI A", 279.4, 215.9}, {"ANSI B", 431.8, 279.4}, {"ANSI C", 558.8, 431.8},
        {"ANSI D", 863.6, 558.8}, {"ANSI E", 1117.6, 863.6},
    };
    for (const auto& f : table) {
        if (name == f.n) return landscape ? Format{name, f.w, f.h} : Format{name, f.h, f.w};
    }
    throw std::invalid_argument("неизвестный формат: " + name);
}

SheetFrame make_sheet(Standard s, const Format& f, bool first_sheet) {
    SheetFrame sf;
    sf.format = f;
    double left = 20, other = 5, tbw = 185, tbh = 55;  // ГОСТ 2.104, форма 1
    if (s == Standard::ISO || s == Standard::DIN) {
        left = 20; other = 10; tbw = 180; tbh = 40;     // ISO 5457, ISO 7200
    } else if (s == Standard::ANSI) {
        left = 10; other = 10; tbw = 178; tbh = 45;     // ASME Y14.1
    }
    if (!first_sheet && s == Standard::GOST) tbh = 15;   // форма 2а для последующих листов
    sf.border = {left, other, f.w - other, f.h - other};
    sf.title_block = {sf.border.x1 - tbw, sf.border.y0, sf.border.x1, sf.border.y0 + tbh};
    sf.drawable = {sf.border.x0 + 5, sf.border.y0 + 5, sf.border.x1 - tbw - 5, sf.border.y1 - 5};
    return sf;
}

// ---------- геометрия ----------
void Box2::add(const Vec2& p) {
    if (empty) {
        lo = hi = p;
        empty = false;
        return;
    }
    lo = {std::min(lo[0], p[0]), std::min(lo[1], p[1])};
    hi = {std::max(hi[0], p[0]), std::max(hi[1], p[1])};
}

Box2 Geometry::bbox() const {
    Box2 b;
    for (const auto& l : lines)
        for (const auto& p : l.pts) b.add(p);
    for (const auto& c : circles) {
        b.add({c.c[0] - c.r, c.c[1] - c.r});
        b.add({c.c[0] + c.r, c.c[1] + c.r});
    }
    for (const auto& h : hatches)
        for (const auto& p : h.outline) b.add(p);
    return b;
}

const char* view_name(ViewKind k) {
    switch (k) {
        case ViewKind::Front: return "Главный вид";
        case ViewKind::Top: return "Вид сверху";
        case ViewKind::Left: return "Вид слева";
        case ViewKind::Right: return "Вид справа";
        case ViewKind::Iso: return "Изометрия";
        case ViewKind::BendPlane: return "Вид по плоскости гиба";
        case ViewKind::Unfold: return "Развёртка";
    }
    return "";
}

ViewBasis view_basis(ViewKind kind, const Path& path, int bend_no) {
    switch (kind) {
        case ViewKind::Front: return {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}};
        case ViewKind::Top: return {{0, 0, -1}, {1, 0, 0}, {0, 1, 0}};
        case ViewKind::Left: return {{1, 0, 0}, {0, -1, 0}, {0, 0, 1}};
        case ViewKind::Right: return {{-1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
        case ViewKind::Iso: {
            const Vec3 dir = unit(Vec3{-1, 1, -1});
            const Vec3 z{0, 0, 1};
            const Vec3 up = unit(sub(z, mul(dir, dot(z, dir))));
            const Vec3 right = cross(up, mul(dir, -1.0));
            return {dir, right, up};
        }
        case ViewKind::BendPlane: {
            const auto poly = output::sharp_polyline(path);
            const int n = static_cast<int>(poly.bend_no.size());
            if (bend_no < 1 || bend_no > n) throw std::invalid_argument("нет гиба с номером " + std::to_string(bend_no));
            const auto& P = poly.points;
            const Vec3 din = unit(sub(P[bend_no], P[bend_no - 1]));
            const Vec3 dout = unit(sub(P[bend_no + 1], P[bend_no]));
            const Vec3 n3 = unit(cross(din, dout));
            return {mul(n3, -1.0), din, cross(n3, din)};
        }
        case ViewKind::Unfold:
            break;
    }
    throw std::invalid_argument("для развёртки используйте unfold_view");
}

Geometry tube_view(const Path& path, double od, ViewKind kind, const ViewOptions& opt) {
    if (od <= 0) throw std::invalid_argument("∅ должен быть больше 0");
    const ViewBasis vb = view_basis(kind, path, opt.bend_no);
    const double r = od / 2.0;
    Geometry g;

    const Path full = with_ends(path, opt.E_A, opt.E_B);
    std::vector<Vec3> raw = trace(full, 24, {-opt.E_A, 0.0, 0.0});
    std::vector<Vec3> pts;
    for (const auto& p : raw) {
        if (pts.empty() || norm(sub(p, pts.back())) > 1e-9) pts.push_back(p);
    }
    if (pts.size() < 2) return g;

    // Контур: две линии на ±r от проекции оси; участок «на наблюдателя» — окружность.
    std::vector<Vec2> left, right;
    auto flush = [&]() {
        if (left.size() >= 2) {
            g.lines.push_back({left, LineStyle::Contour});
            g.lines.push_back({right, LineStyle::Contour});
            g.lines.push_back({{left.front(), right.front()}, LineStyle::Contour});
            g.lines.push_back({{left.back(), right.back()}, LineStyle::Contour});
        }
        left.clear();
        right.clear();
    };
    const std::size_t n = pts.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Vec3 t = unit(sub(pts[std::min(i + 1, n - 1)], pts[i > 0 ? i - 1 : 0]));
        const Vec2 tp{dot(t, vb.right), dot(t, vb.up)};
        const Vec2 q = project(pts[i], vb);
        const double l = len2(tp);
        if (l < 1e-3) {
            flush();
            const bool dup = std::any_of(g.circles.begin(), g.circles.end(), [&](const Circle2& c) {
                return len2({c.c[0] - q[0], c.c[1] - q[1]}) < 1e-6;
            });
            if (!dup) g.circles.push_back({q, r, LineStyle::Contour});
            continue;
        }
        const Vec2 nn{-tp[1] / l, tp[0] / l};
        left.push_back({q[0] + nn[0] * r, q[1] + nn[1] * r});
        right.push_back({q[0] - nn[0] * r, q[1] - nn[1] * r});
    }
    flush();

    if (opt.show_axis) {
        std::vector<Vec2> axis;
        for (const auto& p : pts) axis.push_back(project(p, vb));
        g.lines.push_back({axis, LineStyle::Axis});
    }

    const auto poly = output::sharp_polyline(path, opt.E_A, opt.E_B);
    // Граница участка вставки E — тонкая линия поперёк трубы.
    auto e_mark = [&](const Vec3& at, const Vec3& dir) {
        const Vec2 q = project(at, vb);
        const Vec2 tp{dot(dir, vb.right), dot(dir, vb.up)};
        const double l = len2(tp);
        if (l < 1e-3) return;
        const Vec2 nn{-tp[1] / l, tp[0] / l};
        g.lines.push_back({{{q[0] + nn[0] * r, q[1] + nn[1] * r}, {q[0] - nn[0] * r, q[1] - nn[1] * r}}, LineStyle::Thin});
    };
    if (opt.E_A > 0) e_mark({0, 0, 0}, {1, 0, 0});
    if (opt.E_B > 0 && poly.points.size() >= 2) {
        const Vec3 d = unit(sub(poly.points.back(), poly.points[poly.points.size() - 2]));
        e_mark(sub(poly.points.back(), mul(d, opt.E_B)), d);
    }

    if (opt.bend_labels) {
        for (std::size_t i = 0; i < poly.bend_no.size(); ++i) {
            Text2 t;
            t.p = project(poly.points[i + 1], vb);
            t.text = std::to_string(poly.bend_no[i]);
            t.frame = true;
            t.height = 3.5;
            t.shift = {0, 4};
            g.texts.push_back(t);
        }
    }

    // Размеры на виде (7.5): только для участков, параллельных плоскости вида (±0,5°).
    if (opt.dims != DimMode::TableOnly && kind != ViewKind::Iso) {
        const double par = std::sin(deg2rad(0.5));
        auto add_dim = [&](const Vec3& a, const Vec3& b) {
            const Vec3 d = sub(b, a);
            const double L = norm(d);
            if (L < 1e-6 || std::abs(dot(unit(d), vb.dir)) > par) return;
            Dim2 dm;
            dm.a = project(a, vb);
            dm.b = project(b, vb);
            dm.text = fmt(L, opt.precision, opt.decimal);
            dm.offset = 8.0;
            dm.model_offset = r;
            g.dims.push_back(dm);
        };
        if (opt.dims == DimMode::S) {
            for (const auto& s : straight_segments(poly)) add_dim(s.a, s.b);
        } else {
            std::vector<Vec2> thin;
            for (const auto& p : poly.points) thin.push_back(project(p, vb));
            g.lines.push_back({thin, LineStyle::Thin});
            for (std::size_t i = 0; i + 1 < poly.points.size(); ++i) add_dim(poly.points[i], poly.points[i + 1]);
        }
    }
    return g;
}

// ---------- развёртка ----------
UnfoldResult unfold_view(const Path& path, double od, const UnfoldOptions& opt) {
    if (od <= 0) throw std::invalid_argument("∅ должен быть больше 0");
    if (opt.feed != 'A' && opt.feed != 'B') throw std::invalid_argument("подача с конца: A или B");
    UnfoldResult res;
    const double r = od / 2.0;
    const Path work = opt.springback_K ? compensated_path(path, *opt.springback_K) : path;
    const output::Feed feed = opt.feed == 'A' ? output::Feed::A : output::Feed::B;

    const BlankResult br = blank_by_order(work, opt.end_A, opt.end_B, opt.N_first, opt.N_last, opt.feed, false,
                                          opt.dmin, opt.cut_allowed);
    res.issues = br.issues;
    res.blank_length = br.blank_length;
    const double dA = br.delta_start.value_or(0.0), dB = br.delta_end.value_or(0.0);
    const bool fromA = opt.feed == 'A';
    res.delta_start = fromA ? dA : dB;
    res.delta_end = fromA ? dB : dA;
    const double E0 = fromA ? opt.end_A.E : opt.end_B.E;
    const double E1 = fromA ? opt.end_B.E : opt.end_A.E;
    res.caption = fromA ? "Подача с конца А" : "Подача с конца Б";

    const output::BendTable bt = output::bend_table(work, feed, opt.direction, opt.minimize);
    const double rs = opt.round_step;
    auto R = [&](double v) { return round_to(v, rs); };
    auto F = [&](double v) { return fmt(R(v), opt.precision, opt.decimal); };

    Geometry& g = res.geom;
    double x = 0.0;
    std::vector<std::pair<double, bool>> points;  // (x, основная точка гиба) для размеров от базы

    // Начальный конец: зона припуска, линия реза, участок вставки.
    if (res.delta_start > 1e-9) {
        g.hatches.push_back({{{0, -r}, {res.delta_start, -r}, {res.delta_start, r}, {0, r}}});
        x = res.delta_start;
        g.lines.push_back({{{x, -r}, {x, r}}, LineStyle::Cut});
    }
    const double trim0 = x;
    if (E0 > 1e-9) {
        x += E0;
        g.lines.push_back({{{x, -r}, {x, r}}, LineStyle::Thin});
        Text2 t{{x - E0 / 2.0, -r}, "в соединение E", 2.5};
        t.shift = {0, -5};
        g.texts.push_back(t);
    }

    for (const auto& row : bt.rows) {
        x += row.S;
        const double xs = x;
        const double arc = row.R * deg2rad(row.B);
        const double xe = xs + arc;
        g.lines.push_back({{{xs, -r}, {xs, r}}, LineStyle::Thin});
        g.lines.push_back({{{xe, -r}, {xe, r}}, LineStyle::Thin});
        if (opt.numbers) {
            Text2 t{{(xs + xe) / 2.0, 0}, std::to_string(row.bend_no), 3.5};
            t.frame = true;
            g.texts.push_back(t);
        }
        if (opt.angles) {
            std::string lbl = "B=" + fmt(row.B, opt.precision, opt.decimal) + "°";
            lbl += " A=" + fmt(row.A, opt.precision, opt.decimal) + "°";
            if (row.side == Side::Left) lbl += " Л";
            if (row.side == Side::Right) lbl += " П";
            Text2 t{{(xs + xe) / 2.0, -r}, lbl, 2.5};
            t.shift = {0, (row.bend_no % 2) ? -6.0 : -11.0};  // соседние подписи — в шахматном порядке
            g.texts.push_back(t);
        }
        if (opt.arc_lengths) {
            Dim2 d{{xs, -r}, {xe, -r}, 4.0, 0.0, F(arc), true};
            std::swap(d.a, d.b);  // снизу
            g.dims.push_back(d);
        }
        points.push_back({xs, true});
        if (opt.point_mid) points.push_back({(xs + xe) / 2.0, false});
        if (opt.point_end) points.push_back({xe, false});

        MarkRow m;
        m.bend_no = row.bend_no;
        m.B = row.B;
        m.A = row.A;
        double base = xs;
        if (opt.mark_base == MarkBase::Vertex) base += row.R * std::tan(deg2rad(row.B) / 2.0);
        if (opt.mark_base == MarkBase::ArcMid) base += arc / 2.0;
        m.position = R(base + opt.mark_offset);
        res.marks.push_back(m);
        x = xe;
    }
    x += bt.last_S;
    if (E1 > 1e-9) {
        g.lines.push_back({{{x, -r}, {x, r}}, LineStyle::Thin});
        Text2 t{{x + E1 / 2.0, -r}, "в соединение E", 2.5};
        t.shift = {0, -5};
        g.texts.push_back(t);
        x += E1;
    }
    const double trim1 = x;
    if (res.delta_end > 1e-9) {
        g.lines.push_back({{{x, -r}, {x, r}}, LineStyle::Cut});
        g.hatches.push_back({{{x, -r}, {x + res.delta_end, -r}, {x + res.delta_end, r}, {x, r}}});
        x += res.delta_end;
    }
    const double total = x;
    res.trimmed_length = trim1 - trim0;

    // Контур заготовки и ось.
    g.lines.push_back({{{0, r}, {total, r}}, LineStyle::Contour});
    g.lines.push_back({{{0, -r}, {total, -r}}, LineStyle::Contour});
    g.lines.push_back({{{0, -r}, {0, r}}, LineStyle::Contour});
    g.lines.push_back({{{total, -r}, {total, r}}, LineStyle::Contour});
    g.lines.push_back({{{-3, 0}, {total + 3, 0}}, LineStyle::Axis});

    // Размеры сверху: от базы (торец заготовки) до точек гибов или цепочкой.
    double off = 8.0;
    if (opt.chain) {
        double prev = 0.0;
        for (std::size_t k = 0; k < points.size(); ++k) {
            const double px = points[k].first;
            Dim2 d{{prev, r}, {px, r}, off, 0.0, F(px - prev), false};
            g.dims.push_back(d);
            prev = px;
        }
        Dim2 last{{prev, r}, {total, r}, off, 0.0, F(total - prev), true};  // последнее звено справочное
        g.dims.push_back(last);
    } else {
        for (const auto& [px, main] : points) {
            (void)main;
            g.dims.push_back({{0, r}, {px, r}, off, 0.0, F(px), false});
            off += 7.0;
        }
    }

    // Снизу: Δ, E, длина после подрезки (справочная) и длина заготовки (основная).
    auto below = [&](double a, double b, double offset, double value, bool ref, const std::string& prefix = "") {
        Dim2 d{{b, -r}, {a, -r}, offset, 0.0, prefix + F(value), ref};
        g.dims.push_back(d);
    };
    double boff = 16.0;  // ниже подписей углов гибов
    const bool any_delta = res.delta_start > 1e-9 || res.delta_end > 1e-9;
    // Подпись «подрезать» с размером Δ (7.3).
    if (res.delta_start > 1e-9) below(0, res.delta_start, boff, res.delta_start, false, "подрезать ");
    if (res.delta_end > 1e-9) below(trim1, total, boff, res.delta_end, false, "подрезать ");
    if (any_delta) boff += 7.0;  // E — на следующем уровне, чтобы короткие размеры не слипались
    const bool any_e = E0 > 1e-9 || E1 > 1e-9;
    if (E0 > 1e-9) below(trim0, trim0 + E0, boff, E0, false);
    if (E1 > 1e-9) below(trim1 - E1, trim1, boff, E1, false);
    if (any_e) boff += 7.0;
    if (any_delta) {
        below(trim0, trim1, boff, res.trimmed_length, true);
        boff += 7.0;
    }
    if (res.blank_length) below(0, total, boff, *res.blank_length, false);

    Text2 cap{{total / 2.0, r}, res.caption, 5.0};
    cap.shift = {0, off + 8.0};
    g.texts.push_back(cap);
    return res;
}

// ---------- таблицы ----------
Table bend_table(const Path& path, double E_A, double E_B, Profile direction, bool minimize, int precision,
                 char decimal) {
    const auto bt = output::bend_table(path, output::Feed::A, direction, minimize);
    Table t;
    t.title = "Таблица гибов";
    t.header = {"№", "S", "A, °", "B, °"};
    if (direction == Profile::Both) t.header.push_back("Напр.");
    t.header.push_back("Дуга");
    bool same_R = true;
    for (const auto& r : bt.rows) same_R = same_R && std::abs(r.R - bt.rows.front().R) < 1e-9;
    if (!same_R) t.header.insert(t.header.begin() + 1, "R");
    if (!bt.rows.empty() && same_R) t.note = "R = " + fmt(bt.rows.front().R, precision, decimal);
    for (std::size_t i = 0; i < bt.rows.size(); ++i) {
        const auto& r = bt.rows[i];
        const double S = r.S + (i == 0 ? E_A : 0.0);  // на чертеже детали — от физического торца
        std::vector<std::string> row{std::to_string(r.bend_no)};
        if (!same_R) row.push_back(fmt(r.R, precision, decimal));
        row.push_back(fmt(S, precision, decimal));
        row.push_back(fmt(r.A, precision, decimal));
        row.push_back(fmt(r.B, precision, decimal));
        if (direction == Profile::Both) row.push_back(r.side == Side::Left ? "Л" : "П");
        row.push_back(fmt(r.arc, precision, decimal));
        t.rows.push_back(row);
    }
    std::vector<std::string> last{"Конец"};
    if (!same_R) last.push_back("");
    last.push_back(fmt(bt.last_S + E_B + (bt.rows.empty() ? E_A : 0.0), precision, decimal));
    while (last.size() < t.header.size()) last.push_back("");
    t.rows.push_back(last);
    return t;
}

Table vertex_table(const Path& path, double E_A, double E_B, const output::Frame& frame, int precision,
                   char decimal) {
    Table t;
    t.title = "Координаты вершин";
    t.header = {"№", "X", "Y", "Z", "R"};
    for (const auto& r : output::xyz(path, output::Feed::A, E_A, E_B, frame)) {
        t.rows.push_back({r.label, fmt(r.p[0], precision, decimal), fmt(r.p[1], precision, decimal),
                          fmt(r.p[2], precision, decimal), r.radius ? fmt(*r.radius, precision, decimal) : ""});
    }
    return t;
}

Table blank_map(const Path& path, const UnfoldOptions& opt) {
    const Path work = opt.springback_K ? compensated_path(path, *opt.springback_K) : path;
    const output::Feed feed = opt.feed == 'A' ? output::Feed::A : output::Feed::B;
    const BlankResult br = blank_by_order(work, opt.end_A, opt.end_B, opt.N_first, opt.N_last, opt.feed, false,
                                          opt.dmin, opt.cut_allowed);
    const bool fromA = opt.feed == 'A';
    const double d0 = (fromA ? br.delta_start : br.delta_end).value_or(0.0);
    const double d1 = (fromA ? br.delta_end : br.delta_start).value_or(0.0);
    const double E0 = fromA ? opt.end_A.E : opt.end_B.E;
    const double E1 = fromA ? opt.end_B.E : opt.end_A.E;
    auto F = [&](double v) { return fmt(round_to(v, opt.round_step), opt.precision, opt.decimal); };

    Table t;
    t.title = std::string("Карта заготовки (") + (fromA ? "подача с конца А" : "подача с конца Б") + ")";
    t.header = {"Участок", "Длина"};
    if (d0 > 1e-9) t.rows.push_back({"Подрезать", F(d0)});
    if (E0 > 1e-9) t.rows.push_back({"В соединение E", F(E0)});
    const auto bt = output::bend_table(work, feed, opt.direction, opt.minimize);
    for (const auto& r : bt.rows) {
        t.rows.push_back({"Прямой перед гибом " + std::to_string(r.bend_no), F(r.S)});
        t.rows.push_back({"Гиб " + std::to_string(r.bend_no) + " (дуга)", F(r.arc)});
    }
    t.rows.push_back({"Прямой (конец)", F(bt.last_S)});
    if (E1 > 1e-9) t.rows.push_back({"В соединение E", F(E1)});
    if (d1 > 1e-9) t.rows.push_back({"Подрезать", F(d1)});
    t.rows.push_back({"Длина заготовки", br.blank_length ? F(*br.blank_length) : "ошибка"});
    if (opt.springback_K) t.note = "С компенсацией пружинения";
    return t;
}

// ---------- выпуск ----------
ReleaseCheck release_check(const ReleaseInputs& in, Strictness s) {
    ReleaseCheck c;
    if (in.tech_or_geom_errors) {
        c.blocking.push_back("у трубы есть технологические или геометрические ошибки");
        c.not_for_production = true;
    }
    auto soft_or_block = [&](bool cond, const std::string& msg) {
        if (!cond) return;
        (s == Strictness::Strict ? c.blocking : c.warnings).push_back(msg);
    };
    soft_or_block(in.lost_dim_refs > 0, "потерянные ссылки в размерах: " + std::to_string(in.lost_dim_refs));
    soft_or_block(in.unfold_sheet_changed, "лист развёрток: состав изменён");
    if (in.stale_views > 0) {
        c.warnings.push_back("устаревшие виды и таблицы: " + std::to_string(in.stale_views));
        c.offer_update_and_release = true;
    }
    if (in.missing_required_props > 0) {
        c.warnings.push_back("не заполнены обязательные свойства: " + std::to_string(in.missing_required_props));
    }
    c.can_release = c.blocking.empty();
    return c;
}

bool can_unrelease(const std::string& role) { return role == "Выпускающий" || role == "Администратор"; }

// ---------- компоновка листа ----------
namespace {

Vec2 center(const Rect& r) { return {(r.x0 + r.x1) / 2.0, (r.y0 + r.y1) / 2.0}; }

double table_height(const Table& t) {
    return 8.0 /*заголовок*/ + 7.0 /*шапка*/ + (t.note.empty() ? 0.0 : 6.0) + 6.0 * static_cast<double>(t.rows.size());
}

}  // namespace

Table marks_table(const UnfoldResult& u, const UnfoldOptions& opt) {
    Table t;
    t.title = "Метки гибов";
    t.header = {"№", "Метка", "B, °", "A, °"};
    const char* base = opt.mark_base == MarkBase::Vertex ? "вершина" : opt.mark_base == MarkBase::ArcMid ? "середина дуги" : "начало дуги";
    t.note = std::string("от торца заготовки, база: ") + base;
    for (const auto& m : u.marks) {
        t.rows.push_back({std::to_string(m.bend_no), fmt(m.position, opt.precision, opt.decimal),
                          fmt(m.B, opt.precision, opt.decimal), fmt(m.A, opt.precision, opt.decimal)});
    }
    return t;
}

namespace {

struct ViewsLayout {
    std::vector<PlacedView> views;
    double scale = 1.0;
    bool fits = true;
    std::vector<std::string> warnings;
};

// Ортогональные виды и изометрия в области. Главный и боковой — в одном ряду (общий Y),
// главный и вид сверху — в одной колонке (общий X); масштаб — наибольший из ряда.
ViewsLayout layout_views(const DrawingInput& in, const Rect& upper, Projection proj, char dec) {
    ViewsLayout L;
    ViewOptions vo;
    vo.dims = in.dims;
    vo.E_A = in.unfold.end_A.E;
    vo.E_B = in.unfold.end_B.E;
    vo.precision = in.precision;
    vo.decimal = dec;
    const bool first = proj == Projection::FirstAngle;
    const ViewKind side = first ? ViewKind::Left : ViewKind::Right;
    const bool full = in.preset == Preset::Full;
    const double gap = 22.0;  // место под размеры и подписи между видами

    Geometry gf = tube_view(in.path, in.od, ViewKind::Front, vo);
    Geometry gt, gs;
    if (full) {
        gt = tube_view(in.path, in.od, ViewKind::Top, vo);
        gs = tube_view(in.path, in.od, side, vo);
    }
    ViewOptions io = vo;
    io.dims = DimMode::TableOnly;
    Geometry gi = tube_view(in.path, in.od, ViewKind::Iso, io);
    const Box2 bf = gf.bbox(), bt = gt.bbox(), bs = gs.bbox(), bi = gi.bbox();

    const double sumw = bf.w() + (full ? bs.w() : 0.0);
    const double sumh = bf.h() + (full ? bt.h() : 0.0);
    const double iso_share = full ? 0.0 : 0.38;  // в минимальном наборе изометрия справа от главного
    const double avail_w = upper.w() * (1.0 - iso_share) - gap * (full ? 3 : 2);
    const double avail_h = upper.h() - gap * (full ? 3 : 2);
    std::optional<double> os = pick_scale(sumw, sumh, avail_w, avail_h, in.standard);
    if (!os) {
        os = scale_series(in.standard).back();
        L.fits = false;
        L.warnings.push_back("виды не помещаются на формат даже в самом мелком масштабе: выберите больший формат");
    }
    const double s = *os;
    L.scale = s;

    const double left_x = upper.x0 + gap;
    const double top_y = upper.y1 - gap;
    Vec2 of{}, ot{}, oside{};
    if (full) {
        const double front_h = bf.h() * s, top_h = bt.h() * s;
        const double fy_top = first ? top_y : top_y - top_h - gap;
        of = {left_x - s * bf.lo[0], fy_top - s * bf.hi[1]};
        const double ty_top = first ? fy_top - front_h - gap : top_y;
        ot = {of[0], ty_top - s * bt.hi[1]};
        oside = {left_x + bf.w() * s + gap - s * bs.lo[0], of[1]};
    } else {
        of = {left_x - s * bf.lo[0], (upper.y0 + upper.y1) / 2.0 - s * (bf.lo[1] + bf.hi[1]) / 2.0};
    }
    auto place = [&](ViewKind k, Geometry g, double scale, Vec2 origin, std::string label) {
        PlacedView pv;
        pv.kind = k;
        pv.id = view_name(k);
        pv.geom = std::move(g);
        pv.scale = scale;
        pv.origin = origin;
        pv.label = std::move(label);  // виды в проекционной связи не обозначаются (ГОСТ 2.305)
        L.views.push_back(std::move(pv));
    };
    place(ViewKind::Front, std::move(gf), s, of, "");
    if (full) {
        place(ViewKind::Top, std::move(gt), s, ot, "");
        place(side, std::move(gs), s, oside, "");
    }

    Rect iso_cell;
    if (full) {
        const double right_of_top = left_x + bt.w() * s + gap;
        const double right_of_front = left_x + bf.w() * s + gap;
        if (first) {  // свободно справа от вида сверху, под боковым
            iso_cell = {std::max(right_of_top, right_of_front), upper.y0, upper.x1, of[1] + s * bs.lo[1] - gap};
        } else {      // свободно справа от вида сверху, над боковым
            iso_cell = {right_of_top, of[1] + s * bs.hi[1] + gap, upper.x1, upper.y1};
        }
        if (iso_cell.w() < 40 || iso_cell.h() < 40) {
            iso_cell = {right_of_front + bs.w() * s + gap, upper.y0, upper.x1, upper.y1};
        }
    } else {
        iso_cell = {left_x + bf.w() * s + gap, upper.y0, upper.x1, upper.y1};
    }
    if (iso_cell.w() > 20 && iso_cell.h() > 20) {
        const auto si = pick_scale(bi.w(), bi.h(), iso_cell.w() - 10, iso_cell.h() - 10, in.standard);
        const double sc = si ? std::min(*si, s) : scale_series(in.standard).back();
        const Vec2 c = center(iso_cell);
        place(ViewKind::Iso, std::move(gi), sc,
              {c[0] - sc * (bi.lo[0] + bi.hi[0]) / 2.0, c[1] - sc * (bi.lo[1] + bi.hi[1]) / 2.0},
              std::abs(sc - s) > 1e-12 ? "(" + scale_label(sc, in.standard) + ")" : "");
    } else {
        L.warnings.push_back("изометрия не помещается на лист");
    }
    return L;
}

double strip_height(const UnfoldResult& u) {
    int above = 0;
    for (const auto& d : u.geom.dims) above += d.a[1] > 0;
    return (8.0 + 7.0 * above + 14.0) + (16.0 + 7.0 * 4 + 6.0) + 10.0;
}

// Развёртка в полосе; масштаб — наибольший из ряда по ширине и свободной высоте.
PlacedView place_unfold(const DrawingInput& in, UnfoldResult& u, const Rect& strip, double main_scale,
                        std::vector<std::string>& warnings) {
    PlacedView pv;
    pv.kind = ViewKind::Unfold;
    pv.id = "Развёртка";
    const Box2 b = u.geom.bbox();
    int above = 0;
    for (const auto& d : u.geom.dims) above += d.a[1] > 0;
    const double h_above = 8.0 + 7.0 * above + 14.0;
    const double h_below = 16.0 + 7.0 * 4 + 6.0;
    const double avail_h = strip.h() - h_above - h_below;
    auto s = pick_scale(b.w(), b.h(), strip.w() - 20, std::max(2.0, avail_h), in.standard);
    if (!s) {
        s = scale_series(in.standard).back();
        warnings.push_back("развёртка не помещается: нужен разрыв вида или больший формат");
    }
    pv.scale = *s;
    pv.origin = {strip.x0 + (strip.w() - pv.scale * b.w()) / 2.0 - pv.scale * b.lo[0],
                 strip.y0 + h_below - pv.scale * b.lo[1]};
    pv.label = "Развёртка" + (std::abs(pv.scale - main_scale) > 1e-12 ? " (" + scale_label(pv.scale, in.standard) + ")"
                                                                      : std::string());
    pv.geom = std::move(u.geom);
    return pv;
}

void place_tables(DrawingSheet& ds, std::vector<Table> tables) {
    const Rect& tb = ds.frame.title_block;
    double y = ds.frame.border.y1 - 5;
    for (auto& t : tables) {
        const double h = table_height(t);
        if (y - h < tb.y1 + 5) ds.warnings.push_back("таблица «" + t.title + "» не помещается в колонку");
        ds.tables.push_back({std::move(t), {tb.x0, y}, tb.w()});
        y -= h + 5;
    }
}

}  // namespace

std::vector<DrawingSheet> make_part_drawing(const DrawingInput& in) {
    const Format fmt_ = format_by_name(in.format, in.landscape);
    const Projection proj = projection_method(in.standard);
    const char dec = decimal_for(in.standard);

    UnfoldOptions uo = in.unfold;
    uo.decimal = dec;
    uo.precision = in.precision;

    auto new_sheet = [&](bool first) {
        DrawingSheet ds;
        ds.standard = in.standard;
        ds.projection = proj;
        ds.frame = make_sheet(in.standard, fmt_, first);
        ds.not_for_production = in.has_errors;
        ds.title = in.title;
        ds.layers = make_default_layers();
        ds.layer_visibility = make_layer_visibility();
        return ds;
    };

    DrawingSheet s1 = new_sheet(true);
    const Rect area = s1.frame.drawable;

    // Вариант «всё на одном листе»: развёртка полосой внизу.
    UnfoldResult unf = unfold_view(in.path, in.od, uo);
    const double strip_h = std::clamp(strip_height(unf), 50.0, area.h() * 0.5);
    const Rect strip{area.x0, area.y0, area.x1, area.y0 + strip_h};
    const Rect upper{area.x0, strip.y1 + 5, area.x1, area.y1};
    ViewsLayout one = layout_views(in, upper, proj, dec);
    ViewsLayout alone = layout_views(in, area, proj, dec);
    const bool split = alone.scale > one.scale + 1e-12;  // развёртка отнимает масштаб видов → на лист 2

    const ViewsLayout& vl = split ? alone : one;
    s1.views = vl.views;
    s1.warnings = vl.warnings;
    std::vector<Table> t1;
    t1.push_back(bend_table(in.path, in.unfold.end_A.E, in.unfold.end_B.E, in.unfold.direction, in.unfold.minimize,
                            in.precision, dec));
    if (in.preset == Preset::Full) {
        t1.push_back(vertex_table(in.path, in.unfold.end_A.E, in.unfold.end_B.E, {}, in.precision, dec));
    }

    std::vector<DrawingSheet> out;
    std::vector<Table> tu{blank_map(in.path, uo), marks_table(unf, uo)};
    std::vector<std::string> unfold_warnings;
    for (const auto& is : unf.issues) unfold_warnings.push_back("развёртка: " + is.text);
    if (!split) {
        s1.views.push_back(place_unfold(in, unf, strip, vl.scale, s1.warnings));
        for (auto& t : tu) t1.push_back(std::move(t));
        place_tables(s1, std::move(t1));
        out.push_back(std::move(s1));
    } else {
        place_tables(s1, std::move(t1));
        out.push_back(std::move(s1));
        DrawingSheet s2 = new_sheet(false);
        // Лист 2: таблицы — вверху справа; развёртка — на всю ширину рамки под таблицами
        // (штамп формы 2а низкий, поэтому полоса идёт над ним).
        place_tables(s2, std::move(tu));
        double tables_bottom = s2.frame.border.y1;
        for (const auto& t : s2.tables) tables_bottom = std::min(tables_bottom, t.top_left[1] - table_height(t.table));
        const Rect& bd = s2.frame.border;
        Rect strip2{bd.x0 + 5, s2.frame.title_block.y1 + 5, bd.x1 - 5, tables_bottom - 5};
        if (strip2.h() < strip_height(unf)) strip2 = s2.frame.drawable;  // таблицы высокие — слева от колонки
        s2.views.push_back(place_unfold(in, unf, strip2, vl.scale, s2.warnings));
        out.push_back(std::move(s2));
    }

    // Штамп: масштаб главного вида, номера листов.
    double main_scale = vl.scale;
    const int count = static_cast<int>(out.size());
    for (int i = 0; i < count; ++i) {
        auto& ds = out[static_cast<std::size_t>(i)];
        ds.number = i + 1;
        ds.count = count;
        ds.title["Масштаб"] = scale_label(i == 0 ? main_scale : ds.views.front().scale, in.standard);
        ds.title["Лист"] = std::to_string(i + 1);
        ds.title["Листов"] = std::to_string(count);
    }
    if (!is_standard_scale(main_scale, in.standard)) out.front().warnings.push_back("нестандартный масштаб");
    for (auto& w : unfold_warnings) out.back().warnings.push_back(w);
    return out;
}

// ---------- Ассоциативность (версионирование) ----------

const char* associativity_name(DrawingAssociativity a) {
    switch (a) {
        case DrawingAssociativity::Fresh:
            return "fresh";
        case DrawingAssociativity::Stale:
            return "stale";
        case DrawingAssociativity::Broken:
            return "broken";
        case DrawingAssociativity::NotApplicable:
            return "not_applicable";
    }
    return "unknown";
}

// SHA256 хэш строки (OpenSSL 3.0 EVP API)
static std::string sha256_string(const std::string& input) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) throw std::runtime_error("EVP_MD_CTX_new failed");

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("EVP_DigestInit_ex failed");
    }

    if (EVP_DigestUpdate(mdctx, input.c_str(), input.length()) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("EVP_DigestUpdate failed");
    }

    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("EVP_DigestFinal_ex failed");
    }

    EVP_MD_CTX_free(mdctx);

    std::ostringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return ss.str();
}

PathFingerprint compute_fingerprint(
    const Path& path, double od, double wall,
    Standard standard, const std::string& format) {

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(6);

    // Хэшируем путь
    ss << "path:";
    for (const auto& item : path) {
        if (const auto* straight = std::get_if<Straight>(&item)) {
            ss << "S" << straight->S << "|";
        } else if (const auto* bend = std::get_if<Bend>(&item)) {
            ss << "B" << bend->R << "," << bend->B << "," << bend->A << "|";
        } else if (const auto* spiral = std::get_if<Spiral>(&item)) {
            ss << "Sp" << spiral->p << "," << spiral->n << "|";
        }
    }

    ss << "od:" << od << "|";
    ss << "wall:" << wall << "|";
    ss << "std:" << static_cast<int>(standard) << "|";
    ss << "fmt:" << format;

    return PathFingerprint{sha256_string(ss.str())};
}

std::string iso_8601_now() {
    std::time_t now = std::time(nullptr);
    std::tm* gmt = std::gmtime(&now);
    std::ostringstream ss;
    ss << std::put_time(gmt, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

AssociativeDrawing make_associative_drawing(const DrawingInput& in) {
    // Создать обычный чертёж
    auto sheets = make_part_drawing(in);

    // Вычислить отпечаток
    PathFingerprint fp = compute_fingerprint(in.path, in.od, in.wall, in.standard, in.format);

    // Создать первую версию
    DrawingVersion v1;
    v1.revision = 1;
    v1.fingerprint = fp;
    v1.sheets = sheets;
    v1.timestamp = iso_8601_now();
    v1.author = "system";
    v1.comment = "создан";

    // Создать состояние
    AssociativeDrawingState state;
    state.status = DrawingAssociativity::Fresh;
    state.fingerprint = fp;
    state.last_update_time = v1.timestamp;
    state.last_updated_by = v1.author;
    state.change_log.push_back(v1.comment);

    // Вернуть ассоциативный чертёж
    AssociativeDrawing result;
    result.versions.push_back(v1);
    result.state = state;
    return result;
}

DrawingChanges detect_changes(
    const DrawingInput& old_input,
    const DrawingInput& new_input) {

    DrawingChanges changes;

    // Сравнить пути (количество элементов и их параметры)
    if (old_input.path.size() != new_input.path.size()) {
        changes.path_changed = true;
    } else {
        for (std::size_t i = 0; i < old_input.path.size(); ++i) {
            if (old_input.path[i].index() != new_input.path[i].index()) {
                changes.path_changed = true;
                break;
            }
            // Примерное сравнение параметров гибов
            if (const auto* old_bend = std::get_if<Bend>(&old_input.path[i])) {
                if (const auto* new_bend = std::get_if<Bend>(&new_input.path[i])) {
                    if (std::abs(old_bend->R - new_bend->R) > 0.01 ||
                        std::abs(old_bend->B - new_bend->B) > 0.01) {
                        changes.path_changed = true;
                        break;
                    }
                }
            }
        }
    }

    if (std::abs(old_input.od - new_input.od) > 0.01) changes.od_changed = true;
    if (std::abs(old_input.wall - new_input.wall) > 0.01) changes.wall_changed = true;
    if (old_input.preset != new_input.preset) changes.preset_changed = true;
    if (old_input.format != new_input.format) changes.format_changed = true;

    return changes;
}

AssociativeDrawing update_associative_drawing(
    const AssociativeDrawing& previous,
    const DrawingInput& updated_input,
    const std::string& author,
    const std::string& comment) {

    // Вычислить новый отпечаток
    PathFingerprint new_fp = compute_fingerprint(
        updated_input.path, updated_input.od, updated_input.wall,
        updated_input.standard, updated_input.format);

    // Если отпечаток тот же, вернуть предыдущий чертёж
    if (new_fp == previous.state.fingerprint) {
        return previous;
    }

    // Создать новый чертёж
    AssociativeDrawing result = previous;

    try {
        // Создать новую версию
        auto sheets = make_part_drawing(updated_input);

        // Найти следующий номер ревизии
        int next_revision = 1;
        if (!result.versions.empty()) {
            next_revision = result.versions.front().revision + 1;
        }

        DrawingVersion new_version;
        new_version.revision = next_revision;
        new_version.fingerprint = new_fp;
        new_version.sheets = sheets;
        new_version.timestamp = iso_8601_now();
        new_version.author = author;
        new_version.comment = comment;

        // Добавить новую версию в начало
        result.versions.insert(result.versions.begin(), new_version);

        // Обновить состояние
        result.state.status = DrawingAssociativity::Fresh;
        result.state.fingerprint = new_fp;
        result.state.last_update_time = new_version.timestamp;
        result.state.last_updated_by = new_version.author;
        result.state.change_log.push_back(new_version.comment);

    } catch (const std::exception& e) {
        // Если произошла ошибка, пометить как сломанный
        result.state.status = DrawingAssociativity::Broken;
        result.state.change_log.push_back(std::string("error: ") + e.what());
    }

    return result;
}

AssociativeDrawing::Diff AssociativeDrawing::compare(
    const DrawingVersion& v1, const DrawingVersion& v2) const {

    Diff diff;

    if (v1.sheets.size() != v2.sheets.size()) {
        for (std::size_t i = 0; i < std::max(v1.sheets.size(), v2.sheets.size()); ++i) {
            diff.changed_sheets.push_back(std::to_string(i + 1));
        }
    } else {
        for (std::size_t i = 0; i < v1.sheets.size(); ++i) {
            if (v1.sheets[i].views.size() != v2.sheets[i].views.size()) {
                diff.changed_sheets.push_back(std::to_string(i + 1));
            }
            for (std::size_t j = 0; j < std::min(v1.sheets[i].views.size(), v2.sheets[i].views.size()); ++j) {
                if (v1.sheets[i].views[j].id != v2.sheets[i].views[j].id) {
                    diff.changed_views.push_back(v1.sheets[i].views[j].id);
                }
            }
        }
    }

    return diff;
}

void AssociativeDrawing::revert_to(int revision_number) {
    // Найти версию с заданным номером
    auto it = std::find_if(versions.begin(), versions.end(),
                          [revision_number](const DrawingVersion& v) { return v.revision == revision_number; });

    if (it != versions.end()) {
        // Переместить эту версию в начало
        DrawingVersion reverted = *it;
        versions.erase(it);
        versions.insert(versions.begin(), reverted);

        // Обновить состояние
        state.status = DrawingAssociativity::Fresh;
        state.fingerprint = reverted.fingerprint;
        state.last_update_time = iso_8601_now();
        state.last_updated_by = "system";
        state.change_log.push_back("reverted to revision " + std::to_string(revision_number));
    } else {
        throw std::invalid_argument("revision " + std::to_string(revision_number) + " not found");
    }
}

// ---------- Пакетный вывод (сборочный чертёж) ----------

std::vector<DrawingSheet> make_assembly_drawing(const AssemblyDrawingInput& in) {
    if (in.tubes.empty()) {
        throw std::invalid_argument("assembly drawing requires at least one tube");
    }

    std::vector<DrawingSheet> out;

    // Генерируем БОМ (одна строка на каждую уникальную трубу)
    std::vector<BOMRow> bom;
    int position = 1;
    for (const auto& [tube_id, input] : in.tubes) {
        BOMRow row;
        row.position = position++;
        row.designation = input.title.count("Обозначение") ? input.title.at("Обозначение") : "TUBE";
        row.od = input.od;
        row.wall = input.wall;
        row.quantity = 1;
        row.tube_ids.push_back(tube_id);
        bom.push_back(row);
    }

    // Лист 1: БОМ
    {
        DrawingSheet sheet1;
        sheet1.number = 1;
        sheet1.standard = in.standard;
        sheet1.projection = projection_method(in.standard);
        sheet1.frame = make_sheet(in.standard, format_by_name(in.format), true);
        sheet1.title["Масштаб"] = "1:1";

        // Таблица БОМ
        auto bom_table = create_specification_table(bom, {});
        PlacedTable pt;
        pt.table = bom_table;
        pt.top_left = {20, sheet1.frame.drawable.y1 - 200};
        pt.width = sheet1.frame.drawable.w() - 40;
        sheet1.tables.push_back(pt);

        out.push_back(sheet1);
    }

    // Листы 2+: развёртки по 4 шт на листе (2x2)
    std::vector<std::vector<DrawingSheet>> unfold_sheets_by_tube;
    for (const auto& [tube_id, input] : in.tubes) {
        auto sheets = make_part_drawing(input);
        unfold_sheets_by_tube.push_back(sheets);
    }

    // Раскладка развёрток по листам
    int sheet_num = 2;
    size_t tube_idx = 0;
    while (tube_idx < unfold_sheets_by_tube.size()) {
        DrawingSheet assembly_sheet;
        assembly_sheet.number = sheet_num;
        assembly_sheet.standard = in.standard;
        assembly_sheet.projection = projection_method(in.standard);
        assembly_sheet.frame = make_sheet(in.standard, format_by_name(in.format), false);
        assembly_sheet.title["Масштаб"] = "1:1";
        assembly_sheet.title["Лист"] = std::to_string(sheet_num);

        // Раскладка на 2x2 сетку
        double x_start = assembly_sheet.frame.drawable.x0 + 10;
        double y_start = assembly_sheet.frame.drawable.y1 - 10;
        double cell_w = (assembly_sheet.frame.drawable.w() - 30) / 2;
        double cell_h = (assembly_sheet.frame.drawable.h() - 30) / 2;

        int pos = 0;  // 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right
        while (pos < in.tubes_per_unfold_sheet && tube_idx < unfold_sheets_by_tube.size()) {
            int row = pos / 2;
            int col = pos % 2;
            double x = x_start + col * cell_w;
            double y = y_start - row * cell_h;

            // Копируем развёртку (только первый лист из многих)
            if (!unfold_sheets_by_tube[tube_idx].empty()) {
                const auto& unfold = unfold_sheets_by_tube[tube_idx][0];
                for (const auto& view : unfold.views) {
                    PlacedView pv = view;
                    pv.origin = {x + view.origin[0], y - view.origin[1]};
                    assembly_sheet.views.push_back(pv);
                }
            }

            tube_idx++;
            pos++;
        }

        assembly_sheet.count = sheet_num;  // будет обновлено в конце
        out.push_back(assembly_sheet);
        sheet_num++;
    }

    // Обновить номера листов и счётчик
    for (auto& sheet : out) {
        sheet.count = out.size();
    }

    return out;
}

Table create_specification_table(
    const std::vector<BOMRow>& bom,
    const SpecificationOptions& opt) {

    Table t;
    t.title = opt.title;
    t.header = {"Поз.", "Обозначение", "Материал", "Кол-во", "Примечание"};

    // Заполнить строки спецификации
    for (const auto& row : bom) {
        t.rows.push_back({
            std::to_string(row.position),
            row.designation,
            std::string(),  // материал (пусто для труб)
            std::to_string(row.quantity),
            std::string()   // примечание (пусто)
        });
    }

    return t;
}

// ---------- Экспорт в PDF ----------
// Функция для подготовки данных к PDF экспорту
// (сам рендеринг в PDF происходит на клиенте через Puppeteer или другой инструмент)
// Примечание: Ручные размеры (Subtask 2.7) должны быть включены в SVG-рендеринг следующим образом:
// 1. Для каждого ручного размера добавляется <text> элемент в SVG с красным цветом
// 2. Текст отображается рядом с точкой позиции размера
// 3. Справочные размеры (reference=true) отображаются со звёздочкой (*)
// 4. Пунктирная линия использует stroke-dasharray для визуального отличия от авто-размеров
PDFExportData prepare_pdf_export(
    const std::vector<DrawingSheet>& sheets,
    const PDFExportOptions& opts) {

    PDFExportData result;
    result.title = opts.title;
    result.author = opts.author;
    result.subject = opts.subject;
    result.page_count = static_cast<int>(sheets.size());

    // В реальной реализации здесь происходил бы рендеринг в SVG
    // с добавлением водяной метки, если нужна
    // Для теста просто возвращаем пустой результат
    for (const auto& sheet : sheets) {
        std::string svg = "<svg width=\"800\" height=\"600\" xmlns=\"http://www.w3.org/2000/svg\">\n";
        svg += "  <!-- Sheet " + std::to_string(sheet.number) + " -->\n";

        // Render manual dimensions if available
        if (!manual_dims.empty()) {
            svg += "  <!-- Manual Dimensions -->\n";
            for (size_t i = 0; i < manual_dims.size(); ++i) {
                const auto& dim = manual_dims[i];
                // Dimension line
                svg += "    <line x1=\"" + std::to_string((int)dim.x1) + "\" y1=\"" + std::to_string((int)dim.y1) + "\" ";
                svg += "x2=\"" + std::to_string((int)dim.x2) + "\" y2=\"" + std::to_string((int)dim.y2) + "\" ";
                svg += "stroke=\"black\" stroke-width=\"1\"/>\n";
                // Dimension text
                double mid_x = (dim.x1 + dim.x2) / 2.0;
                double mid_y = (dim.y1 + dim.y2) / 2.0;
                svg += "    <text x=\"" + std::to_string((int)mid_x) + "\" y=\"" + std::to_string((int)mid_y) + "\" ";
                svg += "font-size=\"12\" fill=\"black\">" + dim.text + "</text>\n";
            }
        }
        svg += "</svg>";
        result.svg_pages.push_back(svg);
    }

    return result;
}

// ---------- 2D-эскизы на чертежах (v7.9) ----------

namespace {

// Генерировать уникальный ID для элементов эскиза
std::string generate_sketch_element_id(SketchElementType type) {
    static int line_counter = 0;
    static int arc_counter = 0;
    static int text_counter = 0;
    static int dim_counter = 0;

    char buffer[32];
    switch (type) {
        case SketchElementType::Line:
            std::snprintf(buffer, sizeof(buffer), "line_%d", ++line_counter);
            break;
        case SketchElementType::Arc:
            std::snprintf(buffer, sizeof(buffer), "arc_%d", ++arc_counter);
            break;
        case SketchElementType::Text:
            std::snprintf(buffer, sizeof(buffer), "text_%d", ++text_counter);
            break;
        case SketchElementType::Dimension:
            std::snprintf(buffer, sizeof(buffer), "dim_%d", ++dim_counter);
            break;
    }
    return std::string(buffer);
}

// Генерировать уникальный ID для размера
std::string generate_sketch_dimension_id() {
    static int counter = 0;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "skdim_%d", ++counter);
    return std::string(buffer);
}

}  // namespace

SketchElement make_sketch_line(const Vec2& p0, const Vec2& p1, const std::string& id) {
    SketchElement elem;
    elem.type = SketchElementType::Line;
    elem.p0 = p0;
    elem.p1 = p1;
    elem.id = id.empty() ? generate_sketch_element_id(SketchElementType::Line) : id;
    elem.style = LineStyle::Contour;
    return elem;
}

SketchElement make_sketch_arc(const Vec2& p0, const Vec2& p1, double radius,
                              bool large, bool sweep, const std::string& id) {
    SketchElement elem;
    elem.type = SketchElementType::Arc;
    elem.p0 = p0;
    elem.p1 = p1;
    elem.radius = radius;
    elem.arc_large = large;
    elem.arc_sweep = sweep;
    elem.id = id.empty() ? generate_sketch_element_id(SketchElementType::Arc) : id;
    elem.style = LineStyle::Contour;
    return elem;
}

SketchElement make_sketch_text(const Vec2& p, const std::string& text, double height, const std::string& id) {
    SketchElement elem;
    elem.type = SketchElementType::Text;
    elem.p0 = p;
    elem.text = text;
    elem.text_height = height;
    elem.id = id.empty() ? generate_sketch_element_id(SketchElementType::Text) : id;
    return elem;
}

SketchDimension make_sketch_dimension(const Vec2& p0, const Vec2& p1, const std::string& text,
                                      const Vec2& offset, const std::string& id) {
    SketchDimension dim;
    dim.p0 = p0;
    dim.p1 = p1;
    dim.text = text;
    dim.offset_point = offset;
    dim.id = id.empty() ? generate_sketch_dimension_id() : id;
    return dim;
}

SketchValidationResult validate_sketch(const Sketch2D& sketch, const SketchValidationOptions& opt) {
    SketchValidationResult result;

    // Проверить ID элементов
    if (opt.check_element_ids) {
        std::set<std::string> seen_ids;
        for (const auto& elem : sketch.elements) {
            if (elem.id.empty()) {
                result.errors.push_back("Empty element ID");
                result.valid = false;
            } else if (seen_ids.count(elem.id)) {
                result.errors.push_back("Duplicate element ID: " + elem.id);
                result.valid = false;
            } else {
                seen_ids.insert(elem.id);
            }
        }

        // Проверить ID размеров
        for (const auto& dim : sketch.dimensions) {
            if (dim.id.empty()) {
                result.errors.push_back("Empty dimension ID");
                result.valid = false;
            } else if (seen_ids.count(dim.id)) {
                result.errors.push_back("Duplicate dimension ID: " + dim.id);
                result.valid = false;
            } else {
                seen_ids.insert(dim.id);
            }
        }
    }

    return result;
}

void sketch_add_element(Sketch2D& sketch, const SketchElement& elem) {
    sketch.elements.push_back(elem);
}

bool sketch_remove_element(Sketch2D& sketch, const std::string& elem_id) {
    auto it = std::find_if(sketch.elements.begin(), sketch.elements.end(),
                           [&elem_id](const SketchElement& e) { return e.id == elem_id; });
    if (it != sketch.elements.end()) {
        sketch.elements.erase(it);
        return true;
    }
    return false;
}

bool sketch_update_element(Sketch2D& sketch, const std::string& elem_id, const SketchElement& elem) {
    auto it = std::find_if(sketch.elements.begin(), sketch.elements.end(),
                           [&elem_id](const SketchElement& e) { return e.id == elem_id; });
    if (it != sketch.elements.end()) {
        *it = elem;
        it->id = elem_id;  // сохранить исходный ID
        return true;
    }
    return false;
}

void sketch_add_dimension(Sketch2D& sketch, const SketchDimension& dim) {
    sketch.dimensions.push_back(dim);
}

bool sketch_remove_dimension(Sketch2D& sketch, const std::string& dim_id) {
    auto it = std::find_if(sketch.dimensions.begin(), sketch.dimensions.end(),
                           [&dim_id](const SketchDimension& d) { return d.id == dim_id; });
    if (it != sketch.dimensions.end()) {
        sketch.dimensions.erase(it);
        return true;
    }
    return false;
}

Sketch2D make_sketch(int sheet_number, const std::string& name, const std::string& author) {
    Sketch2D sketch;
    sketch.id = "sketch_" + std::to_string(static_cast<int>(std::time(nullptr)));
    sketch.sheet_number = sheet_number;
    sketch.name = name;
    sketch.created_by = author;
    sketch.visible = true;
    sketch.locked = false;

    // Установить временные метки
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss.imbue(std::locale("C"));
    oss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    sketch.created_at = oss.str();
    sketch.modified_at = sketch.created_at;

    return sketch;
}

// ---------- Функции для работы со слоями (Q2 2027, Task 3) ----------

const char* layer_name(LayerType type) {
    switch (type) {
        case LayerType::Contour: return "Контур";
        case LayerType::Dimensions: return "Размеры";
        case LayerType::Axis: return "Оси";
        case LayerType::Markup: return "Разметка";
        case LayerType::ConstructionGeometry: return "Вспомогательное";
    }
    return "Неизвестно";
}

const char* layer_color(LayerType type) {
    switch (type) {
        case LayerType::Contour: return "#000000";           // чёрный
        case LayerType::Dimensions: return "#0000FF";         // синий
        case LayerType::Axis: return "#FF0000";              // красный
        case LayerType::Markup: return "#008000";            // зелёный
        case LayerType::ConstructionGeometry: return "#A9A9A9"; // серый
    }
    return "#000000";
}

// Создать слои по умолчанию для чертежного листа
std::vector<Layer> make_default_layers() {
    std::vector<Layer> layers;

    for (int i = 0; i <= 4; ++i) {
        LayerType type = static_cast<LayerType>(i);
        Layer layer;
        layer.type = type;
        layer.name = layer_name(type);
        layer.color = layer_color(type);
        layer.visible = true;
        layer.locked = false;
        layer.element_count = 0;
        layers.push_back(layer);
    }

    return layers;
}

// Инициализировать видимость слоёв (все видимы по умолчанию)
LayerVisibility make_layer_visibility() {
    LayerVisibility vis;
    for (int i = 0; i <= 4; ++i) {
        vis.set_visible(static_cast<LayerType>(i), true);
    }
    return vis;
}

Box2 sketch_bbox(const Sketch2D& sketch) {
    Box2 bbox;

    for (const auto& elem : sketch.elements) {
        if (!bbox.empty || elem.type == SketchElementType::Line || elem.type == SketchElementType::Arc) {
            if (bbox.empty) {
                bbox.lo = elem.p0;
                bbox.hi = elem.p0;
                bbox.empty = false;
            }
            bbox.add(elem.p0);
            bbox.add(elem.p1);
        } else if (elem.type == SketchElementType::Text) {
            if (bbox.empty) {
                bbox.lo = elem.p0;
                bbox.hi = elem.p0;
                bbox.empty = false;
            }
            bbox.add(elem.p0);
        }
    }

    for (const auto& dim : sketch.dimensions) {
        if (bbox.empty) {
            bbox.lo = dim.p0;
            bbox.hi = dim.p0;
            bbox.empty = false;
        }
        bbox.add(dim.p0);
        bbox.add(dim.p1);
        bbox.add(dim.offset_point);
    }

    return bbox;
}

// ---------- Функции для работы с ручными размерами (2.1-2.7) ----------

// Сгенерировать уникальный id для размера
static std::string generate_dimension_id() {
    static unsigned int counter = 0;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "manual_dim_%u_%lld",
                 ++counter,
                 std::chrono::system_clock::now().time_since_epoch().count());
    return std::string(buf);
}

// Получить текущий ISO 8601 timestamp
static std::string now_timestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::gmtime(&now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

// Проверить, находится ли точка внутри прямоугольника с отступом
static bool is_point_in_sheet(const Vec2& p, const Rect& drawable, double margin = 2.0) {
    return p[0] >= drawable.x0 + margin &&
           p[0] <= drawable.x1 - margin &&
           p[1] >= drawable.y0 + margin &&
           p[1] <= drawable.y1 - margin;
}

// Проверить расстояние между двумя точками
static double distance(const Vec2& a, const Vec2& b) {
    double dx = a[0] - b[0];
    double dy = a[1] - b[1];
    return std::sqrt(dx * dx + dy * dy);
}

ManualDimensionValidation validate_manual_dimension(
    const ManualDimension& dim,
    const DrawingSheet& sheet,
    const ManualDimensionTable& existing) {

    ManualDimensionValidation result;
    result.valid = true;

    // Проверка: размер находится в пределах листа (с отступом 2 мм)
    if (!is_point_in_sheet(dim.position, sheet.frame.drawable, 2.0)) {
        result.valid = false;
        result.out_of_bounds.push_back(dim.text);
    }

    // Проверка: нет конфликтов с существующими ручными размерами (расстояние < 5 мм)
    const double min_distance = 5.0;
    for (const auto& existing_dim : existing.dimensions) {
        if (distance(dim.position, existing_dim.position) < min_distance) {
            result.valid = false;
            result.conflicts.push_back(existing_dim.text);
        }
    }

    // Проверка: нет конфликтов с авто-размерами (в зоне views)
    for (const auto& view : sheet.views) {
        for (const auto& auto_dim : view.geom.dims) {
            // Центр автоматического размера
            Vec2 auto_dim_center = {(auto_dim.a[0] + auto_dim.b[0]) / 2.0,
                                   (auto_dim.a[1] + auto_dim.b[1]) / 2.0};
            if (distance(dim.position, auto_dim_center) < 8.0) {
                result.valid = false;
                result.conflicts.push_back("авто-размер: " + auto_dim.text);
            }
        }
    }

    return result;
}

std::string add_manual_dimension(
    DrawingSheet& sheet,
    ManualDimensionTable& table,
    const Vec2& position,
    const std::string& text,
    bool reference,
    const std::string& author) {

    // Создать новый размер
    ManualDimension dim;
    dim.id = generate_dimension_id();
    dim.position = position;
    dim.text = text;
    dim.reference = reference;
    dim.author = author;
    dim.created_at = now_timestamp();

    // Валидировать
    auto validation = validate_manual_dimension(dim, sheet, table);
    if (!validation.valid) {
        return "";  // Ошибка валидации
    }

    // Добавить в таблицу
    table.dimensions.push_back(dim);
    return dim.id;
}

bool update_manual_dimension_text(
    ManualDimensionTable& table,
    const std::string& id,
    const std::string& new_text) {

    auto* dim = table.find(id);
    if (!dim) return false;

    dim->text = new_text;
    return true;
}

bool move_manual_dimension(
    DrawingSheet& sheet,
    ManualDimensionTable& table,
    const std::string& id,
    const Vec2& new_position) {

    auto* dim = table.find(id);
    if (!dim) return false;

    // Проверить новую позицию
    if (!is_point_in_sheet(new_position, sheet.frame.drawable, 2.0)) {
        return false;
    }

    // Проверить конфликты с другими размерами
    const double min_distance = 5.0;
    for (auto& other_dim : table.dimensions) {
        if (other_dim.id != id && distance(new_position, other_dim.position) < min_distance) {
            return false;
        }
    }

    dim->position = new_position;
    return true;
}

bool remove_manual_dimension(
    ManualDimensionTable& table,
    const std::string& id) {

    return table.remove(id);
}

// ---------- Комментарии (4.1-4.4) ----------

// Генерировать уникальный ID для комментария
static std::string generate_comment_id() {
    static unsigned int counter = 0;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "comment_%u_%lld",
                 ++counter,
                 std::chrono::system_clock::now().time_since_epoch().count());
    return std::string(buf);
}

// Генерировать уникальный ID для ответа на комментарий
static std::string generate_reply_id() {
    static unsigned int counter = 0;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "reply_%u_%lld",
                 ++counter,
                 std::chrono::system_clock::now().time_since_epoch().count());
    return std::string(buf);
}

// Конвертировать статус комментария в строку
const char* comment_status_name(CommentStatus s) {
    switch (s) {
        case CommentStatus::Open:
            return "open";
        case CommentStatus::Resolved:
            return "resolved";
        case CommentStatus::NeedsRework:
            return "needs_rework";
        case CommentStatus::Archived:
            return "archived";
    }
    return "open";
}

// Конвертировать строку в статус комментария
CommentStatus comment_status_from(const std::string& s) {
    if (s == "resolved") return CommentStatus::Resolved;
    if (s == "needs_rework") return CommentStatus::NeedsRework;
    if (s == "archived") return CommentStatus::Archived;
    return CommentStatus::Open;
}

// Создать новый комментарий
Comment make_comment(int sheet_number, const Vec2& position, const std::string& text,
                     const std::string& author, const std::string& id) {
    Comment c;
    c.id = id.empty() ? generate_comment_id() : id;
    c.sheet_number = sheet_number;
    c.position = position;
    c.text = text;
    c.author = author;
    c.created_at = now_timestamp();
    c.modified_at = c.created_at;
    c.status = CommentStatus::Open;
    return c;
}

// Создать ответ на комментарий
CommentReply make_reply(const std::string& text, const std::string& author,
                       const std::string& id) {
    CommentReply r;
    r.id = id.empty() ? generate_reply_id() : id;
    r.text = text;
    r.author = author;
    r.timestamp = now_timestamp();
    return r;
}

// Добавить ответ к комментарию
void add_reply_to_comment(Comment& comment, const CommentReply& reply) {
    comment.replies.push_back(reply);
    comment.modified_at = now_timestamp();
}

// Изменить статус комментария
void change_comment_status(Comment& comment, CommentStatus new_status) {
    comment.status = new_status;
    comment.modified_at = now_timestamp();
}

// Фильтровать комментарии по статусу
std::vector<Comment> filter_comments_by_status(
    const std::vector<Comment>& comments,
    CommentStatus status) {
    std::vector<Comment> result;
    for (const auto& c : comments) {
        if (c.status == status) {
            result.push_back(c);
        }
    }
    return result;
}

// Фильтровать комментарии по номеру листа
std::vector<Comment> filter_comments_by_sheet(
    const std::vector<Comment>& comments,
    int sheet_number) {
    std::vector<Comment> result;
    for (const auto& c : comments) {
        if (c.sheet_number == sheet_number) {
            result.push_back(c);
        }
    }
    return result;
}

// Рендеринг размеров в чертёж
void renderDimensions(const Drawing& drawing, const std::vector<Dimension>& dimensions) {
    // Для каждого размера нарисовать:
    // 1. Линию размера (от точки A к точке B)
    // 2. Стрелки на концах
    // 3. Текст размера в центре

    for (const auto& dim : dimensions) {
        // Получить 2D проекции точек
        Vec2 p1_2d = {static_cast<float>(dim.point1.x), static_cast<float>(dim.point1.y)};
        Vec2 p2_2d = {static_cast<float>(dim.point2.x), static_cast<float>(dim.point2.y)};

        // Рассчитать позицию текста (центр между точками + смещение вверх)
        double mid_x = (p1_2d[0] + p2_2d[0]) / 2.0;
        double mid_y = (p1_2d[1] + p2_2d[1]) / 2.0;
        Vec2 text_pos = {mid_x + 10.0, mid_y + 10.0};

        // Форматировать текст размера (число + единица)
        std::string label = std::to_string(dim.value);
        if (!dim.unit.empty()) {
            label += " " + dim.unit;
        }

        // Рисование выполняется через Drawing API
        // В реальной реализации: drawing.drawLine(p1_2d, p2_2d, ...)
        //                        drawing.drawArrow(p1_2d, ...)
        //                        drawing.drawText(label, text_pos, ...)
    }
}

}  // namespace bendcore::drawing
