// Калибровка оснастки (2.2.2, 3.6). Порт calib.py.
// Поправка угла B_ст = a·B + b; по одной пробе — только a, по двум и более — МНК (a, b).
#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bendcore {

inline constexpr double kWallTol = 0.1;  // мм, допуск стенки для ключа калибровки (2.2.2)

struct Trial {
    double B_model;   // угол модели, градусы
    double B_set;     // заданный на станке угол
    double B_fact;    // измеренный угол
};

struct Calibration {
    std::string date;
    std::string operator_name;
    double od = 0.0;
    double wall = 0.0;
    double a = 1.0;
    double b = 0.0;
    std::map<int, double> dL;   // номер гиба → измеренное ΔL, мм (заменяет ΔT с каждой стороны)
    bool cancelled = false;
};

// (a, b) по пробам. Пустой набор — std::invalid_argument; одна проба или все на одном угле — только a.
std::pair<double, double> fit_ab(const std::vector<Trial>& trials);

class CalibrationJournal {
public:
    using Tool = std::pair<std::string, std::string>;  // (позиция, профиль)

    void record(const Tool& tool, const Calibration& cal);
    // Действующая запись: последняя не отменённая; подходит по ∅ и стенке (в допуске), иначе — расчёт.
    std::optional<Calibration> current(const Tool& tool, double od, double wall) const;
    // Отмена записи по индексу в истории; сама запись остаётся в журнале.
    void cancel(const Tool& tool, std::size_t index);
    std::vector<Calibration> history(const Tool& tool) const;

private:
    std::map<Tool, std::vector<Calibration>> history_;
};

// B_ст по калибровке (a·B + b) или nullopt — тогда расчёт (3.6).
std::optional<double> corrected_angle(double B, const std::optional<Calibration>& cal);

}  // namespace bendcore
