// Связность треугольной сетки TKE_Shell, сжатой EdgeBreaker (HSF/W3D): декодирование рабочей области
// (опкоды CASE_C/L/E/R/S, таблица MTable с фиктивными вершинами и алиасами патчей). Сжатие координат
// и нормалей здесь не декодируется. Порт src/import/dwfx/edgebreaker-connectivity.mjs из BendPipe-Pro.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace bendcore::dwfx::hsf {

struct EdgeBreakerConnectivity {
    std::uint32_t raw_point_count = 0;         // вершины до удаления фиктивных и алиасов
    std::uint32_t point_count = 0;             // вершины после удаления (сверяется с заголовком)
    std::vector<std::array<std::int32_t, 3>> faces;  // треугольники в индексах итоговых вершин
    std::uint32_t dropped_dummy_faces = 0;     // грани, отброшенные как фиктивные или вырожденные
    std::uint32_t patched_face_references = 0; // ссылки, переназначенные алиасами патчей
};

// Рабочая область EdgeBreaker: заголовок 24 байта, опкоды, MTable, упакованные точки.
// Исключение std::invalid_argument, если область повреждена или содержит неподдержанные варианты
// (слияние CASE_M / CASE_M2 и т.п.).
EdgeBreakerConnectivity decode_edgebreaker_connectivity(const std::uint8_t* data, std::size_t size);

}  // namespace bendcore::dwfx::hsf
