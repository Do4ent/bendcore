#include "bendcore/geometry/geometry_engine.hpp"
#include <cmath>

namespace BendCore::CadCore {

// Matrix4x4 implementation
Matrix4x4::Matrix4x4() noexcept {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            m[i][j] = 0.0;
        }
    }
}

Matrix4x4 Matrix4x4::identity() noexcept {
    Matrix4x4 result;
    for (int i = 0; i < 4; i++) {
        result.m[i][i] = 1.0;
    }
    return result;
}

Matrix4x4 Matrix4x4::translate(const Vector3D& offset) noexcept {
    Matrix4x4 result = identity();
    result.m[0][3] = offset.x;
    result.m[1][3] = offset.y;
    result.m[2][3] = offset.z;
    return result;
}

Matrix4x4 Matrix4x4::scale(const Vector3D& factors) noexcept {
    Matrix4x4 result = identity();
    result.m[0][0] = factors.x;
    result.m[1][1] = factors.y;
    result.m[2][2] = factors.z;
    return result;
}

Matrix4x4 Matrix4x4::rotateX(double radians) noexcept {
    Matrix4x4 result = identity();
    double c = std::cos(radians);
    double s = std::sin(radians);
    result.m[1][1] = c;
    result.m[1][2] = -s;
    result.m[2][1] = s;
    result.m[2][2] = c;
    return result;
}

Matrix4x4 Matrix4x4::rotateY(double radians) noexcept {
    Matrix4x4 result = identity();
    double c = std::cos(radians);
    double s = std::sin(radians);
    result.m[0][0] = c;
    result.m[0][2] = s;
    result.m[2][0] = -s;
    result.m[2][2] = c;
    return result;
}

Matrix4x4 Matrix4x4::rotateZ(double radians) noexcept {
    Matrix4x4 result = identity();
    double c = std::cos(radians);
    double s = std::sin(radians);
    result.m[0][0] = c;
    result.m[0][1] = -s;
    result.m[1][0] = s;
    result.m[1][1] = c;
    return result;
}

Matrix4x4 Matrix4x4::operator*(const Matrix4x4& other) const noexcept {
    Matrix4x4 result;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            result.m[i][j] = 0.0;
            for (int k = 0; k < 4; k++) {
                result.m[i][j] += m[i][k] * other.m[k][j];
            }
        }
    }
    return result;
}

Vector3D Matrix4x4::operator*(const Vector3D& vec) const noexcept {
    double w = m[3][0] * vec.x + m[3][1] * vec.y + m[3][2] * vec.z + m[3][3];
    if (std::abs(w) < 1e-10) w = 1.0;

    return Vector3D(
        (m[0][0] * vec.x + m[0][1] * vec.y + m[0][2] * vec.z + m[0][3]) / w,
        (m[1][0] * vec.x + m[1][1] * vec.y + m[1][2] * vec.z + m[1][3]) / w,
        (m[2][0] * vec.x + m[2][1] * vec.y + m[2][2] * vec.z + m[2][3]) / w
    );
}

double Matrix4x4::determinant() const noexcept {
    double det = 0.0;
    det += m[0][0] * (m[1][1] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
                      m[1][2] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) +
                      m[1][3] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]));
    det -= m[0][1] * (m[1][0] * (m[2][2] * m[3][3] - m[2][3] * m[3][2]) -
                      m[1][2] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
                      m[1][3] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]));
    det += m[0][2] * (m[1][0] * (m[2][1] * m[3][3] - m[2][3] * m[3][1]) -
                      m[1][1] * (m[2][0] * m[3][3] - m[2][3] * m[3][0]) +
                      m[1][3] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));
    det -= m[0][3] * (m[1][0] * (m[2][1] * m[3][2] - m[2][2] * m[3][1]) -
                      m[1][1] * (m[2][0] * m[3][2] - m[2][2] * m[3][0]) +
                      m[1][2] * (m[2][0] * m[3][1] - m[2][1] * m[3][0]));
    return det;
}

