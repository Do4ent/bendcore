// WebAssembly module of the part document (bendcore_modeling_wasm, docs/ui/PART_MODELING_UI.md).
//
// Part documents (bendcore::modeling::PartDocument) live in a registry inside the module and are
// addressed from JS by an integer handle; JS releases them with docDispose(handle). Every entry
// point returns a plain JS object: {ok: true, value} or {ok: false, code, message} with the
// kernel ErrorCode, so no C++ exception crosses into JS. The ES wrapper
// (bendcore_modeling_api.mjs) turns failures into KernelError exceptions.
//
// Shapes are addressed by a target string:
//   "final"          last valid body of the active part of the tree (PartDocument::finalShape)
//   "before:<id>"    body seen by feature <id> (PartDocument::inputOf)
//   "after:<id>"     body produced by feature <id> (only body features in state ok/warning)
//   "sketch:<id>"    faces of the selected regions of sketch <id>
// Element indices of a target follow TopExp::MapShapes order, i.e. they coincide with the
// faceIndex/edgeIndex of its tessellation and with the element map of its NamedShape.

#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>
#include <bendcore/kernel/tessellation.hpp>
#include <bendcore/modeling/naming.hpp>
#include <bendcore/modeling/part_document.hpp>
#include <bendcore/modeling/part_features.hpp>
#include <bendcore/modeling/part_sketch.hpp>

#include "modeling_bindings_internal.hpp"

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>

#include <cmath>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using emscripten::val;
namespace k = bendcore::kernel;
namespace m = bendcore::modeling;

namespace bendcore::wasm_modeling {

std::map<std::uint32_t, std::unique_ptr<m::PartDocument>>& documents() {
    static std::map<std::uint32_t, std::unique_ptr<m::PartDocument>> docs;
    return docs;
}

std::uint32_t& nextHandle() {
    static std::uint32_t next = 1;
    return next;
}

const std::shared_ptr<const m::FeatureRegistry>& registry() {
    static const std::shared_ptr<const m::FeatureRegistry> r = m::partRegistry();
    return r;
}

val success(val value) {
    val out = val::object();
    out.set("ok", true);
    out.set("value", std::move(value));
    return out;
}

val failure(const std::string& code, const std::string& message) {
    val out = val::object();
    out.set("ok", false);
    out.set("code", code);
    out.set("message", message);
    return out;
}

val failure(const k::Error& error) { return failure(k::toString(error.code), error.message); }

// ------------------------------------------------------------------ JS value helpers

bool present(const val& v) { return !v.isUndefined() && !v.isNull(); }

// Non-null object (arrays included).
bool isObject(const val& v) { return present(v) && v.typeOf().as<std::string>() == "object"; }

std::string requireString(const val& v, const std::string& what) {
    if (!v.isString()) throw InputError(what + " must be a string");
    return v.as<std::string>();
}

// Expressions accept strings and finite numbers (a number becomes its shortest JS spelling).
std::string expression(const val& v, const std::string& what) {
    if (v.isString()) return v.as<std::string>();
    if (v.isNumber()) {
        const double d = v.as<double>();
        if (!std::isfinite(d)) throw InputError(what + " must be finite");
        return val::global("String")(v).as<std::string>();
    }
    throw InputError(what + " must be an expression string or a number");
}

int requireInt(const val& v, const std::string& what) {
    if (!v.isNumber()) throw InputError(what + " must be an integer");
    const double d = v.as<double>();
    if (d != std::floor(d) || std::abs(d) > 1e9) throw InputError(what + " must be an integer");
    return static_cast<int>(d);
}

int optionalInt(const val& obj, const char* field, int fallback, const std::string& what) {
    const val v = obj[field];
    return present(v) ? requireInt(v, what + "." + field) : fallback;
}

bool optionalBool(const val& obj, const char* field, bool fallback, const std::string& what) {
    const val v = obj[field];
    if (!present(v)) return fallback;
    if (v.isTrue()) return true;
    if (v.isFalse()) return false;
    throw InputError(what + "." + field + " must be a boolean");
}

std::vector<val> arrayItems(const val& v, const std::string& what) {
    if (!val::global("Array").call<bool>("isArray", v)) throw InputError(what + " must be an array");
    const auto n = v["length"].as<std::size_t>();
    std::vector<val> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) out.push_back(v[i]);
    return out;
}

std::vector<std::string> objectKeys(const val& v, const std::string& what) {
    if (!isObject(v) || val::global("Array").call<bool>("isArray", v)) throw InputError(what + " must be an object");
    std::vector<std::string> out;
    for (const val& key : arrayItems(val::global("Object").call<val>("keys", v), what)) out.push_back(key.as<std::string>());
    return out;
}

std::map<std::string, std::string> stringMap(const val& v, const std::string& what, bool expressions) {
    std::map<std::string, std::string> out;
    for (const std::string& key : objectKeys(v, what)) {
        const val item = v[key];
        out[key] = expressions ? expression(item, what + "." + key) : requireString(item, what + "." + key);
    }
    return out;
}

val point(const k::Point3& p) {
    val out = val::array();
    out.call<void>("push", p[0]);
    out.call<void>("push", p[1]);
    out.call<void>("push", p[2]);
    return out;
}

