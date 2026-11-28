#pragma once

#include <bendcore/assembly_v2/solver.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <random>

namespace bendcore::assembly_v2::testing {

constexpr double kPi = 3.14159265358979323846;

inline double deg(double d) { return d * kPi / 180.0; }

inline Vec3 randomUnit(std::mt19937& rng) {
    std::normal_distribution<double> n(0.0, 1.0);
    Vec3 v(n(rng), n(rng), n(rng));
    return v.normalized();
}

inline Vec3 randomVec(std::mt19937& rng, double half) {
    std::uniform_real_distribution<double> u(-half, half);
    return Vec3(u(rng), u(rng), u(rng));
}

inline Transform randomPose(std::mt19937& rng, double halfTranslation) {
    std::uniform_real_distribution<double> ang(-kPi, kPi);
    return Transform::fromAxisAngle(randomUnit(rng), ang(rng), randomVec(rng, halfTranslation));
}

// Возмущение: поворот на угол не больше maxAngle вокруг случайной оси и сдвиг не больше halfShift по осям.
inline Transform perturb(const Transform& pose, std::mt19937& rng, double halfShift, double maxAngle) {
    std::uniform_real_distribution<double> a(-maxAngle, maxAngle);
    Eigen::Matrix<double, 6, 1> x;
    x.head<3>() = randomVec(rng, halfShift);
    x.tail<3>() = randomUnit(rng) * a(rng);
    return applyIncrement(pose, x);
}

// Сущность, заданная в мировой СК, в локальной СК компонента с положением pose.
inline Entity toLocal(Entity world, const Transform& pose) {
    const Transform inv = pose.inverse();
    world.point = inv.applyToPoint(world.point);
    world.direction = inv.applyToDirection(world.direction);
    return world;
}

inline MateId addOk(Assembly& a, const Mate& m) {
    auto r = a.addMate(m);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    return r.ok() ? r.value() : 0;
}

inline double rotationDistance(const Transform& x, const Transform& y) {
    return (x.rotationMatrix() - y.rotationMatrix()).norm();
}

}  // namespace bendcore::assembly_v2::testing
