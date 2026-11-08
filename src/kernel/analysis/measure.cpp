#include <bendcore/kernel/measure.hpp>

#include "../occt_failure.hpp"

#include <cmath>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>
#include <gp_Cone.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Sphere.hxx>
#include <gp_Torus.hxx>

namespace bendcore::kernel {

namespace {

constexpr double kPi = 3.14159265358979323846;

Point3 toPoint(const gp_Pnt& p) { return {p.X(), p.Y(), p.Z()}; }
Point3 toPoint(const gp_XYZ& p) { return {p.X(), p.Y(), p.Z()}; }

template <typename T>
Result<T> guard(auto&& body) {
    try {
        return body();
    } catch (const Standard_Failure& failure) {
        return Result<T>::failure(ErrorCode::OperationFailed, detail::failureText(failure));
    }
}

// Outward normal of a planar face (orientation of the face in its shell applied).
gp_Dir planarNormal(const TopoDS_Face& face, const BRepAdaptor_Surface& surface) {
    double u1, u2, v1, v2;
    BRepTools::UVBounds(face, u1, u2, v1, v2);
    gp_Pnt p;
    gp_Vec du, dv;
    surface.D1(0.5 * (u1 + u2), 0.5 * (v1 + v2), p, du, dv);
    gp_Dir n(du.Crossed(dv));
    if (face.Orientation() == TopAbs_REVERSED) n.Reverse();
    return n;
}

gp_Pnt faceCentroid(const TopoDS_Face& face) {
    GProp_GProps props;
    BRepGProp::SurfaceProperties(face, props);
    return props.CentreOfMass();
}

AngleResult makeAngle(double radians, const gp_Pnt& vertex, const gp_Pnt& a, const gp_Pnt& b) {
    AngleResult r;
    r.radians = radians;
    r.degrees = radians * 180.0 / kPi;
    r.vertex = toPoint(vertex);
    r.pointOnFirst = toPoint(a);
    r.pointOnSecond = toPoint(b);
    return r;
}

}  // namespace

Result<DistanceResult> minimumDistance(const TopoDS_Shape& first, const TopoDS_Shape& second) {
    if (first.IsNull() || second.IsNull())
        return Result<DistanceResult>::failure(ErrorCode::InvalidInput, "shape is null");
    return guard<DistanceResult>([&]() -> Result<DistanceResult> {
        BRepExtrema_DistShapeShape dist(first, second);
        if (!dist.IsDone() || dist.NbSolution() == 0)
            return Result<DistanceResult>::failure(ErrorCode::OperationFailed, "BRepExtrema_DistShapeShape failed");
        DistanceResult r;
        r.distance = dist.Value();
        r.pointOnFirst = toPoint(dist.PointOnShape1(1));
        r.pointOnSecond = toPoint(dist.PointOnShape2(1));
        r.solutionCount = static_cast<std::size_t>(dist.NbSolution());
        return r;
    });
}

Result<AngleResult> angleBetweenFaces(const TopoDS_Face& first, const TopoDS_Face& second) {
    if (first.IsNull() || second.IsNull()) return Result<AngleResult>::failure(ErrorCode::InvalidInput, "face is null");
    return guard<AngleResult>([&]() -> Result<AngleResult> {
        BRepAdaptor_Surface s1(first), s2(second);
        if (s1.GetType() != GeomAbs_Plane || s2.GetType() != GeomAbs_Plane)
            return Result<AngleResult>::failure(ErrorCode::Unsupported, "angle between faces needs two planar faces");
        const gp_Dir n1 = planarNormal(first, s1);
        const gp_Dir n2 = planarNormal(second, s2);
        const gp_Pnt c1 = faceCentroid(first);
        const gp_Pnt c2 = faceCentroid(second);
        // Arc anchor: middle of a shared edge, otherwise between the centroids.
        gp_Pnt anchor((c1.XYZ() + c2.XYZ()) * 0.5);
        for (TopExp_Explorer e1(first, TopAbs_EDGE); e1.More(); e1.Next()) {
            bool shared = false;
            for (TopExp_Explorer e2(second, TopAbs_EDGE); e2.More() && !shared; e2.Next())
                shared = e1.Current().IsSame(e2.Current());
            if (!shared) continue;
            BRepAdaptor_Curve curve(TopoDS::Edge(e1.Current()));
            anchor = curve.Value(0.5 * (curve.FirstParameter() + curve.LastParameter()));
            break;
        }
        return makeAngle(n1.Angle(n2), anchor, c1, c2);
    });
}

Result<AngleResult> angleBetweenEdges(const TopoDS_Edge& first, const TopoDS_Edge& second) {
    if (first.IsNull() || second.IsNull()) return Result<AngleResult>::failure(ErrorCode::InvalidInput, "edge is null");
    return guard<AngleResult>([&]() -> Result<AngleResult> {
        BRepAdaptor_Curve c1(first), c2(second);
        if (c1.GetType() != GeomAbs_Line || c2.GetType() != GeomAbs_Line)
            return Result<AngleResult>::failure(ErrorCode::Unsupported, "angle between edges needs two straight edges");
        TopoDS_Vertex a1, b1, a2, b2;
        TopExp::Vertices(first, a1, b1, true);
        TopExp::Vertices(second, a2, b2, true);
        if (a1.IsNull() || b1.IsNull() || a2.IsNull() || b2.IsNull())
            return Result<AngleResult>::failure(ErrorCode::InvalidInput, "edge without end vertices");
        const gp_Pnt pa1 = BRep_Tool::Pnt(a1), pb1 = BRep_Tool::Pnt(b1);
        const gp_Pnt pa2 = BRep_Tool::Pnt(a2), pb2 = BRep_Tool::Pnt(b2);
        // Corner angle when the edges share a vertex: rays from it to the far ends.
        const TopoDS_Vertex ends1[2] = {a1, b1};
        const TopoDS_Vertex ends2[2] = {a2, b2};
        for (int i = 0; i < 2; ++i)
            for (int j = 0; j < 2; ++j) {
                if (!ends1[i].IsSame(ends2[j])) continue;
                const gp_Pnt apex = BRep_Tool::Pnt(ends1[i]);
                const gp_Pnt far1 = BRep_Tool::Pnt(ends1[1 - i]);
                const gp_Pnt far2 = BRep_Tool::Pnt(ends2[1 - j]);
                const gp_Vec r1(apex, far1), r2(apex, far2);
                if (r1.Magnitude() <= 0.0 || r2.Magnitude() <= 0.0) continue;
                return makeAngle(r1.Angle(r2), apex, far1, far2);
            }
        const gp_Vec d1(pa1, pb1), d2(pa2, pb2);
        if (d1.Magnitude() <= 0.0 || d2.Magnitude() <= 0.0)
            return Result<AngleResult>::failure(ErrorCode::InvalidInput, "degenerate edge");
        const gp_Pnt m1((pa1.XYZ() + pb1.XYZ()) * 0.5), m2((pa2.XYZ() + pb2.XYZ()) * 0.5);
        return makeAngle(d1.Angle(d2), gp_Pnt((m1.XYZ() + m2.XYZ()) * 0.5), m1, m2);
    });
}

Result<LengthResult> edgeLength(const TopoDS_Edge& edge) {
    if (edge.IsNull()) return Result<LengthResult>::failure(ErrorCode::InvalidInput, "edge is null");
    if (BRep_Tool::Degenerated(edge))
        return Result<LengthResult>::failure(ErrorCode::InvalidInput, "edge is degenerated (no 3D curve)");
    return guard<LengthResult>([&]() -> Result<LengthResult> {
        BRepAdaptor_Curve curve(edge);
        const double first = curve.FirstParameter();
        const double last = curve.LastParameter();
        LengthResult r;
        r.length = GCPnts_AbscissaPoint::Length(curve, first, last);
        GCPnts_AbscissaPoint half(curve, 0.5 * r.length, first);
        if (!half.IsDone())
            return Result<LengthResult>::failure(ErrorCode::OperationFailed, "GCPnts_AbscissaPoint failed");
        const bool reversed = edge.Orientation() == TopAbs_REVERSED;
        r.start = toPoint(curve.Value(reversed ? last : first));
        r.end = toPoint(curve.Value(reversed ? first : last));
        r.middle = toPoint(curve.Value(half.Parameter()));
        return r;
    });
}

Result<AreaResult> faceArea(const TopoDS_Face& face) {
    if (face.IsNull()) return Result<AreaResult>::failure(ErrorCode::InvalidInput, "face is null");
    return guard<AreaResult>([&]() -> Result<AreaResult> {
        GProp_GProps props;
        BRepGProp::SurfaceProperties(face, props);
        AreaResult r;
        r.area = props.Mass();
        r.centroid = toPoint(props.CentreOfMass());
        return r;
    });
}

Result<RadiusResult> radiusOf(const TopoDS_Shape& shape) {
    if (shape.IsNull()) return Result<RadiusResult>::failure(ErrorCode::InvalidInput, "shape is null");
    return guard<RadiusResult>([&]() -> Result<RadiusResult> {
        RadiusResult r;
        if (shape.ShapeType() == TopAbs_EDGE) {
            const TopoDS_Edge& edge = TopoDS::Edge(shape);
            if (BRep_Tool::Degenerated(edge))
                return Result<RadiusResult>::failure(ErrorCode::InvalidInput, "edge is degenerated");
            BRepAdaptor_Curve curve(edge);
            if (curve.GetType() != GeomAbs_Circle)
                return Result<RadiusResult>::failure(ErrorCode::Unsupported, "radius needs a circular edge");
            const gp_Circ circle = curve.Circle();
            r.radius = circle.Radius();
            r.center = toPoint(circle.Location());
            r.axis = toPoint(circle.Axis().Direction().XYZ());
            r.pointOnElement = toPoint(curve.Value(0.5 * (curve.FirstParameter() + curve.LastParameter())));
        } else if (shape.ShapeType() == TopAbs_FACE) {
            const TopoDS_Face& face = TopoDS::Face(shape);
            BRepAdaptor_Surface surface(face);
            double u1, u2, v1, v2;
            BRepTools::UVBounds(face, u1, u2, v1, v2);
            const gp_Pnt p = surface.Value(0.5 * (u1 + u2), 0.5 * (v1 + v2));
            r.pointOnElement = toPoint(p);
            switch (surface.GetType()) {
                case GeomAbs_Cylinder: {
                    const gp_Cylinder cyl = surface.Cylinder();
                    const gp_Ax1 axis = cyl.Axis();
                    const double t = gp_Vec(axis.Location(), p).Dot(gp_Vec(axis.Direction()));
                    r.radius = cyl.Radius();
                    r.center = toPoint(axis.Location().XYZ() + axis.Direction().XYZ() * t);
                    r.axis = toPoint(axis.Direction().XYZ());
                    break;
                }
                case GeomAbs_Sphere: {
                    const gp_Sphere sphere = surface.Sphere();
                    r.radius = sphere.Radius();
                    r.center = toPoint(sphere.Location());
                    r.axis = {0.0, 0.0, 0.0};
                    break;
                }
                case GeomAbs_Cone: {
                    const gp_Cone cone = surface.Cone();
                    r.radius = cone.RefRadius();
                    r.center = toPoint(cone.Location());
                    r.axis = toPoint(cone.Axis().Direction().XYZ());
                    break;
                }
                case GeomAbs_Torus: {
                    const gp_Torus torus = surface.Torus();
                    const gp_Ax1 axis = torus.Axis();
                    const gp_XYZ o = axis.Location().XYZ();
                    const gp_XYZ z = axis.Direction().XYZ();
                    gp_XYZ radial = p.XYZ() - o;
                    radial -= z * radial.Dot(z);
                    if (radial.Modulus() <= 0.0)
                        return Result<RadiusResult>::failure(ErrorCode::OperationFailed, "torus point on the axis");
                    r.radius = torus.MinorRadius();
                    r.center = toPoint(o + radial.Normalized() * torus.MajorRadius());
                    r.axis = toPoint(z);
                    break;
                }
                default:
                    return Result<RadiusResult>::failure(ErrorCode::Unsupported,
                                                         "radius needs a cylindrical, conical, spherical or toroidal face");
            }
        } else {
            return Result<RadiusResult>::failure(ErrorCode::InvalidInput, "radius needs an edge or a face");
        }
        r.diameter = 2.0 * r.radius;
        return r;
    });
}

Result<Point3> vertexCoordinates(const TopoDS_Vertex& vertex) {
    if (vertex.IsNull()) return Result<Point3>::failure(ErrorCode::InvalidInput, "vertex is null");
    return guard<Point3>([&]() -> Result<Point3> { return toPoint(BRep_Tool::Pnt(vertex)); });
}

}  // namespace bendcore::kernel