val stringArray(const std::vector<std::string>& items) {
    val out = val::array();
    for (const std::string& s : items) out.call<void>("push", s);
    return out;
}

// ------------------------------------------------------------------ sketch data <-> JS

val placementJs(const m::SketchPlacement& p) {
    auto dir = [](const gp_Dir& d) { return point({d.X(), d.Y(), d.Z()}); };
    val o = val::object();
    const gp_Pnt c = p.origin();
    o.set("origin", point({c.X(), c.Y(), c.Z()}));
    o.set("xDirection", dir(p.xDirection()));
    o.set("yDirection", dir(p.yDirection()));
    o.set("normal", dir(p.normal()));
    return o;
}

val toJs(const m::SketchData& d) {
    val points = val::array();
    for (const auto& p : d.points) {
        val o = val::object();
        o.set("x", p.x);
        o.set("y", p.y);
        o.set("fixed", p.fixed);
        points.call<void>("push", o);
    }
    val geometry = val::array();
    for (const auto& g : d.geometry) {
        val o = val::object();
        o.set("type", std::string(m::toString(g.type)));
        o.set("start", g.start);
        o.set("end", g.end);
        o.set("center", g.center);
        o.set("radius", g.radius);
        o.set("counterClockwise", g.counterClockwise);
        o.set("construction", g.construction);
        o.set("projection", g.projection);
        if (g.type == m::SketchGeometryType::Ellipse || g.type == m::SketchGeometryType::EllipticalArc) {
            o.set("focus", g.focus);
        }
        if (g.type == m::SketchGeometryType::BSpline) {
            val poles = val::array(), knots = val::array(), mults = val::array(), weights = val::array();
            for (int q : g.poles) poles.call<void>("push", q);
            for (double k : g.knots) knots.call<void>("push", k);
            for (int k : g.multiplicities) mults.call<void>("push", k);
            for (double w : g.weights) weights.call<void>("push", w);
            o.set("poles", poles);
            o.set("degree", g.degree);
            o.set("knots", knots);
            o.set("multiplicities", mults);
            o.set("weights", weights);
            o.set("periodic", g.periodic);
        }
        geometry.call<void>("push", o);
    }
    val constraints = val::array();
    for (const auto& c : d.constraints) {
        val o = val::object();
        o.set("type", std::string(m::toString(c.type)));
        o.set("a", c.a);
        o.set("b", c.b);
        o.set("c", c.c);
        o.set("value", c.value);
        if (c.driven) o.set("driven", true);
        constraints.call<void>("push", o);
    }
    val regions = val::array();
    for (m::SketchEntityId r : d.regions) regions.call<void>("push", static_cast<double>(r));
    val out = val::object();
    out.set("points", points);
    out.set("geometry", geometry);
    out.set("constraints", constraints);
    out.set("regions", regions);
    return out;
}

m::SketchData sketchFromJs(const val& v) {
    if (!isObject(v)) throw InputError("sketch must be an object");
    m::SketchData d;
    std::size_t i = 0;
    for (const val& p : arrayItems(v["points"], "sketch.points")) {
        const std::string what = "sketch.points[" + std::to_string(i++) + "]";
        d.points.push_back({expression(p["x"], what + ".x"), expression(p["y"], what + ".y"),
                            optionalBool(p, "fixed", false, what)});
    }
    i = 0;
    for (const val& g : arrayItems(v["geometry"], "sketch.geometry")) {
        const std::string what = "sketch.geometry[" + std::to_string(i++) + "]";
        m::SketchGeometryDef def;
        const std::string type = requireString(g["type"], what + ".type");
        const auto t = m::sketchGeometryTypeFromString(type);
        if (!t) throw InputError(what + ".type: unknown curve type '" + type + "' (line, circle, arc, ellipse, elliptical-arc, bspline)");
        def.type = *t;
        def.start = optionalInt(g, "start", -1, what);
        def.end = optionalInt(g, "end", -1, what);
        def.center = optionalInt(g, "center", -1, what);
        if (present(g["radius"])) def.radius = expression(g["radius"], what + ".radius");
        def.counterClockwise = optionalBool(g, "counterClockwise", true, what);
        def.construction = optionalBool(g, "construction", false, what);
        def.projection = optionalInt(g, "projection", -1, what);
        def.focus = optionalInt(g, "focus", -1, what);
        def.degree = optionalInt(g, "degree", 3, what);
        def.periodic = optionalBool(g, "periodic", false, what);
        if (present(g["poles"])) {
            for (const val& q : arrayItems(g["poles"], what + ".poles")) def.poles.push_back(requireInt(q, what + ".poles[]"));
        }
        if (present(g["multiplicities"])) {
            for (const val& q : arrayItems(g["multiplicities"], what + ".multiplicities")) {
                def.multiplicities.push_back(requireInt(q, what + ".multiplicities[]"));
            }
        }
        for (const char* field : {"knots", "weights"}) {
            if (!present(g[field])) continue;
            auto& out = std::string(field) == "knots" ? def.knots : def.weights;
            for (const val& q : arrayItems(g[field], what + "." + field)) {
                if (!q.isNumber()) throw InputError(what + "." + field + "[] must be a number");
                out.push_back(q.as<double>());
            }
        }
        d.geometry.push_back(std::move(def));
    }
    i = 0;
    const val constraints = v["constraints"];
    if (present(constraints)) {
        for (const val& c : arrayItems(constraints, "sketch.constraints")) {
            const std::string what = "sketch.constraints[" + std::to_string(i++) + "]";
            m::SketchConstraintDef def;
            const std::string type = requireString(c["type"], what + ".type");
            const auto t = m::sketchConstraintTypeFromString(type);
            if (!t) throw InputError(what + ".type: unknown constraint type '" + type + "'");
            def.type = *t;
            def.a = optionalInt(c, "a", -1, what);
            def.b = optionalInt(c, "b", -1, what);
            def.c = optionalInt(c, "c", -1, what);
            if (present(c["value"])) def.value = expression(c["value"], what + ".value");
            def.driven = optionalBool(c, "driven", false, what);
            d.constraints.push_back(std::move(def));
        }
    }
    const val regions = v["regions"];
    if (present(regions)) {
        for (const val& r : arrayItems(regions, "sketch.regions")) {
            const int id = requireInt(r, "sketch.regions[]");
            if (id < 1) throw InputError("sketch.regions: entity ids start at 1");
            d.regions.push_back(static_cast<m::SketchEntityId>(id));
        }
    }
    return d;
}

