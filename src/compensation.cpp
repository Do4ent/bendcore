#include "bendcore/compensation.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>

#include "bendcore/spiral.hpp"

namespace bendcore {

double b_st(const Bend& bend, double K, const Theoretical& theoretical) {
    if (theoretical) {
        if (const auto t = theoretical(bend)) return *t;
    }
    return bend.B / K;
}

double delta_T(const Bend& bend, double K) {
    return (bend.R / K - bend.R) * std::tan(std::abs(bend.B) * std::numbers::pi / 180.0 / 2.0);
}

Path compensated_path(const Path& path, double K, const Theoretical& theoretical) {
    Path out;
    out.reserve(path.size());
    for (std::size_t i = 0; i < path.size(); ++i) {
        const Item& it = path[i];
        if (const auto* b = std::get_if<Bend>(&it)) {
            out.push_back(Bend{b->R, b_st(*b, K, theoretical), b->A});
        } else if (const auto* sp = std::get_if<Spiral>(&it)) {
            out.push_back(Spiral{sp->R, sp->p, sp->n / K, sp->direction});
        } else if (const auto* s = std::get_if<Straight>(&it)) {
            double cut = 0.0;
            if (i > 0) {
                if (const auto* pb = std::get_if<Bend>(&path[i - 1])) cut += delta_T(*pb, K);
            }
            if (i + 1 < path.size()) {
                if (const auto* nb = std::get_if<Bend>(&path[i + 1])) cut += delta_T(*nb, K);
            }
            out.push_back(Straight{s->S - cut});
        }
    }
    return out;
}

BlankResult compensated_blank(const Path& path, const EndRule& start, const EndRule& end, double K,
                              double dmin, const Theoretical& theoretical) {
    if (K <= 0) throw std::invalid_argument("K должен быть > 0");
    const Path station = compensated_path(path, K, theoretical);
    double total = 0.0;
    for (const auto& it : station) {
        if (const auto* s = std::get_if<Straight>(&it)) {
            total += s->S;
        } else if (const auto* b = std::get_if<Bend>(&it); b && std::abs(b->B) >= kDegenerateBDeg) {
            total += b->R * std::abs(b->B) * std::numbers::pi / 180.0;
        } else if (const auto* sp = std::get_if<Spiral>(&it)) {
            total += spiral_axis_length(sp->R, sp->p, sp->n);
        }
    }
    const double S_start = end_straight(path, true);
    const double S_end = end_straight(path, false);
    auto [d_start, i1] = end_allowance(S_start, start, dmin);
    auto [d_end, i2] = end_allowance(S_end, end, dmin);
    std::vector<Issue> issues;
    if (i1) issues.push_back(*i1);
    if (i2) issues.push_back(*i2);
    const double length = total + start.E + end.E + d_start + d_end;
    return {d_start, d_end, length, issues};
}

std::vector<Issue> check_station_angles(const Path& path, double B_max, double K, const Theoretical& theoretical) {
    std::vector<Issue> issues;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (const auto* b = std::get_if<Bend>(&path[i]); b && std::abs(b->B) >= kDegenerateBDeg) {
            if (std::abs(b_st(*b, K, theoretical)) > B_max) {
                issues.push_back({"technological", static_cast<int>(i), "угол на трубогибе больше макс. угла профиля"});
            }
        }
    }
    return issues;
}

}  // namespace bendcore
