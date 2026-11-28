#pragma once

// Reference parts (docs/modeling/REFERENCE_PARTS.md): the part description used by
// test_modeling_document_reference_parts.cpp and the helpers shared by the parts of phase 1
// (in that file) and phase 2 (reference_parts_phase2.cpp).

#include "modeling_document_test_util.hpp"

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace bendcore::modeling::refparts {

using namespace bendcore::modeling::testutil;

using Values = std::map<std::string, double>;

// Circular edge with the given centre and radius whose axis is parallel to coordinate axis `axis`.
inline TopoDS_Shape circleEdge(const TopoDS_Shape& shape, gp_Pnt center, double radius, int axis) {
    TopoDS_Shape found;
    int count = 0;
    for (TopExp_Explorer ex(shape, TopAbs_EDGE); ex.More(); ex.Next()) {
        BRepAdaptor_Curve c(TopoDS::Edge(ex.Current()));
        if (c.GetType() != GeomAbs_Circle) continue;
        const gp_Circ ci = c.Circle();
        const gp_Dir d = ci.Axis().Direction();
        const double comp[3] = {d.X(), d.Y(), d.Z()};
        if (std::abs(std::abs(comp[axis]) - 1) > 1e-9 || std::abs(ci.Radius() - radius) > 1e-6) continue;
        if (ci.Location().Distance(center) > 1e-6) continue;
        if (!found.IsNull() && found.IsSame(ex.Current())) continue;
        found = ex.Current();
        ++count;
    }
    EXPECT_EQ(count, 1) << "circle edges of radius " << radius;
    return found;
}

// Volume added by a concave fillet r (or removed by a convex one) along a circle of radius rho,
// with the fillet spandrel lying outside that circle (Pappus).
inline double ringFillet(double r, double rho) { return holeRimFilletLoss(r, rho); }
// Volume removed by a 45-degree chamfer d on the outer (+1) or inner (-1) circular edge of radius R.
inline double ringChamfer(double d, double R, int side) { return kPi * d * d * (R - side * d / 3.0); }
// Volume of a solid whose horizontal section at height z is (a0 - 2kz) × (b0 - 2kz), between
// z0 and z1: the integral of the section area (a drafted rectangular block, its offset cavity).
inline double taperedBlock(double a0, double b0, double k, double z0, double z1) {
    return a0 * b0 * (z1 - z0) - k * (a0 + b0) * (z1 * z1 - z0 * z0) + 4 * k * k * (z1 * z1 * z1 - z0 * z0 * z0) / 3;
}
// Frustum of a cone with end radii r1, r2 and height h.
inline double frustum(double r1, double r2, double h) { return kPi * h / 3 * (r1 * r1 + r1 * r2 + r2 * r2); }
// Drill diameter of a coarse metric thread, D − P (pitch from ISO 261, coarse series).
inline double coarseDrill(double nominal) {
    const std::map<double, double> pitch{{3, 0.5}, {4, 0.7}, {5, 0.8}, {6, 1.0}, {8, 1.25}, {10, 1.5}, {12, 1.75}};
    return nominal - pitch.at(nominal);
}

inline FeatureSpec sketchSpec(const std::string& id, const std::string& plane, SketchData data) {
    FeatureSpec s;
    s.type = "Sketch";
    s.id = id;
    s.options = {{"plane", plane}};
    s.sketch = std::move(data);
    return s;
}

inline FeatureSpec faceSketch(const std::string& id, const TopoReference& face, std::string ox, std::string oy,
                              std::string oz, SketchData data) {
    FeatureSpec s = sketchSpec(id, "face", std::move(data));
    s.references["face"] = {face};
    s.parameters = {{"originX", std::move(ox)}, {"originY", std::move(oy)}, {"originZ", std::move(oz)}};
    return s;
}

inline FeatureSpec extrudeSpec(const std::string& id, const std::string& sketch,
                               std::map<std::string, std::string> options, std::map<std::string, std::string> params) {
    FeatureSpec s;
    s.type = "Extrude";
    s.id = id;
    s.options = std::move(options);
    s.parameters = std::move(params);
    s.links = {{"sketch", sketch}};
    return s;
}

inline NamedShape built(PartDocument& doc) {
    RebuildReport rep = doc.rebuild();
    EXPECT_TRUE(rep.failed.empty()) << "failed: " << (rep.failed.empty() ? "" : rep.failed.front() + ": " +
                                                                                    messages(info(doc, rep.failed.front())));
    return doc.finalShape();
}

struct ReferencePart {
    std::string name;
    Values base;
    std::function<void(PartDocument&)> build;
    // Analytic volume, mm³, written out from the dimensions (not measured by OCCT).
    std::function<double(const Values&)> volume;
    std::vector<std::pair<std::string, Values>> scenarios;
    // Material density, kg/m³ (mass = volume · 1e-9 · density, kg).
    double density = 7850;
    std::string material = "steel";
    // Expected number of bodies of the final shape.
    std::size_t bodies = 1;
};

// File name of the part's STEP file in tests/corpus/reference_parts/: the name with spaces
// replaced by underscores.
inline std::string stepFileName(const std::string& name) {
    std::string out = name;
    for (char& c : out)
        if (c == ' ') c = '_';
    return out + ".step";
}

// Parts of phase 2 (reference_parts_phase2.cpp).
std::vector<ReferencePart> phase2Parts();

}  // namespace bendcore::modeling::refparts
