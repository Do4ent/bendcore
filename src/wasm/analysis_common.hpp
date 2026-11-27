#pragma once

// Kernel analysis shared by the kernel, part and assembly WebAssembly modules
// (docs/ui/KERNEL_VIEWPORT.md, "Измерения и сечение"; docs/ui/PART_MODELING_UI.md): the material
// library of bendcore_kernel_analysis, measurements between B-rep elements (measure.hpp) and the
// section of a shape by a plane (section.hpp), converted to plain JS values. Header-only, so each
// module compiles it with its own bindings; failures are k::Result errors.

#include <bendcore/kernel/material.hpp>
#include <bendcore/kernel/measure.hpp>
#include <bendcore/kernel/reference_mesh.hpp>
#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/section.hpp>
#include <bendcore/kernel/shape.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <BRepBuilderAPI_MakeVertex.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>

#include <cmath>
#include <functional>
#include <memory>
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

// ------------------------------------------------------------------ reference meshes

// Mesh of a module's reference mesh by the key the page passes (part: id string, assembly: id).
using MeshLookup = std::function<k::Result<std::shared_ptr<const k::TriangleMesh>>(const val& key)>;

inline bool isMeshElement(const val& ref) {
    return given(ref) && isObj(ref) && ref["kind"].isString() && ref["kind"].as<std::string>() == "mesh";
}

// Element {kind: 'mesh', mesh, triangle, point: [x, y, z], snap?}: the point of the triangle
// nearest to `point`, snapped to a corner within `snap` mm (docs/kernel/ANALYSIS.md, section 7).
inline k::Result<k::MeshPoint> meshPointOf(const val& ref, const MeshLookup& lookup, const std::string& what) {
    if (!lookup) return invalid(what + ": this document has no reference meshes");
    auto mesh = lookup(ref["mesh"]);
    if (!mesh) return mesh.error();
    if (!ref["triangle"].isNumber()) return invalid(what + ".triangle must be a non-negative integer");
    const double t = ref["triangle"].as<double>();
    if (!(t >= 0) || t != std::floor(t)) return invalid(what + ".triangle must be a non-negative integer");
    auto p = readVec3(ref["point"], what + ".point");
    if (!p) return p.error();
    double snap = 0.0;
    if (given(ref["snap"])) {
        if (!ref["snap"].isNumber() || !(ref["snap"].as<double>() >= 0)) return invalid(what + ".snap must be a non-negative number");
        snap = ref["snap"].as<double>();
    }
    return k::pointOnTriangle(*mesh.value(), static_cast<std::size_t>(t), p.value(), snap);
}

inline val typedFloats(const std::vector<float>& v) {
    return val::global("Float32Array").new_(emscripten::typed_memory_view(v.size(), v.data()));
}

// Mesh for display: {positions: Float32Array (xyz), indices: Uint32Array (3 per triangle)}.
inline val meshToJs(const k::TriangleMesh& mesh) {
    std::vector<float> pos;
    pos.reserve(mesh.vertices.size() * 3);
    for (const k::Point3& p : mesh.vertices)
        for (double c : p) pos.push_back(static_cast<float>(c));
    std::vector<std::uint32_t> idx;
    idx.reserve(mesh.triangles.size() * 3);
    for (const auto& t : mesh.triangles)
        for (std::uint32_t i : t) idx.push_back(i);
    val out = val::object();
    out.set("positions", typedFloats(pos));
    out.set("indices", val::global("Uint32Array").new_(emscripten::typed_memory_view(idx.size(), idx.data())));
    return out;
}

// {vertices, triangles, min, max, size, area, volume (null unless closed), closed, openEdges}.
inline val meshPropertiesToJs(const k::MeshProperties& p) {
    val o = val::object();
    o.set("vertices", static_cast<double>(p.vertices));
    o.set("triangles", static_cast<double>(p.triangles));
    o.set("min", vec3(p.min));
    o.set("max", vec3(p.max));
    o.set("size", vec3({p.max[0] - p.min[0], p.max[1] - p.min[1], p.max[2] - p.min[2]}));
    o.set("area", p.area);
    o.set("volume", p.closed ? val(p.volume) : val::null());
    o.set("closed", p.closed);
    o.set("openEdges", static_cast<double>(p.openEdges));
    return o;
}

