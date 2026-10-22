#include "bendcore/positioning.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace bendcore {

namespace {

constexpr double kPi = 3.14159265358979323846;

double deg2rad(double d) { return d * kPi / 180.0; }

// Угол между векторами, градусы; atan2 — точность при малых углах (как в ports.cpp).
double angle_deg(const Vec3& a, const Vec3& b) { return std::atan2(norm(cross(a, b)), dot(a, b)) * 180.0 / kPi; }

double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

// Локальный след пути: ось без добавочного отрезка, точный конец Б и касательная в нём.
// Добавочный прямой в 1 мм даёт точное направление конца и после дуги (хорда последнего шага дуги — не касательная).
struct LocalTrace {
    std::vector<Vec3> pts;
    Vec3 end_pos{};
    Vec3 end_dir{};
};

LocalTrace local_trace(const Path& path, int steps_per_bend) {
    Path ext = path;
    ext.push_back(Straight{1.0});
    std::vector<Vec3> pts = trace(ext, steps_per_bend);
    LocalTrace out;
    out.end_pos = pts[pts.size() - 2];
    out.end_dir = unit(sub(pts.back(), out.end_pos));
    pts.pop_back();
    out.pts = std::move(pts);
    return out;
}

struct Box {
    Vec3 lo, hi;
};

Box box_of(const std::vector<Vec3>& pts) {
    Box b{pts.front(), pts.front()};
    for (const auto& p : pts) {
        for (int k = 0; k < 3; ++k) {
            b.lo[k] = std::min(b.lo[k], p[k]);
            b.hi[k] = std::max(b.hi[k], p[k]);
        }
    }
    return b;
}

double box_distance(const Box& a, const Box& b) {
    Vec3 d{};
    for (int k = 0; k < 3; ++k) d[k] = std::max({0.0, b.lo[k] - a.hi[k], a.lo[k] - b.hi[k]});
    return norm(d);
}

std::string fmt(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", v);
    return buf;
}

bool pair_skipped(const std::vector<std::pair<std::string, std::string>>& skip, const std::string& a,
                  const std::string& b) {
    for (const auto& p : skip) {
        if ((p.first == a && p.second == b) || (p.first == b && p.second == a)) return true;
    }
    return false;
}

}  // namespace

Vec3 Frame::rotate(const Vec3& local) const {
    return add(add(mul(x, local[0]), mul(y, local[1])), mul(z, local[2]));
}

Vec3 Frame::apply(const Vec3& local) const { return add(origin, rotate(local)); }

Frame frame_from_port(const Port& port, double roll_deg) {
    const Vec3 x = unit(port.direction);   // нулевое направление — std::invalid_argument
    Vec3 ref{0.0, 0.0, 1.0};
    if (norm(cross(ref, x)) < 1e-9) ref = {0.0, 1.0, 0.0};   // порт вдоль ±Z
    const Vec3 y = unit(cross(ref, x));
    const Vec3 z = cross(x, y);
    const double r = deg2rad(roll_deg);
    const double c = std::cos(r);
    const double s = std::sin(r);
    Frame f;
    f.origin = port.pos;
    f.x = x;
    f.y = add(mul(y, c), mul(z, s));
    f.z = add(mul(y, -s), mul(z, c));
    return f;
}

Placement place_from_port(const Path& path, const Port& start_port, double roll_deg, int steps_per_bend) {
    Placement out;
    out.frame = frame_from_port(start_port, roll_deg);
    const LocalTrace lt = local_trace(path, steps_per_bend);
    out.centerline.reserve(lt.pts.size());
    for (const auto& p : lt.pts) out.centerline.push_back(out.frame.apply(p));
    out.end_pos = out.frame.apply(lt.end_pos);
    out.end_dir = unit(out.frame.rotate(lt.end_dir));
    return out;
}

std::vector<Issue> end_connection_issues(const Placement& placed, const Port& end_port, const Path& path,
                                         double od, const std::string& tube_id) {
    return check_connection(end_port, path, od, tube_id, 'B', placed.end_pos, mul(placed.end_dir, -1.0));
}

