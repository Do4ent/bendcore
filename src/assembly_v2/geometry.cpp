#include <bendcore/assembly_v2/geometry.hpp>

#include <cmath>

namespace bendcore::assembly_v2 {

namespace {
Vec3 normalizedOrZero(const Vec3& d) {
    const double n = d.norm();
    return n > 0.0 ? Vec3(d / n) : Vec3(Vec3::Zero());
}
}  // namespace

Transform Transform::fromAxisAngle(const Vec3& axis, double angle, const Vec3& t) {
    Transform tr;
    tr.position = t;
    tr.rotation = Quat(Eigen::AngleAxisd(angle, normalizedOrZero(axis)));
    return tr;
}

Transform Transform::inverse() const {
    Transform inv;
    inv.rotation = rotation.conjugate();
    inv.position = -(inv.rotation * position);
    return inv;
}

Transform Transform::operator*(const Transform& other) const {
    Transform r;
    r.rotation = (rotation * other.rotation).normalized();
    r.position = rotation * other.position + position;
    return r;
}

Quat quaternionFromRotationVector(const Vec3& omega) {
    const double angle = omega.norm();
    if (angle < 1e-8) {
        // Ряд Тейлора: q = (1 − θ²/8, ω/2·(1 − θ²/24)).
        const double a2 = angle * angle;
        Quat q(1.0 - a2 / 8.0, 0.0, 0.0, 0.0);
        q.vec() = omega * (0.5 - a2 / 48.0);
        return q.normalized();
    }
    const double half = 0.5 * angle;
    Quat q(std::cos(half), 0.0, 0.0, 0.0);
    q.vec() = omega * (std::sin(half) / angle);
    return q;
}

Transform applyIncrement(const Transform& pose, const Eigen::Matrix<double, 6, 1>& x) {
    Transform r;
    r.position = pose.position + x.head<3>();
    r.rotation = (quaternionFromRotationVector(x.tail<3>()) * pose.rotation).normalized();
    return r;
}

const char* toString(EntityType type) noexcept {
    switch (type) {
        case EntityType::Point: return "Point";
        case EntityType::Axis: return "Axis";
        case EntityType::Plane: return "Plane";
        case EntityType::Cylinder: return "Cylinder";
        case EntityType::Sphere: return "Sphere";
    }
    return "Unknown";
}

Entity Entity::makePoint(const Vec3& p) {
    Entity e;
    e.type = EntityType::Point;
    e.point = p;
    return e;
}

Entity Entity::makeAxis(const Vec3& p, const Vec3& d) {
    Entity e;
    e.type = EntityType::Axis;
    e.point = p;
    e.direction = normalizedOrZero(d);
    return e;
}

Entity Entity::makePlane(const Vec3& p, const Vec3& normal) {
    Entity e;
    e.type = EntityType::Plane;
    e.point = p;
    e.direction = normalizedOrZero(normal);
    return e;
}

Entity Entity::makeCylinder(const Vec3& p, const Vec3& axis, double radius) {
    Entity e;
    e.type = EntityType::Cylinder;
    e.point = p;
    e.direction = normalizedOrZero(axis);
    e.radius = radius;
    return e;
}

Entity Entity::makeSphere(const Vec3& center, double radius) {
    Entity e;
    e.type = EntityType::Sphere;
    e.point = center;
    e.radius = radius;
    return e;
}

void perpendicularBasis(const Vec3& d, Vec3& u, Vec3& v) {
    // Опорная ось — координатная ось с наименьшей проекцией на d (устойчиво для любого d).
    const Vec3 a = d.cwiseAbs();
    Vec3 ref;
    if (a.x() <= a.y() && a.x() <= a.z()) {
        ref = Vec3::UnitX();
    } else if (a.y() <= a.z()) {
        ref = Vec3::UnitY();
    } else {
        ref = Vec3::UnitZ();
    }
    u = (ref - d * d.dot(ref)).normalized();
    v = d.cross(u);
}

}  // namespace bendcore::assembly_v2
