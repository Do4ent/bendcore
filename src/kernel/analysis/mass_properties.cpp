#include <bendcore/kernel/mass_properties.hpp>

#include "../occt_failure.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Mat.hxx>
#include <math_Jacobi.hxx>
#include <math_Matrix.hxx>

namespace bendcore::kernel {

namespace {

constexpr double kMm3PerM3 = 1.0e9;

Matrix3 toMatrix3(const gp_Mat& m) {
    Matrix3 out{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) out[i][j] = m.Value(i + 1, j + 1);
    return out;
}

// I + m * (|d|^2 E - d d^T): parallel axis theorem, tensor convention.
void addSteiner(Matrix3& tensor, double mass, const Vector3& d) {
    const double d2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) tensor[i][j] += mass * ((i == j ? d2 : 0.0) - d[i] * d[j]);
}

Matrix3 rotate(const Matrix3& r, const Matrix3& tensor) {
    // r * tensor * r^T
    Matrix3 tmp{}, out{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k) tmp[i][j] += r[i][k] * tensor[k][j];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k) out[i][j] += tmp[i][k] * r[j][k];
    return out;
}

Result<MassProperties> solidProperties(const TopoDS_Shape& solid, double density,
                                       const MassPropertiesOptions& options, std::size_t index) {
    GProp_GProps volume;
    if (options.relativeTolerance > 0.0)
        BRepGProp::VolumeProperties(solid, volume, options.relativeTolerance, true);
    else
        BRepGProp::VolumeProperties(solid, volume, true);
    if (!(volume.Mass() > 0.0) || !std::isfinite(volume.Mass()))
        return Result<MassProperties>::failure(
            ErrorCode::InvalidShape, "solid #" + std::to_string(index) +
                                         " has non-positive volume (inside out or open shell)");
    GProp_GProps surface;
    if (options.relativeTolerance > 0.0)
        BRepGProp::SurfaceProperties(solid, surface, options.relativeTolerance, true);
    else
        BRepGProp::SurfaceProperties(solid, surface, true);

    const double k = density / kMm3PerM3;  // kg per mm^3
    MassProperties p;
    p.volume = volume.Mass();
    p.mass = p.volume * k;
    p.area = surface.Mass();
    const gp_Pnt c = volume.CentreOfMass();
    p.centerOfMass = {c.X(), c.Y(), c.Z()};
    p.inertiaAtCenter = toMatrix3(volume.MatrixOfInertia());
    for (auto& row : p.inertiaAtCenter)
        for (double& v : row) v *= k;
    p.bodyCount = 1;
    return p;
}

Result<MassProperties> finish(MassProperties p) {
    auto axes = principalAxes(p.inertiaAtCenter);
    if (!axes) return axes.error();
    p.principal = axes.value();
    return p;
}

}  // namespace

Result<PrincipalAxes> principalAxes(const Matrix3& tensor) {
    try {
        math_Matrix a(1, 3, 1, 3);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) a(i + 1, j + 1) = 0.5 * (tensor[i][j] + tensor[j][i]);
        math_Jacobi jacobi(a);
        if (!jacobi.IsDone())
            return Result<PrincipalAxes>::failure(ErrorCode::OperationFailed, "math_Jacobi did not converge");
        std::array<int, 3> order{1, 2, 3};
        std::sort(order.begin(), order.end(), [&](int x, int y) { return jacobi.Value(x) < jacobi.Value(y); });
        PrincipalAxes out;
        for (int i = 0; i < 3; ++i) {
            out.moments[i] = jacobi.Value(order[i]);
            Vector3 v{jacobi.Vectors()(1, order[i]), jacobi.Vectors()(2, order[i]), jacobi.Vectors()(3, order[i])};
            const double n = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            for (double& x : v) x /= n;
            out.axes[i] = v;
        }
        const Vector3& a0 = out.axes[0];
        const Vector3& a1 = out.axes[1];
        out.axes[2] = {a0[1] * a1[2] - a0[2] * a1[1], a0[2] * a1[0] - a0[0] * a1[2], a0[0] * a1[1] - a0[1] * a1[0]};
        return out;
    } catch (const Standard_Failure& failure) {
        return Result<PrincipalAxes>::failure(ErrorCode::OperationFailed, detail::failureText(failure));
    }
}

