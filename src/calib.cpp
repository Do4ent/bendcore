#include "bendcore/calib.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace bendcore {

std::pair<double, double> fit_ab(const std::vector<Trial>& trials) {
    if (trials.empty()) throw std::invalid_argument("нет пробных гибов");
    std::vector<double> xs, ys;
    for (const auto& t : trials) {
        xs.push_back(t.B_model);
        ys.push_back(t.B_model * t.B_set / t.B_fact);   // нужный угол станка для модели
    }
    double min_x = xs.front(), max_x = xs.front();
    double sum_x = 0.0, sum_y = 0.0;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        min_x = std::min(min_x, xs[i]);
        max_x = std::max(max_x, xs[i]);
        sum_x += xs[i];
        sum_y += ys[i];
    }
    if (trials.size() == 1 || max_x - min_x < 1e-9) return {sum_y / sum_x, 0.0};
    const double n = static_cast<double>(xs.size());
    const double mx = sum_x / n, my = sum_y / n;
    double sxx = 0.0, sxy = 0.0;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        sxx += (xs[i] - mx) * (xs[i] - mx);
        sxy += (xs[i] - mx) * (ys[i] - my);
    }
    const double a = sxy / sxx;
    return {a, my - a * mx};
}

void CalibrationJournal::record(const Tool& tool, const Calibration& cal) { history_[tool].push_back(cal); }

std::optional<Calibration> CalibrationJournal::current(const Tool& tool, double od, double wall) const {
    const auto it = history_.find(tool);
    if (it == history_.end()) return std::nullopt;
    for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
        if (rit->cancelled) continue;
        if (std::abs(rit->od - od) < 1e-9 && std::abs(rit->wall - wall) <= kWallTol + 1e-9) return *rit;
        return std::nullopt;   // последняя действующая не подходит по ∅ / стенке — расчёт
    }
    return std::nullopt;
}

void CalibrationJournal::cancel(const Tool& tool, std::size_t index) {
    auto it = history_.find(tool);
    if (it == history_.end() || index >= it->second.size()) throw std::out_of_range("нет такой записи калибровки");
    it->second[index].cancelled = true;
}

std::vector<Calibration> CalibrationJournal::history(const Tool& tool) const {
    const auto it = history_.find(tool);
    return it == history_.end() ? std::vector<Calibration>{} : it->second;
}

std::optional<double> corrected_angle(double B, const std::optional<Calibration>& cal) {
    if (!cal) return std::nullopt;
    return cal->a * B + cal->b;
}

}  // namespace bendcore