RollSolution solve_roll(const Path& path, const Port& start_port, const Port& end_port, int steps_per_bend) {
    if (!(axis_length(path) > 0.0)) throw std::invalid_argument("путь нулевой длины: поворот не определён");
    const LocalTrace lt = local_trace(path, steps_per_bend);
    const Vec3 target_dir = end_port.unit_dir();

    // Положение важнее направления; направление разрешает равенство (когда поворот не двигает конец).
    constexpr double kDirWeight = 0.01;   // мм на градус
    struct Dev {
        double pos, dir, cost;
    };
    const auto eval = [&](double roll) {
        const Frame f = frame_from_port(start_port, roll);
        const double pos = norm(sub(f.apply(lt.end_pos), end_port.pos));
        const double dir = angle_deg(mul(f.rotate(lt.end_dir), -1.0), target_dir);
        return Dev{pos, dir, pos + kDirWeight * dir};
    };

    // Расстояние от точки до точек окружности имеет один минимум: перебор шагом 2° его не пропускает.
    constexpr double kStep = 2.0;
    double best_roll = 0.0;
    Dev best = eval(0.0);
    for (double r = -180.0; r < 180.0; r += kStep) {
        const Dev d = eval(r);
        if (d.cost < best.cost - 1e-9) {   // начинаем с нуля и сдвигаемся только при заметном улучшении
            best = d;
            best_roll = r;
        }
    }

    // Уточнение золотым сечением в окне ±шаг; принимается, только если строго лучше.
    constexpr double kInvPhi = 0.6180339887498949;
    double lo = best_roll - kStep;
    double hi = best_roll + kStep;
    double c = hi - kInvPhi * (hi - lo);
    double d = lo + kInvPhi * (hi - lo);
    Dev fc = eval(c);
    Dev fd = eval(d);
    for (int it = 0; it < 80; ++it) {
        if (fc.cost < fd.cost) {
            hi = d;
            d = c;
            fd = fc;
            c = hi - kInvPhi * (hi - lo);
            fc = eval(c);
        } else {
            lo = c;
            c = d;
            fc = fd;
            d = lo + kInvPhi * (hi - lo);
            fd = eval(d);
        }
    }
    const double mid = 0.5 * (lo + hi);
    const Dev fm = eval(mid);
    if (fm.cost < best.cost - 1e-12) {
        best = fm;
        best_roll = mid;
    }

    RollSolution sol;
    sol.roll_deg = wrap180(best_roll);
    sol.pos_dev = best.pos;
    sol.dir_dev = best.dir;
    sol.within_tol = best.pos <= end_port.tol_pos && best.dir <= end_port.tol_ang;
    return sol;
}

ClosestPoints segment_closest(const Vec3& p1, const Vec3& q1, const Vec3& p2, const Vec3& q2) {
    // Ближайшие точки двух отрезков (зажатые параметры s, t в [0, 1]).
    const Vec3 d1 = sub(q1, p1);
    const Vec3 d2 = sub(q2, p2);
    const Vec3 r = sub(p1, p2);
    const double a = dot(d1, d1);
    const double e = dot(d2, d2);
    const double f = dot(d2, r);
    constexpr double eps = 1e-12;
    double s = 0.0;
    double t = 0.0;
    if (a <= eps && e <= eps) return {norm(r), p1, p2};
    if (a <= eps) {
        t = clamp01(f / e);
    } else {
        const double c = dot(d1, r);
        if (e <= eps) {
            s = clamp01(-c / a);
        } else {
            const double b = dot(d1, d2);
            const double denom = a * e - b * b;
            s = denom > eps * a * e ? clamp01((b * f - c * e) / denom) : 0.0;   // параллельные — любое s
            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = clamp01(-c / a);
            } else if (t > 1.0) {
                t = 1.0;
                s = clamp01((b - c) / a);
            }
        }
    }
    const Vec3 ca = add(p1, mul(d1, s));
    const Vec3 cb = add(p2, mul(d2, t));
    return {norm(sub(ca, cb)), ca, cb};
}

