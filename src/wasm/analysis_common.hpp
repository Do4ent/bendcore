#pragma once

// Kernel analysis shared by the kernel, part and assembly WebAssembly modules
// (docs/ui/KERNEL_VIEWPORT.md, "Измерения и сечение"; docs/ui/PART_MODELING_UI.md): the material
// library of bendcore_kernel_analysis, measurements between B-rep elements (measure.hpp) and the
// section of a shape by a plane (section.hpp), converted to plain JS values. Header-only, so each
// module compiles it with its own bindings; failures are k::Result errors.

#include <bendcore/kernel/material.hpp>
#include <bendcore/kernel/measure.hpp>
#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/section.hpp>
#include <bendcore/kernel/shape.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>

#include <cmath>
#include <string>
#include <vector>

namespace bendcore::wasm_analysis {

using emscripten::val;
namespace k = bendcore::kernel;

inline bool given(const val& v) { return !v.isUndefined() && !v.isNull(); }

inline bool isObj(const val& v) { return v.typeOf().as<std::string>() == "object"; }

inline val vec3(const k::Point3& p) {
    val out = val::array();
    for (double c : p) out.call<void>("push", c);
    return out;
}

inline k::Error invalid(const std::string& message) { return k::Error{k::ErrorCode::InvalidInput, message}; }

inline k::Result<k::Point3> readVec3(const val& v, const std::string& what) {
    if (!given(v) || !v.isArray() || v["length"].as<int>() != 3) return invalid(what + " must be [x, y, z]");
    k::Point3 p{};
    for (int i = 0; i < 3; ++i) {
        if (!v[i].isNumber()) return invalid(what + " must be [x, y, z] of numbers");
        p[static_cast<std::size_t>(i)] = v[i].as<double>();
        if (!std::isfinite(p[static_cast<std::size_t>(i)])) return invalid(what + " must be finite");
    }
    return p;
}

// ------------------------------------------------------------------ materials

// Built-in material library: [{id, name, category, density (kg/m^3), color [r, g, b], source,
// verified, youngModulus?, poissonRatio?, yieldStrength?}], docs/kernel/ANALYSIS.md, section 1.
inline val materialsToJs() {
    val out = val::array();
    for (const k::Material& m : k::builtInMaterials()) {
        val o = val::object();
        o.set("id", m.id);
        o.set("name", m.name);
        o.set("category", std::string(k::toString(m.category)));
        o.set("density", m.density);
        val color = val::array();
        color.call<void>("push", static_cast<int>(m.color.r));
        color.call<void>("push", static_cast<int>(m.color.g));
        color.call<void>("push", static_cast<int>(m.color.b));
        o.set("color", color);
        o.set("source", m.source);
        o.set("verified", m.verified);
        if (m.youngModulus) o.set("youngModulus", *m.youngModulus);
        if (m.poissonRatio) o.set("poissonRatio", *m.poissonRatio);
        if (m.yieldStrength) o.set("yieldStrength", *m.yieldStrength);
        out.call<void>("push", o);
    }
    return out;
}

// ------------------------------------------------------------------ measurements

// Element of a measurement: {kind: 'face' | 'edge' | 'vertex', index} (kernel indices).
inline k::Result<TopoDS_Shape> elementOf(const k::TopologyIndex& topo, const val& ref, const std::string& what) {
    if (!given(ref) || !isObj(ref) || !ref["kind"].isString() || !ref["index"].isNumber()) {
        return invalid(what + " must be {kind, index}");
    }
    const std::string kind = ref["kind"].as<std::string>();
    const double raw = ref["index"].as<double>();
    if (!(raw >= 0) || raw != std::floor(raw)) return invalid(what + ".index must be a non-negative integer");
    const std::size_t i = static_cast<std::size_t>(raw);
    if (kind == "face") {
        if (i >= topo.faces().size()) return invalid(what + ": face " + std::to_string(i) + " does not exist");
        return TopoDS_Shape(topo.faces()[i].face);
    }
    if (kind == "edge") {
        if (i >= topo.edges().size()) return invalid(what + ": edge " + std::to_string(i) + " does not exist");
        return TopoDS_Shape(topo.edges()[i].edge);
    }
    if (kind == "vertex") {
        if (i >= topo.vertices().size()) return invalid(what + ": vertex " + std::to_string(i) + " does not exist");
        return TopoDS_Shape(topo.vertices()[i].vertex);
    }
    return invalid(what + ".kind must be 'face', 'edge' or 'vertex'");
}

inline val measurement(const char* type, const char* unit, double value, std::vector<k::Point3> points) {
    val o = val::object();
    o.set("type", std::string(type));
    o.set("unit", std::string(unit));
    o.set("value", value);
    val pts = val::array();
    for (const k::Point3& p : points) pts.call<void>("push", vec3(p));
    o.set("points", pts);
    return o;
}

// Measure of one element (type 'length' of an edge, 'area' of a face, 'radius' of an edge or face)
// or between two (type 'distance' | 'angle'); `b` is null for the single-element types. Distances
// in mm, angles in degrees. `points` are what to draw: the nearest points of a distance; for an angle
// {vertex, point on the first element, point on the second}; for a length {start, end, middle}.
inline k::Result<val> measureShapes(const std::string& type, const TopoDS_Shape& a, const TopoDS_Shape* b) {
    if (type == "length") {
        if (a.ShapeType() != TopAbs_EDGE) return invalid("length needs an edge");
        auto r = k::edgeLength(TopoDS::Edge(a));
        if (!r) return r.error();
        return measurement("length", "mm", r.value().length, {r.value().start, r.value().end, r.value().middle});
    }
    if (type == "area") {
        if (a.ShapeType() != TopAbs_FACE) return invalid("area needs a face");
        auto r = k::faceArea(TopoDS::Face(a));
        if (!r) return r.error();
        return measurement("area", "mm2", r.value().area, {r.value().centroid});
    }
    if (type == "radius") {
        auto r = k::radiusOf(a);
        if (!r) return r.error();
        val o = measurement("radius", "mm", r.value().radius, {r.value().center, r.value().pointOnElement});
        o.set("diameter", r.value().diameter);
        return o;
    }
    if (type != "distance" && type != "angle") {
        return invalid("type must be 'distance', 'angle', 'length', 'area' or 'radius'");
    }
    if (!b) return invalid("a " + type + " needs two elements");
    if (type == "distance") {
        auto r = k::minimumDistance(a, *b);
        if (!r) return r.error();
        return measurement("distance", "mm", r.value().distance, {r.value().pointOnFirst, r.value().pointOnSecond});
    }
    const TopAbs_ShapeEnum ta = a.ShapeType();
    const TopAbs_ShapeEnum tb = b->ShapeType();
    if (ta == TopAbs_FACE && tb == TopAbs_FACE) {
        auto r = k::angleBetweenFaces(TopoDS::Face(a), TopoDS::Face(*b));
        if (!r) return r.error();
        return measurement("angle", "deg", r.value().degrees, {r.value().vertex, r.value().pointOnFirst, r.value().pointOnSecond});
    }
    if (ta == TopAbs_EDGE && tb == TopAbs_EDGE) {
        auto r = k::angleBetweenEdges(TopoDS::Edge(a), TopoDS::Edge(*b));
        if (!r) return r.error();
        return measurement("angle", "deg", r.value().degrees, {r.value().vertex, r.value().pointOnFirst, r.value().pointOnSecond});
    }
    return invalid("an angle is measured between two faces or two edges");
}

// spec {type, a: {kind, index}, b: {kind, index}} on the elements of one shape.
inline k::Result<val> measureElements(const k::TopologyIndex& topo, const val& spec) {
    if (!given(spec) || !isObj(spec) || !spec["type"].isString()) return invalid("spec.type must be a string");
    const std::string type = spec["type"].as<std::string>();
    auto a = elementOf(topo, spec["a"], "spec.a");
    if (!a) return a.error();
    if (type == "length" || type == "area" || type == "radius") return measureShapes(type, a.value(), nullptr);
    auto b = elementOf(topo, spec["b"], "spec.b");
    if (!b) return b.error();
    return measureShapes(type, a.value(), &b.value());
}

// ------------------------------------------------------------------ section

// Section of a shape by the plane {origin, normal}: {area (mm^2, solids only), polylines: [Float32Array
// of xyz triples], faces: number of section regions, wires}. Display deflection in mm.
inline k::Result<val> sectionShape(const k::Shape& shape, const val& plane, double deflection) {
    if (!given(plane) || !isObj(plane)) return invalid("plane must be {origin, normal}");
    auto origin = readVec3(plane["origin"], "plane.origin");
    if (!origin) return origin.error();
    auto normal = readVec3(plane["normal"], "plane.normal");
    if (!normal) return normal.error();
    const k::Point3 n = normal.value();
    if (!(std::hypot(n[0], n[1], n[2]) > 1e-12)) return invalid("plane.normal must not be zero");
    auto s = k::sectionByPlane(shape, k::SectionPlane{origin.value(), n}, deflection);
    if (!s) return s.error();
    val out = val::object();
    out.set("area", s.value().area);
    val lines = val::array();
    for (const std::vector<k::Point3>& line : s.value().polylines) {
        std::vector<float> flat;
        flat.reserve(line.size() * 3);
        for (const k::Point3& p : line) {
            for (double c : p) flat.push_back(static_cast<float>(c));
        }
        lines.call<void>("push", val::global("Float32Array").new_(emscripten::typed_memory_view(flat.size(), flat.data())));
    }
    out.set("polylines", lines);
    return out;
}

}  // namespace bendcore::wasm_analysis
