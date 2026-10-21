#include "bendcore/dwfx/autoport_detection.hpp"

#include <cmath>
#include <algorithm>

namespace bendcore::dwfx {

namespace {

// Вспомогательные векторные операции
double dot(const Vec3d& a, const Vec3d& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double length(const Vec3d& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3d normalize(const Vec3d& v) {
    const double len = length(v);
    if (len < 1e-10) return {0, 0, 1};
    return {v.x / len, v.y / len, v.z / len};
}

Vec3d subtract(const Vec3d& a, const Vec3d& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

// Ошибка соосности: расстояние между осями (в миллиметрах)
double coaxiality_error(const Vec3d& tube_pos, const Vec3d& tube_dir,
                        const Vec3d& feature_pos, const Vec3d& feature_dir) {
    // Вектор между позициями
    const Vec3d v = subtract(feature_pos, tube_pos);

    // Компонента v вдоль трубной оси
    const double along_tube = dot(v, tube_dir);

    // Компонента перпендикулярно трубной оси — это ошибка соосности
    const double perp_x = v.x - along_tube * tube_dir.x;
    const double perp_y = v.y - along_tube * tube_dir.y;
    const double perp_z = v.z - along_tube * tube_dir.z;

    return std::sqrt(perp_x * perp_x + perp_y * perp_y + perp_z * perp_z);
}

// Угловая ошибка между осями (в градусах)
double angular_error(const Vec3d& dir1, const Vec3d& dir2) {
    const double cos_angle = std::abs(dot(dir1, dir2));
    // Клампируем для избежания ошибок численности
    const double clamped = std::min(1.0, std::max(-1.0, cos_angle));
    const double angle_rad = std::acos(clamped);
    return angle_rad * 180.0 / M_PI;
}

}  // namespace

std::vector<CylindricalFeature> find_coaxial_features(
    const Vec3d& tube_end_position,
    const Vec3d& tube_axis_direction,
    double tube_od_mm,
    const std::vector<CylindricalFeature>& environment_features,
    const AutoportSettings& settings) {

    std::vector<CylindricalFeature> candidates;
    const double min_od_tolerance = std::max(settings.od_tolerance_min_mm,
                                             tube_od_mm * settings.od_tolerance_rel);

    for (const auto& feature : environment_features) {
        // Проверка 1: диаметр в допуске
        const double diameter_diff = std::abs(feature.diameter_mm - tube_od_mm);
        if (diameter_diff > min_od_tolerance) continue;

        // Проверка 2: соосность в допуске
        const double coax_error = coaxiality_error(tube_end_position, tube_axis_direction,
                                                   feature.axis_point, feature.axis_direction);
        if (coax_error > settings.linear_tolerance_mm) continue;

        // Проверка 3: угловое совпадение (оси параллельны или антипараллельны)
        const double ang_error = angular_error(tube_axis_direction, feature.axis_direction);
        if (ang_error > settings.angular_tolerance_deg &&
            (180.0 - ang_error) > settings.angular_tolerance_deg) continue;

        candidates.push_back(feature);
    }

    // Сортируем по ошибке соосности (лучше первым)
    std::sort(candidates.begin(), candidates.end(),
        [&tube_end_position, &tube_axis_direction](const CylindricalFeature& a,
                                                   const CylindricalFeature& b) {
            const double err_a = coaxiality_error(tube_end_position, tube_axis_direction,
                                                  a.axis_point, a.axis_direction);
            const double err_b = coaxiality_error(tube_end_position, tube_axis_direction,
                                                  b.axis_point, b.axis_direction);
            return err_a < err_b;
        });

    return candidates;
}

std::vector<DetectedPort> detect_ports(
    const Vec3d& end_a_pos,
    const Vec3d& end_b_pos,
    const Vec3d& tube_axis,
    double tube_od_mm,
    const std::vector<CylindricalFeature>& environment,
    const AutoportSettings& settings) {

    std::vector<DetectedPort> result;

    // Нормализуем ось трубы
    const Vec3d norm_axis = normalize(tube_axis);

    // Поиск портов для конца A
    const auto candidates_a = find_coaxial_features(end_a_pos, norm_axis, tube_od_mm,
                                                     environment, settings);
    for (const auto& cand : candidates_a) {
        DetectedPort port;
        port.tube_end = 0;
        port.feature = cand;
        port.coaxiality_error_mm = coaxiality_error(end_a_pos, norm_axis,
                                                     cand.axis_point, cand.axis_direction);
        port.diameter_match_error_rel = std::abs(cand.diameter_mm - tube_od_mm) / tube_od_mm;
        result.push_back(port);
    }

    // Поиск портов для конца B
    const auto candidates_b = find_coaxial_features(end_b_pos, norm_axis, tube_od_mm,
                                                     environment, settings);
    for (const auto& cand : candidates_b) {
        DetectedPort port;
        port.tube_end = 1;
        port.feature = cand;
        port.coaxiality_error_mm = coaxiality_error(end_b_pos, norm_axis,
                                                     cand.axis_point, cand.axis_direction);
        port.diameter_match_error_rel = std::abs(cand.diameter_mm - tube_od_mm) / tube_od_mm;
        result.push_back(port);
    }

    return result;
}

}  // namespace bendcore::dwfx