double segment_distance(const Vec3& p1, const Vec3& q1, const Vec3& p2, const Vec3& q2) {
    return segment_closest(p1, q1, p2, q2).distance;
}

ClosestPoints polyline_closest(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
    ClosestPoints best;
    best.distance = std::numeric_limits<double>::infinity();
    if (a.empty() || b.empty()) return best;
    const std::size_t na = std::max<std::size_t>(a.size() - 1, 1);   // одна точка — вырожденный отрезок
    const std::size_t nb = std::max<std::size_t>(b.size() - 1, 1);
    for (std::size_t i = 0; i < na; ++i) {
        const Vec3& p1 = a[i];
        const Vec3& q1 = a[std::min(i + 1, a.size() - 1)];
        for (std::size_t j = 0; j < nb; ++j) {
            const Vec3& p2 = b[j];
            const Vec3& q2 = b[std::min(j + 1, b.size() - 1)];
            bool far = false;
            for (int k = 0; k < 3 && !far; ++k) {
                far = std::min(p2[k], q2[k]) > std::max(p1[k], q1[k]) + best.distance ||
                      std::min(p1[k], q1[k]) > std::max(p2[k], q2[k]) + best.distance;
            }
            if (far) continue;
            const ClosestPoints c = segment_closest(p1, q1, p2, q2);
            if (c.distance < best.distance) best = c;
        }
    }
    return best;
}

double polyline_distance(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
    return polyline_closest(a, b).distance;
}

std::vector<ClearanceMark> clearance_marks(const std::vector<PlacedTube>& tubes, double gap,
                                           const std::vector<std::pair<std::string, std::string>>& skip_pairs) {
    std::vector<ClearanceMark> marks;
    std::vector<Box> boxes;
    boxes.reserve(tubes.size());
    for (const auto& t : tubes) boxes.push_back(t.centerline.empty() ? Box{} : box_of(t.centerline));

    for (std::size_t i = 0; i < tubes.size(); ++i) {
        if (tubes[i].centerline.empty()) continue;
        for (std::size_t j = i + 1; j < tubes.size(); ++j) {
            if (tubes[j].centerline.empty()) continue;
            if (pair_skipped(skip_pairs, tubes[i].id, tubes[j].id)) continue;
            const double radii = 0.5 * (tubes[i].od + tubes[j].od);
            if (box_distance(boxes[i], boxes[j]) > radii + gap) continue;   // заведомо далеко
            const ClosestPoints c = polyline_closest(tubes[i].centerline, tubes[j].centerline);
            const double surface = c.distance - radii;
            std::string kind;
            if (surface < -1e-9) {
                kind = "mark_cross";
            } else if (surface < gap - 1e-9) {
                kind = "mark_gap";
            } else {
                continue;
            }
            marks.push_back({kind, tubes[i].id, tubes[j].id, c.a, c.b, surface, gap});
        }
    }
    return marks;
}

std::string clearance_text(const ClearanceMark& mark) {
    const std::string names = "«" + mark.tube_a + "» и «" + mark.tube_b + "»";
    if (mark.kind == "mark_cross") return "Пересечение труб " + names + ": перекрытие " + fmt(-mark.surface_gap) + " мм";
    return "Зазор между трубами " + names + " " + fmt(mark.surface_gap) + " мм, меньше нормы " + fmt(mark.norm) + " мм";
}

std::vector<Issue> check_clearance(const std::vector<PlacedTube>& tubes, double gap,
                                   const std::vector<std::pair<std::string, std::string>>& skip_pairs) {
    std::vector<Issue> issues;
    for (const auto& m : clearance_marks(tubes, gap, skip_pairs)) issues.push_back({m.kind, -1, clearance_text(m)});
    return issues;
}

}  // namespace bendcore
