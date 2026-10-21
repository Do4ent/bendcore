#include "bendcore/dwfx/mesh_sampling.hpp"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <array>

namespace bendcore::dwfx {

namespace {

// Вспомогательные функции
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

// Найти главную ось облака точек (как направление наибольшего разброса)
// Упрощённо: ось максимального расстояния между точками
Vec3d find_principal_axis(const std::vector<Vec3d>& vertices) {
    if (vertices.size() < 2) return {0, 0, 1};

    // Найти расстояние между точками вдоль каждой оси
    double min_x = vertices[0].x, max_x = vertices[0].x;
    double min_y = vertices[0].y, max_y = vertices[0].y;
    double min_z = vertices[0].z, max_z = vertices[0].z;

    for (const auto& v : vertices) {
        min_x = std::min(min_x, v.x);
        max_x = std::max(max_x, v.x);
        min_y = std::min(min_y, v.y);
        max_y = std::max(max_y, v.y);
        min_z = std::min(min_z, v.z);
        max_z = std::max(max_z, v.z);
    }

    const double dx = max_x - min_x;
    const double dy = max_y - min_y;
    const double dz = max_z - min_z;

    // Ось с наибольшим диапазоном — главная ось
    if (dz >= dx && dz >= dy) return {0, 0, 1};
    if (dy >= dx && dy >= dz) return {0, 1, 0};
    return {1, 0, 0};
}

// Проекция точки на ось (вдоль направления)
double project_on_axis(const Vec3d& point, const Vec3d& axis_dir) {
    return dot(point, axis_dir);
}

// Расстояние точки от оси (перпендикулярное расстояние)
double distance_from_axis(const Vec3d& point, const Vec3d& axis_point, const Vec3d& axis_dir) {
    const Vec3d v = subtract(point, axis_point);
    const double along = dot(v, axis_dir);
    const Vec3d perp = {
        v.x - along * axis_dir.x,
        v.y - along * axis_dir.y,
        v.z - along * axis_dir.z
    };
    return length(perp);
}

}  // namespace

MeshSamplingResult sample_tube_from_mesh(
    const std::vector<Vec3d>& vertices,
    const std::vector<std::array<std::int32_t, 3>>& faces,
    const MeshSamplingSettings& settings) {

    MeshSamplingResult result;

    if (vertices.empty()) {
        result.validation_status = "insufficient_data";
        return result;
    }

    // Найти главную ось (по которой расположена труба)
    const Vec3d axis_dir = normalize(find_principal_axis(vertices));

    // Найти центр массы вершин как начальную точку на оси
    Vec3d center = {0, 0, 0};
    for (const auto& v : vertices) {
        center.x += v.x;
        center.y += v.y;
        center.z += v.z;
    }
    center.x /= static_cast<double>(vertices.size());
    center.y /= static_cast<double>(vertices.size());
    center.z /= static_cast<double>(vertices.size());

    // Найти диапазон проекций вдоль оси
    double min_proj = project_on_axis(vertices[0], axis_dir);
    double max_proj = min_proj;
    for (const auto& v : vertices) {
        const double proj = project_on_axis(v, axis_dir);
        min_proj = std::min(min_proj, proj);
        max_proj = std::max(max_proj, proj);
    }

    // Начальная и конечная точки
    const Vec3d start_base = center;
    Vec3d start_point = {
        start_base.x + min_proj * axis_dir.x,
        start_base.y + min_proj * axis_dir.y,
        start_base.z + min_proj * axis_dir.z
    };
    Vec3d end_point = {
        start_base.x + max_proj * axis_dir.x,
        start_base.y + max_proj * axis_dir.y,
        start_base.z + max_proj * axis_dir.z
    };

    result.start_point = start_point;
    result.end_point = end_point;

    // Делим трубу на секции вдоль оси
    const int num_samples = std::max(settings.min_samples, static_cast<int>((max_proj - min_proj) / 10.0 + 1));
    const double sample_step = (max_proj - min_proj) / static_cast<double>(num_samples - 1);

    std::vector<double> all_radii;

    for (int i = 0; i < num_samples; ++i) {
        const double proj = min_proj + i * sample_step;
        const Vec3d sample_center = {
            start_base.x + proj * axis_dir.x,
            start_base.y + proj * axis_dir.y,
            start_base.z + proj * axis_dir.z
        };

        // Найти вершины около этой позиции (в пределах 15 мм вдоль оси)
        std::vector<double> radii;
        const double axis_tolerance = 15.0;

        for (const auto& v : vertices) {
            const double v_proj = project_on_axis(v, axis_dir);
            // Проверяем, близко ли вершина к этой позиции вдоль оси
            if (std::abs(v_proj - proj) < axis_tolerance) {
                const double radius = distance_from_axis(v, sample_center, axis_dir);
                if (radius > 1e-6) {  // Исключаем вершины прямо на оси
                    radii.push_back(radius);
                    all_radii.push_back(radius);
                }
            }
        }

        if (radii.size() >= 3) {  // Нужно минимум 3 точки для расчёта радиуса
            // Вычисляем средний радиус (может быть лучше медиана)
            std::sort(radii.begin(), radii.end());
            const double median_radius = radii[radii.size() / 2];

            TubeSample sample;
            sample.position = sample_center;
            sample.diameter_mm = 2.0 * median_radius;
            sample.sample_count = static_cast<int>(radii.size());

            result.samples.push_back(sample);
        }
    }

    if (result.samples.empty()) {
        result.validation_status = "insufficient_data";
        return result;
    }

    // Средний диаметр
    if (!all_radii.empty()) {
        double avg_radius = std::accumulate(all_radii.begin(), all_radii.end(), 0.0) / all_radii.size();
        result.avg_diameter_mm = 2.0 * avg_radius;
    }

    // Проверка постоянства диаметра (для трубы это должно быть примерно постоянным)
    if (!result.samples.empty()) {
        double min_dia = result.samples[0].diameter_mm;
        double max_dia = result.samples[0].diameter_mm;

        for (const auto& s : result.samples) {
            min_dia = std::min(min_dia, s.diameter_mm);
            max_dia = std::max(max_dia, s.diameter_mm);
        }

        const double allowed_tolerance = std::max(
            settings.radius_tolerance_mm * 2.0,
            result.avg_diameter_mm * settings.radius_tolerance_rel
        );

        if ((max_dia - min_dia) > allowed_tolerance) {
            result.validation_status = "mesh_not_tubular";
        } else {
            result.validation_status = "OK";
        }
    }

    return result;
}

}  // namespace bendcore::dwfx
