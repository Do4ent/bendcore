// Этап 2 (часть 1). Припуски концов и длина заготовки — без компенсации. Порт blank.py.
#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "bendcore/tubegeom.hpp"

namespace bendcore {

struct EndRule {
    double E = 0.0;              // глубина вставки в соединение, мм
    double N = 0.0;              // минимальный участок заготовки на конце, мм
    bool cut_allowed = true;     // подрезка допускается
};

struct BlankResult {
    std::optional<double> delta_start;  // nullopt — ошибка ручного участка, значение не показывается
    std::optional<double> delta_end;
    std::optional<double> blank_length;
    std::vector<Issue> issues;
};

struct EndSpec {
    double E = 0.0;
    std::optional<double> manual;  // nullopt — «Авто»
};

double end_straight(const Path& path, bool from_start);
std::pair<double, std::optional<Issue>> end_allowance(double S_end, const EndRule& rule, double dmin);
BlankResult blank_length(const Path& path, const EndRule& start, const EndRule& end, double dmin = 0.0);
std::vector<Issue> check_min_straights(const Path& path, double Lmin);

double end_minimum(double S_end, double E, double N, double dmin, bool cut_allowed);
std::pair<std::optional<double>, std::vector<Issue>> end_segment(double S_end, const EndSpec& end, double N, double dmin,
                                                  bool cut_allowed);
BlankResult blank_by_order(const Path& path, const EndSpec& end_A, const EndSpec& end_B, double N_first,
                           double N_last, char feed_from = 'A', bool both_orders = false, double dmin = 0.0,
                           bool cut_allowed = true);

std::vector<Issue> check_blank_limits(double length, std::optional<double> stock_length,
                                      std::optional<double> default_stock, std::optional<double> machine_max,
                                      bool coil = false);

}  // namespace bendcore
