#include <bendcore/kernel/section.hpp>

#include "../occt_failure.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include <BOPAlgo_Tools.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <gp_Ax2.hxx>
#include <gp_Pln.hxx>

namespace bendcore::kernel {

namespace {

template <typename T>
Result<T> guard(auto&& body) {
    try {
        return body();
    } catch (const Standard_Failure& failure) {
        return Result<T>::failure(ErrorCode::OperationFailed, detail::failureText(failure));
    }
}

TopoDS_Compound emptyCompound() {
    TopoDS_Compound c;
    BRep_Builder().MakeCompound(c);
    return c;
}

std::optional<gp_Ax2> planeAxes(const SectionPlane& plane) {
    const gp_Vec n(plane.normal[0], plane.normal[1], plane.normal[2]);
    if (!(n.Magnitude() > 1.0e-12) || !std::isfinite(n.Magnitude())) return std::nullopt;
    return gp_Ax2(gp_Pnt(plane.origin[0], plane.origin[1], plane.origin[2]), gp_Dir(n));
}

// Half-size of a square on the plane (and depth of the cutting box) that covers the shape:
// twice the bounding box diagonal plus the distance from the plane origin to the box.
double coverSize(const TopoDS_Shape& shape, const gp_Pnt& origin) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    if (box.IsVoid()) return 1.0;
    double x1, y1, z1, x2, y2, z2;
    box.Get(x1, y1, z1, x2, y2, z2);
    const gp_Pnt lo(x1, y1, z1), hi(x2, y2, z2);
    const gp_Pnt mid((lo.XYZ() + hi.XYZ()) * 0.5);
    return 2.0 * lo.Distance(hi) + origin.Distance(mid) + 1.0;
}

TopoDS_Shape solidsOf(const TopoDS_Shape& shape, std::size_t& count) {
    TopoDS_Compound c = emptyCompound();
    BRep_Builder builder;
    count = 0;
    for (TopExp_Explorer it(shape, TopAbs_SOLID); it.More(); it.Next()) {
        builder.Add(c, it.Current());
        ++count;
    }
    return c;
}

}  // namespace

Result<Shape> clipByPlane(const Shape& shape, const SectionPlane& plane) {
    if (shape.isNull()) return Result<Shape>::failure(ErrorCode::InvalidInput, "shape is null");
    const auto axes = planeAxes(plane);
    if (!axes) return Result<Shape>::failure(ErrorCode::InvalidInput, "plane normal is zero");
    return guard<Shape>([&]() -> Result<Shape> {
        const double size = coverSize(shape.occt(), axes->Location());
        const gp_XYZ corner = axes->Location().XYZ() - axes->XDirection().XYZ() * size - axes->YDirection().XYZ() * size;
        // Box from the plane towards +normal: [-size, size]^2 x [0, size] in plane axes.
        BRepPrimAPI_MakeBox tool(gp_Ax2(gp_Pnt(corner), axes->Direction(), axes->XDirection()), 2.0 * size,
                                 2.0 * size, size);
        BRepAlgoAPI_Cut cut(shape.occt(), tool.Shape());
        if (!cut.IsDone() || cut.HasErrors())
            return Result<Shape>::failure(ErrorCode::OperationFailed, "BRepAlgoAPI_Cut with the half-space failed");
        return Shape(cut.Shape());
    });
}

Result<SectionResult> sectionByPlane(const Shape& shape, const SectionPlane& plane, double deflection) {
    if (shape.isNull()) return Result<SectionResult>::failure(ErrorCode::InvalidInput, "shape is null");
    if (!(deflection > 0.0)) return Result<SectionResult>::failure(ErrorCode::InvalidInput, "deflection must be positive");
    const auto axes = planeAxes(plane);
    if (!axes) return Result<SectionResult>::failure(ErrorCode::InvalidInput, "plane normal is zero");
    return guard<SectionResult>([&]() -> Result<SectionResult> {
        const gp_Pln pln(*axes);
        SectionResult result;

        BRepAlgoAPI_Section section(shape.occt(), pln, false);
        section.Approximation(true);
        section.ComputePCurveOn1(true);
        section.Build();
        if (!section.IsDone() || section.HasErrors())
            return Result<SectionResult>::failure(ErrorCode::OperationFailed, "BRepAlgoAPI_Section failed");
        result.edges = Shape(section.Shape());

        TopoDS_Shape wires = emptyCompound();
        bool anyEdge = false;
        for (TopExp_Explorer it(section.Shape(), TopAbs_EDGE); it.More(); it.Next()) anyEdge = true;
        if (anyEdge) BOPAlgo_Tools::EdgesToWires(section.Shape(), wires, true);
        result.wires = Shape(wires);

        // Faces: the part of a covering planar face inside the solids (boolean common), which
        // gives the section regions with their holes.
        std::size_t solidCount = 0;
        const TopoDS_Shape solids = solidsOf(shape.occt(), solidCount);
        TopoDS_Shape faces = emptyCompound();
        if (solidCount > 0 && anyEdge) {
            const double size = coverSize(shape.occt(), axes->Location());
            BRepBuilderAPI_MakeFace face(pln, -size, size, -size, size);
            if (!face.IsDone())
                return Result<SectionResult>::failure(ErrorCode::OperationFailed, "covering face not built");
            BRepAlgoAPI_Common common(solids, face.Face());
            if (!common.IsDone() || common.HasErrors())
                return Result<SectionResult>::failure(ErrorCode::OperationFailed, "BRepAlgoAPI_Common with the plane failed");
            TopoDS_Compound c = emptyCompound();
            BRep_Builder builder;
            for (TopExp_Explorer it(common.Shape(), TopAbs_FACE); it.More(); it.Next()) {
                builder.Add(c, it.Current());
                GProp_GProps props;
                BRepGProp::SurfaceProperties(it.Current(), props);
                result.area += props.Mass();
            }
            faces = c;
        }
        result.faces = Shape(faces);

        for (TopExp_Explorer it(section.Shape(), TopAbs_EDGE); it.More(); it.Next()) {
            const TopoDS_Edge& edge = TopoDS::Edge(it.Current());
            if (BRep_Tool::Degenerated(edge)) continue;
            BRepAdaptor_Curve curve(edge);
            GCPnts_QuasiUniformDeflection points(curve, deflection);
            if (!points.IsDone())
                return Result<SectionResult>::failure(ErrorCode::OperationFailed, "edge discretisation failed");
            std::vector<Point3> line;
            for (int i = 1; i <= points.NbPoints(); ++i) {
                const gp_Pnt p = points.Value(i);
                line.push_back({p.X(), p.Y(), p.Z()});
            }
            result.polylines.push_back(std::move(line));
        }
        return result;
    });
}

}  // namespace bendcore::kernel
