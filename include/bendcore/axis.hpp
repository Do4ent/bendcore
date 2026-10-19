// Осевая линия пути как кусочно-заданная геометрия: прямые, дуги и спираль.
// Не зависит от геометрического ядра: по этим данным строится тело трубы (src/occt, для OCCT).
// Логика кадров (касательная, верх, поворот A, спираль) совпадает с tubegeom::trace.
#pragma once

#include <vector>

#include "bendcore/tubegeom.hpp"
#include "bendcore/vec3.hpp"

namespace bendcore {

struct AxisPiece {
    enum class Kind { Line, Arc, Spiral };
    Kind kind = Kind::Line;
    Vec3 p0{};                // начало
    Vec3 p1{};                // конец
    Vec3 t0{};                // касательная в начале (единичная)
    Vec3 t1{};                // касательная в конце (единичная)
    Vec3 mid{};               // середина дуги (Arc)
    Vec3 centre{};            // центр дуги (Arc)
    Vec3 axis{};              // ось дуги (Arc); ось навивки (Spiral)
    double R = 0.0;           // радиус дуги или витка, мм
    std::vector<Vec3> pts;    // отсчёты центральной линии (Spiral), включая p0 и p1
};

// Кусочная ось пути. spiral_steps — отсчётов на виток (ошибка длины ~ (2π/steps)²/24).
// Вырожденные гибы (|B| < 0.5°) пропускаются, как в trace; нулевые прямые — тоже.
std::vector<AxisPiece> axis_pieces(const Path& path, Vec3 start = {0.0, 0.0, 0.0}, int spiral_steps = 64);

}  // namespace bendcore
