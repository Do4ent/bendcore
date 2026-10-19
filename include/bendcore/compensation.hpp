// Компенсация пружинения (3.6). Порт compensation.py.
#pragma once

#include <functional>
#include <optional>

#include "bendcore/blank.hpp"
#include "bendcore/tubegeom.hpp"

namespace bendcore {

// Теоретический B_ст (например, из ручного ввода или калибровки): nullopt — не задан.
using Theoretical = std::function<std::optional<double>(const Bend&)>;

double b_st(const Bend& bend, double K, const Theoretical& theoretical = nullptr);
double delta_T(const Bend& bend, double K);   // ΔT = (R/K − R)·tan(B/2)
Path compensated_path(const Path& path, double K, const Theoretical& theoretical = nullptr);
BlankResult compensated_blank(const Path& path, const EndRule& start, const EndRule& end, double K,
                              double dmin = 0.0, const Theoretical& theoretical = nullptr);
std::vector<Issue> check_station_angles(const Path& path, double B_max, double K,
                                        const Theoretical& theoretical = nullptr);

}  // namespace bendcore
