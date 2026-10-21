// Извлечение образцов трубы из HSF-сетки (spec 9.10).
// Анализ сетки для получения центральной линии и диаметра.
#pragma once

#include <vector>
#include <optional>
#include <array>
#include <string>
#include <cstdint>

#include "bendcore/dwfx/vec3d.hpp"

namespace bendcore::dwfx {

// Параметры анализа сетки
struct MeshSamplingSettings {
    double radius_tolerance_rel = 0.1;  // допуск радиуса, доля от среднего
    double radius_tolerance_mm = 0.5;   // минимальный допуск, мм
    int min_samples = 3;                // минимум точек образца для валидации
};

// Образец трубы: точка на центральной линии с измеренным диаметром
struct TubeSample {
    Vec3d position;              // точка на центральной линии
    double diameter_mm = 0.0;    // диаметр в этой точке
    int sample_count = 0;        // число вершин, используемых для расчёта диаметра
};

// Результат анализа сетки
struct MeshSamplingResult {
    std::vector<TubeSample> samples;  // образцы вдоль линии
    std::optional<Vec3d> start_point; // начальная точка центральной линии
    std::optional<Vec3d> end_point;   // конечная точка центральной линии
    double avg_diameter_mm = 0.0;     // средний диаметр по сетке
    std::string validation_status;    // "OK" | "insufficient_data" | "mesh_not_tubular"
};

// Анализ HSF-сетки для извлечения образцов трубы.
// На входе: вершины сетки, грани (опционально).
// Результат: центральная линия и диаметры вдоль пути.
MeshSamplingResult sample_tube_from_mesh(
    const std::vector<Vec3d>& vertices,
    const std::vector<std::array<std::int32_t, 3>>& faces,  // может быть пусто
    const MeshSamplingSettings& settings);

}  // namespace bendcore::dwfx
