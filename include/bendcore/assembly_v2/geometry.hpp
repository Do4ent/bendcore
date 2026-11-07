#pragma once

// Геометрия решателя сопряжений: жёсткое преобразование компонента и сущности сопряжений
// (точка, ось, плоскость, цилиндр, сфера), заданные в локальной системе координат компонента.

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <cstdint>

namespace bendcore::assembly_v2 {

using Vec3 = Eigen::Vector3d;
using Mat3 = Eigen::Matrix3d;
using Quat = Eigen::Quaterniond;

using ComponentId = std::uint32_t;
using MateId = std::uint32_t;

// Жёсткое преобразование: world = rotation * local + position. rotation — единичный кватернион.
struct Transform {
    Vec3 position = Vec3::Zero();
    Quat rotation = Quat::Identity();

    static Transform identity() { return {}; }
    static Transform fromTranslation(const Vec3& t) { return {t, Quat::Identity()}; }
    // Поворот на angle (рад) вокруг оси axis (нормализуется), затем перенос t.
    static Transform fromAxisAngle(const Vec3& axis, double angle, const Vec3& t = Vec3::Zero());

    Vec3 applyToPoint(const Vec3& p) const { return rotation * p + position; }
    Vec3 applyToDirection(const Vec3& d) const { return rotation * d; }
    Mat3 rotationMatrix() const { return rotation.toRotationMatrix(); }

    Transform inverse() const;
    // (this * other)(x) = this(other(x)).
    Transform operator*(const Transform& other) const;
};

// Кватернион поворота exp(ω): поворот на |ω| вокруг ω/|ω|. Без особенностей при |ω| → 0.
Quat quaternionFromRotationVector(const Vec3& omega);

// Приращение позы в переменных решателя: t ← t + dt, R ← exp(ω)·R (поворот в мировой СК
// вокруг начала СК компонента). x = (dt, ω), 6 чисел.
Transform applyIncrement(const Transform& pose, const Eigen::Matrix<double, 6, 1>& x);

enum class EntityType { Point, Axis, Plane, Cylinder, Sphere };

const char* toString(EntityType type) noexcept;

// Геометрическая сущность в локальной СК компонента.
//  Point:    point
//  Axis:     point + direction (единичный)
//  Plane:    point + direction (единичная нормаль)
//  Cylinder: point + direction (ось) + radius
//  Sphere:   point (центр) + radius
struct Entity {
    EntityType type = EntityType::Point;
    Vec3 point = Vec3::Zero();
    Vec3 direction = Vec3::UnitZ();
    double radius = 0.0;

    static Entity makePoint(const Vec3& p);
    static Entity makeAxis(const Vec3& p, const Vec3& d);
    static Entity makePlane(const Vec3& p, const Vec3& normal);
    static Entity makeCylinder(const Vec3& p, const Vec3& axis, double radius);
    static Entity makeSphere(const Vec3& center, double radius);

    // Есть опорная точка, совпадающая с самой сущностью (точка, центр сферы).
    bool isPointLike() const { return type == EntityType::Point || type == EntityType::Sphere; }
    // Прямая: ось или ось цилиндра.
    bool isLineLike() const { return type == EntityType::Axis || type == EntityType::Cylinder; }
    // Есть направление: ось, нормаль плоскости, ось цилиндра.
    bool hasDirection() const { return isLineLike() || type == EntityType::Plane; }
};

// Сущность, привязанная к компоненту.
struct Feature {
    ComponentId component = 0;
    Entity entity;
};

// Детерминированный ортонормированный базис (u, v), перпендикулярный единичному d.
void perpendicularBasis(const Vec3& d, Vec3& u, Vec3& v);

// Кососимметричная матрица: skew(a) * b = a × b.
inline Mat3 skew(const Vec3& a) {
    Mat3 m;
    m << 0.0, -a.z(), a.y(), a.z(), 0.0, -a.x(), -a.y(), a.x(), 0.0;
    return m;
}

}  // namespace bendcore::assembly_v2
