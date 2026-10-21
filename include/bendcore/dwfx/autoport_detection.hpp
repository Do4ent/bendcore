// Автоматическое создание портов при импорте (spec 9.2.2).
// Поиск соосных признаков окружения (cylindrical faces/edges) и создание портов.
#pragma once

#include <vector>
#include <string>

namespace bendcore::dwfx {

struct Vec3 { float x = 0, y = 0, z = 0; };
struct Vec3d { double x = 0, y = 0, z = 0; };

// Параметры поиска портов
struct AutoportSettings {
    double linear_tolerance_mm = 0.1;    // допуск совпадения оси, мм
    double angular_tolerance_deg = 1.0;  // допуск угла, градусы
    double od_tolerance_rel = 0.05;      // допуск диаметра, доля от ∅
    double od_tolerance_min_mm = 0.5;    // минимальный допуск, мм
};

// Результат поиска признака окружения (cylindrical feature)
struct CylindricalFeature {
    Vec3d axis_point;           // точка на оси цилиндра
    Vec3d axis_direction;       // направление оси (нормализовано)
    double diameter_mm = 0.0;   // диаметр признака
    bool is_hole = false;       // true = отверстие, false = стержень
    std::string feature_name;   // имя признака (для отчёта)
};

// Полный результат распознавания портов
struct DetectedPort {
    std::size_t tube_end = 0;        // 0 = конец A, 1 = конец B
    CylindricalFeature feature;      // найденный признак
    double coaxiality_error_mm = 0.0; // ошибка соосности
    double diameter_match_error_rel = 0.0; // относительная ошибка диаметра
};

// Поиск соосного признака окружения для конца трубы.
// На входе: позиция конца, направление оси, диаметр трубы, список признаков окружения.
// На выходе: найденные кандидаты (отсортированы по ошибке, лучший первый).
std::vector<CylindricalFeature> find_coaxial_features(
    const Vec3d& tube_end_position,
    const Vec3d& tube_axis_direction,
    double tube_od_mm,
    const std::vector<CylindricalFeature>& environment_features,
    const AutoportSettings& settings);

// Создание портов: из найденных признаков выбираем один на конец трубы.
// При нескольких кандидатах возвращаем все (пользователь выбирает).
// Торец без пары → без порта.
std::vector<DetectedPort> detect_ports(
    const Vec3d& end_a_pos,     // позиция конца A трубы
    const Vec3d& end_b_pos,     // позиция конца B трубы
    const Vec3d& tube_axis,     // направление оси трубы
    double tube_od_mm,          // диаметр трубы
    const std::vector<CylindricalFeature>& environment,
    const AutoportSettings& settings);

}  // namespace bendcore::dwfx
