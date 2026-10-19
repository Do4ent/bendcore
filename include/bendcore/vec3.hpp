// Векторные операции для ядра (внутренний заголовок). Порядок операций совпадает с numpy-эталоном.
#pragma once

#include <array>
#include <cmath>
#include <stdexcept>

namespace bendcore {

using Vec3 = std::array<double, 3>;

inline Vec3 add(const Vec3& a, const Vec3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
inline Vec3 sub(const Vec3& a, const Vec3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
inline Vec3 mul(const Vec3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
inline double dot(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
inline double norm(const Vec3& a) { return std::sqrt(dot(a, a)); }

// Единичный вектор; нулевой — ошибка (как _unit в Python).
inline Vec3 unit(const Vec3& v) {
    const double n = norm(v);
    if (n < 1e-12) throw std::invalid_argument("нулевой вектор");
    return mul(v, 1.0 / n);
}

}  // namespace bendcore
