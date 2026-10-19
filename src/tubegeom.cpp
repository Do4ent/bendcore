#include "bendcore/tubegeom.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <tuple>
#include <stdexcept>

#include "bendcore/spiral.hpp"

namespace bendcore {
namespace {

double deg2rad(double d) { return d * std::numbers::pi / 180.0; }

// Поворот вектора v вокруг единичной оси axis (формула Родрига), как _rot в Python.
Vec3 rot(const Vec3& v, const Vec3& axis, double angle_rad) {
    const double c = std::cos(angle_rad);
    const double s = std::sin(angle_rad);
    return add(add(mul(v, c), mul(cross(axis, v), s)), mul(axis, dot(axis, v) * (1.0 - c)));
}

}  // namespace

double tangent_length(double R, double B_deg) {
    return R * std::tan(deg2rad(B_deg) / 2.0);
}

double axis_length(const Path& path) {
    double total = 0.0;
    for (const auto& it : path) {
        if (const auto* s = std::get_if<Straight>(&it)) {
            total += s->S;
        } else if (const auto* b = std::get_if<Bend>(&it)) {
            if (std::abs(b->B) >= kDegenerateBDeg) total += b->R * deg2rad(std::abs(b->B));
        } else if (const auto* sp = std::get_if<Spiral>(&it)) {
            total += spiral_axis_length(sp->R, sp->p, sp->n);
        }
    }
    return total;
}

Bend normalize_rotation(const Bend& bend, Profile direction) {
    if (direction != Profile::Both) return bend;
    if (bend.A > 90.0) return Bend{bend.R, -bend.B, bend.A - 180.0};
    if (bend.A < -90.0) return Bend{bend.R, -bend.B, bend.A + 180.0};
    return bend;
}

double wrap180(double a) {
    a = std::fmod(a, 360.0);
    if (a <= -180.0) {
        a += 360.0;
    } else if (a > 180.0) {
        a -= 360.0;
    }
    return a;
}

TableRow table_row(const Bend& bend, Profile direction, bool minimize) {
    if (direction == Profile::Both) {
        const Bend b = minimize ? normalize_rotation(bend, Profile::Both) : bend;
        const Side side = b.B >= 0 ? Side::Right : Side::Left;  // «П» / «Л»
        return {wrap180(b.A), std::abs(b.B), side};
    }
    const double A = bend.A + (bend.B < 0 ? 180.0 : 0.0);
    return {wrap180(A), std::abs(bend.B), Side::None};
}

std::vector<Vec3> trace(const Path& path, int steps_per_bend, Vec3 start) {
    Vec3 pos = start;
    Vec3 heading{1.0, 0.0, 0.0};
    Vec3 up{0.0, 0.0, 1.0};
    std::vector<Vec3> pts{pos};

    for (const auto& it : path) {
        if (const auto* s = std::get_if<Straight>(&it)) {
            if (s->S < 0) throw std::invalid_argument("S < 0: геометрическая ошибка");
            if (s->S > 0) {
                pos = add(pos, mul(heading, s->S));
                pts.push_back(pos);
            }
        } else if (const auto* b = std::get_if<Bend>(&it)) {
            if (std::abs(b->B) < kDegenerateBDeg) continue;  // вырожденный гиб — прямой участок
            // поворот A вокруг касательной перед гибом
            up = rot(up, heading, deg2rad(b->A));
            const double sgn = b->B >= 0 ? 1.0 : -1.0;
            const Vec3 side = mul(cross(up, heading), sgn);
            const Vec3 centre = add(pos, mul(side, b->R));
            const double total = deg2rad(std::abs(b->B));
            const int n = std::max(2, static_cast<int>(steps_per_bend * std::abs(b->B) / 90.0) + 1);
            for (int k = 1; k <= n; ++k) {
                const double t = total * k / n;
                const Vec3 q = rot(sub(pos, centre), up, sgn * t);
                pts.push_back(add(centre, q));
            }
            pos = pts.back();
            heading = rot(heading, up, sgn * total);
        } else if (const auto* sp = std::get_if<Spiral>(&it)) {
            // 1.2.1: ось — проекция «верха» на плоскость ⊥ касательной; при вырождении — ошибка
            const Vec3 up_perp = sub(up, mul(heading, dot(up, heading)));
            if (norm(up_perp) < 1e-9) throw std::invalid_argument("не задан верх трубы для оси спирали");
            const Vec3 axis = spiral_axis(heading, sp->R, sp->p, sp->direction, unit(up_perp));
            const SpiralCentreline sc = spiral_centreline(pos, heading, sp->R, sp->p, sp->n, sp->direction,
                                                          axis, steps_per_bend);
            for (std::size_t k = 1; k < sc.pts.size(); ++k) pts.push_back(sc.pts[k]);
            pos = sc.pts.back();
            heading = unit(sc.tend);
            // следующий гиб: A отсчитывается от плоскости «касательная выхода + ось»; знак — по прежнему «верху»
            Vec3 n_plane = cross(heading, axis);
            if (norm(n_plane) < 1e-9) n_plane = sub(up_perp, mul(heading, dot(up_perp, heading)));
            up = unit(n_plane);
            if (dot(up, up_perp) < 0) up = mul(up, -1.0);
        }
    }
    return pts;
}

// ---------- Полилиния и ресэмплинг ----------
namespace {

double polyline_length(const std::vector<Vec3>& pts) {
    double s = 0.0;
    for (std::size_t i = 1; i < pts.size(); ++i) s += norm(sub(pts[i], pts[i - 1]));
    return s;
}

// Точки осевой с шагом не больше step и длина по осевой до каждой точки.
void resample(const std::vector<Vec3>& pts, double step, std::vector<Vec3>& P, std::vector<double>& s) {
    P.clear();
    P.push_back(pts.front());
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const Vec3& a = pts[i];
        const Vec3& b = pts[i + 1];
        const double d = norm(sub(b, a));
        const int k = std::max(1, static_cast<int>(std::ceil(d / step)));
        for (int j = 1; j <= k; ++j) P.push_back(add(a, mul(sub(b, a), static_cast<double>(j) / k)));
    }
    s.assign(P.size(), 0.0);
    for (std::size_t i = 1; i < P.size(); ++i) s[i] = s[i - 1] + norm(sub(P[i], P[i - 1]));
}

// Конец и единичное направление; как _end_pose в Python (8 шагов на гиб).
std::pair<Vec3, Vec3> end_pose(const Path& path) {
    const auto pts = trace(path, 8);
    const Vec3 d = sub(pts[pts.size() - 1], pts[pts.size() - 2]);
    return {pts.back(), unit(d)};
}

}  // namespace