Matrix4x4 Matrix4x4::inverse() const noexcept {
    double det = determinant();
    if (std::abs(det) < 1e-10) return identity();

    Matrix4x4 result;
    // Simplified: just return identity for now, proper inverse computation is complex
    return result;
}

// Sphere implementation
Sphere::Sphere(const Vector3D& center, double radius) noexcept
    : center(center), radius(radius) {}

double Sphere::volume() const noexcept {
    return (4.0 / 3.0) * M_PI * radius * radius * radius;
}

double Sphere::surfaceArea() const noexcept {
    return 4.0 * M_PI * radius * radius;
}

Vector3D Sphere::centroid() const noexcept {
    return center;
}

// Box implementation
Box::Box(const Vector3D& min, const Vector3D& max) noexcept
    : minPoint(min), maxPoint(max) {}

double Box::volume() const noexcept {
    double dx = maxPoint.x - minPoint.x;
    double dy = maxPoint.y - minPoint.y;
    double dz = maxPoint.z - minPoint.z;
    return dx * dy * dz;
}

double Box::surfaceArea() const noexcept {
    double dx = maxPoint.x - minPoint.x;
    double dy = maxPoint.y - minPoint.y;
    double dz = maxPoint.z - minPoint.z;
    return 2.0 * (dx * dy + dy * dz + dz * dx);
}

Vector3D Box::centroid() const noexcept {
    return Vector3D(
        (minPoint.x + maxPoint.x) / 2.0,
        (minPoint.y + maxPoint.y) / 2.0,
        (minPoint.z + maxPoint.z) / 2.0
    );
}

// Cylinder implementation
Cylinder::Cylinder(const Vector3D& base, const Vector3D& axis, double radius, double height) noexcept
    : base(base), axis(axis), radius(radius), height(height) {}

double Cylinder::volume() const noexcept {
    return M_PI * radius * radius * height;
}

double Cylinder::surfaceArea() const noexcept {
    return 2.0 * M_PI * radius * height + 2.0 * M_PI * radius * radius;
}

Vector3D Cylinder::centroid() const noexcept {
    return base + axis * 0.5;
}

// GeometryEngine implementation
bool GeometryEngine::pointInBox(const Vector3D& point, const Vector3D& min, const Vector3D& max) noexcept {
    return point.x >= min.x && point.x <= max.x &&
           point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

bool GeometryEngine::pointInSphere(const Vector3D& point, const Vector3D& center, double radius) noexcept {
    Vector3D diff = point - center;
    return diff.magnitude() <= radius;
}

double GeometryEngine::distanceToLineSegment(const Vector3D& point,
                                             const Vector3D& lineStart,
                                             const Vector3D& lineEnd) noexcept {
    Vector3D v = lineEnd - lineStart;
    Vector3D w = point - lineStart;

    double c1 = w.dot(v);
    if (c1 <= 0) return w.magnitude();

    double c2 = v.dot(v);
    if (c1 >= c2) return (point - lineEnd).magnitude();

    double b = c1 / c2;
    Vector3D closest = lineStart + v * b;
    return (point - closest).magnitude();
}

bool GeometryEngine::spheresIntersect(const Vector3D& center1, double radius1,
                                      const Vector3D& center2, double radius2) noexcept {
    Vector3D diff = center1 - center2;
    double dist = diff.magnitude();
    return dist <= radius1 + radius2;
}

bool GeometryEngine::sphereBoxIntersect(const Vector3D& center, double radius,
                                        const Vector3D& boxMin, const Vector3D& boxMax) noexcept {
    Vector3D closest(
        std::max(boxMin.x, std::min(center.x, boxMax.x)),
        std::max(boxMin.y, std::min(center.y, boxMax.y)),
        std::max(boxMin.z, std::min(center.z, boxMax.z))
    );

    Vector3D diff = closest - center;
    return diff.magnitude() <= radius;
}

Vector3D GeometryEngine::transformPoint(const Vector3D& point, const Matrix4x4& transform) noexcept {
    return transform * point;
}

} // namespace BendCore::CadCore
