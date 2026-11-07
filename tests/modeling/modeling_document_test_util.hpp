#pragma once

// Geometry probes for the part document tests: they locate elements by independent geometric
// criteria (position, direction, radius), never through the element map under test.

#include <bendcore/modeling/basic_features.hpp>
#include <bendcore/modeling/part_document.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <gtest/gtest.h>

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace bendcore::modeling::testutil {

constexpr double kPi = 3.14159265358979323846;

inline double volume(const TopoDS_Shape& shape) {
    GProp_GProps props;
    BRepGProp::VolumeProperties(shape, props);
    return props.Mass();
}

// Volume removed by a fillet of radius r along a straight convex 90-degree edge of length len.
inline double filletLoss(double r, double len) { return (1.0 - kPi / 4.0) * r * r * len; }
// Volume removed by an equal-distance chamfer d along a straight 90-degree edge.
inline double chamferLoss(double d, double len) { return 0.5 * d * d * len; }
// Volume removed by a fillet r on the rim of a round hole of radius rho in a flat face (Pappus):
// spandrel area (1 - pi/4) r^2, centroid at r (10 - 3 pi) / (3 (4 - pi)) from the hole wall.
inline double holeRimFilletLoss(double r, double rho) {
    const double xc = r * (10.0 - 3.0 * kPi) / (3.0 * (4.0 - kPi));
    return (1.0 - kPi / 4.0) * r * r * 2.0 * kPi * (rho + xc);
}

// Straight edge parallel to axis `axis` (0=x,1=y,2=z) through the line where the other two
// coordinates equal (c1, c2) (in increasing axis order).
inline std::vector<TopoDS_Shape> linesAlong(const TopoDS_Shape& shape, int axis, double c1, double c2,
                                            double tol = 1e-6) {
    std::vector<TopoDS_Shape> out;
    for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next()) {
        BRepAdaptor_Curve c(TopoDS::Edge(ex.Current()));
        if (c.GetType() != GeomAbs_Line) continue;
        gp_Dir d = c.Line().Direction();
        double comp[3] = {d.X(), d.Y(), d.Z()};
        if (std::abs(std::abs(comp[axis]) - 1.0) > 1e-9) continue;
        gp_Pnt p = c.Value(0.5 * (c.FirstParameter() + c.LastParameter()));
        double pc[3] = {p.X(), p.Y(), p.Z()};
        int o1 = axis == 0 ? 1 : 0, o2 = axis == 2 ? 1 : 2;
        if (std::abs(pc[o1] - c1) < tol && std::abs(pc[o2] - c2) < tol) {
            bool dup = false;
            for (const auto& s : out) dup = dup || s.IsSame(ex.Current());
            if (!dup) out.push_back(ex.Current());
        }
    }
    return out;
}

inline TopoDS_Shape singleLine(const TopoDS_Shape& shape, int axis, double c1, double c2) {
    auto v = linesAlong(shape, axis, c1, c2);
    EXPECT_EQ(v.size(), 1u) << "edges along axis " << axis << " at (" << c1 << ", " << c2 << ")";
    return v.empty() ? TopoDS_Shape() : v.front();
}

// Planar face with outward normal `n` whose plane passes through offset `d` (n . p = d).
inline TopoDS_Shape planarFace(const TopoDS_Shape& shape, gp_Dir n, double d, double tol = 1e-6) {
    TopoDS_Shape found;
    int count = 0;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face& f = TopoDS::Face(ex.Current());
        BRepAdaptor_Surface s(f, false);
        if (s.GetType() != GeomAbs_Plane) continue;
        gp_Dir fn = s.Plane().Axis().Direction();
        if (!s.Plane().Position().Direct()) fn.Reverse();  // left-handed plane: Du x Dv = -axis
        if (f.Orientation() == TopAbs_REVERSED) fn.Reverse();
        if (fn.Dot(n) < 1 - 1e-9) continue;
        if (std::abs(s.Plane().Location().XYZ().Dot(n.XYZ()) - d) > tol) continue;
        found = f;
        ++count;
    }
    EXPECT_EQ(count, 1) << "planar faces with the requested normal and offset " << d;
    return found;
}

