#pragma once

// Mass properties of solids (BRepGProp): mass, volume, area, centre of mass, inertia tensor
// about the centre of mass and about any frame, principal moments and axes; totals for a
// multi-body part and for an assembly of placed occurrences (docs/kernel/ANALYSIS.md).
//
// Units: lengths mm, density kg/m^3, mass kg, inertia kg*mm^2.
// Inertia tensor convention: diagonal Ixx = integral of (y^2 + z^2) dm, off-diagonal
// Ixy = -integral of x*y dm, so that angular momentum L = I * omega.

#include <bendcore/kernel/material.hpp>
#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>

#include <array>
#include <vector>

#include <gp_Ax3.hxx>
#include <gp_Trsf.hxx>

namespace bendcore::kernel {

using Vector3 = std::array<double, 3>;
using Matrix3 = std::array<std::array<double, 3>, 3>;

struct PrincipalAxes {
    Vector3 moments{};              // ascending, kg*mm^2
    std::array<Vector3, 3> axes{};  // unit vectors, axes[i] belongs to moments[i], right-handed
};

struct MassProperties {
    double mass = 0.0;      // kg
    double volume = 0.0;    // mm^3
    double area = 0.0;      // mm^2, sum of the boundary areas of the bodies
    Point3 centerOfMass{};  // mm, global coordinates
    Matrix3 inertiaAtCenter{};  // kg*mm^2, axes parallel to the global axes
    PrincipalAxes principal;    // of inertiaAtCenter
    std::size_t bodyCount = 0;
};

struct MassPropertiesOptions {
    // 0: BRepGProp's default Gauss integration (exact for planes and quadrics within
    // floating point). > 0: adaptive integration with this relative error bound, for
    // freeform (B-spline) bodies.
    double relativeTolerance = 0.0;
};

// Every solid of `shape` (a solid, compsolid or compound of solids) with one density.
// Overlapping solids are counted twice: unite them first (boolean.hpp) for a single body.
// Errors: InvalidInput (null shape, no solids, density <= 0), InvalidShape (a solid with
// non-positive volume, i.e. inside out or open), OperationFailed (OCCT exception).
Result<MassProperties> computeMassProperties(const Shape& shape, double densityKgPerM3,
                                             const MassPropertiesOptions& options = {});
Result<MassProperties> computeMassProperties(const Shape& shape, const Material& material,
                                             const MassPropertiesOptions& options = {});

struct Body {
    Shape shape;
    double densityKgPerM3 = 0.0;
};

// Bodies of different materials (multi-body part).
Result<MassProperties> computeMassProperties(const std::vector<Body>& bodies,
                                             const MassPropertiesOptions& options = {});

// Rigid placement of already computed properties: centre transformed, tensor rotated.
// InvalidInput when the transformation scales or mirrors.
Result<MassProperties> transformed(const MassProperties& props, const gp_Trsf& placement);

struct Occurrence {
    MassProperties part;  // in the part's own coordinates
    gp_Trsf placement;    // part -> assembly
};

// Sum over occurrences with the parallel axis (Steiner) theorem.
Result<MassProperties> combine(const std::vector<Occurrence>& occurrences);
Result<MassProperties> combine(const std::vector<MassProperties>& parts);

// Inertia tensor about the origin of `frame`, components in the frame's axes.
Matrix3 inertiaAboutFrame(const MassProperties& props, const gp_Ax3& frame);

// Eigen decomposition of a symmetric tensor (math_Jacobi).
Result<PrincipalAxes> principalAxes(const Matrix3& tensor);

}  // namespace bendcore::kernel
