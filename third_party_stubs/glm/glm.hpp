#pragma once

#include <cmath>
#include <iostream>

namespace glm {

// Простой 3D вектор для замены glm::vec3
struct vec3 {
    float x, y, z;

    vec3() : x(0), y(0), z(0) {}
    vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    vec3 operator+(const vec3& other) const {
        return vec3(x + other.x, y + other.y, z + other.z);
    }

    vec3 operator-(const vec3& other) const {
        return vec3(x - other.x, y - other.y, z - other.z);
    }

    vec3 operator*(float scalar) const {
        return vec3(x * scalar, y * scalar, z * scalar);
    }

    float dot(const vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    float length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
};

// Матрица 4x4 (заглушка - минимальная функциональность)
struct mat4 {
    float data[16];

    mat4() {
        for (int i = 0; i < 16; ++i) {
            data[i] = (i % 5 == 0) ? 1.0f : 0.0f;
        }
    }
};

} // namespace glm