std::vector<Issue> validate(const Path& path, double B_max, double B_min, std::optional<double> od,
                            std::optional<double> spiral_min_pitch, bool spiral_allowed) {
    std::vector<Issue> issues;
    std::string prev_kind;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const int idx = static_cast<int>(i);
        if (const auto* s = std::get_if<Straight>(&path[i])) {
            if (s->S < 0) issues.push_back({"geometric", idx, "S < 0"});
            if (prev_kind == "straight") issues.push_back({"suggestion", idx, "коллинеарные участки — слить"});
            prev_kind = "straight";
        } else if (const auto* b = std::get_if<Bend>(&path[i])) {
            if (b->R <= 0) issues.push_back({"geometric", idx, "R ≤ 0"});
            // A определён только при ненулевом прямом участке перед гибом; первый гиб A не учитывает (решение 18.10.2026)
            bool earlier_bend = false;
            for (std::size_t j = 0; j < i; ++j) earlier_bend = earlier_bend || std::holds_alternative<Bend>(path[j]);
            if (earlier_bend && std::abs(b->A) > 1e-9) {
                double gap = 0.0;
                if (i > 0) {
                    if (const auto* ps = std::get_if<Straight>(&path[i - 1])) gap = ps->S;
                }
                if (gap <= 0) {
                    issues.push_back({"geometric", idx, "ввод A запрещён: нулевой участок между гибами"});
                }
            }
            const double bb = std::abs(b->B);
            if (bb < kDegenerateBDeg) {
                issues.push_back({"warning", idx, "вырожденный гиб: гиба нет"});
                issues.push_back({"suggestion", idx, "вырожденный гиб между прямыми — слить участки"});
                continue;  // для слияния гиб считается прямой
            }
            if (bb > B_max) {
                issues.push_back({"technological", idx, "B > B_max"});
            } else if (bb < B_min) {
                issues.push_back({"technological", idx, "B вне диапазона профиля"});
            }
            prev_kind = "bend";
        } else if (const auto* sp = std::get_if<Spiral>(&path[i])) {
            if (!spiral_allowed) issues.push_back({"technological", idx, "навивка на строке профиля без разрешения"});
            if (spiral_min_pitch && sp->p < *spiral_min_pitch) {
                issues.push_back({"technological", idx, "шаг спирали меньше мин. шага профиля"});
            }
            if (od && sp->p < *od) issues.push_back({"technological", idx, "шаг меньше наружного ∅: самопересечение витков"});
            prev_kind = "spiral";
        }
    }
    const bool any_geometric = std::any_of(issues.begin(), issues.end(), [](const Issue& x) { return x.kind == "geometric"; });
    if (od && !any_geometric) {
        const auto hits = self_intersections(path, *od);
        if (!hits.empty()) {
            issues.push_back({"technological", -1, "самопересечение осевой"});
        }
    }
    return issues;
}

