// Осевая линия пути как кусочно-заданная геометрия: прямые и дуги с точными параметрами.
// Не зависит от геометрического ядра: по этим данным строится тело трубы (src/occt, для OCCT).
// Логика кадров (касательная, верх, поворот A) совпадает с tubegeom::trace.
#pragma once

#include <vector>

#include "bendcore/tubegeom.hpp"
#include "bendcore/vec3.hpp"

namespace bendcore {

struct AxisPiece {
    enum class Kind { Line, Arc };
    Kind kind = Kind::Line;
    Vec3 p0{};        // начало
    Vec3 p1{};        // конец
    Vec3 mid{};       // середина дуги (для Line не используется)
    Vec3 centre{};    // центр дуги (для Line не используется)
    Vec3 axis{};      // ось дуги, единичная (для Line не используется)
    double R = 0.0;   // радиус дуги, мм
};

// Кусочная ось пути. Спираль пока не поддержана — std::invalid_argument.
// Вырожденные гибы (|B| < kDegenerateBDeg) пропускаются, как в trace; нулевые прямые — тоже.
std::vector<AxisPiece> axis_pieces(const Path& path, Vec3 start = {0.0, 0.0, 0.0});

}  // namespace bendcore
