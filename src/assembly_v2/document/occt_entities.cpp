#include <bendcore/assembly_v2/occt_entities.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>
#include <gp_Cone.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Sphere.hxx>

#include <string>

namespace bendcore::assembly_v2 {

using kernel::ErrorCode;
using kernel::Result;

namespace {

Vec3 toVec(const gp_XYZ& p) { return Vec3(p.X(), p.Y(), p.Z()); }

Result<Entity> unsupported(const std::string& what, EntityUse use) {
    return Result<Entity>::failure(ErrorCode::Unsupported,
                                   what + " cannot be used as a mate entity (use " + toString(use) + ")");
}

Vec3 faceCentroid(const TopoDS_Face& face) {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    return toVec(props.CentreOfMass().XYZ());
}

// Проекция точки на ось (p0, d).
Vec3 projectOnAxis(const Vec3& p, const Vec3& p0, const Vec3& d) { return p0 + d * d.dot(p - p0); }

Result<Entity> fromFace(const TopoDS_Face& face, EntityUse use) {
    BRepAdaptor_Surface surface(face);
    switch (surface.GetType()) {
        case GeomAbs_Plane: {
            if (use != EntityUse::Auto) return unsupported("planar face", use);
            const gp_Ax3 pos = surface.Plane().Position();
            gp_Dir n = pos.XDirection().Crossed(pos.YDirection());
            if (face.Orientation() == TopAbs_REVERSED) n.Reverse();
            return Entity::makePlane(faceCentroid(face), toVec(n.XYZ()));
        }
        case GeomAbs_Cylinder: {
            if (use == EntityUse::Point) return unsupported("cylindrical face", use);
            const gp_Cylinder c = surface.Cylinder();
            const Vec3 p0 = toVec(c.Location().XYZ());
            const Vec3 d = toVec(c.Axis().Direction().XYZ());
            const Vec3 p = projectOnAxis(faceCentroid(face), p0, d);
            if (use == EntityUse::Axis) return Entity::makeAxis(p, d);
            return Entity::makeCylinder(p, d, c.Radius());
        }
        case GeomAbs_Cone: {
            if (use == EntityUse::Point) return unsupported("conical face", use);
            const gp_Cone c = surface.Cone();
            const Vec3 p0 = toVec(c.Location().XYZ());
            const Vec3 d = toVec(c.Axis().Direction().XYZ());
            return Entity::makeAxis(projectOnAxis(faceCentroid(face), p0, d), d);
        }
        case GeomAbs_Sphere: {
            if (use == EntityUse::Axis) return unsupported("spherical face", use);
            const gp_Sphere s = surface.Sphere();
            const Vec3 c = toVec(s.Location().XYZ());
            if (use == EntityUse::Point) return Entity::makePoint(c);
            return Entity::makeSphere(c, s.Radius());
        }
        default: break;
    }
    std::string kind = "face with surface type ";
    switch (surface.GetType()) {
        case GeomAbs_Torus: kind += "Torus"; break;
        case GeomAbs_BezierSurface: kind += "Bezier"; break;
        case GeomAbs_BSplineSurface: kind += "BSpline"; break;
        case GeomAbs_SurfaceOfRevolution: kind += "SurfaceOfRevolution"; break;
        case GeomAbs_SurfaceOfExtrusion: kind += "SurfaceOfExtrusion"; break;
        case GeomAbs_OffsetSurface: kind += "Offset"; break;
        default: kind += "Other"; break;
    }
    return unsupported(kind, use);
}

Result<Entity> fromEdge(const TopoDS_Edge& edge, EntityUse use) {
    if (BRep_Tool::Degenerated(edge)) return unsupported("degenerated edge", use);
    BRepAdaptor_Curve curve(edge);
    switch (curve.GetType()) {
        case GeomAbs_Line: {
            if (use == EntityUse::Point) return unsupported("straight edge", use);
            const gp_Lin l = curve.Line();
            const double mid = 0.5 * (curve.FirstParameter() + curve.LastParameter());
            return Entity::makeAxis(toVec(curve.Value(mid).XYZ()), toVec(l.Direction().XYZ()));
        }
        case GeomAbs_Circle: {
            const gp_Circ c = curve.Circle();
            const Vec3 center = toVec(c.Location().XYZ());
            if (use == EntityUse::Point) return Entity::makePoint(center);
            return Entity::makeAxis(center, toVec(c.Axis().Direction().XYZ()));
        }
        case GeomAbs_Ellipse: return unsupported("elliptical edge", use);
        case GeomAbs_BSplineCurve: return unsupported("BSpline edge", use);
        case GeomAbs_BezierCurve: return unsupported("Bezier edge", use);
        default: return unsupported("edge with curve type Other", use);
    }
}

}  // namespace

const char* toString(EntityUse use) noexcept {
    switch (use) {
        case EntityUse::Auto: return "Auto";
        case EntityUse::Axis: return "Axis";
        case EntityUse::Point: return "Point";
    }
    return "Unknown";
}

Result<Entity> entityFromElement(const TopoDS_Shape& element, EntityUse use) {
    if (element.IsNull()) return Result<Entity>::failure(ErrorCode::InvalidInput, "element is null");
    try {
        switch (element.ShapeType()) {
            case TopAbs_FACE: return fromFace(TopoDS::Face(element), use);
            case TopAbs_EDGE: return fromEdge(TopoDS::Edge(element), use);
            case TopAbs_VERTEX:
                if (use == EntityUse::Axis) return unsupported("vertex", use);
                return Entity::makePoint(toVec(BRep_Tool::Pnt(TopoDS::Vertex(element)).XYZ()));
            default:
                return Result<Entity>::failure(ErrorCode::InvalidInput,
                                               "mate element must be a face, an edge or a vertex");
        }
    } catch (const Standard_Failure& e) {
        return Result<Entity>::failure(ErrorCode::OperationFailed,
                                       std::string("OCCT exception while reading mate geometry: ") + e.what());
    }
}

std::size_t elementCount(const kernel::TopologyIndex& topology, ElementKind kind) {
    switch (kind) {
        case ElementKind::Face: return topology.faces().size();
        case ElementKind::Edge: return topology.edges().size();
        case ElementKind::Vertex: return topology.vertices().size();
    }
    return 0;
}

TopoDS_Shape elementAt(const kernel::TopologyIndex& topology, ElementKind kind, std::uint32_t index) {
    if (index >= elementCount(topology, kind)) return {};
    switch (kind) {
        case ElementKind::Face: return topology.faces()[index].face;
        case ElementKind::Edge: return topology.edges()[index].edge;
        case ElementKind::Vertex: return topology.vertices()[index].vertex;
    }
    return {};
}

Result<Entity> entityFromIndex(const kernel::TopologyIndex& topology, ElementKind kind, std::uint32_t index,
                               EntityUse use) {
    if (index >= elementCount(topology, kind))
        return Result<Entity>::failure(ErrorCode::InvalidInput,
                                       std::string(modeling::toString(kind)) + " index " + std::to_string(index) +
                                           " is out of range (" + std::to_string(elementCount(topology, kind)) +
                                           " elements)");
    return entityFromElement(elementAt(topology, kind, index), use);
}

Entity transformEntity(const Entity& entity, const Transform& transform) {
    Entity out = entity;
    out.point = transform.applyToPoint(entity.point);
    out.direction = transform.applyToDirection(entity.direction).normalized();
    return out;
}

}  // namespace bendcore::assembly_v2
