#pragma once

// Общие построители форм и поиск элементов для тестов документа сборки.

#include <bendcore/assembly_v2/assembly_document.hpp>

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <gp_Ax2.hxx>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <functional>
#include <optional>

namespace bendcore::assembly_v2::testing {

constexpr double kPi = 3.14159265358979323846;

inline TopoDS_Shape box(double x, double y, double z, double dx, double dy, double dz) {
    return BRepPrimAPI_MakeBox(gp_Pnt(x, y, z), dx, dy, dz).Shape();
}

// Цилиндр по оси Z: основание в (x, y, z), высота h.
inline TopoDS_Shape cylinder(double x, double y, double z, double r, double h) {
    return BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x, y, z), gp::DZ()), r, h).Shape();
}

inline TopoDS_Shape cut(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    BRepAlgoAPI_Cut op(a, b);
    EXPECT_TRUE(op.IsDone());
    return op.Shape();
}

inline TopoDS_Shape fuse(const TopoDS_Shape& a, const TopoDS_Shape& b) {
    BRepAlgoAPI_Fuse op(a, b);
    EXPECT_TRUE(op.IsDone());
    return op.Shape();
}

// Втулка: наружный радиус R, отверстие r, высота h, ось Z, основание z = 0.
inline TopoDS_Shape bushing(double R, double r, double h) {
    return cut(cylinder(0, 0, 0, R, h), cylinder(0, 0, -1, r, h + 2));
}

// Болт: стержень радиуса rs от z = −length до 0, головка радиуса rh от z = 0 до hh.
inline TopoDS_Shape bolt(double rs, double length, double rh, double hh) {
    return fuse(cylinder(0, 0, -length, rs, length), cylinder(0, 0, 0, rh, hh));
}

inline std::optional<std::uint32_t> findFace(const TopoDS_Shape& shape,
                                             const std::function<bool(const BRepAdaptor_Surface&, const TopoDS_Face&)>& pred) {
    auto index = kernel::TopologyIndex::build(kernel::Shape(shape));
    if (!index) return std::nullopt;
    for (std::uint32_t i = 0; i < index.value().faces().size(); ++i) {
        const TopoDS_Face& f = index.value().faces()[i].face;
        if (pred(BRepAdaptor_Surface(f), f)) return i;
    }
    return std::nullopt;
}

// Цилиндрическая грань радиуса r с осью через (x, y).
inline std::uint32_t cylinderFace(const TopoDS_Shape& shape, double r, double x = 0, double y = 0) {
    auto i = findFace(shape, [&](const BRepAdaptor_Surface& s, const TopoDS_Face&) {
        if (s.GetType() != GeomAbs_Cylinder) return false;
        const gp_Cylinder c = s.Cylinder();
        return std::abs(c.Radius() - r) < 1e-9 && std::abs(c.Location().X() - x) < 1e-9 &&
               std::abs(c.Location().Y() - y) < 1e-9;
    });
    EXPECT_TRUE(i.has_value()) << "no cylinder face r=" << r;
    return i.value_or(0);
}

// Плоская грань z = level с наружной нормалью (0, 0, nz).
inline std::uint32_t horizontalFace(const TopoDS_Shape& shape, double level, double nz) {
    auto i = findFace(shape, [&](const BRepAdaptor_Surface& s, const TopoDS_Face& f) {
        if (s.GetType() != GeomAbs_Plane) return false;
        const gp_Ax3 a = s.Plane().Position();
        gp_Dir n = a.XDirection().Crossed(a.YDirection());
        if (f.Orientation() == TopAbs_REVERSED) n.Reverse();
        return std::abs(a.Location().Z() - level) < 1e-9 && std::abs(n.Z() - nz) < 1e-9;
    });
    EXPECT_TRUE(i.has_value()) << "no planar face z=" << level;
    return i.value_or(0);
}

inline double volumeOf(const TopoDS_Shape& s) {
    GProp_GProps p;
    BRepGProp::VolumeProperties(s, p);
    return p.Mass();
}

inline std::array<double, 6> bounds(const TopoDS_Shape& s) {
    Bnd_Box b;
    BRepBndLib::AddOptimal(s, b, false, false);
    std::array<double, 6> out{};
    b.Get(out[0], out[1], out[2], out[3], out[4], out[5]);
    return out;
}

inline GeometryRef faceRef(OccurrenceId occ, std::uint32_t index) { return {{occ}, ElementRef::face(index), EntityUse::Auto}; }

inline void expectVecNear(const Vec3& a, const Vec3& b, double tol) {
    EXPECT_NEAR((a - b).norm(), 0.0, tol) << "got (" << a.transpose() << "), expected (" << b.transpose() << ")";
}

inline const Occurrence& occAt(const AssemblyDocument& doc, OccurrenceId id) {
    const Occurrence* o = doc.findOccurrence(id);
    EXPECT_NE(o, nullptr);
    return *o;
}

}  // namespace bendcore::assembly_v2::testing
