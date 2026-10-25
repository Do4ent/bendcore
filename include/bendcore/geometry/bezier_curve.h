#pragma once

#include <vector>
#include <array>
#include <cmath>

namespace bendcore {

struct Vector3D {
    double x, y, z;

    Vector3D() : x(0), y(0), z(0) {}
    Vector3D(double x, double y, double z) : x(x), y(y), z(z) {}

    Vector3D operator+(const Vector3D& v) const {
        return Vector3D(x + v.x, y + v.y, z + v.z);
    }

    Vector3D operator-(const Vector3D& v) const {
        return Vector3D(x - v.x, y - v.y, z - v.z);
    }

    Vector3D operator*(double t) const {
        return Vector3D(x * t, y * t, z * t);
    }

    double dot(const Vector3D& v) const {
        return x * v.x + y * v.y + z * v.z;
    }

    Vector3D cross(const Vector3D& v) const {
        return Vector3D(
            y * v.z - z * v.y,
            z * v.x - x * v.z,
            x * v.y - y * v.x
        );
    }

    double length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vector3D normalize() const {
        double len = length();
        if (len == 0) return Vector3D();
        return Vector3D(x / len, y / len, z / len);
    }
};

class BezierCurve {
public:
    BezierCurve(const std::vector<Vector3D>& controlPoints)
        : controlPoints(controlPoints) {}

    Vector3D evaluate(double t) const;
    Vector3D derivative(double t) const;
    std::vector<Vector3D> discretize(int segments) const;
    double length(int segments = 100) const;

private:
    std::vector<Vector3D> controlPoints;

    double bernstein(int i, int n, double t) const;
};

class NURBSCurve {
public:
    NURBSCurve(const std::vector<Vector3D>& controlPoints,
               const std::vector<double>& weights,
               const std::vector<double>& knots,
               int degree)
        : controlPoints(controlPoints), weights(weights),
          knots(knots), degree(degree) {}

    Vector3D evaluate(double u) const;
    std::vector<Vector3D> discretize(int segments) const;

private:
    std::vector<Vector3D> controlPoints;
    std::vector<double> weights;
    std::vector<double> knots;
    int degree;

    double basisFunction(int i, int p, double u) const;
};

} // namespace bendcore