std::vector<std::tuple<double, double, double>> self_intersections(const Path& path, double od, int max_points) {
    const auto pts = trace(path);
    const double total = polyline_length(pts);
    const double step = std::max(od / 4.0, total / max_points);
    std::vector<Vec3> P;
    std::vector<double> s;
    resample(pts, step, P, s);
    const double gap = std::numbers::pi * od / 2.0;
    std::vector<std::tuple<double, double, double>> hits;
    for (std::size_t i = 0; i < P.size(); ++i) {
        double best = 0.0;
        double best_s = 0.0;
        bool found = false;
        for (std::size_t j = 0; j < P.size(); ++j) {
            if (!(s[j] - s[i] > gap)) continue;
            const double d = norm(sub(P[j], P[i]));
            if (!found || d < best) {
                best = d;
                best_s = s[j];
                found = true;
            }
        }
        if (found && best < od - 1e-9) hits.emplace_back(s[i], best_s, best);
    }
    return hits;
}

double manual_angle_tolerance(double B, const std::vector<double>& ticks, const std::vector<double>& stops,
                              double on_tick, double on_stop, double between, double eps) {
    const double b = std::abs(B);
    for (double s : stops) {
        if (std::abs(b - s) < eps) return on_stop;
    }
    for (double t : ticks) {
        if (std::abs(b - t) < eps) return on_tick;
    }
    return between;
}

Drift end_drift(const Path& path, const std::vector<double>& tol_B, double tol_A, double tol_S,
                double tol_pos, double tol_dir_deg) {
    const auto [p0, d0] = end_pose(path);
    struct Contrib { int bend_no; std::string src; double dp; double dd; };
    std::vector<Contrib> contrib;
    int bend_no = 0;
    const double h = 1e-4;
    for (std::size_t idx = 0; idx < path.size(); ++idx) {
        if (const auto* b = std::get_if<Bend>(&path[idx]); b && std::abs(b->B) >= kDegenerateBDeg) {
            ++bend_no;
            std::vector<std::pair<std::string, double>> sources{{"B", tol_B[bend_no - 1]}};
            if (bend_no > 1) sources.emplace_back("A", tol_A);
            for (const auto& [src, tol] : sources) {
                Path q = path;
                q[idx] = Bend{b->R, b->B + (src == "B" ? h : 0.0), b->A + (src == "A" ? h : 0.0)};
                const auto [p1, d1] = end_pose(q);
                const double dp = norm(sub(p1, p0)) / h * tol;
                // угол через atan2 (как в tubegeom.py): acos при малых углах теряет точность
                const double dd = std::atan2(norm(cross(d0, d1)), dot(d0, d1)) * 180.0 / std::numbers::pi / h * tol;
                contrib.push_back({bend_no, src, dp, dd});
            }
        } else if (const auto* s = std::get_if<Straight>(&path[idx])) {
            Path q = path;
            q[idx] = Straight{s->S + h};
            const auto [p1, d1] = end_pose(q);
            (void)d1;
            contrib.push_back({bend_no + 1, "S", norm(sub(p1, p0)) / h * tol_S, 0.0});
        }
    }
    double worst_p = 0.0, worst_d = 0.0, sq_p = 0.0, sq_d = 0.0;
    for (const auto& c : contrib) {
        worst_p += c.dp;
        worst_d += c.dd;
        sq_p += c.dp * c.dp;
        sq_d += c.dd * c.dd;
    }
    double var = 0.0;
    for (const auto& c : contrib) var += (c.dp / tol_pos) * (c.dp / tol_pos) + (c.dd / tol_dir_deg) * (c.dd / tol_dir_deg);
    if (var == 0.0) var = 1.0;  // как «or 1.0» в Python
    Drift out{};
    out.worst_pos = worst_p;
    out.worst_dir = worst_d;
    out.probable_pos = std::sqrt(sq_p);
    out.probable_dir = std::sqrt(sq_d);
    for (const auto& c : contrib) {
        const double share = ((c.dp / tol_pos) * (c.dp / tol_pos) + (c.dd / tol_dir_deg) * (c.dd / tol_dir_deg)) / var;
        out.shares.emplace_back(c.bend_no, c.src, share);
    }
    std::stable_sort(out.shares.begin(), out.shares.end(),
                     [](const auto& x, const auto& y) { return std::get<2>(x) > std::get<2>(y); });
    out.warning = out.probable_pos > tol_pos || out.probable_dir > tol_dir_deg;
    return out;
}

Path path_from_table(const std::vector<std::array<double, 4>>& rows) {
    Path path;
    for (const auto& r : rows) {
        const double S = r[0], R = r[1], B = r[2], A = r[3];
        path.push_back(Straight{S});
        if (B != 0.0) path.push_back(Bend{R, B, A});
    }
    return path;
}

}  // namespace bendcore
