// Geometric signatures of faces, edges and vertices (docs/modeling/TOPOLOGICAL_NAMING.md).

#include <bendcore/modeling/naming.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>
#include <gp_Circ.hxx>
#include <gp_Cone.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Elips.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <gp_Sphere.hxx>
#include <gp_Torus.hxx>

#include <algorithm>
#include <cmath>
#include <limits>

namespace bendcore::modeling {

namespace {

kernel::Point3 toPoint(const gp_XYZ& p) { return {p.X(), p.Y(), p.Z()}; }

// Axes and lines have no meaningful sense: the first non-negligible component is made positive.
kernel::Point3 canonicalAxis(const gp_Dir& d) {
    kernel::Point3 v{d.X(), d.Y(), d.Z()};
    for (double c : v) {
        if (std::abs(c) > 1e-9) {
            if (c < 0) {
                for (double& x : v) x = -x;
            }
            break;
        }
    }
    return v;
}

void faceGeometry(const TopoDS_Face& face, GeometricSignature& sig) {
    BRepAdaptor_Surface surface(face, false);
    switch (surface.GetType()) {
        case GeomAbs_Plane: {
            sig.geometry = "Plane";
            gp_Dir n = surface.Plane().Axis().Direction();
            // For a left-handed plane system the parametric normal Du x Dv is opposite to the axis.
            if (!surface.Plane().Position().Direct()) n.Reverse();
            if (face.Orientation() == TopAbs_REVERSED) n.Reverse();
            sig.hasDirection = true;
            sig.direction = {n.X(), n.Y(), n.Z()};
            break;
        }
        case GeomAbs_Cylinder:
            sig.geometry = "Cylinder";
            sig.hasDirection = true;
            sig.direction = canonicalAxis(surface.Cylinder().Axis().Direction());
            sig.radius = surface.Cylinder().Radius();
            break;
        case GeomAbs_Cone:
            sig.geometry = "Cone";
            sig.hasDirection = true;
            sig.direction = canonicalAxis(surface.Cone().Axis().Direction());
            sig.radius = surface.Cone().RefRadius();
            break;
        case GeomAbs_Sphere:
            sig.geometry = "Sphere";
            sig.radius = surface.Sphere().Radius();
            break;
        case GeomAbs_Torus:
            sig.geometry = "Torus";
            sig.hasDirection = true;
            sig.direction = canonicalAxis(surface.Torus().Axis().Direction());
            sig.radius = surface.Torus().MajorRadius();
            break;
        case GeomAbs_BSplineSurface:
            sig.geometry = "BSpline";
            break;
        default:
            sig.geometry = "Other";
            break;
    }
}

void edgeGeometry(const TopoDS_Edge& edge, GeometricSignature& sig) {
    if (BRep_Tool::Degenerated(edge)) {
        sig.geometry = "Degenerated";
        return;
    }
    BRepAdaptor_Curve curve(edge);
    switch (curve.GetType()) {
        case GeomAbs_Line:
            sig.geometry = "Line";
            sig.hasDirection = true;
            sig.direction = canonicalAxis(curve.Line().Direction());
            break;
        case GeomAbs_Circle:
            sig.geometry = "Circle";
            sig.hasDirection = true;
            sig.direction = canonicalAxis(curve.Circle().Axis().Direction());
            sig.radius = curve.Circle().Radius();
            break;
        case GeomAbs_Ellipse:
            sig.geometry = "Ellipse";
            sig.hasDirection = true;
            sig.direction = canonicalAxis(curve.Ellipse().Axis().Direction());
            sig.radius = curve.Ellipse().MajorRadius();
            break;
        case GeomAbs_BSplineCurve:
            sig.geometry = "BSpline";
            break;
        default:
            sig.geometry = "Other";
            break;
    }
}

}  // namespace

Result<GeometricSignature> computeSignature(const TopoDS_Shape& element) {
    if (element.IsNull()) return Result<GeometricSignature>::failure(ErrorCode::InvalidInput, "element is null");
    auto kind = elementKindOf(element);
    if (!kind) {
        return Result<GeometricSignature>::failure(ErrorCode::InvalidInput,
                                                   "element is not a face, edge or vertex");
    }
    try {
        GeometricSignature sig;
        sig.valid = true;
        sig.kind = *kind;
        switch (*kind) {
            case ElementKind::Face: {
                const TopoDS_Face& face = TopoDS::Face(element);
                GProp_GProps props;
                BRepGProp::SurfaceProperties(face, props);
                sig.size = std::abs(props.Mass());
                sig.center = toPoint(props.CentreOfMass().XYZ());
                faceGeometry(face, sig);
                break;
            }
            case ElementKind::Edge: {
                const TopoDS_Edge& edge = TopoDS::Edge(element);
                edgeGeometry(edge, sig);
                if (sig.geometry == "Degenerated") {
                    sig.size = 0.0;
                    // A degenerated edge has no 3D curve; its position is that of its vertex.
                    for (TopoDS_Iterator it(edge); it.More(); it.Next()) {
                        if (it.Value().ShapeType() == TopAbs_VERTEX) {
                            sig.center = toPoint(BRep_Tool::Pnt(TopoDS::Vertex(it.Value())).XYZ());
                            break;
                        }
                    }
                } else {
                    GProp_GProps props;
                    BRepGProp::LinearProperties(edge, props);
                    sig.size = std::abs(props.Mass());
                    sig.center = toPoint(props.CentreOfMass().XYZ());
                }
                break;
            }
            case ElementKind::Vertex:
                sig.geometry = "Point";
                sig.center = toPoint(BRep_Tool::Pnt(TopoDS::Vertex(element)).XYZ());
                break;
        }
        return sig;
    } catch (const Standard_Failure& failure) {
        return Result<GeometricSignature>::failure(
            ErrorCode::OperationFailed,
            std::string("signature computation failed: ") + failure.what());
    }
}

double signatureDistance(const GeometricSignature& a, const GeometricSignature& b, double scale) {
    constexpr double kInf = std::numeric_limits<double>::infinity();
    if (!a.valid || !b.valid || a.kind != b.kind || a.geometry != b.geometry) return kInf;
    if (!(scale > 0)) scale = 1.0;
    double score = 0.0;
    if (a.hasDirection != b.hasDirection) return kInf;
    if (a.hasDirection) {
        double dot = a.direction[0] * b.direction[0] + a.direction[1] * b.direction[1] +
                     a.direction[2] * b.direction[2];
        // Plane normals are oriented; axes and line directions are canonical, so the sign of
        // the dot product is meaningful in both cases.
        if (dot < 0.995) return kInf;
        score += (1.0 - dot) * 10.0;
    }
    double dx = a.center[0] - b.center[0], dy = a.center[1] - b.center[1], dz = a.center[2] - b.center[2];
    score += std::sqrt(dx * dx + dy * dy + dz * dz) / scale;
    double rmax = std::max(a.radius, b.radius);
    if (rmax > 0) score += std::abs(a.radius - b.radius) / rmax;
    if (a.size > 0 && b.size > 0) {
        score += 0.25 * std::abs(std::log(a.size / b.size));
    } else if (a.size != b.size) {
        score += 1.0;
    }
    return score;
}

}  // namespace bendcore::modeling