// ------------------------------------------------------------------ references <-> JS

m::ElementKind kindFromJs(const val& v, const std::string& what) {
    const std::string text = requireString(v, what);
    const auto kind = m::elementKindFromString(text);
    if (!kind) throw InputError(what + ": unknown element kind '" + text + "' (face, edge, vertex)");
    return *kind;
}

val toJs(const m::GeometricSignature& s) {
    val o = val::object();
    o.set("valid", s.valid);
    o.set("kind", std::string(m::toString(s.kind)));
    o.set("geometry", s.geometry);
    o.set("center", point(s.center));
    o.set("hasDirection", s.hasDirection);
    o.set("direction", point(s.direction));
    o.set("radius", s.radius);
    o.set("size", s.size);
    return o;
}

k::Point3 pointFromJs(const val& v, const std::string& what) {
    const std::vector<val> items = arrayItems(v, what);
    if (items.size() != 3) throw InputError(what + " must have 3 components");
    k::Point3 p{};
    for (std::size_t i = 0; i < 3; ++i) {
        if (!items[i].isNumber()) throw InputError(what + " must hold numbers");
        p[i] = items[i].as<double>();
    }
    return p;
}

val toJs(const m::TopoReference& r) {
    val o = val::object();
    o.set("name", r.name);
    o.set("kind", std::string(m::toString(r.kind)));
    o.set("signature", toJs(r.signature));
    return o;
}

m::TopoReference referenceFromJs(const val& v, const std::string& what) {
    if (!isObject(v)) throw InputError(what + " must be a reference object {name, kind, signature}");
    m::TopoReference r;
    r.name = requireString(v["name"], what + ".name");
    r.kind = kindFromJs(v["kind"], what + ".kind");
    const val s = v["signature"];
    if (present(s)) {
        r.signature.valid = optionalBool(s, "valid", false, what + ".signature");
        if (r.signature.valid) {
            r.signature.kind = kindFromJs(s["kind"], what + ".signature.kind");
            r.signature.geometry = requireString(s["geometry"], what + ".signature.geometry");
            r.signature.center = pointFromJs(s["center"], what + ".signature.center");
            r.signature.hasDirection = optionalBool(s, "hasDirection", false, what + ".signature");
            r.signature.direction = pointFromJs(s["direction"], what + ".signature.direction");
            if (!s["radius"].isNumber() || !s["size"].isNumber()) {
                throw InputError(what + ".signature: radius and size must be numbers");
            }
            r.signature.radius = s["radius"].as<double>();
            r.signature.size = s["size"].as<double>();
        }
    }
    return r;
}

std::map<std::string, std::vector<m::TopoReference>> referencesFromJs(const val& v, const std::string& what) {
    std::map<std::string, std::vector<m::TopoReference>> out;
    for (const std::string& slot : objectKeys(v, what)) {
        std::vector<m::TopoReference> refs;
        std::size_t i = 0;
        for (const val& r : arrayItems(v[slot], what + "." + slot)) {
            refs.push_back(referenceFromJs(r, what + "." + slot + "[" + std::to_string(i++) + "]"));
        }
        out[slot] = std::move(refs);
    }
    return out;
}

// ------------------------------------------------------------------ document views

val toJs(const m::RebuildReport& r) {
    val o = val::object();
    o.set("recomputed", stringArray(r.recomputed));
    o.set("reused", stringArray(r.reused));
    o.set("fromCache", stringArray(r.fromCache));
    o.set("failed", stringArray(r.failed));
    return o;
}

val toJs(const std::vector<m::TransactionInfo>& items) {
    val out = val::array();
    for (const auto& t : items) {
        val o = val::object();
        o.set("name", t.name);
        o.set("edits", static_cast<double>(t.edits));
        out.call<void>("push", o);
    }
    return out;
}

