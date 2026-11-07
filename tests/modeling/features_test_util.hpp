#pragma once

// Общие построения для тестов операций детали.

#include <bendcore/modeling/features.hpp>

#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopoDS.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>

#include <cmath>
#include <functional>
#include <vector>

namespace ft {

using namespace bendcore::modeling;
using ShapeMap = NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher>;

inline double volume(const TopoDS_Shape& s) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(s, p);
    return p.Mass();
}

inline double area(const TopoDS_Shape& s) {
    GProp_GProps p;
    BRepGProp::SurfaceProperties(s, p);
    return p.Mass();
}

inline gp_Pnt faceCentroid(const TopoDS_Shape& s) {
    GProp_GProps p;
    BRepGProp::SurfaceProperties(s, p);
    return p.CentreOfMass();
}

inline int count(const TopoDS_Shape& s, TopAbs_ShapeEnum type) {
    ShapeMap m;
    TopExp::MapShapes(s, type, m);
    return m.Extent();
}

inline bool valid(const TopoDS_Shape& s) { return !s.IsNull() && BRepCheck_Analyzer(s).IsValid(); }

// Профиль-прямоугольник в плоскости z = z0 (нормаль +Z), опционально с круглым отверстием.
inline Profile rect(double x0, double y0, double x1, double y1, double z0 = 0.0, double holeR = 0.0,
                    double hx = 0.0, double hy = 0.0) {
    BRepBuilderAPI_MakePolygon poly(gp_Pnt(x0, y0, z0), gp_Pnt(x1, y0, z0), gp_Pnt(x1, y1, z0), gp_Pnt(x0, y1, z0), true);
    const gp_Pln pln(gp_Pnt(0, 0, z0), gp::DZ());
    BRepBuilderAPI_MakeFace mf(pln, poly.Wire(), true);
    if (holeR > 0.0) {
        TopoDS_Edge c = BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(hx, hy, z0), gp::DZ()), holeR));
        TopoDS_Wire w = BRepBuilderAPI_MakeWire(c);
        w.Reverse();
        mf.Add(w);
    }
    return Profile{mf.Face(), gp_Ax3(gp_Pnt(0, 0, z0), gp::DZ(), gp::DX())};
}

inline TopoDS_Shape box(double a, double b, double c) { return BRepPrimAPI_MakeBox(a, b, c).Shape(); }

// Первая грань, удовлетворяющая условию.
inline TopoDS_Face findFace(const TopoDS_Shape& s, const std::function<bool(const TopoDS_Face&)>& pred) {
    for (TopExp_Explorer ex(s, TopAbs_FACE); ex.More(); ex.Next())
        if (pred(TopoDS::Face(ex.Current()))) return TopoDS::Face(ex.Current());
    return {};
}

// Плоская грань, центр масс которой имеет данную координату axis (0 x, 1 y, 2 z) и
// нормаль параллельна этой оси.
inline TopoDS_Face planeFaceAt(const TopoDS_Shape& s, int axis, double value, double tol = 1e-6) {
    return findFace(s, [&](const TopoDS_Face& f) {
        BRepAdaptor_Surface a(f);
        if (a.GetType() != GeomAbs_Plane) return false;
        const gp_Dir n = a.Plane().Axis().Direction();
        const gp_Dir ax = axis == 0 ? gp::DX() : axis == 1 ? gp::DY() : gp::DZ();
        if (!n.IsParallel(ax, 1e-9)) return false;
        return std::abs(faceCentroid(f).Coord(axis + 1) - value) < tol;
    });
}

// Ребро коробки, параллельное оси axis, проходящее через точку p (две другие координаты).
inline TopoDS_Edge edgeThrough(const TopoDS_Shape& s, const gp_Pnt& a, const gp_Pnt& b, double tol = 1e-6) {
    for (TopExp_Explorer ex(s, TopAbs_EDGE); ex.More(); ex.Next()) {
        TopoDS_Vertex v1, v2;
        TopExp::Vertices(TopoDS::Edge(ex.Current()), v1, v2);
        const gp_Pnt p1 = BRep_Tool::Pnt(v1), p2 = BRep_Tool::Pnt(v2);
        if ((p1.Distance(a) < tol && p2.Distance(b) < tol) || (p1.Distance(b) < tol && p2.Distance(a) < tol))
            return TopoDS::Edge(ex.Current());
    }
    return {};
}

inline bool contains(const TopoDS_Shape& whole, const TopoDS_Shape& part) {
    ShapeMap m;
    TopExp::MapShapes(whole, part.ShapeType(), m);
    return m.Contains(part);
}

}  // namespace ft
