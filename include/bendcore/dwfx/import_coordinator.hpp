// Координатор импорта DWFx: интеграция Points 2, 3, 4 (spec 9.9, 9.2.2, 9.10).
// Оркестрирует процесс: сетка → образцы → округление → детектирование портов → TubeModel.
#pragma once

#include <vector>
#include <optional>
#include <string>
#include <array>
#include <cstdint>

#include "bendcore/tubegeom.hpp"
#include "bendcore/ports.hpp"
#include "bendcore/tube_model.hpp"
#include "bendcore/dwfx/mesh_sampling.hpp"

namespace bendcore::dwfx {

// Параметры координатора импорта
struct ImportCoordinatorSettings {
    // Point 4: параметры сэмплирования сетки
    double mesh_radius_tolerance_rel = 0.1;
    double mesh_radius_tolerance_mm = 0.5;
    int mesh_min_samples = 3;

    // Point 2: параметры округления
    double round_length_mm = 1.0;
    double round_angle_deg = 0.5;
    double round_align_deg = 0.5;
    double round_diameter_wall_mm = 0.1;
    double max_deviation_mm = 2.0;

    // Point 3: параметры детектирования портов
    double port_linear_tolerance_mm = 0.1;
    double port_angular_tolerance_deg = 1.0;
    double port_od_tolerance_rel = 0.05;
    double port_od_tolerance_min_mm = 0.5;
};

// Результат координированного импорта
struct ImportCoordinatorResult {
    bool success = false;
    std::string error_message;

    // Результаты из каждой точки
    std::string mesh_sampling_status;     // "OK" | "insufficient_data" | "mesh_not_tubular"
    std::string geometry_rounding_status; // "OK" | "exceeds_tolerance"
    int detected_ports_count = 0;         // количество найденных портов

    // Финальный результат
    std::optional<TubeModel> tube_model;
};

// Главный импорт: сетка → TubeModel.
// tube_id: идентификатор трубы в файле
// vertices: вершины сетки
// faces: грани сетки (опционально)
// environment_features: признаки окружения для детектирования портов
// settings: параметры процесса
ImportCoordinatorResult import_tube_from_mesh(
    const std::string& tube_id,
    const std::vector<Vec3d>& vertices,
    const std::vector<std::array<std::int32_t, 3>>& faces,
    const std::vector<Vec3d>& environment_features,  // TODO: полная информация о признаках
    double od_mm,
    double wall_mm,
    const ImportCoordinatorSettings& settings);

}  // namespace bendcore::dwfx