val featureView(const m::FeatureInfo& f, std::size_t index) {
    val o = val::object();
    o.set("index", static_cast<double>(index));
    o.set("id", f.id);
    o.set("type", f.type);
    o.set("label", f.label);
    o.set("suppressed", f.suppressed);
    o.set("state", std::string(m::toString(f.state)));
    o.set("messages", stringArray(f.messages));
    val params = val::object();
    for (const auto& [name, expr] : f.parameters) params.set(name, expr);
    o.set("parameters", params);
    val values = val::object();
    for (const auto& [name, v] : f.values) values.set(name, v);
    o.set("values", values);
    val options = val::object();
    for (const auto& [name, v] : f.options) options.set(name, v);
    o.set("options", options);
    val links = val::object();
    for (const auto& [slot, target] : f.links) links.set(slot, target);
    o.set("links", links);
    val refs = val::object();
    for (const auto& [slot, list] : f.references) {
        val arr = val::array();
        for (const auto& r : list) arr.call<void>("push", toJs(r));
        refs.set(slot, arr);
    }
    o.set("references", refs);
    val reports = val::array();
    for (const auto& rep : f.referenceReports) {
        val r = val::object();
        r.set("slot", rep.slot);
        r.set("name", rep.reference.name);
        r.set("kind", std::string(m::toString(rep.reference.kind)));
        r.set("method", std::string(m::toString(rep.resolution.method)));
        r.set("names", stringArray(rep.resolution.names));
        r.set("detail", rep.resolution.detail);
        reports.call<void>("push", r);
    }
    o.set("referenceReports", reports);
    o.set("hasBody", static_cast<bool>(f.output));
    if (f.sketch) o.set("sketch", toJs(*f.sketch));
    if (f.sketchResult) {
        val s = val::object();
        s.set("dof", f.sketchResult->dof);
        s.set("solverStatus", f.sketchResult->solverStatus);
        s.set("curves", static_cast<double>(f.sketchResult->curves.size()));
        s.set("regions", static_cast<double>(f.sketchResult->regions.size()));
        s.set("standalonePoints", static_cast<double>(f.sketchResult->standalonePoints.size()));
        s.set("hasFaces", static_cast<bool>(f.sketchResult->faces));
        s.set("definition", f.sketchResult->definition);
        val redundant = val::array(), pointDefined = val::array(), entityDefined = val::array(), values = val::array();
        for (int k : f.sketchResult->redundant) redundant.call<void>("push", k);
        for (bool b : f.sketchResult->pointDefined) pointDefined.call<void>("push", b);
        for (bool b : f.sketchResult->entityDefined) entityDefined.call<void>("push", b);
        for (double v : f.sketchResult->constraintValues) values.call<void>("push", v);
        s.set("redundant", redundant);
        s.set("pointDefined", pointDefined);
        s.set("entityDefined", entityDefined);
        s.set("constraintValues", values);
        s.set("placement", placementJs(f.sketchResult->placement));
        o.set("sketchResult", s);
    }
    if (f.datum) {
        val d = val::object();
        if (f.datum->plane) {
            const gp_Pln& p = *f.datum->plane;
            d.set("planeOrigin", point({p.Location().X(), p.Location().Y(), p.Location().Z()}));
            const gp_Dir n = p.Axis().Direction();
            d.set("planeNormal", point({n.X(), n.Y(), n.Z()}));
        }
        if (f.datum->axis) {
            const gp_Ax1& a = *f.datum->axis;
            d.set("axisOrigin", point({a.Location().X(), a.Location().Y(), a.Location().Z()}));
            d.set("axisDirection", point({a.Direction().X(), a.Direction().Y(), a.Direction().Z()}));
        }
        o.set("datum", d);
    }
    return o;
}

// Shape addressed by a target string (see the file header).
m::NamedShape targetShape(const m::PartDocument& doc, const val& targetValue) {
    const std::string target = present(targetValue) ? requireString(targetValue, "target") : std::string("final");
    if (target == "final") return doc.finalShape();
    const auto colon = target.find(':');
    if (colon == std::string::npos) {
        throw InputError("unknown target '" + target + "' (final, before:<id>, after:<id>, sketch:<id>)");
    }
    const std::string kind = target.substr(0, colon);
    const std::string id = target.substr(colon + 1);
    if (kind == "before") {
        auto s = doc.inputOf(id);
        if (!s) throw InputError(s.error().message);
        return std::move(s).value();
    }
    auto info = doc.feature(id);
    if (!info) throw InputError(info.error().message);
    if (kind == "after") {
        if (!info.value().output) {
            throw InputError("feature '" + id + "' has no body result (state " +
                             std::string(m::toString(info.value().state)) + ")");
        }
        return *info.value().output;
    }
    if (kind == "sketch") {
        if (!info.value().sketchResult || !info.value().sketchResult->faces) {
            throw InputError("feature '" + id + "' has no sketch regions");
        }
        return *info.value().sketchResult->faces;
    }
    throw InputError("unknown target kind '" + kind + "' (final, before, after, sketch)");
}

// ------------------------------------------------------------------ module-level functions

val modelingInfo() {
    val out = val::object();
    out.set("occtVersion", std::string(OCC_VERSION_COMPLETE));
    out.set("liveDocuments", static_cast<double>(documents().size()));
    out.set("formatVersion", m::PartDocument::kFormatVersion);
    return out;
}