inline val meshReportToJs(const k::MeshReadReport& r) {
    val o = val::object();
    o.set("encoding", r.encoding);
    o.set("facetsRead", static_cast<double>(r.facetsRead));
    o.set("degenerateDropped", static_cast<double>(r.degenerateDropped));
    o.set("objects", static_cast<double>(r.objects));
    return o;
}

// Section of a mesh by {origin, normal}: {area, length, polylines: [Float32Array], closed: [bool]}
// (a closed contour repeats its first point at the end).
inline k::Result<val> sectionMeshJs(const k::TriangleMesh& mesh, const val& plane) {
    if (!given(plane) || !isObj(plane)) return invalid("plane must be {origin, normal}");
    auto origin = readVec3(plane["origin"], "plane.origin");
    if (!origin) return origin.error();
    auto normal = readVec3(plane["normal"], "plane.normal");
    if (!normal) return normal.error();
    auto s = k::sectionMesh(mesh, k::SectionPlane{origin.value(), normal.value()});
    if (!s) return s.error();
    val out = val::object();
    out.set("area", s.value().area);
    out.set("length", s.value().length);
    val lines = val::array();
    val closed = val::array();
    for (const k::MeshSectionContour& c : s.value().contours) {
        std::vector<float> flat;
        for (const k::Point3& p : c.points)
            for (double x : p) flat.push_back(static_cast<float>(x));
        if (c.closed && !c.points.empty())
            for (double x : c.points.front()) flat.push_back(static_cast<float>(x));
        lines.call<void>("push", typedFloats(flat));
        closed.call<void>("push", c.closed);
    }
    out.set("polylines", lines);
    out.set("closed", closed);
    return out;
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

// Measurement where one or both elements may be points on reference meshes ({kind: 'mesh', ...}):
// only 'distance' (BRepExtrema between the point as a vertex and the other element, or between two
// points). Specs without mesh elements go to `plain`. Result points: the two nearest points;
// `vertexA`/`vertexB` tell whether a point snapped to a mesh vertex.
using ElementLookup = std::function<k::Result<TopoDS_Shape>(const val& ref, const std::string& what)>;

inline k::Result<val> measureWithMeshes(const val& spec, const MeshLookup& lookup, const ElementLookup& element,
                                        const std::function<k::Result<val>()>& plain) {
    if (!given(spec) || !isObj(spec) || !spec["type"].isString()) return invalid("spec.type must be a string");
    const std::string type = spec["type"].as<std::string>();
    const bool ma = isMeshElement(spec["a"]);
    const bool mb = isMeshElement(spec["b"]);
    if (!ma && !mb) return plain();
    if (type != "distance") return invalid("only a distance is measured on a reference mesh");
    struct Side {
        TopoDS_Shape shape;
        bool snapped = false;
    };
    auto sideOf = [&](const val& ref, bool mesh, const std::string& what) -> k::Result<Side> {
        if (mesh) {
            auto p = meshPointOf(ref, lookup, what);
            if (!p) return p.error();
            const k::Point3& q = p.value().point;
            return Side{BRepBuilderAPI_MakeVertex(gp_Pnt(q[0], q[1], q[2])).Shape(), p.value().vertex.has_value()};
        }
        auto e = element(ref, what);
        if (!e) return e.error();
        return Side{e.value(), false};
    };
    auto a = sideOf(spec["a"], ma, "spec.a");
    if (!a) return a.error();
    auto b = sideOf(spec["b"], mb, "spec.b");
    if (!b) return b.error();
    auto r = k::minimumDistance(a.value().shape, b.value().shape);
    if (!r) return r.error();
    val o = measurement("distance", "mm", r.value().distance, {r.value().pointOnFirst, r.value().pointOnSecond});
    o.set("vertexA", a.value().snapped);
    o.set("vertexB", b.value().snapped);
    return o;
}

}  // namespace bendcore::wasm_analysis
