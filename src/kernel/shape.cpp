#include <bendcore/kernel/shape.hpp>

#include "occt_failure.hpp"

#include <map>
#include <sstream>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepCheck_Result.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <TopoDS.hxx>

namespace bendcore::kernel {

namespace {

const char* shapeTypeName(TopAbs_ShapeEnum type) {
    switch (type) {
        case TopAbs_COMPOUND: return "COMPOUND";
        case TopAbs_COMPSOLID: return "COMPSOLID";
        case TopAbs_SOLID: return "SOLID";
        case TopAbs_SHELL: return "SHELL";
        case TopAbs_FACE: return "FACE";
        case TopAbs_WIRE: return "WIRE";
        case TopAbs_EDGE: return "EDGE";
        case TopAbs_VERTEX: return "VERTEX";
        case TopAbs_SHAPE: return "SHAPE";
    }
    return "SHAPE";
}

std::string statusText(BRepCheck_Status status) {
    std::ostringstream out;
    BRepCheck::Print(status, out);
    std::string text = out.str();
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
        text.pop_back();
    }
    return text;
}

using detail::failureText;

Point3 toPoint(const gp_Pnt& p) { return {p.X(), p.Y(), p.Z()}; }

SurfaceType classifySurface(const TopoDS_Face& face) {
    if (BRep_Tool::Surface(face).IsNull()) {
        return SurfaceType::Other;
    }
    BRepAdaptor_Surface surface(face, false);
    switch (surface.GetType()) {
        case GeomAbs_Plane: return SurfaceType::Plane;
        case GeomAbs_Cylinder: return SurfaceType::Cylinder;
        case GeomAbs_Cone: return SurfaceType::Cone;
        case GeomAbs_Sphere: return SurfaceType::Sphere;
        case GeomAbs_Torus: return SurfaceType::Torus;
        case GeomAbs_BSplineSurface: return SurfaceType::BSpline;
        default: return SurfaceType::Other;
    }
}

CurveType classifyCurve(const TopoDS_Edge& edge) {
    if (BRep_Tool::Degenerated(edge)) {
        return CurveType::Degenerated;
    }
    if (!BRep_Tool::IsGeometric(edge)) {
        return CurveType::Other;
    }
    BRepAdaptor_Curve curve(edge);
    switch (curve.GetType()) {
        case GeomAbs_Line: return CurveType::Line;
        case GeomAbs_Circle: return CurveType::Circle;
        case GeomAbs_Ellipse: return CurveType::Ellipse;
        case GeomAbs_BSplineCurve: return CurveType::BSpline;
        default: return CurveType::Other;
    }
}

std::optional<std::uint32_t> lookup(const ShapeIndexMap& map, const TopoDS_Shape& shape) {
    if (shape.IsNull()) {
        return std::nullopt;
    }
    const int index = map.FindIndex(shape);
    if (index <= 0) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(index - 1);
}

}  // namespace

const char* toString(SurfaceType type) noexcept {
    switch (type) {
        case SurfaceType::Plane: return "Plane";
        case SurfaceType::Cylinder: return "Cylinder";
        case SurfaceType::Cone: return "Cone";
        case SurfaceType::Sphere: return "Sphere";
        case SurfaceType::Torus: return "Torus";
        case SurfaceType::BSpline: return "BSpline";
        case SurfaceType::Other: return "Other";
    }
    return "Other";
}

const char* toString(CurveType type) noexcept {
    switch (type) {
        case CurveType::Line: return "Line";
        case CurveType::Circle: return "Circle";
        case CurveType::Ellipse: return "Ellipse";
        case CurveType::BSpline: return "BSpline";
        case CurveType::Other: return "Other";
        case CurveType::Degenerated: return "Degenerated";
    }
    return "Other";
}

ValidityReport Shape::checkValidity() const {
    ValidityReport report;
    if (shape_.IsNull()) {
        report.issues.emplace_back("shape is null");
        return report;
    }
    try {
        BRepCheck_Analyzer analyzer(shape_, true);
        report.valid = analyzer.IsValid();
        if (report.valid) {
            return report;
        }
        // Per-type maps give each sub-shape a stable "<TYPE> #<index>" label.
        std::map<TopAbs_ShapeEnum, ShapeIndexMap> byType;
        ShapeIndexMap all;
        all.Add(shape_);
        TopExp::MapShapes(shape_, all);
        for (int i = 1; i <= all.Extent(); ++i) {
            byType[all(i).ShapeType()].Add(all(i));
        }
        auto label = [&byType](const TopoDS_Shape& s) {
            const int index = byType[s.ShapeType()].FindIndex(s);
            return std::string(shapeTypeName(s.ShapeType())) + " #" + std::to_string(index - 1);
        };
        for (int i = 1; i <= all.Extent(); ++i) {
            const TopoDS_Shape& sub = all(i);
            occ::handle<BRepCheck_Result> result;
            try {
                result = analyzer.Result(sub);
            } catch (const Standard_Failure&) {
                continue;  // sub-shape type not analysed by BRepCheck (e.g. compound)
            }
            if (result.IsNull()) {
                continue;
            }
            for (const BRepCheck_Status status : result->Status()) {
                if (status != BRepCheck_NoError) {
                    report.issues.push_back(label(sub) + ": " + statusText(status));
                }
            }
            for (result->InitContextIterator(); result->MoreShapeInContext(); result->NextShapeInContext()) {
                for (const BRepCheck_Status status : result->StatusOnShape()) {
                    if (status != BRepCheck_NoError) {
                        report.issues.push_back(label(sub) + " in " + label(result->ContextualShape()) + ": " +
                                                statusText(status));
                    }
                }
            }
        }
        if (report.issues.empty()) {
            report.issues.emplace_back("BRepCheck_Analyzer reports the shape invalid without a status");
        }
    } catch (const Standard_Failure& failure) {
        report.valid = false;
        report.issues.push_back("BRepCheck_Analyzer failed: " + failureText(failure));
    }
    return report;
}

