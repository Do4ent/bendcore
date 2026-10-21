// Геометрическое упрощение распознанной геометрии при импорте DWFx (spec 9.9).
// Алгоритм S–A–B: округление участков (S), углов (A), выравнивание (B) по шагам.
// Защита отклонением: накопленное смещение сравнивается с макс. отклонением.
#pragma once

#include <vector>

namespace bendcore::dwfx {

struct RoundingSettings {
    double round_length_mm = 1.0;            // шаг длин
    double round_angle_deg = 0.5;            // шаг углов
    double round_align_deg = 0.5;            // шаг выравнивания (кратность ориентации)
    double round_diameter_wall_mm = 0.1;     // шаг ∅ и стенки
    double max_deviation_mm = 0.5;           // защита отклонением
};

struct RoundedDimension {
    double od_mm = 0.0;          // наружный диаметр (после округления)
    double wall_mm = 0.0;        // толщина стенки (после округления)
    double deviation_mm = 0.0;   // отклонение от исходного ∅
};

struct RoundedSegment {
    double length_mm = 0.0;      // прямой участок: длина (округлено)
    double radius_mm = 0.0;      // гиб: радиус
    double angle_deg = 0.0;      // гиб: угол (округлено)
    double rotation_deg = 0.0;   // гиб: поворот оснастки
    double deviation_mm = 0.0;   // отклонение этого участка
};

// Округление диаметра и стенки трубы по шагу.
// Возвращает округленные значения и отклонение от исходных.
RoundedDimension round_diameter_wall(double od_mm, double wall_mm, const RoundingSettings& settings);

// Округление длины прямого участка по шагу.
double round_length(double length_mm, const RoundingSettings& settings);

// Округление угла гиба по шагам (угол) и выравнивание (ориентация).
// angle_deg: угол гиба; rotation_deg: текущий поворот оснастки.
// Возвращает округленный угол и рекомендуемый поворот для выравнивания.
struct RoundedAngle {
    double angle_deg = 0.0;       // угол (округлено по шагу)
    double rotation_deg = 0.0;    // поворот (выровнено по шагу выравнивания)
};
RoundedAngle round_angle(double angle_deg, double rotation_deg, const RoundingSettings& settings);

// Полное округление пути: S–A–B алгоритм.
// На входе: исходные параметры (длины, радиусы, углы, ориентации, ∅, стенка).
// На выходе: округленные участки и итоговое отклонение.
// Исключение std::invalid_argument, если отклонение превышает макс.
struct PathSegment {
    enum Kind { Straight, Bend } kind;
    double length_mm = 0.0;      // для Straight
    double radius_mm = 0.0;      // для Bend
    double angle_deg = 0.0;      // для Bend
    double rotation_deg = 0.0;   // для Bend
};

struct RoundingResult {
    std::vector<RoundedSegment> segments;
    double total_deviation_mm = 0.0;
    bool exceeds_tolerance = false;  // true, если отклонение > max_deviation_mm
};

// Округление последовательности участков.
// Параметры трубы (∅, стенка) останутся без изменений, но отклонение отдельно.
RoundingResult round_path(const std::vector<PathSegment>& original, double od_mm, double wall_mm,
                          const RoundingSettings& settings);

}  // namespace bendcore::dwfx
