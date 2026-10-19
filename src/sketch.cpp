#include "bendcore/sketch.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <stdexcept>

#include <Eigen/Dense>

namespace bendcore::sketch {
namespace {

// Как _unit в sketch.py: нулевой вектор возвращается без изменений.
Vec3 unit_or_self(const Vec3& v) {
    const double n = norm(v);
    if (n > 1e-12) return {v[0] / n, v[1] / n, v[2] / n};
    return v;
}

double radians_of(double deg) { return deg * std::numbers::pi / 180.0; }
double degrees_of(double rad) { return rad * 180.0 / std::numbers::pi; }

// Угол гиба в вершине b, градусы (0 — прямо). atan2 вместо acos: точность у малых углов (как в tubegeom).
double bend_angle(const std::vector<Vec3>& P, int a, int b, int c) {
    const Vec3 u = unit_or_self(sub(P[b], P[a]));
    const Vec3 v = unit_or_self(sub(P[c], P[b]));
    return degrees_of(std::atan2(norm(cross(u, v)), dot(u, v)));
}

double tangent(const std::vector<Vec3>& P, int prev, int i, int nxt, double R) {
    if (prev < 0 || nxt < 0) return 0.0;
    return R * std::tan(radians_of(bend_angle(P, prev, i, nxt)) / 2.0);
}

std::vector<Constraint> active(const std::vector<Constraint>& cons) {
    std::vector<Constraint> out;
    for (const auto& c : cons) {
        if (!c.reference) out.push_back(c);
    }
    return out;
}

std::vector<double> rows(const std::vector<Vec3>& P, const Constraint& c) {
    const auto& k = c.kind;
    if (k == "fix" || k == "coincide") {
        const Vec3 d = sub(P[c.idx.at(0)], *c.target);
        return {d[0], d[1], d[2]};
    }
    if (is_dimension(k)) return {measure(P, c) - c.value};
    if (k == "parallel" || k == "along_axis") {
        const int a = c.idx.at(0), b = c.idx.at(1);
        return {norm(cross(sub(P[b], P[a]), unit_or_self(*c.normal)))};
    }
    if (k == "perp") {
        const int a = c.idx.at(0), b = c.idx.at(1);
        return {dot(sub(P[b], P[a]), unit_or_self(*c.normal))};
    }
    if (k == "equal_len") {
        const int a = c.idx.at(0), b = c.idx.at(1), cc = c.idx.at(2), d = c.idx.at(3);
        return {norm(sub(P[a], P[b])) - norm(sub(P[cc], P[d]))};
    }
    if (k == "coplanar") {
        const int a = c.idx.at(0), b = c.idx.at(1), cc = c.idx.at(2);
        const Vec3 n = unit_or_self(cross(sub(P[b], P[a]), sub(P[cc], P[a])));
        std::vector<double> out;
        for (std::size_t j = 3; j < c.idx.size(); ++j) {
            out.push_back(dot(sub(P[c.idx[j]], P[a]), n));
        }
        return out;
    }
    if (k == "symmetry") {
        const int a = c.idx.at(0), b = c.idx.at(1);
        const Vec3 n = unit_or_self(*c.normal);
        const Vec3 mid = mul(add(P[a], P[b]), 0.5);
        return {dot(sub(mid, *c.origin), n), norm(cross(sub(P[b], P[a]), n))};
    }
    throw std::invalid_argument("неизвестное ограничение: " + k);
}

std::vector<double> residuals(const std::vector<double>& x, std::size_t n, const std::vector<Constraint>& cons,
                              bool weighted, const std::vector<double>* x_shape) {
    std::vector<Vec3> P(n);
    for (std::size_t i = 0; i < n; ++i) P[i] = {x[3 * i], x[3 * i + 1], x[3 * i + 2]};
    std::vector<double> r;
    for (const auto& c : cons) {
        const double w = weighted ? weight_of(c.source) : 1.0;
        for (double v : rows(P, c)) r.push_back(w * v);
    }
    if (x_shape) {
        for (std::size_t k = 0; k < x.size(); ++k) r.push_back(kShapeWeight * (x[k] - (*x_shape)[k]));
    }
    return r;
}

// Якобиан по центральным разностям (ошибка ~h², в отличие от прямых разностей).
template <class F>
Eigen::MatrixXd numeric_jacobian(F&& f, const Eigen::VectorXd& x) {
    const Eigen::Index rows_n = f(x).size();
    Eigen::MatrixXd J(rows_n, x.size());
    for (Eigen::Index k = 0; k < x.size(); ++k) {
        const double h = 1e-6 * std::max(1.0, std::abs(x[k]));
        Eigen::VectorXd xp = x, xm = x;
        xp[k] += h;
        xm[k] -= h;
        J.col(k) = (f(xp) - f(xm)) / (2.0 * h);
    }
    return J;
}

std::vector<double> to_std(const Eigen::VectorXd& v) {
    return std::vector<double>(v.data(), v.data() + v.size());
}

// Левенберг–Марквардт: (JᵀJ + λI)δ = −Jᵀr. Масштабирование по диагонали (Marquardt) здесь не годится:
// на малых диагоналях шаг уходит в слабое направление. Для недоопределённых систем демпфирование даёт
// поправку малой нормы.
std::vector<double> levenberg_marquardt(const std::vector<double>& x0, std::size_t n,
                                        const std::vector<Constraint>& act, bool preserve,
                                        const std::vector<double>* x_shape) {
    auto F = [&](const Eigen::VectorXd& xv) {
        const auto r = residuals(to_std(xv), n, act, preserve, x_shape);
        return Eigen::VectorXd(Eigen::Map<const Eigen::VectorXd>(r.data(), static_cast<Eigen::Index>(r.size())));
    };
    Eigen::VectorXd x = Eigen::Map<const Eigen::VectorXd>(x0.data(), static_cast<Eigen::Index>(x0.size()));
    Eigen::VectorXd r = F(x);
    double cost = r.squaredNorm();
    double lambda = 1e-3;
    for (int it = 0; it < 300 && cost > 1e-30; ++it) {
        const Eigen::MatrixXd J = numeric_jacobian(F, x);
        const Eigen::VectorXd g = J.transpose() * r;
        const Eigen::MatrixXd H = J.transpose() * J;
        bool accepted = false;
        for (int k = 0; k < 60 && !accepted; ++k) {
            Eigen::MatrixXd A = H;
            A.diagonal().array() += lambda;
            const Eigen::VectorXd delta = -A.ldlt().solve(g);
            const Eigen::VectorXd xn = x + delta;
            const Eigen::VectorXd rn = F(xn);
            const double cn = rn.squaredNorm();
            if (std::isfinite(cn) && cn < cost) {
                const bool small = delta.norm() <= 1e-14 * (1.0 + x.norm());
                x = xn;
                r = rn;
                cost = cn;
                lambda = std::max(lambda / 3.0, 1e-12);
                accepted = true;
                if (small) return to_std(x);
            } else {
                lambda *= 4.0;
            }
        }
        if (!accepted) break;
    }
    return to_std(x);
}

int numeric_rank(const Eigen::MatrixXd& J, double tol = 1e-8) {
    if (J.size() == 0) return 0;
    Eigen::JacobiSVD<Eigen::MatrixXd> svd(J);
    int rank = 0;
    for (Eigen::Index i = 0; i < svd.singularValues().size(); ++i) {
        if (svd.singularValues()[i] > tol) ++rank;
    }
    return rank;
}

}  // namespace

double weight_of(const std::string& source) {
    if (source == "user_dim") return 100.0;
    if (source == "explicit") return 10.0;
    if (source == "auto") return 1.0;
    return 10.0;
}

bool is_dimension(const std::string& kind) {
    return kind == "dist" || kind == "straight_S" || kind == "to_tangent" || kind == "bend_angle" ||
           kind == "plane_dist" || kind == "line_dist";
}

double measure(const std::vector<Vec3>& P, const Constraint& c) {
    const auto& k = c.kind;
    if (k == "dist") {
        return norm(sub(P[c.idx.at(0)], P[c.idx.at(1)]));
    }
    if (k == "straight_S") {                               // idx: (пред., i, j, след.) — −1 у концов пути
        const int p = c.idx.at(0), i = c.idx.at(1), j = c.idx.at(2), q = c.idx.at(3);
        return norm(sub(P[j], P[i])) - tangent(P, p, i, j, c.R) - tangent(P, i, j, q, c.R);
    }
    if (k == "to_tangent") {                               // idx: (точка, пред., вершина гиба, след.)
        const int a = c.idx.at(0), p = c.idx.at(1), i = c.idx.at(2), q = c.idx.at(3);
        const Vec3 tp = sub(P[i], mul(unit_or_self(sub(P[i], P[p])), tangent(P, p, i, q, c.R)));
        return norm(sub(P[a], tp));
    }
    if (k == "bend_angle") {
        return bend_angle(P, c.idx.at(0), c.idx.at(1), c.idx.at(2));
    }
    if (k == "plane_dist") {
        return dot(sub(P[c.idx.at(0)], *c.origin), unit_or_self(*c.normal));
    }
    if (k == "line_dist") {
        return norm(cross(sub(P[c.idx.at(0)], *c.origin), unit_or_self(*c.normal)));
    }
    throw std::invalid_argument(k + " — не размер");
}

SolveResult solve(const std::vector<Vec3>& points, const std::vector<Constraint>& cons, bool preserve_shape) {
    const std::size_t n = points.size();
    std::vector<double> x0;
    x0.reserve(3 * n);
    for (const auto& p : points) x0.insert(x0.end(), p.begin(), p.end());

    const auto act = active(cons);
    std::optional<std::vector<double>> x_shape;
    if (preserve_shape) x_shape = x0;
    const std::vector<double>* shape_ptr = x_shape ? &*x_shape : nullptr;

    const bool has_residuals = !act.empty() || preserve_shape;
    std::vector<double> res_x = x0;
    if (has_residuals) {
        res_x = levenberg_marquardt(x0, n, act, preserve_shape, shape_ptr);
    }

    std::vector<Vec3> P(n);
    for (std::size_t i = 0; i < n; ++i) P[i] = {res_x[3 * i], res_x[3 * i + 1], res_x[3 * i + 2]};

    // якобиан без весов и без сохранения формы — для степеней свободы
    Eigen::MatrixXd jac(0, static_cast<Eigen::Index>(3 * n));
    if (!act.empty()) {
        jac = numeric_jacobian(
            [&](const Eigen::VectorXd& xv) {
                const auto r = residuals(to_std(xv), n, act, false, nullptr);
                return Eigen::VectorXd(Eigen::Map<const Eigen::VectorXd>(r.data(), static_cast<Eigen::Index>(r.size())));
            },
            Eigen::Map<const Eigen::VectorXd>(res_x.data(), static_cast<Eigen::Index>(res_x.size())));
    }
    const int rank = numeric_rank(jac);
    const int dof = static_cast<int>(3 * n) - rank;

    const auto r = residuals(res_x, n, act, false, nullptr);
    double total = 0.0;
    for (double v : r) total += v * v;
    total = std::sqrt(total);

    SolveResult out;
    out.dof = dof;
    out.points = P;
    out.residual = total;
    for (const auto& c : cons) {
        if (c.reference) out.reference_values[c.label.empty() ? c.kind : c.label] = measure(P, c);
    }
    if (total > 1e-6) {
        out.status = "конфликт";
        for (const auto& c : act) {
            const auto rw = rows(P, c);
            double s = 0.0;
            for (double v : rw) s += v * v;
            if (std::sqrt(s) > 1e-6) out.conflicts.push_back(c.label.empty() ? c.kind : c.label);
        }
        return out;
    }
    out.status = dof == 0 ? "определено" : "недоопределено";
    return out;
}

std::vector<Constraint> delete_constraint(const std::vector<Constraint>& cons, const std::string& label) {
    std::vector<Constraint> out;
    for (const auto& c : cons) {
        if (c.label != label) out.push_back(c);
    }
    return out;
}

std::vector<Constraint> make_reference(const std::vector<Constraint>& cons, const std::string& label) {
    std::vector<Constraint> out;
    for (auto c : cons) {
        if (c.label == label) {
            if (!is_dimension(c.kind)) throw std::invalid_argument("справочным можно сделать только размер");
            c.reference = true;
        }
        out.push_back(c);
    }
    return out;
}

SnapResult snap_point(const Vec3& target, const std::vector<Vec3>& ports, const std::vector<Vec3>& features,
                      double radius, const std::vector<Vec3>& vertices, const std::vector<Guide>& guides,
                      const std::optional<Grid>& grid) {
    for (const auto& p : ports) {
        if (norm(sub(p, target)) <= radius) return {p, "порт"};
    }
    for (const auto& p : vertices) {
        if (norm(sub(p, target)) <= radius) return {p, "вершина"};
    }
    for (const auto& f : features) {
        if (norm(sub(f, target)) <= radius) return {f, "признак"};
    }
    std::optional<std::pair<double, Vec3>> best;
    for (const auto& g : guides) {
        const Vec3 d = mul(g.direction, 1.0 / norm(g.direction));
        const Vec3 q = add(g.origin, mul(d, dot(sub(target, g.origin), d)));
        const double dist = norm(sub(q, target));
        if (dist <= radius && (!best || dist < best->first)) best = {dist, q};
    }
    if (best) return {best->second, "направляющая"};
    if (grid) {
        const Vec3 rel = sub(target, grid->origin);
        const double a = std::nearbyint(dot(rel, grid->u) / grid->step) * grid->step;
        const double b = std::nearbyint(dot(rel, grid->v) / grid->step) * grid->step;
        const Vec3 q = add(add(grid->origin, mul(grid->u, a)), mul(grid->v, b));
        if (norm(sub(q, target)) <= radius) return {q, "сетка"};
    }
    return {target, "свободно"};
}

Vec3 ortho_step(const Vec3& prev, const Vec3& target) {
    Vec3 d{std::abs(target[0] - prev[0]), std::abs(target[1] - prev[1]), std::abs(target[2] - prev[2])};
    std::size_t axis = 0;
    for (std::size_t k = 1; k < 3; ++k) {
        if (d[k] > d[axis]) axis = k;
    }
    Vec3 out = prev;
    out[axis] = target[axis];
    return out;
}

double snap_angle(double angle_deg, const std::vector<double>& allowed) {
    if (allowed.empty()) throw std::invalid_argument("нет допустимых углов");
    double best = allowed.front();
    for (double a : allowed) {
        if (std::abs(a - angle_deg) < std::abs(best - angle_deg)) best = a;
    }
    return best;
}

BendSnap snap_bend_angle(double angle_deg, bool manual, const std::vector<double>& preferred,
                         const std::vector<double>& ticks, const std::vector<double>& stops,
                         std::optional<double> step, bool typed, double hard_radius, double soft_radius,
                         double eps) {
    double a = angle_deg;
    if (!typed) {
        if (manual) {
            std::vector<double> cands = preferred;
            cands.insert(cands.end(), ticks.begin(), ticks.end());
            if (!cands.empty()) {
                const double c = snap_angle(a, cands);
                if (std::abs(c - a) <= hard_radius) a = c;
            }
        } else {
            std::vector<double> cands = preferred;
            cands.insert(cands.end(), stops.begin(), stops.end());
            if (step && *step != 0.0) cands.push_back(std::nearbyint(a / *step) * *step);
            if (!cands.empty()) {
                const double c = snap_angle(a, cands);
                if (std::abs(c - a) <= soft_radius) a = c;
            }
        }
    }
    BendSnap out{a, std::nullopt};
    if (manual && !ticks.empty()) {
        const bool on_tick = std::any_of(ticks.begin(), ticks.end(),
                                         [&](double t) { return std::abs(a - t) < eps; });
        if (!on_tick) out.warning = kScaleWarning;
    }
    return out;
}

EntryArc entry_arc_to_vertex(double R_arc, double R_profile, double tol_R) {
    const double dev = std::abs(R_arc - R_profile);
    if (dev <= tol_R + 1e-12) {
        return {"ok", R_profile, "", {},
                std::format("R дуги отличается на {:.3f} мм, принят R профиля", dev)};
    }
    return {"conflict", std::nullopt, "keep_body", {"accept_profile", "custom_bend_from_model", "keep_body"},
            std::format("конфликт радиуса: R модели {} ≠ R профиля {} (допуск {} мм)", R_arc, R_profile, tol_R)};
}

}  // namespace bendcore::sketch
