#pragma once

#include <cmath>
#include <array>

namespace BendCore::CadCore {

/**
 * @brief 3D vector operations
 */
class Vector3D {
public:
    Vector3D() noexcept : x(0.0), y(0.0), z(0.0) {}
    Vector3D(double x, double y, double z) noexcept : x(x), y(y), z(z) {}

    double x, y, z;

    double magnitude() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vector3D normalized() const noexcept {
        double mag = magnitude();
        if (mag < 1e-10) return Vector3D(0, 0, 0);
        return Vector3D(x / mag, y / mag, z / mag);
    }

    Vector3D operator+(const Vector3D& other) const noexcept {
        return Vector3D(x + other.x, y + other.y, z + other.z);
    }

    Vector3D operator-(const Vector3D& other) const noexcept {
        return Vector3D(x - other.x, y - other.y, z - other.z);
    }

    Vector3D operator*(double scalar) const noexcept {
        return Vector3D(x * scalar, y * scalar, z * scalar);
    }

    double dot(const Vector3D& other) const noexcept {
        return x * other.x + y * other.y + z * other.z;
    }

    Vector3D cross(const Vector3D& other) const noexcept {
        return Vector3D(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }

    bool operator==(const Vector3D& other) const noexcept {
        return std::abs(x - other.x) < 1e-10 &&
               std::abs(y - other.y) < 1e-10 &&
               std::abs(z - other.z) < 1e-10;
    }
};

/**
 * @brief 2D vector operations
 */
class Vector2D {
public:
    Vector2D() noexcept : x(0.0), y(0.0) {}
    Vector2D(double x, double y) noexcept : x(x), y(y) {}

    double x, y;

    double magnitude() const noexcept {
        return std::sqrt(x * x + y * y);
    }

    Vector2D normalized() const noexcept {
        double mag = magnitude();
        if (mag < 1e-10) return Vector2D(0, 0);
        return Vector2D(x / mag, y / mag);
    }

    double dot(const Vector2D& other) const noexcept {
        return x * other.x + y * other.y;
    }

    Vector2D operator+(const Vector2D& other) const noexcept {
        return Vector2D(x + other.x, y + other.y);
    }

    Vector2D operator-(const Vector2D& other) const noexcept {
        return Vector2D(x - other.x, y - other.y);
    }

    Vector2D operator*(double scalar) const noexcept {
        return Vector2D(x * scalar, y * scalar);
    }

    bool operator==(const Vector2D& other) const noexcept {
        return std::abs(x - other.x) < 1e-10 &&
               std::abs(y - other.y) < 1e-10;
    }
};

} // namespace BendCore::CadCore
