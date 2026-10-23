#include "bendcore/drawing.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>

#include "bendcore/compensation.hpp"

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

SheetFrame make_sheet(Standard s, const Format& f) {
    SheetFrame sf;
    sf.format = f;
    double left = 20, other = 5, tbw = 185, tbh = 55;  // ГОСТ 2.104, форма 1
    if (s == Standard::ISO || s == Standard::DIN) {
        left = 20; other = 10; tbw = 180; tbh = 40;     // ISO 5457, ISO 7200
    } else if (s == Standard::ANSI) {
        left = 10; other = 10; tbw = 178; tbh = 45;     // ASME Y14.1
    }
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
        Text2 t{{x / 2.0, r}, "подрезать", 2.5};
        t.shift = {0, 3};
        g.texts.push_back(t);
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
            t.shift = {0, -9};
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
        Text2 t{{x + res.delta_end / 2.0, r}, "подрезать", 2.5};
        t.shift = {0, 3};
        g.texts.push_back(t);
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
    auto below = [&](double a, double b, double offset, double value, bool ref) {
        Dim2 d{{b, -r}, {a, -r}, offset, 0.0, F(value), ref};
        g.dims.push_back(d);
    };
    double boff = 14.0;
    if (res.delta_start > 1e-9) below(0, res.delta_start, boff, res.delta_start, false);
    if (res.delta_end > 1e-9) below(trim1, total, boff, res.delta_end, false);
    if (E0 > 1e-9) below(trim0, trim0 + E0, boff, E0, false);
    if (E1 > 1e-9) below(trim1 - E1, trim1, boff, E1, false);
    boff += 7.0;
    if (res.delta_start > 1e-9 || res.delta_end > 1e-9) {
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

struct Cell {
    Rect r;
};

Vec2 center(const Rect& r) { return {(r.x0 + r.x1) / 2.0, (r.y0 + r.y1) / 2.0}; }

double table_height(const Table& t) {
    return 8.0 /*заголовок*/ + 7.0 /*шапка*/ + (t.note.empty() ? 0.0 : 6.0) + 6.0 * static_cast<double>(t.rows.size());
}

}  // namespace

DrawingSheet make_part_drawing(const DrawingInput& in) {
    DrawingSheet ds;
    ds.standard = in.standard;
    ds.projection = projection_method(in.standard);
    ds.frame = make_sheet(in.standard, format_by_name(in.format, in.landscape));
    ds.not_for_production = in.has_errors;
    const char dec = decimal_for(in.standard);
    const Rect& area = ds.frame.drawable;

    // Развёртка: полоса внизу поля, высота — под стопку размеров.
    UnfoldOptions uo = in.unfold;
    uo.decimal = dec;
    uo.precision = in.precision;
    UnfoldResult unf = unfold_view(in.path, in.od, uo);
    int dims_above = 0;
    for (const auto& d : unf.geom.dims) dims_above += d.a[1] > 0;
    const double dims_h_above = 8.0 + 7.0 * dims_above + 12.0;  // плюс подпись
    const double dims_h_below = 14.0 + 7.0 * 3 + 12.0;
    const double strip_h = std::clamp(dims_h_above + dims_h_below + 10.0, 50.0, area.h() * 0.5);
    const Rect strip{area.x0, area.y0, area.x1, area.y0 + strip_h};
    const Rect upper{area.x0, strip.y1 + 5, area.x1, area.y1};

    // Ортогональные виды и изометрия.
    ViewOptions vo;
    vo.dims = in.dims;
    vo.E_A = in.unfold.end_A.E;
    vo.E_B = in.unfold.end_B.E;
    vo.precision = in.precision;
    vo.decimal = dec;
    const bool first = ds.projection == Projection::FirstAngle;
    const ViewKind side = first ? ViewKind::Left : ViewKind::Right;

    struct Planned {
        ViewKind kind;
        Rect cell;
        Geometry g;
    };
    std::vector<Planned> plan;
    if (in.preset == Preset::Minimal) {
        const double xm = upper.x0 + upper.w() * 0.6;
        plan.push_back({ViewKind::Front, {upper.x0, upper.y0, xm, upper.y1}, {}});
        plan.push_back({ViewKind::Iso, {xm, upper.y0, upper.x1, upper.y1}, {}});
    } else {
        const double xm = upper.x0 + upper.w() * 0.55;
        const double ym = upper.y0 + upper.h() * 0.5;
        const Rect tl{upper.x0, ym, xm, upper.y1}, tr{xm, ym, upper.x1, upper.y1};
        const Rect bl{upper.x0, upper.y0, xm, ym}, brc{xm, upper.y0, upper.x1, ym};
        if (first) {  // вид сверху под главным, вид слева справа от главного
            plan.push_back({ViewKind::Front, tl, {}});
            plan.push_back({side, tr, {}});
            plan.push_back({ViewKind::Top, bl, {}});
            plan.push_back({ViewKind::Iso, brc, {}});
        } else {      // третий угол: вид сверху над главным
            plan.push_back({ViewKind::Top, tl, {}});
            plan.push_back({ViewKind::Iso, tr, {}});
            plan.push_back({ViewKind::Front, bl, {}});
            plan.push_back({side, brc, {}});
        }
    }
    const double pad = 30.0;  // место под размеры и подписи
    double ortho_scale = std::numeric_limits<double>::max();
    bool fits = true;
    for (auto& p : plan) {
        ViewOptions o = vo;
        if (p.kind == ViewKind::Iso) o.dims = DimMode::TableOnly;
        p.g = tube_view(in.path, in.od, p.kind, o);
        if (p.kind == ViewKind::Iso) continue;
        const Box2 b = p.g.bbox();
        const auto s = pick_scale(b.w(), b.h(), p.cell.w() - pad, p.cell.h() - pad, in.standard);
        if (!s) {
            fits = false;
            ortho_scale = std::min(ortho_scale, scale_series(in.standard).back());
        } else {
            ortho_scale = std::min(ortho_scale, *s);
        }
    }
    if (!fits) ds.warnings.push_back("виды не помещаются на формат даже в самом мелком масштабе: выберите больший формат");

    // Главный вид задаёт выравнивание: у вида сверху общий X, у бокового — общий Y.
    const Planned* front = nullptr;
    for (const auto& p : plan)
        if (p.kind == ViewKind::Front) front = &p;
    const Box2 fb = front->g.bbox();
    const Vec2 fc = center(front->cell);
    const Vec2 front_origin{fc[0] - ortho_scale * (fb.lo[0] + fb.hi[0]) / 2.0,
                            fc[1] - ortho_scale * (fb.lo[1] + fb.hi[1]) / 2.0};
    for (auto& p : plan) {
        PlacedView pv;
        pv.kind = p.kind;
        pv.id = view_name(p.kind);
        pv.geom = std::move(p.g);
        const Box2 b = pv.geom.bbox();
        const Vec2 c = center(p.cell);
        if (p.kind == ViewKind::Iso) {
            const auto s = pick_scale(b.w(), b.h(), p.cell.w() - 10, p.cell.h() - 10, in.standard);
            pv.scale = s ? std::min(*s, ortho_scale) : scale_series(in.standard).back();
            pv.origin = {c[0] - pv.scale * (b.lo[0] + b.hi[0]) / 2.0, c[1] - pv.scale * (b.lo[1] + b.hi[1]) / 2.0};
        } else {
            pv.scale = ortho_scale;
            pv.origin = {c[0] - pv.scale * (b.lo[0] + b.hi[0]) / 2.0, c[1] - pv.scale * (b.lo[1] + b.hi[1]) / 2.0};
            if (p.kind == ViewKind::Front) pv.origin = front_origin;
            if (p.kind == ViewKind::Top) pv.origin[0] = front_origin[0];
            if (p.kind == ViewKind::Left || p.kind == ViewKind::Right) pv.origin[1] = front_origin[1];
        }
        pv.label = pv.kind == ViewKind::Front ? "" : std::string(view_name(pv.kind));
        if (std::abs(pv.scale - ortho_scale) > 1e-12) pv.label += " (" + scale_label(pv.scale, in.standard) + ")";
        ds.views.push_back(std::move(pv));
    }

    // Развёртка.
    {
        PlacedView pv;
        pv.kind = ViewKind::Unfold;
        pv.id = "Развёртка";
        const Box2 b = unf.geom.bbox();
        const double avail_h = strip.h() - dims_h_above - dims_h_below;
        auto s = pick_scale(b.w(), b.h(), strip.w() - 20, std::max(2.0, avail_h), in.standard);
        if (!s) {
            s = scale_series(in.standard).back();
            ds.warnings.push_back("развёртка не помещается: нужен разрыв вида или больший формат");
        }
        pv.scale = *s;
        pv.origin = {strip.x0 + (strip.w() - pv.scale * b.w()) / 2.0 - pv.scale * b.lo[0],
                     strip.y0 + dims_h_below + (avail_h > 0 ? avail_h / 2.0 : 0.0)};
        pv.label = "Развёртка (" + scale_label(pv.scale, in.standard) + ")";
        pv.geom = std::move(unf.geom);
        ds.views.push_back(std::move(pv));
    }

    // Таблицы — колонка над штампом.
    const Rect& tb = ds.frame.title_block;
    std::vector<Table> tables;
    tables.push_back(bend_table(in.path, in.unfold.end_A.E, in.unfold.end_B.E, in.unfold.direction,
                                in.unfold.minimize, in.precision, dec));
    tables.push_back(blank_map(in.path, uo));
    if (in.preset == Preset::Full) {
        tables.push_back(vertex_table(in.path, in.unfold.end_A.E, in.unfold.end_B.E, {}, in.precision, dec));
    }
    double y = ds.frame.border.y1 - 5;
    for (auto& t : tables) {
        const double h = table_height(t);
        if (y - h < tb.y1 + 5) ds.warnings.push_back("таблица «" + t.title + "» не помещается в колонку");
        ds.tables.push_back({t, {tb.x0, y}, tb.w()});
        y -= h + 5;
    }

    // Штамп.
    ds.title = in.title;
    const auto& fview = ds.views.front();
    double main_scale = fview.scale;
    for (const auto& v : ds.views)
        if (v.kind == ViewKind::Front) main_scale = v.scale;
    ds.title["Масштаб"] = scale_label(main_scale, in.standard);
    ds.title["Лист"] = "1";
    ds.title["Листов"] = "1";
    if (!is_standard_scale(main_scale, in.standard)) ds.warnings.push_back("нестандартный масштаб");
    for (const auto& is : unf.issues) ds.warnings.push_back("развёртка: " + is.text);
    return ds;
}

}  // namespace bendcore::drawing