Result<MassProperties> combine(const std::vector<MassProperties>& parts) {
    if (parts.empty()) return Result<MassProperties>::failure(ErrorCode::InvalidInput, "nothing to combine");
    MassProperties total;
    Vector3 moment{0.0, 0.0, 0.0};
    for (const MassProperties& p : parts) {
        if (!(p.mass > 0.0))
            return Result<MassProperties>::failure(ErrorCode::InvalidInput, "part with non-positive mass");
        total.mass += p.mass;
        total.volume += p.volume;
        total.area += p.area;
        total.bodyCount += p.bodyCount;
        for (int i = 0; i < 3; ++i) moment[i] += p.mass * p.centerOfMass[i];
    }
    for (int i = 0; i < 3; ++i) total.centerOfMass[i] = moment[i] / total.mass;
    for (const MassProperties& p : parts) {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) total.inertiaAtCenter[i][j] += p.inertiaAtCenter[i][j];
        const Vector3 d{p.centerOfMass[0] - total.centerOfMass[0], p.centerOfMass[1] - total.centerOfMass[1],
                        p.centerOfMass[2] - total.centerOfMass[2]};
        addSteiner(total.inertiaAtCenter, p.mass, d);
    }
    return finish(total);
}

Result<MassProperties> computeMassProperties(const std::vector<Body>& bodies, const MassPropertiesOptions& options) {
    if (bodies.empty()) return Result<MassProperties>::failure(ErrorCode::InvalidInput, "no bodies");
    try {
        std::vector<MassProperties> parts;
        std::size_t index = 0;
        for (const Body& body : bodies) {
            if (body.shape.isNull())
                return Result<MassProperties>::failure(ErrorCode::InvalidInput, "body shape is null");
            if (!std::isfinite(body.densityKgPerM3) || body.densityKgPerM3 <= 0.0)
                return Result<MassProperties>::failure(ErrorCode::InvalidInput, "density must be positive");
            for (TopExp_Explorer it(body.shape.occt(), TopAbs_SOLID); it.More(); it.Next()) {
                auto p = solidProperties(it.Current(), body.densityKgPerM3, options, index++);
                if (!p) return p.error();
                parts.push_back(p.value());
            }
        }
        if (parts.empty())
            return Result<MassProperties>::failure(ErrorCode::InvalidInput, "shape contains no solids");
        return combine(parts);
    } catch (const Standard_Failure& failure) {
        return Result<MassProperties>::failure(ErrorCode::OperationFailed, detail::failureText(failure));
    }
}

Result<MassProperties> computeMassProperties(const Shape& shape, double densityKgPerM3,
                                             const MassPropertiesOptions& options) {
    return computeMassProperties(std::vector<Body>{Body{shape, densityKgPerM3}}, options);
}

Result<MassProperties> computeMassProperties(const Shape& shape, const Material& material,
                                             const MassPropertiesOptions& options) {
    return computeMassProperties(shape, material.density, options);
}

Result<MassProperties> transformed(const MassProperties& props, const gp_Trsf& placement) {
    const gp_Mat v = placement.VectorialPart();
    Matrix3 r = toMatrix3(v);
    // Orthonormal columns (rotation or mirror) are required: a scale changes the mass.
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double dot = 0.0;
            for (int k = 0; k < 3; ++k) dot += r[k][i] * r[k][j];
            if (std::abs(dot - (i == j ? 1.0 : 0.0)) > 1.0e-9)
                return Result<MassProperties>::failure(ErrorCode::InvalidInput,
                                                       "placement is not rigid (scales the part)");
        }
    MassProperties out = props;
    gp_Pnt c(props.centerOfMass[0], props.centerOfMass[1], props.centerOfMass[2]);
    c.Transform(placement);
    out.centerOfMass = {c.X(), c.Y(), c.Z()};
    out.inertiaAtCenter = rotate(r, props.inertiaAtCenter);
    return finish(out);
}

Result<MassProperties> combine(const std::vector<Occurrence>& occurrences) {
    if (occurrences.empty()) return Result<MassProperties>::failure(ErrorCode::InvalidInput, "no occurrences");
    std::vector<MassProperties> placed;
    placed.reserve(occurrences.size());
    for (const Occurrence& o : occurrences) {
        auto p = transformed(o.part, o.placement);
        if (!p) return p.error();
        placed.push_back(p.value());
    }
    return combine(placed);
}

Matrix3 inertiaAboutFrame(const MassProperties& props, const gp_Ax3& frame) {
    Matrix3 about = props.inertiaAtCenter;
    const gp_Pnt& o = frame.Location();
    const Vector3 d{props.centerOfMass[0] - o.X(), props.centerOfMass[1] - o.Y(), props.centerOfMass[2] - o.Z()};
    addSteiner(about, props.mass, d);
    // Rows of r are the frame axes in global coordinates: components = r * I * r^T.
    const gp_Dir axes[3] = {frame.XDirection(), frame.YDirection(), frame.Direction()};
    Matrix3 r{};
    for (int i = 0; i < 3; ++i) r[i] = {axes[i].X(), axes[i].Y(), axes[i].Z()};
    return rotate(r, about);
}

}  // namespace bendcore::kernel