val featureCatalog() {
    return guarded([] {
        val out = val::array();
        for (const std::string& type : registry()->types()) {
            const std::unique_ptr<m::Feature> f = registry()->create(type);
            val o = val::object();
            o.set("type", type);
            val params = val::array();
            for (const auto& p : f->parameterSpecs()) {
                val e = val::object();
                e.set("name", p.name);
                e.set("defaultExpression", p.defaultExpression);
                params.call<void>("push", e);
            }
            o.set("parameters", params);
            val refs = val::array();
            for (const auto& r : f->referenceSlots()) {
                val e = val::object();
                e.set("name", r.name);
                e.set("kind", std::string(m::toString(r.kind)));
                e.set("multiple", r.multiple);
                e.set("optional", r.optional);
                refs.call<void>("push", e);
            }
            o.set("references", refs);
            val options = val::array();
            for (const auto& s : f->optionSpecs()) {
                val e = val::object();
                e.set("name", s.name);
                e.set("allowed", stringArray(s.allowed));
                options.call<void>("push", e);
            }
            o.set("options", options);
            val links = val::array();
            for (const auto& l : f->linkSlots()) {
                val e = val::object();
                e.set("name", l.name);
                e.set("types", stringArray(l.types));
                e.set("optional", l.optional);
                links.call<void>("push", e);
            }
            o.set("links", links);
            o.set("hasSketch", f->hasSketchData());
            out.call<void>("push", o);
        }
        return success(out);
    });
}

// Sketch data built by SketchBuilder from profile templates:
//   {profiles: [{kind: 'rectangle', x, y, width, height}, {kind: 'circle', cx, cy, radius},
//               {kind: 'polygon', points: [[x, y], ...]}, {kind: 'point', x, y}]}
// Rectangle: fixed corner, horizontal/vertical sides and Distance dimensions; circle: fixed
// centre and Radius dimension; polygon: fixed corners; point: fixed standalone point (hole centre).
val sketchTemplate(val spec) {
    return guarded([&] {
        if (!isObject(spec)) throw InputError("sketchTemplate expects {profiles: [...]}");
        const std::vector<val> profiles = arrayItems(spec["profiles"], "profiles");
        if (profiles.empty()) throw InputError("profiles must not be empty");
        m::SketchBuilder b;
        std::size_t i = 0;
        for (const val& p : profiles) {
            const std::string what = "profiles[" + std::to_string(i++) + "]";
            if (!isObject(p)) throw InputError(what + " must be an object");
            const std::string kind = requireString(p["kind"], what + ".kind");
            if (kind == "rectangle") {
                b.rectangle(expression(p["x"], what + ".x"), expression(p["y"], what + ".y"),
                            expression(p["width"], what + ".width"), expression(p["height"], what + ".height"));
            } else if (kind == "circle") {
                b.fixedCircle(expression(p["cx"], what + ".cx"), expression(p["cy"], what + ".cy"),
                              expression(p["radius"], what + ".radius"));
            } else if (kind == "polygon") {
                std::vector<std::pair<std::string, std::string>> corners;
                std::size_t j = 0;
                for (const val& c : arrayItems(p["points"], what + ".points")) {
                    const std::string cw = what + ".points[" + std::to_string(j++) + "]";
                    const std::vector<val> xy = arrayItems(c, cw);
                    if (xy.size() != 2) throw InputError(cw + " must be [x, y]");
                    corners.emplace_back(expression(xy[0], cw + "[0]"), expression(xy[1], cw + "[1]"));
                }
                if (corners.size() < 3) throw InputError(what + ".points: a polygon needs at least 3 corners");
                b.fixedPolygon(corners);
            } else if (kind == "point") {
                b.point(expression(p["x"], what + ".x"), expression(p["y"], what + ".y"), true);
            } else {
                throw InputError(what + ".kind: unknown profile '" + kind + "' (rectangle, circle, polygon, point)");
            }
        }
        return success(toJs(b.build()));
    });
}

val docCreate() {
    return guarded([] {
        const std::uint32_t handle = nextHandle()++;
        documents().emplace(handle, std::make_unique<m::PartDocument>(registry()));
        return success(val(handle));
    });
}

val docLoad(val json) {
    return guarded([&] {
        const std::string text = requireString(json, "document JSON");
        m::RebuildReport report;
        auto doc = m::PartDocument::fromJson(text, registry(), &report);
        if (!doc) return failure(doc.error());
        const std::uint32_t handle = nextHandle()++;
        documents().emplace(handle, std::make_unique<m::PartDocument>(std::move(doc).value()));
        val out = val::object();
        out.set("handle", handle);
        out.set("report", toJs(report));
        return success(out);
    });
}

val docDispose(std::uint32_t handle) {
    if (documents().erase(handle) == 0) {
        return failure("InvalidInput", "unknown document handle " + std::to_string(handle) +
                                           " (never created or already disposed)");
    }
    return success(val::undefined());
}

val docSave(std::uint32_t handle, val options) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        m::SaveOptions o;
        if (present(options)) o.includeCache = optionalBool(options, "includeCache", false, "options");
        auto json = doc.toJson(o);
        if (!json) return failure(json.error());
        return success(val(json.value()));
    });
}

val status(const m::Status& s) { return s ? success(val::undefined()) : failure(s.error()); }

