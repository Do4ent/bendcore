#pragma once

#include "../glm.hpp"

namespace glm {

// Простые функции для трансформации матриц (заглушки)
inline mat4 translate(const mat4& m, const vec3& v) {
    mat4 result = m;
    result.data[12] = v.x;
    result.data[13] = v.y;
    result.data[14] = v.z;
    return result;
}

inline mat4 rotate(const mat4& m, float angle, const vec3& axis) {
    return m;  // Заглушка
}

inline mat4 scale(const mat4& m, const vec3& s) {
    mat4 result = m;
    result.data[0] *= s.x;
    result.data[5] *= s.y;
    result.data[10] *= s.z;
    return result;
}

} // namespace glm
