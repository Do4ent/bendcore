#pragma once
#include <vector>
#include "bendcore/geometry_kernel.hpp"

namespace bendcore::assembly {

enum class ConstraintType {
    COINCIDENT,   // Points/edges aligned
    DISTANCE,     // Fixed distance
    PARALLEL,     // Parallel geometry
    PERPENDICULAR,// 90 degree angle
    FIXED,        // Locked position
    SYMMETRIC,    // Symmetric about plane
    EQUAL,        // Same dimension
    TANGENT       // Tangent contact
};

struct AssemblyConstraint {
    ConstraintType type;
    std::vector<int> entity_ids;
    double value = 0.0;
};

class AssemblyConstraintSolver {
public:
    struct Result {
        bool success = false;
        std::vector<Transform3D> component_transforms;
    };

    static Result solve_constraints(
        const std::vector<AssemblyConstraint>& constraints,
        const std::vector<Solid>& components
    );

    static bool validate_constraint(const AssemblyConstraint& c);
};

}
