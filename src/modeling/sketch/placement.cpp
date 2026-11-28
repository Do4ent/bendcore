#include "sketch_internal.hpp"

#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_Orientation.hxx>
#include <gp_Vec.hxx>

namespace bendcore::modeling {

namespace {

constexpr double kParallelEps = 1e-9;

}  // namespace

Result<SketchPlacement> SketchPlacement::fromAxes(const gp_Pnt& origin, const gp_Dir& normal,
                                                  const gp_Dir& xDirection) {
    const gp_Vec n(normal);
    const gp_Vec x(xDirection);
    const gp_Vec inPlane = x - n * x.Dot(n);
    if (inPlane.Magnitude() <= kParallelEps) {
        return Result<SketchPlacement>::failure(
            ErrorCode::InvalidInput, "sketch X direction is parallel to the sketch normal");
    }
    return SketchPlacement(gp_Ax3(origin, normal, gp_Dir(inPlane)));
}

Result<SketchPlacement> SketchPlacement::fromFrame(const gp_Ax3& frame) {
    if (!frame.Direct()) {
        return Result<SketchPlacement>::failure(ErrorCode::InvalidInput,
                                                "sketch frame is left-handed");
    }
    return SketchPlacement(frame);
}

SketchPlacement SketchPlacement::fromPlane(const gp_Pln& plane) {
    gp_Ax3 frame = plane.Position();
    if (!frame.Direct()) frame.YReverse();
    return SketchPlacement(frame);
}

Result<SketchPlacement> SketchPlacement::onFace(const TopoDS_Face& face,
                                                const FacePlacementOptions& options) {
    if (face.IsNull()) {
        return Result<SketchPlacement>::failure(ErrorCode::InvalidInput, "face is null");
    }
    try {
        if (BRep_Tool::Surface(face).IsNull()) {
            return Result<SketchPlacement>::failure(ErrorCode::InvalidInput,
                                                    "face has no surface");
        }
        const BRepAdaptor_Surface surface(face, false);
        if (surface.GetType() != GeomAbs_Plane) {
            return Result<SketchPlacement>::failure(ErrorCode::InvalidInput,
                                                    "sketch face is not planar");
        }
        const gp_Pln plane = surface.Plane();
        const gp_Ax3& position = plane.Position();
        // Surface normal is Du x Dv = X x Y (opposite to Direction() for a left-handed frame);
        // the face normal is reversed for a reversed face.
        gp_Dir normal = position.XDirection().Crossed(position.YDirection());
        if (face.Orientation() == TopAbs_REVERSED) normal.Reverse();

        gp_Pnt origin = plane.Location();
        if (options.origin) {
            const gp_Vec v(plane.Location(), *options.origin);
            origin = options.origin->Translated(-gp_Vec(normal) * v.Dot(gp_Vec(normal)));
        }
        const gp_Dir x = options.xDirection ? *options.xDirection : position.XDirection();
        return fromAxes(origin, normal, x);
    } catch (const Standard_Failure& failure) {
        return Result<SketchPlacement>::failure(ErrorCode::OperationFailed,
                                                detail::failureText(failure));
    }
}

Result<SketchPlacement> SketchPlacement::onFace(const kernel::TopologyIndex& topology,
                                                std::uint32_t faceIndex,
                                                const FacePlacementOptions& options) {
    if (faceIndex >= topology.faces().size()) {
        return Result<SketchPlacement>::failure(
            ErrorCode::InvalidInput, "face index " + std::to_string(faceIndex) + " out of range (" +
                                         std::to_string(topology.faces().size()) + " faces)");
    }
    return onFace(topology.faces()[faceIndex].face, options);
}

gp_Pnt SketchPlacement::toWorld(const Point2& p) const {
    return frame_.Location()
        .Translated(gp_Vec(frame_.XDirection()) * p.x + gp_Vec(frame_.YDirection()) * p.y);
}

Point2 SketchPlacement::toLocal(const gp_Pnt& p) const {
    const gp_Vec v(frame_.Location(), p);
    return Point2{v.Dot(gp_Vec(frame_.XDirection())), v.Dot(gp_Vec(frame_.YDirection()))};
}

double SketchPlacement::signedDistance(const gp_Pnt& p) const {
    return gp_Vec(frame_.Location(), p).Dot(gp_Vec(frame_.Direction()));
}

gp_Trsf SketchPlacement::localToWorld() const {
    gp_Trsf worldToLocal;
    worldToLocal.SetTransformation(frame_);
    return worldToLocal.Inverted();
}

}  // namespace bendcore::modeling
