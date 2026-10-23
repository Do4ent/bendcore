#include "bendcore/batch_check.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "bendcore/blank.hpp"

namespace bendcore::check {
namespace {

Severity from_kind(const std::string& kind) {
    if (kind == "geometric") return Severity::Geometric;
    if (kind == "technological") return Severity::Technological;
    if (kind == "warning") return Severity::Warning;
    return Severity::Info;  // "suggestion"
}

std::string rule_code(const std::string& message) {
    struct Map { const char* needle; const char* code; };
    static const Map table[] = {
        {"S < 0", "S<0"},
        {"R ≤ 0", "R<=0"},
        {"ввод A запрещён", "A_zero_gap"},
        {"вырожденный гиб", "degenerate_bend"},
        {"B > B_max", "B_max"},
        {"B вне диапазона", "B_range"},
        {"навивка", "spiral_not_allowed"},
        {"шаг спирали", "spiral_pitch"},
        {"шаг меньше наружного", "spiral_self"},
        {"самопересечение", "self_intersection"},
        {"коллинеарные", "merge_straights"},
        {"S + E меньше N", "N_end"},
    };
    for (const auto& m : table) {
        if (message.find(m.needle) != std::string::npos) return m.code;
    }
    return "other";
}

// Точка детали → СК сборки.
Vec3 from_frame(const Vec3& p, const output::Frame& f) {
    return add(f.origin, add(add(mul(f.x, p[0]), mul(f.y, p[1])), mul(f.z, p[2])));
}

struct Box {
    Vec3 lo{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
            std::numeric_limits<double>::max()};
    Vec3 hi{-std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(),
            -std::numeric_limits<double>::max()};
    void add_point(const Vec3& p) {
        for (int k = 0; k < 3; ++k) {
            lo[k] = std::min(lo[k], p[k]);
            hi[k] = std::max(hi[k], p[k]);
        }
    }
    void inflate(double d) {
        for (int k = 0; k < 3; ++k) {
            lo[k] -= d;
            hi[k] += d;
        }
    }
    bool overlaps(const Box& o) const {
        for (int k = 0; k < 3; ++k) {
            if (hi[k] < o.lo[k] || o.hi[k] < lo[k]) return false;
        }
        return true;
    }
};

// Ближайшие точки двух отрезков (Ericson, Real-Time Collision Detection, 5.1.9).
// Возвращает квадрат расстояния и параметры s, t ∈ [0, 1].
double seg_seg(const Vec3& p1, const Vec3& q1, const Vec3& p2, const Vec3& q2, double& s, double& t) {
    const Vec3 d1 = sub(q1, p1);
    const Vec3 d2 = sub(q2, p2);
    const Vec3 r = sub(p1, p2);
    const double a = dot(d1, d1);
    const double e = dot(d2, d2);
    const double f = dot(d2, r);
    const double eps = 1e-18;
    if (a <= eps && e <= eps) {
        s = t = 0.0;
        return dot(r, r);
    }
    if (a <= eps) {
        s = 0.0;
        t = std::clamp(f / e, 0.0, 1.0);
    } else {
        const double c = dot(d1, r);
        if (e <= eps) {
            t = 0.0;
            s = std::clamp(-c / a, 0.0, 1.0);
        } else {
            const double b = dot(d1, d2);
            const double denom = a * e - b * b;
            s = denom > eps ? std::clamp((b * f - c * e) / denom, 0.0, 1.0) : 0.0;
            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = std::clamp(-c / a, 0.0, 1.0);
            } else if (t > 1.0) {
                t = 1.0;
                s = std::clamp((b - c) / a, 0.0, 1.0);
            }
        }
    }
    const Vec3 c1 = add(p1, mul(d1, s));
    const Vec3 c2 = add(p2, mul(d2, t));
    const Vec3 d = sub(c1, c2);
    return dot(d, d);
}

std::vector<double> cumulative(const std::vector<Vec3>& p) {
    std::vector<double> s(p.size(), 0.0);
    for (std::size_t i = 1; i < p.size(); ++i) s[i] = s[i - 1] + norm(sub(p[i], p[i - 1]));
    return s;
}

void fnv(std::uint64_t& h, const void* data, std::size_t n) {
    const auto* b = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < n; ++i) {
        h ^= b[i];
        h *= 1099511628211ull;
    }
}
void fnv_d(std::uint64_t& h, double v) { fnv(h, &v, sizeof v); }

}  // namespace

const char* severity_name(Severity s) {
    switch (s) {
        case Severity::Geometric: return "Геометрическая ошибка";
        case Severity::Technological: return "Технологическая ошибка";
        case Severity::Warning: return "Предупреждение";
        case Severity::Mark: return "Пометка";
        case Severity::Info: return "Информация";
    }
    return "";
}

