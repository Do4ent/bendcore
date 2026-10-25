#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <array>
#include <cmath>

namespace bendcore::assembly {

struct Vector3 {
    double x, y, z;
    Vector3(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    double length() const { return std::sqrt(x*x + y*y + z*z); }
    Vector3 normalized() const {
        double len = length();
        return len > 0 ? Vector3(x/len, y/len, z/len) : Vector3(0, 0, 0);
    }
};

enum class ConstraintType {
    COINCIDENT,      // Points/edges coincide
    PARALLEL,        // Edges/faces are parallel
    PERPENDICULAR,   // Edges/faces are perpendicular
    TANGENT,         // Surfaces are tangent
    DISTANCE,        // Fixed distance between elements
    ANGLE,           // Fixed angle between elements
    CONCENTRIC,      // Circles/cylinders share axis
    SYMMETRIC,       // Symmetric about a plane
    HORIZONTAL,      // Horizontal orientation
    VERTICAL         // Vertical orientation
};

struct ConstraintTarget {
    std::string entityId1;
    std::string entityId2;
    std::optional<double> value;  // For distance/angle constraints
};

class Constraint {
public:
    Constraint(ConstraintType type, const ConstraintTarget& target)
        : type_(type), target_(target), isActive_(true) {}

    ConstraintType getType() const { return type_; }
    const ConstraintTarget& getTarget() const { return target_; }
    bool isActive() const { return isActive_; }
    void setActive(bool active) { isActive_ = active; }

    bool satisfies(double tolerance = 0.01) const;
    std::string getDescription() const;

private:
    ConstraintType type_;
    ConstraintTarget target_;
    bool isActive_;
};

class ConstraintSystem {
public:
    ConstraintSystem() = default;

    void addConstraint(const Constraint& constraint);
    void removeConstraint(const std::string& constraintId);
    void clearAllConstraints();

    const std::vector<Constraint>& getConstraints() const { return constraints_; }
    std::vector<Constraint> getConstraintsByType(ConstraintType type) const;
    std::vector<Constraint> getConstraintsByEntity(const std::string& entityId) const;

    bool resolveConstraints(const std::vector<Vector3>& initialPositions,
                          std::vector<Vector3>& resolvedPositions);

    bool validateAllConstraints(double tolerance = 0.01) const;
    std::vector<std::string> getConflictingConstraints() const;

private:
    std::vector<Constraint> constraints_;

    bool solveIterative(std::vector<Vector3>& positions,
                       unsigned int iterations = 100);
};

} // namespace bendcore::assembly
