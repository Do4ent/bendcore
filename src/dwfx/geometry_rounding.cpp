#include "bendcore/dwfx/geometry_rounding.hpp"

#include <cmath>
#include <stdexcept>

namespace bendcore::dwfx {

namespace {

// Вспомогательная функция: округление до ближайшего кратного шагу.
// Добавляем малую epsilon для обработки ошибок плавающей точки.
double round_to_step(double value, double step) {
    if (step <= 0.0) return value;
    const double epsilon = step * 1e-10;
    return std::round((value + epsilon) / step) * step;
}

// Вспомогательная функция: нормализация угла в диапазон [-180, 180].
double normalize_angle(double deg) {
    while (deg > 180.0) deg -= 360.0;
    while (deg <= -180.0) deg += 360.0;
    return deg;
}

}  // namespace

RoundedDimension round_diameter_wall(double od_mm, double wall_mm, const RoundingSettings& settings) {
    RoundedDimension result;
    result.od_mm = round_to_step(od_mm, settings.round_diameter_wall_mm);
    result.wall_mm = round_to_step(wall_mm, settings.round_diameter_wall_mm);

    // Отклонение — разница между исходным и округленным диаметром
    result.deviation_mm = std::abs(result.od_mm - od_mm);

    return result;
}

double round_length(double length_mm, const RoundingSettings& settings) {
    return round_to_step(length_mm, settings.round_length_mm);
}

RoundedAngle round_angle(double angle_deg, double rotation_deg, const RoundingSettings& settings) {
    RoundedAngle result;

    // Шаг 1: округлить угол гиба по шагу angle
    result.angle_deg = round_to_step(angle_deg, settings.round_angle_deg);

    // Шаг 2: выравнить поворот оснастки по шагу align
    // Поворот должен быть кратен шагу выравнивания, но только если он задан (не пусто)
    result.rotation_deg = round_to_step(rotation_deg, settings.round_align_deg);

    // Нормализуем угол в диапазон [-180, 180]
    result.angle_deg = normalize_angle(result.angle_deg);
    result.rotation_deg = normalize_angle(result.rotation_deg);

    return result;
}

RoundingResult round_path(const std::vector<PathSegment>& original, double od_mm, double wall_mm,
                          const RoundingSettings& settings) {
    RoundingResult result;

    if (original.empty()) {
        return result;
    }

    // Шаг 1: округляем ∅ и стенку один раз
    const auto dim_rounded = round_diameter_wall(od_mm, wall_mm, settings);

    // Шаг 2: округляем каждый участок, отслеживая накопленное смещение
    double cumulative_deviation = dim_rounded.deviation_mm;
    double current_rotation = 0.0;

    for (const auto& seg : original) {
        RoundedSegment rounded;

        if (seg.kind == PathSegment::Straight) {
            // Округляем длину участка
            rounded.length_mm = round_length(seg.length_mm, settings);
            rounded.deviation_mm = std::abs(rounded.length_mm - seg.length_mm);
        } else {
            // Bend: округляем радиус (не меняется), угол и поворот
            rounded.radius_mm = seg.radius_mm;  // Радиус из профиля

            // Округляем угол и поворот
            const auto angle_rounded = round_angle(seg.angle_deg, seg.rotation_deg, settings);
            rounded.angle_deg = angle_rounded.angle_deg;
            rounded.rotation_deg = angle_rounded.rotation_deg;

            // Отклонение для гиба — по углу
            rounded.deviation_mm = std::abs(rounded.angle_deg - seg.angle_deg);
        }

        cumulative_deviation += rounded.deviation_mm;
        result.segments.push_back(rounded);
    }

    result.total_deviation_mm = cumulative_deviation;

    // Проверка: превышение допуска
    if (cumulative_deviation > settings.max_deviation_mm) {
        result.exceeds_tolerance = true;
        // Исключение не бросаем — это предупреждение (spec: "превышение — предупреждение без блокировки вывода")
    }

    return result;
}

}  // namespace bendcore::dwfx
