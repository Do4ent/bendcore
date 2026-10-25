#pragma once

#include "vector.hpp"
#include <memory>

namespace BendCore::CadCore {

/**
 * @brief Matrix for 3D transformations
 */
class Matrix4x4 {
public:
    Matrix4x4() noexcept;

    static Matrix4x4 identity() noexcept;
    static Matrix4x4 translate(const Vector3D& offset) noexcept;
    static Matrix4x4 scale(const Vector3D& factors) noexcept;
    static Matrix4x4 rotateX(double radians) noexcept;
    static Matrix4x4 rotateY(double radians) noexcept;
    static Matrix4x4 rotateZ(double radians) noexcept;

    Matrix4x4 operator*(const Matrix4x4& other) const noexcept;
    Vector3D operator*(const Vector3D& vec) const noexcept;

    Matrix4x4 inverse() const noexcept;
    double determinant() const noexcept;

    double m[4][4];
};

/**
 * @brief Base class for geometric primitives
 */
class GeometricPrimitive {
public:
    virtual ~GeometricPrimitive() = default;

    virtual double volume() const noexcept = 0;
    virtual double surfaceArea() const noexcept = 0;
    virtual Vector3D centroid() const noexcept = 0;
};

/**
 * @brief Sphere primitive
 */
class Sphere : public GeometricPrimitive {
public:
    Sphere(const Vector3D& center, double radius) noexcept;

    double volume() const noexcept override;
    double surfaceArea() const noexcept override;
    Vector3D centroid() const noexcept override;

    Vector3D center;
    double radius;
};

/**
 * @brief Box/Cuboid primitive
 */
class Box : public GeometricPrimitive {
public:
    Box(const Vector3D& min, const Vector3D& max) noexcept;

    double volume() const noexcept override;
    double surfaceArea() const noexcept override;
    Vector3D centroid() const noexcept override;

    Vector3D minPoint, maxPoint;
};

/**
 * @brief Cylinder primitive
 */
class Cylinder : public GeometricPrimitive {
public:
    Cylinder(const Vector3D& base, const Vector3D& axis, double radius, double height) noexcept;

    double volume() const noexcept override;
    double surfaceArea() const noexcept override;
    Vector3D centroid() const noexcept override;

    Vector3D base, axis;
    double radius, height;
};

/**
 * @brief Geometry engine for primitives and transformations
 */
class GeometryEngine {
public:
    GeometryEngine() noexcept = default;

    /**
     * @brief Check if point is inside axis-aligned bounding box
     */
    static bool pointInBox(const Vector3D& point, const Vector3D& min, const Vector3D& max) noexcept;

    /**
     * @brief Check if point is inside sphere
     */
    static bool pointInSphere(const Vector3D& point, const Vector3D& center, double radius) noexcept;

    /**
     * @brief Distance from point to line segment
     */
    static double distanceToLineSegment(const Vector3D& point,
                                       const Vector3D& lineStart,
                                       const Vector3D& lineEnd) noexcept;

    /**
     * @brief Check if two spheres intersect
     */
    static bool spheresIntersect(const Vector3D& center1, double radius1,
                                 const Vector3D& center2, double radius2) noexcept;

    /**
     * @brief Check if sphere and box intersect
     */
    static bool sphereBoxIntersect(const Vector3D& center, double radius,
                                   const Vector3D& boxMin, const Vector3D& boxMax) noexcept;

    /**
     * @brief Apply transformation to point
     */
    static Vector3D transformPoint(const Vector3D& point, const Matrix4x4& transform) noexcept;
};

} // namespace BendCore::CadCore
