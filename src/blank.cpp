#include "bendcore/blank.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "bendcore/spiral.hpp"

namespace bendcore {

double end_straight(const Path& path, bool from_start) {
    double total = 0.0;
    auto visit = [&total](const Item& it) {
        if (const auto* s = std::get_if<Straight>(&it)) {
            total += s->S;
            return true;
        }
        return false;
    };
    if (from_start) {
        for (const auto& it : path) {
            if (!visit(it)) break;
        }
    } else {
        for (auto it = path.rbegin(); it != path.rend(); ++it) {
            if (!visit(*it)) break;
        }
    }
    return total;
}

std::pair<double, std::optional<Issue>> end_allowance(double S_end, const EndRule& rule, double dmin) {
    if (rule.cut_allowed) return {std::max(dmin, rule.N - (S_end + rule.E)), std::nullopt};
    if (S_end + rule.E < rule.N) {
        return {0.0, Issue{"technological", -1, "без подрезки: S + E меньше N"}};
    }
    return {0.0, std::nullopt};
}

BlankResult blank_length(const Path& path, const EndRule& start, const EndRule& end, double dmin) {
    const double S_start = end_straight(path, true);
    const double S_end = end_straight(path, false);
    auto [d_start, i1] = end_allowance(S_start, start, dmin);
    auto [d_end, i2] = end_allowance(S_end, end, dmin);
    std::vector<Issue> issues;
    if (i1) issues.push_back(*i1);
    if (i2) issues.push_back(*i2);
    const double length = axis_length(path) + start.E + end.E + d_start + d_end;
    return {d_start, d_end, length, issues};
}

std::vector<Issue> check_min_straights(const Path& path, double Lmin) {
    std::vector<Issue> issues;
    std::vector<std::size_t> bends;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (std::holds_alternative<Bend>(path[i]) || std::holds_alternative<Spiral>(path[i])) bends.push_back(i);
    }
    for (std::size_t k = 0; k + 1 < bends.size(); ++k) {
        const std::size_t a = bends[k];
        const std::size_t b = bends[k + 1];
        double between = 0.0;
        for (std::size_t j = a + 1; j < b; ++j) {
            if (const auto* s = std::get_if<Straight>(&path[j])) between += s->S;
        }
        if (between < Lmin) {
            issues.push_back({"technological", static_cast<int>(b), "прямой между гибами меньше Lmin"});
        }
    }
    return issues;
}

double end_minimum(double S_end, double E, double N, double dmin, bool cut_allowed) {
    if (!cut_allowed) return S_end + E;
    return std::max(S_end + E + dmin, N);
}

std::pair<std::optional<double>, std::vector<Issue>> end_segment(double S_end, const EndSpec& end, double N,
                                                                double dmin, bool cut_allowed) {
    std::vector<Issue> issues;
    if (!cut_allowed) {
        if (end.manual) issues.push_back({"technological", -1, "подрезка не допускается: ручной участок недоступен"});
        if (S_end + end.E < N) issues.push_back({"technological", -1, "без подрезки: S + E меньше N"});
        return {0.0, issues};
    }
    if (!end.manual) return {std::max(dmin, N - (S_end + end.E)), issues};
    const double minimum = end_minimum(S_end, end.E, N, dmin, true);
    if (*end.manual < minimum - 1e-9) {
        // решение 19.10.2026: ошибка вместо значения, Δ не показывается
        issues.push_back({"technological", -1, "участок заготовки меньше минимума"});
        return {std::nullopt, issues};
    }
    return {*end.manual - (S_end + end.E), issues};
}

BlankResult blank_by_order(const Path& path, const EndSpec& end_A, const EndSpec& end_B, double N_first,
                           double N_last, char feed_from, bool both_orders, double dmin, bool cut_allowed) {
    const double S_A = end_straight(path, true);
    const double S_B = end_straight(path, false);
    std::vector<char> orders = both_orders ? std::vector<char>{'A', 'B'} : std::vector<char>{feed_from};
    std::vector<std::optional<double>> dA_all, dB_all;
    std::vector<Issue> issues;
    for (char o : orders) {
        const auto [NA, NB] = o == 'A' ? std::pair{N_first, N_last} : std::pair{N_last, N_first};
        auto [dA, iA] = end_segment(S_A, end_A, NA, dmin, cut_allowed);
        auto [dB, iB] = end_segment(S_B, end_B, NB, dmin, cut_allowed);
        dA_all.push_back(dA);
        dB_all.push_back(dB);
        issues.insert(issues.end(), iA.begin(), iA.end());
        issues.insert(issues.end(), iB.begin(), iB.end());
    }
    // Δ с ошибкой не показывается и длина не считается (решение 19.10.2026)
    const auto all_set = [](const std::vector<std::optional<double>>& v) {
        return std::all_of(v.begin(), v.end(), [](const auto& d) { return d.has_value(); });
    };
    if (!all_set(dA_all) || !all_set(dB_all)) {
        std::vector<Issue> uniq_bad;
        for (const auto& i : issues) {
            const bool seen = std::any_of(uniq_bad.begin(), uniq_bad.end(), [&](const Issue& u) { return u.text == i.text; });
            if (!seen) uniq_bad.push_back(i);
        }
        return {std::nullopt, std::nullopt, std::nullopt, uniq_bad};
    }
    double dA = *dA_all.front();
    double dB = *dB_all.front();
    for (const auto& d : dA_all) dA = std::max(dA, *d);
    for (const auto& d : dB_all) dB = std::max(dB, *d);
    const double length = axis_length(path) + end_A.E + end_B.E + dA + dB;
    std::vector<Issue> uniq;
    for (const auto& i : issues) {
        const bool seen = std::any_of(uniq.begin(), uniq.end(), [&](const Issue& u) { return u.text == i.text; });
        if (!seen) uniq.push_back(i);
    }
    return {dA, dB, length, uniq};
}

std::vector<Issue> check_blank_limits(double length, std::optional<double> stock_length,
                                      std::optional<double> default_stock, std::optional<double> machine_max,
                                      bool coil) {
    std::vector<Issue> issues;
    if (!coil) {
        const std::optional<double> stock = stock_length ? stock_length : default_stock;
        if (!stock) {
            issues.push_back({"info", -1, "длина хлыста не задана ни в записи, ни по умолчанию"});
        } else if (length > *stock) {
            issues.push_back({"technological", -1, "заготовка длиннее хлыста"});
        }
    }
    if (machine_max && length > *machine_max) {
        issues.push_back({"technological", -1, "заготовка длиннее макс. длины на станке"});
    }
    return issues;
}

}  // namespace bendcore