struct CylinderFaceInfo {
    double radius;
    gp_Pnt location;
    gp_Dir axis;
};

inline std::vector<CylinderFaceInfo> cylinderFaces(const TopoDS_Shape& shape) {
    std::vector<CylinderFaceInfo> out;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        BRepAdaptor_Surface s(TopoDS::Face(ex.Current()), false);
        if (s.GetType() != GeomAbs_Cylinder) continue;
        out.push_back({s.Cylinder().Radius(), s.Cylinder().Location(), s.Cylinder().Axis().Direction()});
    }
    return out;
}

// Is there a cylindrical face of radius r whose axis is parallel to `dir` and passes through
// the line where the two other coordinates are (c1, c2)?
inline bool hasCylinder(const TopoDS_Shape& shape, double r, int axis, double c1, double c2, double tol = 1e-6) {
    for (const auto& c : cylinderFaces(shape)) {
        double a[3] = {c.axis.X(), c.axis.Y(), c.axis.Z()};
        if (std::abs(std::abs(a[axis]) - 1.0) > 1e-9 || std::abs(c.radius - r) > tol) continue;
        double p[3] = {c.location.X(), c.location.Y(), c.location.Z()};
        int o1 = axis == 0 ? 1 : 0, o2 = axis == 2 ? 1 : 2;
        if (std::abs(p[o1] - c1) < tol && std::abs(p[o2] - c2) < tol) return true;
    }
    return false;
}

// Number of planar faces whose outward normal is the bisector of the two given directions
// and whose plane has offset d along it (chamfer faces).
inline int planarFacesWithNormal(const TopoDS_Shape& shape, gp_Dir n, double d, double tol = 1e-6) {
    int count = 0;
    for (TopExp_Explorer ex(shape, TopAbs_FACE); ex.More(); ex.Next()) {
        const TopoDS_Face& f = TopoDS::Face(ex.Current());
        BRepAdaptor_Surface s(f, false);
        if (s.GetType() != GeomAbs_Plane) continue;
        gp_Dir fn = s.Plane().Axis().Direction();
        if (!s.Plane().Position().Direct()) fn.Reverse();  // left-handed plane: Du x Dv = -axis
        if (f.Orientation() == TopAbs_REVERSED) fn.Reverse();
        if (fn.Dot(n) < 1 - 1e-9) continue;
        if (std::abs(s.Plane().Location().XYZ().Dot(n.XYZ()) - d) < tol) ++count;
    }
    return count;
}

inline TopoReference refTo(const NamedShape& shape, const TopoDS_Shape& element) {
    auto r = makeReference(shape, element);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    return r.ok() ? r.value() : TopoReference{};
}

inline FeatureInfo info(const PartDocument& doc, const std::string& id) {
    auto r = doc.feature(id);
    EXPECT_TRUE(r.ok()) << id;
    return r.ok() ? r.value() : FeatureInfo{};
}

inline std::string messages(const FeatureInfo& i) {
    std::string s;
    for (const auto& m : i.messages) s += m + "\n";
    return s;
}

inline std::string add(PartDocument& doc, FeatureSpec spec, std::optional<std::size_t> pos = std::nullopt) {
    auto r = doc.addFeature(std::move(spec), pos);
    EXPECT_TRUE(r.ok()) << (r.ok() ? "" : r.error().message);
    return r.ok() ? r.value() : std::string();
}

inline void setVar(PartDocument& doc, const std::string& name, double value) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.17g", value);
    auto st = doc.setVariable(name, buf);
    ASSERT_TRUE(st.ok()) << st.error().message;
}

}  // namespace bendcore::modeling::testutil