Result<BoundingBox> Shape::boundingBox() const {
    if (shape_.IsNull()) {
        return Result<BoundingBox>::failure(ErrorCode::InvalidInput, "shape is null");
    }
    try {
        Bnd_Box box;
        BRepBndLib::AddOptimal(shape_, box, false, false);
        if (box.IsVoid()) {
            return Result<BoundingBox>::failure(ErrorCode::InvalidShape, "shape has no geometry to bound");
        }
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        return BoundingBox{{x0, y0, z0}, {x1, y1, z1}};
    } catch (const Standard_Failure& failure) {
        return Result<BoundingBox>::failure(ErrorCode::OperationFailed, failureText(failure));
    }
}

Result<VolumeProperties> Shape::volumeProperties() const {
    if (shape_.IsNull()) {
        return Result<VolumeProperties>::failure(ErrorCode::InvalidInput, "shape is null");
    }
    if (counts().solids == 0) {
        return Result<VolumeProperties>::failure(ErrorCode::InvalidInput, "shape contains no solid");
    }
    try {
        GProp_GProps props;
        BRepGProp::VolumeProperties(shape_, props);
        return VolumeProperties{props.Mass(), toPoint(props.CentreOfMass())};
    } catch (const Standard_Failure& failure) {
        return Result<VolumeProperties>::failure(ErrorCode::OperationFailed, failureText(failure));
    }
}

Result<SurfaceProperties> Shape::surfaceProperties() const {
    if (shape_.IsNull()) {
        return Result<SurfaceProperties>::failure(ErrorCode::InvalidInput, "shape is null");
    }
    if (counts().faces == 0) {
        return Result<SurfaceProperties>::failure(ErrorCode::InvalidInput, "shape contains no face");
    }
    try {
        GProp_GProps props;
        BRepGProp::SurfaceProperties(shape_, props);
        return SurfaceProperties{props.Mass(), toPoint(props.CentreOfMass())};
    } catch (const Standard_Failure& failure) {
        return Result<SurfaceProperties>::failure(ErrorCode::OperationFailed, failureText(failure));
    }
}

TopologyCounts Shape::counts() const {
    TopologyCounts counts;
    if (shape_.IsNull()) {
        return counts;
    }
    auto count = [this](TopAbs_ShapeEnum type) {
        ShapeIndexMap map;
        TopExp::MapShapes(shape_, type, map);
        return static_cast<std::size_t>(map.Extent());
    };
    counts.solids = count(TopAbs_SOLID);
    counts.shells = count(TopAbs_SHELL);
    counts.faces = count(TopAbs_FACE);
    counts.edges = count(TopAbs_EDGE);
    counts.vertices = count(TopAbs_VERTEX);
    return counts;
}

Result<TopologyIndex> TopologyIndex::build(const Shape& shape) {
    if (shape.isNull()) {
        return Result<TopologyIndex>::failure(ErrorCode::InvalidInput, "shape is null");
    }
    try {
        TopologyIndex index;
        ShapeIndexMap& faces = index.faceMap_;
        ShapeIndexMap& edges = index.edgeMap_;
        ShapeIndexMap& vertices = index.vertexMap_;
        TopExp::MapShapes(shape.occt(), TopAbs_FACE, faces);
        TopExp::MapShapes(shape.occt(), TopAbs_EDGE, edges);
        TopExp::MapShapes(shape.occt(), TopAbs_VERTEX, vertices);
        index.faces_.reserve(faces.Extent());
        for (int i = 1; i <= faces.Extent(); ++i) {
            const TopoDS_Face& face = TopoDS::Face(faces(i));
            index.faces_.push_back(FaceInfo{face, classifySurface(face)});
        }
        index.edges_.reserve(edges.Extent());
        for (int i = 1; i <= edges.Extent(); ++i) {
            const TopoDS_Edge& edge = TopoDS::Edge(edges(i));
            index.edges_.push_back(EdgeInfo{edge, classifyCurve(edge)});
        }
        index.vertices_.reserve(vertices.Extent());
        for (int i = 1; i <= vertices.Extent(); ++i) {
            const TopoDS_Vertex& vertex = TopoDS::Vertex(vertices(i));
            index.vertices_.push_back(VertexInfo{vertex, toPoint(BRep_Tool::Pnt(vertex))});
        }
        return index;
    } catch (const Standard_Failure& failure) {
        return Result<TopologyIndex>::failure(ErrorCode::OperationFailed, failureText(failure));
    }
}

std::optional<std::uint32_t> TopologyIndex::faceIndex(const TopoDS_Shape& face) const {
    return lookup(faceMap_, face);
}

std::optional<std::uint32_t> TopologyIndex::edgeIndex(const TopoDS_Shape& edge) const {
    return lookup(edgeMap_, edge);
}

std::optional<std::uint32_t> TopologyIndex::vertexIndex(const TopoDS_Shape& vertex) const {
    return lookup(vertexMap_, vertex);
}

}  // namespace bendcore::kernel
