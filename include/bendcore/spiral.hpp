// Спираль (навивка) в 3D — спецификация 1.2.1. Порт spiral.py.
#pragma once

#include <optional>
#include <vector>

#include "bendcore/vec3.hpp"

namespace bendcore {

// Ось навивки, касательная к входящей прямой (h·a = sinα); w — проекция «верха» на плоскость ⊥ h.
Vec3 spiral_axis(const Vec3& heading, double R, double p, int direction = +1, Vec3 up = {0.0, 0.0, 1.0});

struct SpiralCentreline {
    std::vector<Vec3> pts;   // включая начальную точку
    Vec3 tend;               // единичная касательная в конце
    Vec3 axis;               // ось навивки
};

// Центральная линия спирали. Явно заданная ось проверяется на касание; иначе ошибка.
SpiralCentreline spiral_centreline(const Vec3& start, const Vec3& heading, double R, double p, double n,
                                   int direction = +1, std::optional<Vec3> axis = std::nullopt,
                                   int steps_per_turn = 64);

double spiral_axis_length(double R, double p, double n);  // n·√((2πR)² + p²)

}  // namespace bendcore