std::uint64_t fingerprint(const TubeInput& t) {
    std::uint64_t h = 1469598103934665603ull;
    fnv(h, t.id.data(), t.id.size());
    for (const auto& it : t.path) {
        const int k = static_cast<int>(it.index());
        fnv(h, &k, sizeof k);
        if (const auto* s = std::get_if<Straight>(&it)) {
            fnv_d(h, s->S);
        } else if (const auto* b = std::get_if<Bend>(&it)) {
            fnv_d(h, b->R); fnv_d(h, b->B); fnv_d(h, b->A);
        } else if (const auto* sp = std::get_if<Spiral>(&it)) {
            fnv_d(h, sp->R); fnv_d(h, sp->p); fnv_d(h, sp->n); fnv_d(h, sp->direction);
        }
    }
    for (double v : {t.od, t.B_min, t.B_max, t.L_min, t.N_first, t.N_last, t.E_A, t.E_B}) fnv_d(h, v);
    fnv_d(h, t.cut_allowed ? 1.0 : 0.0);
    fnv_d(h, t.spiral_allowed ? 1.0 : 0.0);
    fnv_d(h, t.spiral_min_pitch.value_or(-1.0));
    for (const Vec3* v : {&t.placement.origin, &t.placement.x, &t.placement.y, &t.placement.z})
        for (double c : *v) fnv_d(h, c);
    return h;
}

Closest closest_polylines(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
    if (a.size() < 2 || b.size() < 2) throw std::invalid_argument("полилиния из менее чем двух точек");
    const auto sa = cumulative(a);
    const auto sb = cumulative(b);
    Closest best;
    double best2 = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i + 1 < a.size(); ++i) {
        for (std::size_t j = 0; j + 1 < b.size(); ++j) {
            // Быстрый отказ: расстояние между габаритами отрезков не меньше текущего минимума.
            double gap2 = 0.0;
            for (int k = 0; k < 3; ++k) {
                const double alo = std::min(a[i][k], a[i + 1][k]), ahi = std::max(a[i][k], a[i + 1][k]);
                const double blo = std::min(b[j][k], b[j + 1][k]), bhi = std::max(b[j][k], b[j + 1][k]);
                const double g = std::max({0.0, blo - ahi, alo - bhi});
                gap2 += g * g;
            }
            if (gap2 >= best2) continue;
            double s = 0.0, t = 0.0;
            const double d2 = seg_seg(a[i], a[i + 1], b[j], b[j + 1], s, t);
            if (d2 < best2) {
                best2 = d2;
                best.s_a = sa[i] + s * (sa[i + 1] - sa[i]);
                best.s_b = sb[j] + t * (sb[j + 1] - sb[j]);
            }
        }
    }
    best.distance = std::sqrt(best2);
    return best;
}