val docVariables(std::uint32_t handle) {
    return withDocument(handle, [](m::PartDocument& doc) {
        val out = val::array();
        for (const auto& [name, expr] : doc.variables()) {
            val o = val::object();
            o.set("name", name);
            o.set("expression", expr);
            auto v = doc.variableValue(name);
            if (v) {
                o.set("value", v.value());
                o.set("error", val::null());
            } else {
                o.set("value", val::null());
                o.set("error", v.error().message);
            }
            out.call<void>("push", o);
        }
        return success(out);
    });
}

val docSetVariable(std::uint32_t handle, val name, val expr) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        return status(doc.setVariable(requireString(name, "variable name"), expression(expr, "variable expression")));
    });
}

val docRemoveVariable(std::uint32_t handle, val name) {
    return withDocument(handle,
                        [&](m::PartDocument& doc) { return status(doc.removeVariable(requireString(name, "variable name"))); });
}

val docFeatures(std::uint32_t handle) {
    return withDocument(handle, [](m::PartDocument& doc) {
        val out = val::array();
        const std::vector<std::string> ids = doc.featureIds();
        for (std::size_t i = 0; i < ids.size(); ++i) {
            auto info = doc.feature(ids[i]);
            if (!info) return failure(info.error());
            out.call<void>("push", featureView(info.value(), i));
        }
        return success(out);
    });
}

val docState(std::uint32_t handle) {
    return withDocument(handle, [](m::PartDocument& doc) {
        val o = val::object();
        o.set("size", static_cast<double>(doc.size()));
        o.set("rollback", doc.rollback() ? val(static_cast<double>(*doc.rollback())) : val::null());
        o.set("canUndo", doc.canUndo());
        o.set("canRedo", doc.canRedo());
        o.set("inTransaction", doc.inTransaction());
        o.set("undoHistory", toJs(doc.undoHistory()));
        o.set("redoHistory", toJs(doc.redoHistory()));
        return success(o);
    });
}

std::optional<std::size_t> positionFromJs(const val& v, const char* what) {
    if (!present(v)) return std::nullopt;
    const int p = requireInt(v, what);
    if (p < 0) throw InputError(std::string(what) + " must be >= 0");
    return static_cast<std::size_t>(p);
}

val docAddFeature(std::uint32_t handle, val spec, val position) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        if (!isObject(spec)) throw InputError("feature spec must be an object");
        m::FeatureSpec s;
        s.type = requireString(spec["type"], "spec.type");
        if (present(spec["id"])) s.id = requireString(spec["id"], "spec.id");
        if (present(spec["label"])) s.label = requireString(spec["label"], "spec.label");
        if (present(spec["parameters"])) s.parameters = stringMap(spec["parameters"], "spec.parameters", true);
        if (present(spec["options"])) s.options = stringMap(spec["options"], "spec.options", false);
        if (present(spec["links"])) s.links = stringMap(spec["links"], "spec.links", false);
        if (present(spec["references"])) s.references = referencesFromJs(spec["references"], "spec.references");
        if (present(spec["sketch"])) s.sketch = sketchFromJs(spec["sketch"]);
        s.suppressed = optionalBool(spec, "suppressed", false, "spec");
        auto id = doc.addFeature(std::move(s), positionFromJs(position, "position"));
        if (!id) return failure(id.error());
        return success(val(id.value()));
    });
}

// Applies several edits of one feature. Outside an open transaction they form one undo entry
// ("Edit <id>"); a failing edit restores the state before the call.
val docEditFeature(std::uint32_t handle, val idValue, val edits) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        const std::string id = requireString(idValue, "feature id");
        if (!isObject(edits)) throw InputError("edits must be an object");
        // Convert everything first: malformed input changes nothing.
        std::optional<std::string> label;
        if (present(edits["label"])) label = requireString(edits["label"], "edits.label");
        std::map<std::string, std::string> params, options, links;
        if (present(edits["parameters"])) params = stringMap(edits["parameters"], "edits.parameters", true);
        if (present(edits["options"])) options = stringMap(edits["options"], "edits.options", false);
        if (present(edits["links"])) links = stringMap(edits["links"], "edits.links", false);
        std::map<std::string, std::vector<m::TopoReference>> refs;
        if (present(edits["references"])) refs = referencesFromJs(edits["references"], "edits.references");
        std::optional<m::SketchData> sketch;
        if (present(edits["sketch"])) sketch = sketchFromJs(edits["sketch"]);
        std::optional<bool> suppressed;
        if (present(edits["suppressed"])) suppressed = optionalBool(edits, "suppressed", false, "edits");

        if (!doc.indexOf(id)) return failure("InvalidInput", "no feature '" + id + "'");
        const bool own = !doc.inTransaction();
        if (own) {
            if (auto s = doc.beginTransaction("Edit " + id); !s) return failure(s.error());
        }
        auto apply = [&]() -> m::Status {
            if (label) {
                if (auto s = doc.setLabel(id, *label); !s) return s;
            }
            for (const auto& [n, e] : params) {
                if (auto s = doc.setParameter(id, n, e); !s) return s;
            }
            for (const auto& [n, v] : options) {
                if (auto s = doc.setOption(id, n, v); !s) return s;
            }
            for (const auto& [slot, target] : links) {
                if (auto s = doc.setLink(id, slot, target); !s) return s;
            }
            for (auto& [slot, list] : refs) {
                if (auto s = doc.setReferences(id, slot, list); !s) return s;
            }
            if (sketch) {
                if (auto s = doc.setSketch(id, *sketch); !s) return s;
            }
            if (suppressed) {
                if (auto s = doc.setSuppressed(id, *suppressed); !s) return s;
            }
            return std::monostate{};
        };
        const m::Status result = apply();
        if (own) {
            if (result) {
                if (auto s = doc.commitTransaction(); !s) return failure(s.error());
            } else {
                doc.abortTransaction();
            }
        }
        return status(result);
    });
}

