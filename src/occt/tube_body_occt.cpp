#include "bendcore/tube_body.hpp"

#include "bendcore/axis.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeSolid.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_Sewing.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <Geom_BSplineCurve.hxx>
#include <GeomAbs_Shape.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <GProp_GProps.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <Standard_Handle.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace bendcore::geom {
namespace {

gp_Pnt to_pnt(const Vec3& v) { return gp_Pnt(v[0], v[1], v[2]); }
gp_Dir to_dir(const Vec3& v) { return gp_Dir(v[0], v[1], v[2]); }

TopoDS_Edge make_edge(const AxisPiece& p) {
    switch (p.kind) {
    case AxisPiece::Kind::Line:
        return BRepBuilderAPI_MakeEdge(to_pnt(p.p0), to_pnt(p.p1)).Edge();
    case AxisPiece::Kind::Arc: {
        GC_MakeArcOfCircle arc(to_pnt(p.p0), to_pnt(p.mid), to_pnt(p.p1));
        if (!arc.IsDone()) throw std::invalid_argument("не строится дуга оси");
        Handle(Geom_TrimmedCurve) curve = arc.Value();
        return BRepBuilderAPI_MakeEdge(curve).Edge();
    }
    case AxisPiece::Kind::Spiral: {
        // Отсчёты центральной линии аппроксимируются B-сплайном (степень до 8, C2, допуск 1e-4 мм).
        TColgp_Array1OfPnt points(1, static_cast<Standard_Integer>(p.pts.size()));
        for (std::size_t k = 0; k < p.pts.size(); ++k) {
            points.SetValue(static_cast<Standard_Integer>(k) + 1, to_pnt(p.pts[k]));
        }
        GeomAPI_PointsToBSpline fit(points, 3, 8, GeomAbs_C2, 1e-4);
        if (!fit.IsDone()) throw std::invalid_argument("не строится сплайн спирали");
        return BRepBuilderAPI_MakeEdge(fit.Curve()).Edge();
    }
    }
    throw std::invalid_argument("неизвестный кусок оси");
}

}  // namespace

TubeSolidResult make_solid_tube(const Path& path, double od_mm) {
    // 256 отсчётов на виток: ошибка длины спирали ~ 1e-5 относительно точной.
    const auto pieces = axis_pieces(path, {0.0, 0.0, 0.0}, 256);
    if (pieces.empty()) throw std::invalid_argument("путь без участков");

    TubeSolidResult res;
    res.axis_length = axis_length(path);

    BRepBuilderAPI_MakeWire spineMaker;
    for (const auto& p : pieces) spineMaker.Add(make_edge(p));
    const TopoDS_Wire spine = spineMaker.Wire();

    const double r = od_mm / 2.0;
    const Vec3 t0 = pieces.front().t0;
    const Vec3 t1 = pieces.back().t1;
    const gp_Circ startCirc(gp_Ax2(to_pnt(pieces.front().p0), to_dir(t0)), r);
    const gp_Circ endCirc(gp_Ax2(to_pnt(pieces.back().p1), to_dir(t1)), r);

    BRepBuilderAPI_MakeWire profileMaker;
    profileMaker.Add(BRepBuilderAPI_MakeEdge(startCirc).Edge());
    BRepOffsetAPI_MakePipe pipe(spine, profileMaker.Wire());
    pipe.Build();
    if (!pipe.IsDone()) throw std::invalid_argument("протяжка профиля не выполнена");

    // MakePipe даёт только боковую поверхность: добавляем торцы и сшиваем в solid.
    BRepBuilderAPI_MakeWire endWire;
    endWire.Add(BRepBuilderAPI_MakeEdge(endCirc).Edge());
    BRepBuilderAPI_MakeFace faceStart(profileMaker.Wire());
    BRepBuilderAPI_MakeFace faceEnd(endWire.Wire());

    BRepBuilderAPI_Sewing sewing(1e-3);
    sewing.Add(pipe.Shape());
    sewing.Add(faceStart.Face());
    sewing.Add(faceEnd.Face());
    sewing.Perform();

    TopExp_Explorer shellEx(sewing.SewedShape(), TopAbs_SHELL);
    if (!shellEx.More()) throw std::invalid_argument("сшивка не дала оболочку");
    BRepBuilderAPI_MakeSolid makeSolid(TopoDS::Shell(shellEx.Current()));
    const TopoDS_Solid solid = makeSolid.Solid();

    BRepCheck_Analyzer analyzer(solid);
    res.valid = analyzer.IsValid();
    GProp_GProps props;
    BRepGProp::VolumeProperties(solid, props);
    res.volume = props.Mass();
    return res;
}

}  // namespace bendcore::geom