Result run(const std::vector<TubeInput>& tubes, const Options& opt) {
    Result res;
    struct Body {
        std::size_t idx;
        std::vector<Vec3> axis;  // в СК сборки
        Box box;
        double length = 0.0;
    };
    std::vector<Body> bodies;

    for (std::size_t ti = 0; ti < tubes.size(); ++ti) {
        const TubeInput& t = tubes[ti];
        if (res.tubes.count(t.id)) throw std::invalid_argument("повтор id трубы: " + t.id);
        TubeSummary sum;
        sum.fingerprint = fingerprint(t);
        std::vector<Violation> found;
        auto add_issue = [&](const Issue& is, std::optional<double> value = {}, std::optional<double> limit = {}) {
            Violation v;
            v.tube_id = t.id;
            v.element = is.index;
            v.severity = from_kind(is.kind);
            v.rule = rule_code(is.text);
            v.message = is.text;
            v.value = value;
            v.limit = limit;
            found.push_back(v);
        };

        const std::optional<double> od = t.od > 0 ? std::optional<double>(t.od) : std::nullopt;
        for (const auto& is : validate(t.path, t.B_max, t.B_min, od, t.spiral_min_pitch, t.spiral_allowed)) {
            std::optional<double> value, limit;
            if (is.index >= 0) {
                if (const auto* b = std::get_if<Bend>(&t.path[static_cast<std::size_t>(is.index)])) {
                    value = std::abs(b->B);
                    if (is.text.find("B_max") != std::string::npos) limit = t.B_max;
                    if (is.text.find("диапазона") != std::string::npos) limit = t.B_min;
                }
            }
            add_issue(is, value, limit);
        }
        const bool geometric = std::any_of(found.begin(), found.end(),
                                           [](const Violation& v) { return v.severity == Severity::Geometric; });

        // Мин. прямой между гибами — с фактом и нормой.
        if (t.L_min > 0.0) {
            int prev_bend = -1;
            double between = 0.0;
            for (std::size_t i = 0; i < t.path.size(); ++i) {
                const auto& it = t.path[i];
                const bool is_bend = (std::holds_alternative<Bend>(it) &&
                                      std::abs(std::get<Bend>(it).B) >= kDegenerateBDeg) ||
                                     std::holds_alternative<Spiral>(it);
                if (is_bend) {
                    if (prev_bend >= 0 && between < t.L_min - 1e-9) {
                        Violation v;
                        v.tube_id = t.id;
                        v.element = static_cast<int>(i);
                        v.severity = Severity::Technological;
                        v.rule = "L_min";
                        v.message = "прямой между гибами меньше минимума";
                        v.value = between;
                        v.limit = t.L_min;
                        found.push_back(v);
                    }
                    prev_bend = static_cast<int>(i);
                    between = 0.0;
                } else if (const auto* s = std::get_if<Straight>(&it)) {
                    between += s->S;
                }
            }
        }

        // Концы: N как жёсткий лимит при «подрезка не допускается» (2.5, 4.5).
        if (!geometric && (t.N_first > 0.0 || t.N_last > 0.0)) {
            const BlankResult br = blank_length(t.path, EndRule{t.E_A, t.N_first, t.cut_allowed},
                                                EndRule{t.E_B, t.N_last, t.cut_allowed});
            for (const auto& is : br.issues) add_issue(is, std::nullopt, std::nullopt);
        }

        for (const auto& v : found) {
            if (!sum.worst || v.severity < *sum.worst) sum.worst = v.severity;
            if (v.severity == Severity::Geometric || v.severity == Severity::Technological) sum.blocks_output = true;
        }
        res.violations.insert(res.violations.end(), found.begin(), found.end());

        if (!geometric && opt.tube_to_tube && t.od > 0.0) {
            Body body;
            body.idx = ti;
            for (const auto& p : trace(t.path, 16)) body.axis.push_back(from_frame(p, t.placement));
            for (const auto& p : body.axis) body.box.add_point(p);
            body.box.inflate(t.od / 2.0 + opt.clearance / 2.0);
            body.length = axis_length(t.path);
            if (body.axis.size() >= 2) bodies.push_back(std::move(body));
        }
        res.tubes[t.id] = sum;
    }

    // Широкая фаза: сортировка по нижней границе X и проход с «окном».
    std::sort(bodies.begin(), bodies.end(), [](const Body& a, const Body& b) { return a.box.lo[0] < b.box.lo[0]; });
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size() && bodies[j].box.lo[0] <= bodies[i].box.hi[0]; ++j) {
            if (!bodies[i].box.overlaps(bodies[j].box)) continue;
            ++res.pairs_tested;
            const TubeInput& ta = tubes[bodies[i].idx];
            const TubeInput& tb = tubes[bodies[j].idx];
            const Closest c = closest_polylines(bodies[i].axis, bodies[j].axis);
            const double gap = c.distance - ta.od / 2.0 - tb.od / 2.0;
            if (gap >= opt.clearance - 1e-9) continue;
            // Касание торцами (трубы соединены встык) не считается коллизией.
            const double tol = 1e-6;
            const bool a_end = c.s_a < tol || c.s_a > bodies[i].length - tol;
            const bool b_end = c.s_b < tol || c.s_b > bodies[j].length - tol;
            if (a_end && b_end && c.distance < tol) continue;
            for (int side = 0; side < 2; ++side) {
                Violation v;
                v.tube_id = side == 0 ? ta.id : tb.id;
                v.other_tube = side == 0 ? tb.id : ta.id;
                v.severity = Severity::Mark;
                v.rule = gap < 0.0 ? "tube_intersection" : "clearance";
                v.message = gap < 0.0 ? "пересечение с трубой " + v.other_tube
                                      : "зазор до трубы " + v.other_tube + " меньше нормы";
                v.value = gap < 0.0 ? -gap : gap;  // глубина или «факт»
                if (gap >= 0.0) v.limit = opt.clearance;
                v.at_s = side == 0 ? c.s_a : c.s_b;
                res.violations.push_back(v);
                auto& sum = res.tubes[v.tube_id];
                if (!sum.worst || v.severity < *sum.worst) sum.worst = v.severity;
            }
        }
    }

    std::stable_sort(res.violations.begin(), res.violations.end(), [](const Violation& a, const Violation& b) {
        if (a.severity != b.severity) return a.severity < b.severity;
        if (a.tube_id != b.tube_id) return a.tube_id < b.tube_id;
        return a.element < b.element;
    });
    for (const auto& v : res.violations) res.counts[v.severity]++;
    for (auto& [id, sum] : res.tubes) sum.state = sum.worst ? TubeState::HasViolations : TubeState::Passed;
    return res;
}

}  // namespace bendcore::check