val docRemoveFeature(std::uint32_t handle, val id) {
    return withDocument(handle,
                        [&](m::PartDocument& doc) { return status(doc.removeFeature(requireString(id, "feature id"))); });
}

val docMoveFeature(std::uint32_t handle, val id, val index) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        const auto p = positionFromJs(index, "index");
        if (!p) throw InputError("index is required");
        return status(doc.moveFeature(requireString(id, "feature id"), *p));
    });
}

val docSetSuppressed(std::uint32_t handle, val id, val suppressed) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        if (!suppressed.isTrue() && !suppressed.isFalse()) throw InputError("suppressed must be a boolean");
        return status(doc.setSuppressed(requireString(id, "feature id"), suppressed.isTrue()));
    });
}

val docSetRollback(std::uint32_t handle, val position) {
    return withDocument(handle,
                        [&](m::PartDocument& doc) { return status(doc.setRollback(positionFromJs(position, "position"))); });
}

val docBeginTransaction(std::uint32_t handle, val name) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        return status(doc.beginTransaction(requireString(name, "transaction name")));
    });
}

val docCommitTransaction(std::uint32_t handle) {
    return withDocument(handle, [](m::PartDocument& doc) { return status(doc.commitTransaction()); });
}

val docAbortTransaction(std::uint32_t handle) {
    return withDocument(handle, [](m::PartDocument& doc) { return status(doc.abortTransaction()); });
}

val docUndo(std::uint32_t handle) {
    return withDocument(handle, [](m::PartDocument& doc) {
        auto r = doc.undo();
        return r ? success(toJs(r.value())) : failure(r.error());
    });
}

val docRedo(std::uint32_t handle) {
    return withDocument(handle, [](m::PartDocument& doc) {
        auto r = doc.redo();
        return r ? success(toJs(r.value())) : failure(r.error());
    });
}

val docRebuild(std::uint32_t handle) {
    return withDocument(handle, [](m::PartDocument& doc) { return success(toJs(doc.rebuild())); });
}

std::size_t elementIndex(const m::NamedShape& shape, m::ElementKind kind, const val& index) {
    const int i = requireInt(index, "element index");
    if (i < 0 || static_cast<std::size_t>(i) >= shape.count(kind)) {
        throw InputError(std::string(m::toString(kind)) + " index " + std::to_string(i) + " is out of range (target has " +
                         std::to_string(shape.count(kind)) + ")");
    }
    return static_cast<std::size_t>(i);
}

// Reference to element `index` of `kind` in the target shape: its current name and signature.
val docMakeReference(std::uint32_t handle, val target, val kind, val index) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        const m::NamedShape shape = targetShape(doc, target);
        const m::ElementKind k = kindFromJs(kind, "kind");
        const std::size_t i = elementIndex(shape, k, index);
        auto ref = m::makeReference(shape, shape.element(k, i));
        if (!ref) return failure(ref.error());
        return success(toJs(ref.value()));
    });
}

val docElement(std::uint32_t handle, val target, val kind, val index) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        const m::NamedShape shape = targetShape(doc, target);
        const m::ElementKind k = kindFromJs(kind, "kind");
        const std::size_t i = elementIndex(shape, k, index);
        const m::ElementRecord& r = shape.record(k, i);
        val o = val::object();
        o.set("index", static_cast<double>(i));
        o.set("kind", std::string(m::toString(k)));
        o.set("name", r.name);
        o.set("origin", std::string(m::toString(r.origin)));
        o.set("feature", r.feature);
        o.set("aliases", stringArray(r.aliases));
        o.set("signature", toJs(r.signature));
        return success(o);
    });
}

double numberField(const val& params, const char* name, double fallback) {
    const val v = params[name];
    if (!present(v)) return fallback;
    if (!v.isNumber()) throw InputError(std::string(name) + " must be a number");
    return v.as<double>();
}

val docTessellate(std::uint32_t handle, val target, val params) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        const m::NamedShape shape = targetShape(doc, target);
        k::TessellationParams p;
        if (present(params)) {
            p.linearDeflection = numberField(params, "linearDeflection", p.linearDeflection);
            p.angularDeflection = numberField(params, "angularDeflection", p.angularDeflection);
            p.relative = optionalBool(params, "relative", p.relative, "params");
            p.requireAllFaces = optionalBool(params, "requireAllFaces", p.requireAllFaces, "params");
        }
        p.inParallel = false;  // single-threaded module (no -pthread)
        val out = val::object();
        if (shape.isNull()) {
            // Empty tree or nothing built yet: an empty mesh, not an error.
            const std::vector<float> nof;
            const std::vector<std::uint32_t> noi;
            out.set("positions", typedArray("Float32Array", nof));
            out.set("normals", typedArray("Float32Array", nof));
            out.set("indices", typedArray("Uint32Array", noi));
            out.set("faceSpans", typedArray("Uint32Array", noi));
            out.set("edges", val::array());
            out.set("missingFaces", typedArray("Uint32Array", noi));
            out.set("missingEdges", typedArray("Uint32Array", noi));
            return success(out);
        }
        auto mesh = k::tessellate(k::Shape(shape.shape()), p);
        if (!mesh) return failure(mesh.error());
        const k::Mesh& msh = mesh.value();
        std::vector<std::uint32_t> spans;
        spans.reserve(msh.faceSpans.size() * 3);
        for (const k::FaceSpan& s : msh.faceSpans) {
            spans.push_back(s.faceIndex);
            spans.push_back(s.firstIndex);
            spans.push_back(s.indexCount);
        }
        val edges = val::array();
        for (const k::EdgePolyline& poly : msh.edgePolylines) {
            val e = val::object();
            e.set("edgeIndex", poly.edgeIndex);
            e.set("points", typedArray("Float32Array", poly.points));
            edges.call<void>("push", e);
        }
        out.set("positions", typedArray("Float32Array", msh.positions));
        out.set("normals", typedArray("Float32Array", msh.normals));
        out.set("indices", typedArray("Uint32Array", msh.indices));
        out.set("faceSpans", typedArray("Uint32Array", spans));
        out.set("edges", edges);
        out.set("missingFaces", typedArray("Uint32Array", msh.missingFaces));
        out.set("missingEdges", typedArray("Uint32Array", msh.missingEdges));
        return success(out);
    });
}

// Mass properties of the target. density: kg/m^3 (optional); mass in kg = volume[mm^3] * 1e-9 * density.
val docProperties(std::uint32_t handle, val target, val density) {
    return withDocument(handle, [&](m::PartDocument& doc) {
        const m::NamedShape shape = targetShape(doc, target);
        std::optional<double> rho;
        if (present(density)) {
            if (!density.isNumber() || !(density.as<double>() > 0) || !std::isfinite(density.as<double>())) {
                throw InputError("density must be a positive number (kg/m^3)");
            }
            rho = density.as<double>();
        }
        val o = val::object();
        const k::Shape s(shape.shape());
        const k::TopologyCounts c = s.counts();
        val counts = val::object();
        counts.set("solids", static_cast<double>(c.solids));
        counts.set("faces", static_cast<double>(c.faces));
        counts.set("edges", static_cast<double>(c.edges));
        counts.set("vertices", static_cast<double>(c.vertices));
        o.set("counts", counts);
        o.set("volume", val::null());
        o.set("centerOfMass", val::null());
        o.set("mass", val::null());
        o.set("area", val::null());
        o.set("boundingBox", val::null());
        if (shape.isNull()) return success(o);
        if (c.solids > 0) {
            auto v = s.volumeProperties();
            if (!v) return failure(v.error());
            o.set("volume", v.value().volume);
            o.set("centerOfMass", point(v.value().centerOfMass));
            if (rho) o.set("mass", v.value().volume * 1e-9 * *rho);
        }
        if (c.faces > 0) {
            auto a = s.surfaceProperties();
            if (!a) return failure(a.error());
            o.set("area", a.value().area);
        }
        auto b = s.boundingBox();
        if (!b) return failure(b.error());
        val box = val::object();
        box.set("min", point(b.value().min));
        box.set("max", point(b.value().max));
        o.set("boundingBox", box);
        return success(o);
    });
}

}  // namespace bendcore::wasm_modeling

using namespace bendcore::wasm_modeling;

EMSCRIPTEN_BINDINGS(bendcore_modeling) {
    emscripten::function("modelingInfo", &modelingInfo);
    emscripten::function("featureCatalog", &featureCatalog);
    emscripten::function("sketchTemplate", &sketchTemplate);
    emscripten::function("docCreate", &docCreate);
    emscripten::function("docLoad", &docLoad);
    emscripten::function("docDispose", &docDispose);
    emscripten::function("docSave", &docSave);
    emscripten::function("docVariables", &docVariables);
    emscripten::function("docSetVariable", &docSetVariable);
    emscripten::function("docRemoveVariable", &docRemoveVariable);
    emscripten::function("docFeatures", &docFeatures);
    emscripten::function("docState", &docState);
    emscripten::function("docAddFeature", &docAddFeature);
    emscripten::function("docEditFeature", &docEditFeature);
    emscripten::function("docRemoveFeature", &docRemoveFeature);
    emscripten::function("docMoveFeature", &docMoveFeature);
    emscripten::function("docSetSuppressed", &docSetSuppressed);
    emscripten::function("docSetRollback", &docSetRollback);
    emscripten::function("docBeginTransaction", &docBeginTransaction);
    emscripten::function("docCommitTransaction", &docCommitTransaction);
    emscripten::function("docAbortTransaction", &docAbortTransaction);
    emscripten::function("docUndo", &docUndo);
    emscripten::function("docRedo", &docRedo);
    emscripten::function("docRebuild", &docRebuild);
    emscripten::function("docMakeReference", &docMakeReference);
    emscripten::function("docElement", &docElement);
    emscripten::function("docTessellate", &docTessellate);
    emscripten::function("docProperties", &docProperties);
}
