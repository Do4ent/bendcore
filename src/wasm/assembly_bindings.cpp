// WebAssembly module of the assembly document (bendcore_assembly_wasm, docs/ui/ASSEMBLY_UI.md).
//
// Assembly documents (bendcore::assembly_v2::AssemblyDocument wrapped in AssemblyHistory for
// undo/redo) live in a registry inside the module and are addressed from JS by an integer
// handle; JS releases them with asmDispose(handle). Every entry point returns a plain JS object:
// {ok: true, value} or {ok: false, code, message} with the kernel ErrorCode, so no C++
// exception crosses into JS. The ES wrapper (bendcore_assembly_api.mjs) turns failures into
// KernelError exceptions.
//
// Every edit is one history transaction that ends with solving the mates (AssemblyHistory::apply
// with solve = true). Dragging works on a working copy of the document: asmDragMove solves the
// copy with the drag goal, asmDragEnd commits the copy as one history step, asmDragCancel drops it.
//
// Geometry: each part definition is tessellated once in its own coordinate system (cache keyed by
// the OCCT shape); every placed occurrence carries the key of its definition mesh and its world
// matrix. Face and edge indices of a mesh are kernel::TopologyIndex indices of the definition
// shape, i.e. the indices of ElementRef::face / ElementRef::edge in mates.

#include <bendcore/assembly_v2/assembly_document.hpp>
#include <bendcore/assembly_v2/bom.hpp>
#include <bendcore/assembly_v2/fasteners.hpp>
#include <bendcore/assembly_v2/history.hpp>
#include <bendcore/assembly_v2/interference.hpp>
#include <bendcore/kernel/result.hpp>
#include <bendcore/kernel/shape.hpp>
#include <bendcore/kernel/step_io.hpp>
#include <bendcore/kernel/tessellation.hpp>
#include <bendcore/modeling/part_document.hpp>
#include <bendcore/modeling/part_features.hpp>

#include "exchange_common.hpp"

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_TShape.hxx>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using emscripten::val;
namespace k = bendcore::kernel;
namespace a = bendcore::assembly_v2;
namespace f = bendcore::assembly_v2::fasteners;
namespace m = bendcore::modeling;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Malformed argument from JS (wrong type, missing field); reported as InvalidInput.
struct InputError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Kernel error raised inside a body; reported with its own code.
struct KernelFailure : std::runtime_error {
    k::Error error;
    explicit KernelFailure(k::Error e) : std::runtime_error(e.message), error(std::move(e)) {}
};

template <typename T>
T take(k::Result<T> r) {
    if (!r) throw KernelFailure(r.error());
    return std::move(r).value();
}

void check(const a::Status& s) {
    if (!s) throw KernelFailure(s.error());
}

struct MeshEntry {
    TopoDS_Shape keep;  // keeps the TShape alive so that its address stays a unique key
    std::string key;
};

struct Session {
    a::AssemblyHistory history;
    std::optional<a::AssemblyDocument> drag;  // working copy while dragging
    a::OccurrenceId dragOccurrence = 0;
    a::Vec3 dragLocal = a::Vec3::Zero();

    explicit Session(a::AssemblyDocument doc) : history(std::move(doc), historyOptions()) {}

    static a::HistoryOptions historyOptions() {
        a::HistoryOptions o;
        o.solveAfterEdit = true;
        return o;
    }
    const a::AssemblyDocument& active() const { return drag ? *drag : history.document(); }
};

std::map<std::uint32_t, std::unique_ptr<Session>>& sessions() {
    static std::map<std::uint32_t, std::unique_ptr<Session>> s;
    return s;
}

std::uint32_t& nextHandle() {
    static std::uint32_t next = 1;
    return next;
}

// Texts of external subassembly documents by source name (asmRegisterSource).
std::map<std::string, std::string>& sources() {
    static std::map<std::string, std::string> s;
    return s;
}

std::map<const TopoDS_TShape*, MeshEntry>& meshKeys() {
    static std::map<const TopoDS_TShape*, MeshEntry> keys;
    return keys;
}

std::uint32_t& nextMeshKey() {
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

template <typename Body>
val guarded(Body&& body) {
    try {
        return body();
    } catch (const InputError& e) {
        return failure("InvalidInput", e.what());
    } catch (const KernelFailure& e) {
        return failure(e.error);
    } catch (const Standard_Failure& e) {
        return failure("OperationFailed", std::string("OCCT: ") + e.what());
    } catch (const std::exception& e) {
        return failure("OperationFailed", e.what());
    }
}

template <typename Body>
val withSession(std::uint32_t handle, Body&& body) {
    auto it = sessions().find(handle);
    if (it == sessions().end()) {
        return failure("InvalidInput", "unknown assembly handle " + std::to_string(handle) +
                                           " (never created or already disposed)");
    }
    Session& s = *it->second;
    return guarded([&] { return body(s); });
}

// Edits are refused while a drag is open: the drag copy would silently discard them.
void requireNoDrag(const Session& s) {
    if (s.drag) throw InputError("a drag is in progress: call dragEnd() or dragCancel() first");
}

// One history step; the edit runs on the document and the mates are solved afterwards.
void edit(Session& s, const std::string& label, const std::function<void(a::AssemblyDocument&)>& body,
          bool solve = true) {
    requireNoDrag(s);
    check(s.history.apply(
        label,
        [&](a::AssemblyDocument& d) -> a::Status {
            try {
                body(d);
            } catch (const KernelFailure& e) {
                return e.error;
            } catch (const InputError& e) {
                return k::Error{k::ErrorCode::InvalidInput, e.what()};
            }
            return std::monostate{};
        },
        solve));
}

// ------------------------------------------------------------------ JS value helpers

bool present(const val& v) { return !v.isUndefined() && !v.isNull(); }

bool isObject(const val& v) { return present(v) && v.typeOf().as<std::string>() == "object"; }

bool isArray(const val& v) { return present(v) && val::global("Array").call<bool>("isArray", v); }

std::string requireString(const val& v, const std::string& what) {
    if (!v.isString()) throw InputError(what + " must be a string");
    return v.as<std::string>();
}

std::string optionalString(const val& obj, const char* field, const std::string& fallback, const std::string& what) {
    const val v = obj[field];
    return present(v) ? requireString(v, what + "." + field) : fallback;
}

double requireNumber(const val& v, const std::string& what) {
    if (!v.isNumber()) throw InputError(what + " must be a number");
    const double d = v.as<double>();
    if (!std::isfinite(d)) throw InputError(what + " must be finite");
    return d;
}

double optionalNumber(const val& obj, const char* field, double fallback, const std::string& what) {
    const val v = obj[field];
    return present(v) ? requireNumber(v, what + "." + field) : fallback;
}

int requireInt(const val& v, const std::string& what) {
    const double d = requireNumber(v, what);
    if (d != std::floor(d) || std::abs(d) > 1e9) throw InputError(what + " must be an integer");
    return static_cast<int>(d);
}

std::uint32_t requireId(const val& v, const std::string& what) {
    const int i = requireInt(v, what);
    if (i < 0) throw InputError(what + " must be a non-negative integer");
    return static_cast<std::uint32_t>(i);
}

bool optionalBool(const val& obj, const char* field, bool fallback, const std::string& what) {
    const val v = obj[field];
    if (!present(v)) return fallback;
    if (v.isTrue()) return true;
    if (v.isFalse()) return false;
    throw InputError(what + "." + field + " must be a boolean");
}

std::vector<val> arrayItems(const val& v, const std::string& what) {
    if (!isArray(v)) throw InputError(what + " must be an array");
    const auto n = v["length"].as<std::size_t>();
    std::vector<val> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) out.push_back(v[i]);
    return out;
}

a::Vec3 vec3(const val& v, const std::string& what) {
    const auto items = arrayItems(v, what);
    if (items.size() != 3) throw InputError(what + " must be [x, y, z]");
    return {requireNumber(items[0], what + "[0]"), requireNumber(items[1], what + "[1]"),
            requireNumber(items[2], what + "[2]")};
}

a::Vec3 optionalVec3(const val& obj, const char* field, const a::Vec3& fallback, const std::string& what) {
    const val v = obj[field];
    return present(v) ? vec3(v, what + "." + field) : fallback;
}

val jsVec(const a::Vec3& p) {
    val out = val::array();
    out.call<void>("push", p.x());
    out.call<void>("push", p.y());
    out.call<void>("push", p.z());
    return out;
}

template <typename T>
val idArray(const std::vector<T>& ids) {
    val out = val::array();
    for (const T& id : ids) out.call<void>("push", static_cast<double>(id));
    return out;
}

std::vector<std::uint32_t> idList(const val& v, const std::string& what) {
    std::vector<std::uint32_t> out;
    for (const val& x : arrayItems(v, what)) out.push_back(requireId(x, what + "[]"));
    return out;
}

template <typename T>
val typedArray(const char* constructor, const std::vector<T>& data) {
    return val::global(constructor).new_(emscripten::typed_memory_view(data.size(), data.data()));
}

// Placement {position: [x, y, z], rotation: [x, y, z, w]} (unit quaternion) or
// {position, axis: [x, y, z], angle: degrees}. Missing fields: identity.
a::Transform transformFromJs(const val& v, const std::string& what) {
    a::Transform t;
    if (!present(v)) return t;
    if (!isObject(v)) throw InputError(what + " must be an object");
    t.position = optionalVec3(v, "position", a::Vec3::Zero(), what);
    if (present(v["rotation"])) {
        const auto q = arrayItems(v["rotation"], what + ".rotation");
        if (q.size() != 4) throw InputError(what + ".rotation must be a quaternion [x, y, z, w]");
        a::Quat quat(requireNumber(q[3], what + ".rotation[3]"), requireNumber(q[0], what + ".rotation[0]"),
                     requireNumber(q[1], what + ".rotation[1]"), requireNumber(q[2], what + ".rotation[2]"));
        if (!(quat.norm() > 1e-12)) throw InputError(what + ".rotation must not be zero");
        t.rotation = quat.normalized();
    } else if (present(v["axis"])) {
        const a::Vec3 axis = vec3(v["axis"], what + ".axis");
        if (!(axis.norm() > 1e-12)) throw InputError(what + ".axis must not be zero");
        t.rotation = a::Transform::fromAxisAngle(axis, optionalNumber(v, "angle", 0.0, what) * kPi / 180.0).rotation;
    }
    return t;
}

val transformToJs(const a::Transform& t) {
    val out = val::object();
    out.set("position", jsVec(t.position));
    val q = val::array();
    q.call<void>("push", t.rotation.x());
    q.call<void>("push", t.rotation.y());
    q.call<void>("push", t.rotation.z());
    q.call<void>("push", t.rotation.w());
    out.set("rotation", q);
    // Column-major 4×4 (three.js Matrix4.fromArray).
    const a::Mat3 r = t.rotationMatrix();
    val mtx = val::array();
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            double x;
            if (c < 3) x = row < 3 ? r(row, c) : 0.0;
            else x = row < 3 ? t.position[row] : 1.0;
            mtx.call<void>("push", x);
        }
    }
    out.set("matrix", mtx);
    return out;
}

a::OccurrencePath pathFromJs(const val& v, const std::string& what) {
    a::OccurrencePath p = idList(v, what);
    if (p.empty()) throw InputError(what + " must not be empty");
    return p;
}

m::ElementKind kindFromString(const std::string& s, const std::string& what) {
    if (s == "face") return m::ElementKind::Face;
    if (s == "edge") return m::ElementKind::Edge;
    if (s == "vertex") return m::ElementKind::Vertex;
    throw InputError(what + ": unknown element kind '" + s + "' (face, edge, vertex)");
}

const char* kindName(m::ElementKind k) {
    switch (k) {
        case m::ElementKind::Face: return "face";
        case m::ElementKind::Edge: return "edge";
        case m::ElementKind::Vertex: return "vertex";
    }
    return "face";
}

a::EntityUse useFromString(const std::string& s, const std::string& what) {
    if (s == "auto") return a::EntityUse::Auto;
    if (s == "axis") return a::EntityUse::Axis;
    if (s == "point") return a::EntityUse::Point;
    throw InputError(what + ": unknown use '" + s + "' (auto, axis, point)");
}

const char* useName(a::EntityUse u) {
    switch (u) {
        case a::EntityUse::Auto: return "auto";
        case a::EntityUse::Axis: return "axis";
        case a::EntityUse::Point: return "point";
    }
    return "auto";
}

// Reference {path: [occurrence ids], kind, index | name, use}.
a::GeometryRef refFromJs(const val& v, const std::string& what, bool requireElement = true) {
    if (!isObject(v)) throw InputError(what + " must be an object");
    a::GeometryRef r;
    r.path = pathFromJs(v["path"], what + ".path");
    if (!requireElement && !present(v["kind"])) return r;
    r.element.kind = kindFromString(optionalString(v, "kind", "face", what), what + ".kind");
    if (present(v["index"])) r.element.index = requireId(v["index"], what + ".index");
    else if (present(v["name"])) r.element.name = requireString(v["name"], what + ".name");
    else if (requireElement) throw InputError(what + " needs index or name");
    r.use = useFromString(optionalString(v, "use", "auto", what), what + ".use");
    return r;
}

val refToJs(const a::GeometryRef& r) {
    val out = val::object();
    out.set("path", idArray(r.path));
    out.set("kind", std::string(kindName(r.element.kind)));
    if (r.element.index) out.set("index", static_cast<double>(*r.element.index));
    if (!r.element.name.empty()) out.set("name", r.element.name);
    out.set("use", std::string(useName(r.use)));
    return out;
}

a::MateType mateTypeFromString(const std::string& s, const std::string& what) {
    static const std::map<std::string, a::MateType> types = {
        {"coincident", a::MateType::Coincident}, {"concentric", a::MateType::Concentric},
        {"distance", a::MateType::Distance},     {"angle", a::MateType::Angle},
        {"parallel", a::MateType::Parallel},     {"perpendicular", a::MateType::Perpendicular},
        {"tangent", a::MateType::Tangent},       {"rigid", a::MateType::Rigid},
    };
    auto it = types.find(s);
    if (it == types.end()) {
        throw InputError(what + ": unknown mate type '" + s +
                         "' (coincident, concentric, distance, angle, parallel, perpendicular, tangent, rigid)");
    }
    return it->second;
}

std::string mateTypeName(a::MateType t) {
    std::string s = a::toString(t);
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

a::Alignment alignmentFromString(const std::string& s, const std::string& what) {
    if (s == "closest") return a::Alignment::Closest;
    if (s == "aligned") return a::Alignment::Aligned;
    if (s == "anti-aligned") return a::Alignment::AntiAligned;
    throw InputError(what + ": unknown alignment '" + s + "' (closest, aligned, anti-aligned)");
}

const char* alignmentName(a::Alignment al) {
    switch (al) {
        case a::Alignment::Closest: return "closest";
        case a::Alignment::Aligned: return "aligned";
        case a::Alignment::AntiAligned: return "anti-aligned";
    }
    return "closest";
}

// Mate {type, a, b, value (mm; angle: degrees), alignment, internal, name}.
a::MateSpec mateFromJs(const val& v) {
    if (!isObject(v)) throw InputError("mate must be an object");
    a::MateSpec spec;
    spec.type = mateTypeFromString(requireString(v["type"], "mate.type"), "mate.type");
    const bool rigid = spec.type == a::MateType::Rigid;
    spec.a = refFromJs(v["a"], "mate.a", !rigid);
    spec.b = refFromJs(v["b"], "mate.b", !rigid);
    const double value = optionalNumber(v, "value", 0.0, "mate");
    spec.value = spec.type == a::MateType::Angle ? value * kPi / 180.0 : value;
    spec.alignment = alignmentFromString(optionalString(v, "alignment", "closest", "mate"), "mate.alignment");
    spec.internalTangency = optionalBool(v, "internal", false, "mate");
    spec.name = optionalString(v, "name", "", "mate");
    return spec;
}

val entityToJs(const a::Entity& e) {
    val out = val::object();
    std::string t = a::toString(e.type);
    for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    out.set("type", t);
    out.set("point", jsVec(e.point));
    if (e.type != a::EntityType::Point && e.type != a::EntityType::Sphere) out.set("direction", jsVec(e.direction));
    if (e.type == a::EntityType::Cylinder || e.type == a::EntityType::Sphere) out.set("radius", e.radius);
    return out;
}

k::AssemblyColor colorFromJs(const val& v, const std::string& what) {
    const auto c = arrayItems(v, what);
    if (c.size() != 3 && c.size() != 4) throw InputError(what + " must be [r, g, b] or [r, g, b, alpha] in 0..1");
    k::AssemblyColor out;
    out.r = requireNumber(c[0], what);
    out.g = requireNumber(c[1], what);
    out.b = requireNumber(c[2], what);
    if (c.size() == 4) out.alpha = requireNumber(c[3], what);
    return out;
}

val colorToJs(const std::optional<k::AssemblyColor>& c) {
    if (!c) return val::null();
    val out = val::array();
    out.call<void>("push", c->r);
    out.call<void>("push", c->g);
    out.call<void>("push", c->b);
    out.call<void>("push", c->alpha);
    return out;
}

a::PartProperties propertiesFromJs(const val& v, a::PartProperties base) {
    if (!present(v)) return base;
    if (!isObject(v)) throw InputError("properties must be an object");
    base.designation = optionalString(v, "designation", base.designation, "properties");
    base.name = optionalString(v, "name", base.name, "properties");
    base.material = optionalString(v, "material", base.material, "properties");
    const val d = v["density"];
    if (d.isNull()) base.density.reset();
    else if (!d.isUndefined()) {
        const double rho = requireNumber(d, "properties.density");
        if (!(rho > 0)) throw InputError("properties.density must be positive (kg/m^3)");
        base.density = rho;
    }
    return base;
}

val propertiesToJs(const a::PartProperties& p) {
    val out = val::object();
    out.set("designation", p.designation);
    out.set("name", p.name);
    out.set("material", p.material);
    out.set("density", p.density ? val(*p.density) : val::null());
    return out;
}

// ------------------------------------------------------------------ fastener standards

struct StandardName {
    f::FastenerStandard standard;
    const char* id;
};

const std::vector<StandardName>& standardNames() {
    static const std::vector<StandardName> names = {
        {f::FastenerStandard::Gost7798, "gost-7798"},   {f::FastenerStandard::Iso4014, "iso-4014"},
        {f::FastenerStandard::Iso4017, "iso-4017"},     {f::FastenerStandard::Gost11738, "gost-11738"},
        {f::FastenerStandard::Iso4762, "iso-4762"},     {f::FastenerStandard::Gost5915, "gost-5915"},
        {f::FastenerStandard::Iso4032, "iso-4032"},     {f::FastenerStandard::Gost11371, "gost-11371"},
        {f::FastenerStandard::Iso7089, "iso-7089"},     {f::FastenerStandard::Gost6402, "gost-6402"},
        {f::FastenerStandard::Gost3128, "gost-3128"},   {f::FastenerStandard::Iso2338, "iso-2338"},
    };
    return names;
}

const char* standardId(f::FastenerStandard s) {
    for (const auto& n : standardNames())
        if (n.standard == s) return n.id;
    return "";
}

f::FastenerStandard standardFromJs(const val& v, const std::string& what) {
    const std::string s = requireString(v, what);
    for (const auto& n : standardNames())
        if (s == n.id) return n.standard;
    throw InputError(what + ": unknown fastener standard '" + s + "'");
}

std::string kindId(f::FastenerKind k) {
    switch (k) {
        case f::FastenerKind::HexBolt: return "hex-bolt";
        case f::FastenerKind::SocketHeadScrew: return "socket-head-screw";
        case f::FastenerKind::HexNut: return "hex-nut";
        case f::FastenerKind::PlainWasher: return "plain-washer";
        case f::FastenerKind::SpringWasher: return "spring-washer";
        case f::FastenerKind::ParallelPin: return "parallel-pin";
    }
    return "";
}

// {standard, size, length}
f::FastenerSpec fastenerSpecFromJs(const val& v, const std::string& what) {
    if (!isObject(v)) throw InputError(what + " must be an object {standard, size, length}");
    f::FastenerSpec spec;
    spec.standard = standardFromJs(v["standard"], what + ".standard");
    spec.size = requireInt(v["size"], what + ".size");
    spec.length = optionalNumber(v, "length", 0.0, what);
    return spec;
}

// Definition of the fastener in the document: an existing part with the same designation and
// the same volume is reused so that the BOM counts one position.
a::DefinitionId fastenerDefinition(a::AssemblyDocument& doc, const f::FastenerSpec& spec) {
    f::Fastener fastener = take(f::makeFastener(spec));
    for (const a::Definition& d : doc.definitions()) {
        if (d.kind == a::DefinitionKind::Part && d.properties.name == fastener.properties.designation &&
            d.name == fastener.properties.designation) {
            return d.id;
        }
    }
    return take(f::addFastenerPart(doc, fastener));
}

// ------------------------------------------------------------------ state → JS

int occurrenceDof(const a::DocumentSolveReport& report, const a::Occurrence& o, bool* shared) {
    *shared = false;
    if (o.fixed) return 0;
    for (const auto& s : report.subsystems) {
        if (std::find(s.occurrences.begin(), s.occurrences.end(), o.id) != s.occurrences.end()) {
            *shared = s.occurrences.size() > 1;
            return s.dof;
        }
    }
    return 6;
}

val solveReportToJs(const a::DocumentSolveReport& r) {
    val out = val::object();
    out.set("success", r.success);
    out.set("totalDof", r.totalDof);
    out.set("residualNorm", r.residualNorm);
    val subs = val::array();
    for (const auto& s : r.subsystems) {
        val o = val::object();
        o.set("occurrences", idArray(s.occurrences));
        o.set("mates", idArray(s.mates));
        o.set("status", std::string(a::toString(s.status)));
        o.set("dof", s.dof);
        o.set("rank", s.rank);
        o.set("redundantMates", idArray(s.redundantMates));
        o.set("conflictingMates", idArray(s.conflictingMates));
        subs.call<void>("push", o);
    }
    out.set("subsystems", subs);
    out.set("brokenMates", idArray(r.brokenMates));
    out.set("suppressedMates", idArray(r.suppressedMates));
    return out;
}

// Diagnostics of the current positions without changing the document (solved copy).
a::DocumentSolveReport currentReport(const a::AssemblyDocument& doc) {
    a::AssemblyDocument copy = doc;
    return take(copy.solve());
}

val occurrencesToJs(const a::AssemblyDocument& doc, const a::Definition& def, const a::DocumentSolveReport* report) {
    val list = val::array();
    for (const a::Occurrence& o : def.occurrences) {
        val x = val::object();
        x.set("id", static_cast<double>(o.id));
        x.set("name", o.name);
        x.set("definition", static_cast<double>(o.definition));
        x.set("placement", transformToJs(o.placement));
        x.set("fixed", o.fixed);
        x.set("suppressed", o.suppressed);
        x.set("designation", o.designation);
        x.set("color", colorToJs(o.color));
        x.set("pattern", o.pattern ? val(static_cast<double>(*o.pattern)) : val::null());
        if (report) {
            bool shared = false;
            x.set("dof", occurrenceDof(*report, o, &shared));
            x.set("dofShared", shared);
        }
        const a::Definition& child = doc.definition(o.definition);
        x.set("kind", std::string(child.kind == a::DefinitionKind::Part ? "part" : "assembly"));
        if (child.kind == a::DefinitionKind::Assembly) x.set("children", occurrencesToJs(doc, child, nullptr));
        list.call<void>("push", x);
    }
    return list;
}

val stateToJs(const Session& s) {
    const a::AssemblyDocument& doc = s.active();
    const a::DocumentSolveReport report = currentReport(doc);
    val out = val::object();
    out.set("name", doc.name());
    val defs = val::array();
    for (const a::Definition& d : doc.definitions()) {
        val x = val::object();
        x.set("id", static_cast<double>(d.id));
        x.set("kind", std::string(d.kind == a::DefinitionKind::Part ? "part" : "assembly"));
        x.set("name", d.name);
        x.set("properties", propertiesToJs(d.properties));
        x.set("source", d.source);
        x.set("external", d.external);
        x.set("readOnly", d.kind == a::DefinitionKind::Assembly && doc.isReadOnlyAssembly(d.id));
        x.set("loadError", d.loadError);
        x.set("color", colorToJs(d.color));
        if (d.kind == a::DefinitionKind::Part && d.topology) {
            x.set("faces", static_cast<double>(d.topology->faces().size()));
            x.set("edges", static_cast<double>(d.topology->edges().size()));
        }
        defs.call<void>("push", x);
    }
    out.set("definitions", defs);
    out.set("occurrences", occurrencesToJs(doc, doc.definition(doc.root()), &report));
    val mates = val::array();
    for (const a::DocumentMate& mt : doc.mates()) {
        val x = val::object();
        x.set("id", static_cast<double>(mt.id));
        x.set("type", mateTypeName(mt.spec.type));
        x.set("name", mt.spec.name);
        x.set("a", refToJs(mt.spec.a));
        x.set("b", refToJs(mt.spec.b));
        x.set("value", mt.spec.type == a::MateType::Angle ? mt.spec.value * 180.0 / kPi : mt.spec.value);
        x.set("alignment", std::string(alignmentName(mt.spec.alignment)));
        x.set("internal", mt.spec.internalTangency);
        x.set("suppressed", mt.suppressed);
        x.set("broken", mt.brokenReason);
        std::string status = "solved";
        if (mt.broken()) status = "broken";
        else if (mt.suppressed || std::count(report.suppressedMates.begin(), report.suppressedMates.end(), mt.id))
            status = "suppressed";
        for (const auto& sub : report.subsystems) {
            if (std::count(sub.conflictingMates.begin(), sub.conflictingMates.end(), mt.id)) status = "conflicting";
            else if (std::count(sub.redundantMates.begin(), sub.redundantMates.end(), mt.id)) status = "redundant";
            else if (std::count(sub.mates.begin(), sub.mates.end(), mt.id) && sub.status != a::SubsystemStatus::Solved &&
                     status == "solved")
                status = "unsolved";
        }
        x.set("status", status);
        mates.call<void>("push", x);
    }
    out.set("mates", mates);
    val patterns = val::array();
    for (const a::ComponentPattern& p : doc.patterns()) {
        val x = val::object();
        x.set("id", static_cast<double>(p.id));
        x.set("kind", std::string(a::toString(p.kind)));
        x.set("seed", static_cast<double>(p.seed));
        x.set("instances", idArray(p.instances));
        patterns.call<void>("push", x);
    }
    out.set("patterns", patterns);
    val views = val::array();
    for (const a::ExplodedView& v : doc.explodedViews()) {
        val x = val::object();
        x.set("name", v.name);
        val steps = val::array();
        for (const a::ExplodeStep& st : v.steps) {
            val y = val::object();
            y.set("occurrences", idArray(st.occurrences));
            y.set("direction", jsVec(st.direction));
            y.set("distance", st.distance);
            steps.call<void>("push", y);
        }
        x.set("steps", steps);
        views.call<void>("push", x);
    }
    out.set("explodedViews", views);
    out.set("solve", solveReportToJs(report));
    val hist = val::object();
    hist.set("canUndo", s.history.canUndo());
    hist.set("canRedo", s.history.canRedo());
    hist.set("undoLabel", s.history.undoLabel());
    hist.set("redoLabel", s.history.redoLabel());
    hist.set("depth", static_cast<double>(s.history.undoDepth()));
    out.set("history", hist);
    out.set("dragging", s.drag ? val(static_cast<double>(s.dragOccurrence)) : val::null());
    return out;
}

val loadReportToJs(const a::LoadReport& r) {
    val out = val::object();
    out.set("fileVersion", r.fileVersion);
    out.set("migrated", r.migrated);
    val srcs = val::array();
    for (const auto& s : r.sources) {
        val x = val::object();
        x.set("definition", static_cast<double>(s.definition));
        x.set("name", s.name);
        x.set("source", s.source);
        x.set("status", std::string(a::toString(s.status)));
        x.set("message", s.message);
        srcs.call<void>("push", x);
    }
    out.set("sources", srcs);
    val mates = val::array();
    for (const auto& mt : r.mates) {
        val x = val::object();
        x.set("mate", static_cast<double>(mt.mate));
        x.set("name", mt.name);
        x.set("outcome", std::string(a::toString(mt.outcome)));
        x.set("message", mt.message);
        mates.call<void>("push", x);
    }
    out.set("mates", mates);
    val warnings = val::array();
    for (const auto& w : r.warnings) warnings.call<void>("push", w);
    out.set("warnings", warnings);
    return out;
}

a::LoadOptions memoryLoadOptions() {
    a::LoadOptions o;
    o.assemblies = [](const std::string& source) -> k::Result<std::string> {
        auto it = sources().find(source);
        if (it == sources().end()) {
            return k::Error{k::ErrorCode::IoError, "subassembly source '" + source +
                                                       "' is not registered (registerSource)"};
        }
        return it->second;
    };
    return o;
}

std::uint32_t addSession(a::AssemblyDocument doc) {
    const std::uint32_t h = nextHandle()++;
    sessions()[h] = std::make_unique<Session>(std::move(doc));
    return h;
}

// Exploded-view overrides {view: index, progress} or none.
std::optional<std::map<a::OccurrenceId, a::Transform>> overridesFromJs(const a::AssemblyDocument& doc, const val& v) {
    if (!present(v) || !present(v["view"])) return std::nullopt;
    const std::uint32_t index = requireId(v["view"], "options.view");
    if (index >= doc.explodedViews().size()) throw InputError("options.view: no exploded view " + std::to_string(index));
    const double progress = optionalNumber(v, "progress", 0.0, "options");
    return take(a::explodedPlacements(doc, doc.explodedViews()[index], progress));
}

std::string meshKeyOf(const TopoDS_Shape& shape) {
    auto& keys = meshKeys();
    auto it = keys.find(shape.TShape().get());
    if (it != keys.end()) return it->second.key;
    MeshEntry e{shape, "m" + std::to_string(nextMeshKey()++)};
    const std::string key = e.key;
    keys.emplace(shape.TShape().get(), std::move(e));
    return key;
}

val placedToJs(const a::AssemblyDocument& doc, const std::vector<a::PlacedOccurrence>& placed) {
    val list = val::array();
    for (const a::PlacedOccurrence& p : placed) {
        const a::Definition& def = doc.definition(p.definition);
        val x = val::object();
        x.set("path", idArray(p.path));
        x.set("pathName", p.pathName);
        x.set("definition", static_cast<double>(p.definition));
        x.set("mesh", def.shape.IsNull() ? val::null() : val(meshKeyOf(def.shape)));
        x.set("world", transformToJs(p.world));
        std::optional<k::AssemblyColor> color = def.color;
        for (const a::OccurrenceId id : p.path) {
            const a::Occurrence* o = doc.findOccurrence(id);
            if (o && o->color) color = o->color;
        }
        x.set("color", colorToJs(color));
        list.call<void>("push", x);
    }
    return list;
}

val meshToJs(const k::Mesh& msh) {
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
    val out = val::object();
    out.set("positions", typedArray("Float32Array", msh.positions));
    out.set("normals", typedArray("Float32Array", msh.normals));
    out.set("indices", typedArray("Uint32Array", msh.indices));
    out.set("faceSpans", typedArray("Uint32Array", spans));
    out.set("edges", edges);
    out.set("missingFaces", typedArray("Uint32Array", msh.missingFaces));
    out.set("missingEdges", typedArray("Uint32Array", msh.missingEdges));
    return out;
}

val bomRowToJs(const a::BomRow& r) {
    val x = val::object();
    x.set("position", r.position);
    x.set("positionPath", r.positionPath);
    x.set("level", r.level);
    x.set("definition", static_cast<double>(r.definition));
    x.set("kind", std::string(r.kind == a::DefinitionKind::Part ? "part" : "assembly"));
    x.set("designation", r.designation);
    x.set("name", r.name);
    x.set("material", r.material);
    x.set("quantity", r.quantity);
    x.set("unitVolume", r.unitVolume);
    x.set("unitMass", r.unitMass ? val(*r.unitMass) : val::null());
    x.set("totalMass", r.totalMass ? val(*r.totalMass) : val::null());
    val children = val::array();
    for (const a::BomRow& c : r.children) children.call<void>("push", bomRowToJs(c));
    x.set("children", children);
    return x;
}

// ------------------------------------------------------------------ module

val assemblyInfo() {
    val out = val::object();
    out.set("occtVersion", std::string(OCC_VERSION_COMPLETE));
    out.set("liveDocuments", static_cast<double>(sessions().size()));
    out.set("cachedMeshes", static_cast<double>(meshKeys().size()));
    val names = val::array();
    for (const auto& [name, text] : sources()) names.call<void>("push", name);
    out.set("sources", names);
    out.set("formatVersion", a::AssemblyDocument::kFormatVersion);
    return out;
}

val fastenerCatalog() {
    return guarded([&] {
        val list = val::array();
        for (f::FastenerStandard s : f::allStandards()) {
            const f::TableInfo info = f::tableInfo(s);
            val x = val::object();
            x.set("id", std::string(standardId(s)));
            x.set("designation", std::string(f::toString(s)));
            x.set("kind", kindId(f::kindOf(s)));
            x.set("gost", f::isGost(s));
            const auto cp = f::counterpart(s);
            x.set("counterpart", cp ? val(std::string(standardId(*cp))) : val::null());
            val sizes = val::array();
            for (int d : info.sizes) sizes.call<void>("push", d);
            x.set("sizes", sizes);
            x.set("verified", info.verified);
            x.set("source", info.source);
            x.set("verification", info.verification);
            list.call<void>("push", x);
        }
        return success(list);
    });
}

// Model of one fastener without a document: designation, volume, mass, thread, dimensions.
val fastenerDescribe(val spec) {
    return guarded([&] {
        const f::Fastener fs = take(f::makeFastener(fastenerSpecFromJs(spec, "spec")));
        val out = val::object();
        out.set("kind", kindId(fs.kind));
        out.set("name", fs.properties.name);
        out.set("designation", fs.properties.designation);
        out.set("equivalentDesignation", fs.properties.equivalentDesignation);
        out.set("material", fs.properties.material);
        out.set("density", fs.properties.density);
        out.set("volume", fs.properties.volume);
        out.set("mass", fs.properties.mass);
        val thread = val::object();
        thread.set("present", fs.thread.present);
        thread.set("designation", fs.thread.designation);
        thread.set("pitch", fs.thread.pitch);
        thread.set("length", fs.thread.length);
        out.set("thread", thread);
        val dims = val::object();
        for (const auto& [key, value] : fs.dimensions) dims.set(key, value);
        out.set("dimensions", dims);
        return success(out);
    });
}

val asmRegisterSource(std::string name, std::string text) {
    if (name.empty()) return failure("InvalidInput", "source name is empty");
    sources()[name] = std::move(text);
    return success(val(true));
}

val asmCreate(std::string name) {
    return guarded([&] { return success(val(addSession(a::AssemblyDocument(name.empty() ? "Assembly" : name)))); });
}

val asmLoad(std::string json) {
    return guarded([&] {
        a::LoadedAssembly loaded = take(a::AssemblyDocument::fromJson(json, memoryLoadOptions()));
        val out = val::object();
        out.set("report", loadReportToJs(loaded.report));
        out.set("handle", addSession(std::move(loaded.document)));
        return success(out);
    });
}

val asmImportStep(std::string step) {
    return guarded([&] { return success(val(addSession(take(a::AssemblyDocument::importStep(step))))); });
}

namespace ex = bendcore::wasm_exchange;

// Option errors of the exchange helpers are argument errors of the caller.
template <typename Body>
auto withOptions(Body&& body) {
    try {
        return body();
    } catch (const ex::OptionError& e) {
        throw InputError(e.what());
    }
}

// IGES through XCAF (docs/ui/DATA_EXCHANGE.md): a root of several solids becomes an assembly
// of one part per solid; names and colours as in the file.
val asmImportIges(std::string iges) {
    return guarded([&] {
        const k::AssemblyDocument xcaf = take(k::readIgesAssembly(iges));
        return success(val(addSession(take(a::AssemblyDocument::fromXcaf(xcaf)))));
    });
}

// {format: step|iges|stl|3mf, options} -> Uint8Array. Structure, names and colours as for
// exportStep; STL merges the parts, 3MF writes one object per part occurrence.
val asmExport(std::uint32_t handle, std::string format, val options) {
    return withSession(handle, [&](Session& s) {
        const k::AssemblyDocument xcaf = take(s.history.document().toXcaf());
        const std::string bytes = take(withOptions([&] { return ex::exportDocument(xcaf, format, options); }));
        return success(ex::bytesToJs(bytes));
    });
}

// Mass properties of the assembly (bendcore_kernel_analysis): every part occurrence with the
// density of its definition, summed with the parallel axis theorem. Parts without a density
// are listed and left out of the mass; volume and area cover all parts.
val asmMassProperties(std::uint32_t handle) {
    return withSession(handle, [&](Session& s) {
        const a::AssemblyDocument& doc = s.history.document();
        const auto placed = take(doc.placedParts());
        std::vector<k::MassProperties> withMass;
        double volume = 0.0, area = 0.0;
        val missing = val::array();
        for (const a::PlacedOccurrence& p : placed) {
            if (p.shape.IsNull()) continue;
            const a::Definition& def = doc.definition(p.definition);
            const double rho = def.properties.density.value_or(1000.0);
            k::MassProperties mp = take(k::computeMassProperties(k::Shape(p.shape), rho));
            volume += mp.volume;
            area += mp.area;
            if (def.properties.density) withMass.push_back(mp);
            else missing.call<void>("push", p.pathName);
        }
        val o = val::object();
        o.set("volume", volume);
        o.set("area", area);
        o.set("parts", static_cast<double>(placed.size()));
        o.set("partsWithoutDensity", missing);
        if (withMass.empty()) {
            o.set("mass", val::null());
        } else {
            const k::MassProperties total = take(k::combine(withMass));
            o.set("mass", ex::massToJs(total));
        }
        return success(o);
    });
}

val asmDispose(std::uint32_t handle) {
    if (!sessions().erase(handle)) return failure("InvalidInput", "unknown assembly handle " + std::to_string(handle));
    // Meshes no longer referenced by any document keep only their cache entry: drop them.
    std::set<const TopoDS_TShape*> used;
    for (const auto& [h, s] : sessions())
        for (const a::Definition& d : s->history.document().definitions())
            if (!d.shape.IsNull()) used.insert(d.shape.TShape().get());
    for (auto it = meshKeys().begin(); it != meshKeys().end();) {
        if (!used.count(it->first)) it = meshKeys().erase(it);
        else ++it;
    }
    return success(val(true));
}

val asmState(std::uint32_t handle) {
    return withSession(handle, [&](Session& s) { return success(stateToJs(s)); });
}

val asmSave(std::uint32_t handle, val options) {
    return withSession(handle, [&](Session& s) {
        a::SaveOptions o;
        if (present(options)) {
            o.embedGeometry = optionalBool(options, "embedGeometry", true, "options");
            o.indent = present(options["indent"]) ? requireInt(options["indent"], "options.indent") : 1;
        }
        return success(val(take(s.history.document().toJson(o))));
    });
}

val asmExportStep(std::uint32_t handle, val options) {
    return withSession(handle, [&](Session& s) {
        k::AssemblyWriteOptions o;
        if (present(options)) {
            const std::string schema = optionalString(options, "schema", "AP214", "options");
            if (schema == "AP242") o.schema = k::StepSchema::AP242;
            else if (schema != "AP214") throw InputError("options.schema must be AP214 or AP242");
        }
        return success(val(take(s.history.document().exportStep(o))));
    });
}

// Moves the new top-level occurrence `occ` (inserted at the origin) next to the rest of the
// assembly along +X with a 20 mm gap, bottoms level and Y centres aligned (world bounding boxes).
void placeBeside(a::AssemblyDocument& d, a::OccurrenceId occ) {
    const auto placed = take(d.placedParts());
    bool haveScene = false, havePart = false;
    k::BoundingBox scene{}, part{};
    auto grow = [](k::BoundingBox& box, bool& have, const k::BoundingBox& b) {
        if (!have) {
            box = b;
            have = true;
            return;
        }
        for (int c = 0; c < 3; ++c) {
            box.min[c] = std::min(box.min[c], b.min[c]);
            box.max[c] = std::max(box.max[c], b.max[c]);
        }
    };
    for (const a::PlacedOccurrence& p : placed) {
        if (p.shape.IsNull()) continue;
        auto b = k::Shape(p.shape).boundingBox();
        if (!b) continue;
        if (p.path.front() == occ) grow(part, havePart, b.value());
        else grow(scene, haveScene, b.value());
    }
    if (!haveScene || !havePart) return;
    const a::Occurrence* o = d.findOccurrence(occ);
    a::Transform t = o->placement;
    t.position += a::Vec3(scene.max[0] + 20.0 - part.min[0], (scene.min[1] + scene.max[1]) / 2 - (part.min[1] + part.max[1]) / 2,
                          scene.min[2] - part.min[2]);
    check(d.setPlacement(occ, t));
}

// Part data from {format: 'part-json' | 'step' | 'iges', data: text}.
a::PartData partFromJs(const val& v) {
    const std::string format = optionalString(v, "format", "part-json", "part");
    const std::string data = requireString(v["data"], "part.data");
    if (format == "part-json") {
        m::PartDocument part = take(m::PartDocument::fromJson(data, registry()));
        return take(a::partDataFromDocument(part));
    }
    if (format == "step") {
        k::Shape shape = take(k::readStep(data));
        return a::PartData{shape.occt(), nullptr};
    }
    if (format == "iges") {
        // All placed parts of the file as one part (as readStep does for STEP).
        const auto placed = take(k::flattenAssembly(take(k::readIgesAssembly(data))));
        BRep_Builder builder;
        TopoDS_Compound all;
        builder.MakeCompound(all);
        for (const k::PlacedPart& p : placed) builder.Add(all, p.shape.occt());
        if (placed.size() == 1) return a::PartData{placed.front().shape.occt(), nullptr};
        return a::PartData{all, nullptr};
    }
    throw InputError("part.format must be 'part-json', 'step' or 'iges'");
}

// {name, format, data, placement, properties, fixed, parent} -> {definition, occurrence}
val asmInsertPart(std::uint32_t handle, val spec) {
    return withSession(handle, [&](Session& s) {
        if (!isObject(spec)) throw InputError("part must be an object");
        const std::string name = optionalString(spec, "name", "Part", "part");
        a::PartData data = partFromJs(spec);
        const a::Transform placement = transformFromJs(spec["placement"], "part.placement");
        const bool fixed = optionalBool(spec, "fixed", false, "part");
        const bool beside = optionalBool(spec, "beside", false, "part");
        std::optional<a::DefinitionId> parent;
        if (present(spec["parent"])) parent = requireId(spec["parent"], "part.parent");
        if (beside && parent) throw InputError("part.beside applies to top-level components only");
        a::DefinitionId def = 0;
        a::OccurrenceId occ = 0;
        edit(s, "Insert " + name, [&](a::AssemblyDocument& d) {
            def = take(d.addPart(name, data, propertiesFromJs(spec["properties"], {})));
            occ = take(d.addOccurrence(def, placement, name, parent));
            if (beside) placeBeside(d, occ);
            if (fixed) check(d.setFixed(occ, true));
        });
        val out = val::object();
        out.set("definition", static_cast<double>(def));
        out.set("occurrence", static_cast<double>(occ));
        return success(out);
    });
}

// Another occurrence of an existing definition: {definition, placement, name, parent, fixed}.
val asmInsertOccurrence(std::uint32_t handle, val spec) {
    return withSession(handle, [&](Session& s) {
        if (!isObject(spec)) throw InputError("occurrence must be an object");
        const a::DefinitionId def = requireId(spec["definition"], "occurrence.definition");
        const a::Transform placement = transformFromJs(spec["placement"], "occurrence.placement");
        const std::string name = optionalString(spec, "name", "", "occurrence");
        std::optional<a::DefinitionId> parent;
        if (present(spec["parent"])) parent = requireId(spec["parent"], "occurrence.parent");
        const bool fixed = optionalBool(spec, "fixed", false, "occurrence");
        const bool beside = optionalBool(spec, "beside", false, "occurrence");
        if (beside && parent) throw InputError("occurrence.beside applies to top-level components only");
        a::OccurrenceId occ = 0;
        edit(s, "Insert component", [&](a::AssemblyDocument& d) {
            occ = take(d.addOccurrence(def, placement, name, parent));
            if (beside) placeBeside(d, occ);
            if (fixed) check(d.setFixed(occ, true));
        });
        return success(val(static_cast<double>(occ)));
    });
}

// External subassembly from a registered source: {source, name, placement, fixed}.
val asmInsertSubassembly(std::uint32_t handle, val spec) {
    return withSession(handle, [&](Session& s) {
        if (!isObject(spec)) throw InputError("subassembly must be an object");
        const std::string source = requireString(spec["source"], "subassembly.source");
        const std::string name = optionalString(spec, "name", "", "subassembly");
        const a::Transform placement = transformFromJs(spec["placement"], "subassembly.placement");
        const bool fixed = optionalBool(spec, "fixed", false, "subassembly");
        const bool beside = optionalBool(spec, "beside", false, "subassembly");
        a::DefinitionId def = 0;
        a::OccurrenceId occ = 0;
        edit(s, "Insert subassembly " + source, [&](a::AssemblyDocument& d) {
            def = take(d.addSubassemblyFromSource(name, source, memoryLoadOptions()));
            occ = take(d.addOccurrence(def, placement, name.empty() ? d.definition(def).name : name));
            if (beside) placeBeside(d, occ);
            if (fixed) check(d.setFixed(occ, true));
        });
        val out = val::object();
        out.set("definition", static_cast<double>(def));
        out.set("occurrence", static_cast<double>(occ));
        return success(out);
    });
}

val asmRemoveOccurrence(std::uint32_t handle, std::uint32_t id) {
    return withSession(handle, [&](Session& s) {
        edit(s, "Remove component", [&](a::AssemblyDocument& d) { check(d.removeOccurrence(id)); });
        return success(val(true));
    });
}

// {fixed?, suppressed?, name?, designation?, placement?, color? (null clears)}
val asmSetOccurrence(std::uint32_t handle, std::uint32_t id, val changes) {
    return withSession(handle, [&](Session& s) {
        if (!isObject(changes)) throw InputError("changes must be an object");
        std::string label = "Edit component";
        if (present(changes["fixed"])) label = optionalBool(changes, "fixed", false, "changes") ? "Fix component" : "Float component";
        if (present(changes["suppressed"]))
            label = optionalBool(changes, "suppressed", false, "changes") ? "Suppress component" : "Unsuppress component";
        if (present(changes["placement"])) label = "Move component";
        edit(s, label, [&](a::AssemblyDocument& d) {
            if (present(changes["fixed"])) check(d.setFixed(id, optionalBool(changes, "fixed", false, "changes")));
            if (present(changes["suppressed"]))
                check(d.setOccurrenceSuppressed(id, optionalBool(changes, "suppressed", false, "changes")));
            if (present(changes["name"])) check(d.setOccurrenceName(id, requireString(changes["name"], "changes.name")));
            if (present(changes["designation"]))
                check(d.setOccurrenceDesignation(id, requireString(changes["designation"], "changes.designation")));
            if (present(changes["placement"]))
                check(d.setPlacement(id, transformFromJs(changes["placement"], "changes.placement")));
            if (changes["color"].isNull()) check(d.setOccurrenceColor(id, std::nullopt));
            else if (present(changes["color"])) check(d.setOccurrenceColor(id, colorFromJs(changes["color"], "changes.color")));
        });
        return success(val(true));
    });
}

val asmSetProperties(std::uint32_t handle, std::uint32_t definition, val properties) {
    return withSession(handle, [&](Session& s) {
        edit(s, "Properties", [&](a::AssemblyDocument& d) {
            if (!d.hasDefinition(definition)) throw InputError("unknown definition " + std::to_string(definition));
            check(d.setProperties(definition, propertiesFromJs(properties, d.definition(definition).properties)));
        }, false);
        return success(val(true));
    });
}

val asmAddMate(std::uint32_t handle, val spec) {
    return withSession(handle, [&](Session& s) {
        a::MateSpec m = mateFromJs(spec);
        a::MateId id = 0;
        edit(s, "Add " + mateTypeName(m.type) + " mate", [&](a::AssemblyDocument& d) { id = take(d.addMate(m)); });
        return success(val(static_cast<double>(id)));
    });
}

val asmEditMate(std::uint32_t handle, std::uint32_t id, val spec) {
    return withSession(handle, [&](Session& s) {
        a::MateSpec m = mateFromJs(spec);
        edit(s, "Edit mate", [&](a::AssemblyDocument& d) { check(d.editMate(id, m)); });
        return success(val(true));
    });
}

val asmRemoveMate(std::uint32_t handle, std::uint32_t id) {
    return withSession(handle, [&](Session& s) {
        edit(s, "Remove mate", [&](a::AssemblyDocument& d) { check(d.removeMate(id)); });
        return success(val(true));
    });
}

val asmSetMateSuppressed(std::uint32_t handle, std::uint32_t id, bool suppressed) {
    return withSession(handle, [&](Session& s) {
        edit(s, suppressed ? "Suppress mate" : "Unsuppress mate",
             [&](a::AssemblyDocument& d) { check(d.setMateSuppressed(id, suppressed)); });
        return success(val(true));
    });
}

val asmSolve(std::uint32_t handle) {
    return withSession(handle, [&](Session& s) {
        a::DocumentSolveReport report;
        edit(s, "Solve", [&](a::AssemblyDocument& d) { report = take(d.solve()); }, false);
        return success(solveReportToJs(report));
    });
}

val asmUndo(std::uint32_t handle) {
    return withSession(handle, [&](Session& s) {
        requireNoDrag(s);
        check(s.history.undo());
        return success(val(true));
    });
}

val asmRedo(std::uint32_t handle) {
    return withSession(handle, [&](Session& s) {
        requireNoDrag(s);
        check(s.history.redo());
        return success(val(true));
    });
}

// ---- drag

// Starts dragging the top-level occurrence at the world point `point` (the grabbed point).
val asmDragBegin(std::uint32_t handle, std::uint32_t occurrence, val point) {
    return withSession(handle, [&](Session& s) {
        requireNoDrag(s);
        const a::AssemblyDocument& doc = s.history.document();
        const a::Occurrence* o = doc.findOccurrence(occurrence);
        if (!o || doc.parentOf(occurrence) != doc.root())
            throw InputError("occurrence " + std::to_string(occurrence) + " is not a top-level component");
        if (o->fixed) throw InputError("component '" + o->name + "' is fixed");
        if (o->pattern) throw InputError("component '" + o->name + "' is a pattern instance: its pattern places it");
        const a::Vec3 world = vec3(point, "point");
        s.drag = doc;
        s.dragOccurrence = occurrence;
        s.dragLocal = o->placement.inverse().applyToPoint(world);
        val out = val::object();
        out.set("localPoint", jsVec(s.dragLocal));
        return success(out);
    });
}

// Moves the grabbed point towards `target` (world) with the mates kept: {report, placements}.
val asmDragMove(std::uint32_t handle, val target) {
    return withSession(handle, [&](Session& s) {
        if (!s.drag) throw InputError("no drag in progress (dragBegin)");
        a::DocumentSolveReport report = take(s.drag->drag(s.dragOccurrence, s.dragLocal, vec3(target, "target")));
        const a::Occurrence* o = s.drag->findOccurrence(s.dragOccurrence);
        val out = val::object();
        out.set("solve", solveReportToJs(report));
        out.set("placement", transformToJs(o->placement));
        out.set("grabbed", jsVec(o->placement.applyToPoint(s.dragLocal)));
        out.set("placements", placedToJs(*s.drag, take(s.drag->placedParts())));
        return success(out);
    });
}

val asmDragEnd(std::uint32_t handle) {
    return withSession(handle, [&](Session& s) {
        if (!s.drag) throw InputError("no drag in progress (dragBegin)");
        a::AssemblyDocument dragged = std::move(*s.drag);
        s.drag.reset();
        check(s.history.apply("Drag component", [&](a::AssemblyDocument& d) -> a::Status {
            d = std::move(dragged);
            return std::monostate{};
        }));
        return success(val(true));
    });
}

val asmDragCancel(std::uint32_t handle) {
    return withSession(handle, [&](Session& s) {
        s.drag.reset();
        return success(val(true));
    });
}

// ---- patterns

val asmAddPattern(std::uint32_t handle, val spec) {
    return withSession(handle, [&](Session& s) {
        if (!isObject(spec)) throw InputError("pattern must be an object");
        const std::string kind = requireString(spec["kind"], "pattern.kind");
        const a::OccurrenceId seed = requireId(spec["seed"], "pattern.seed");
        std::optional<a::OccurrenceId> reference;
        if (present(spec["reference"])) reference = requireId(spec["reference"], "pattern.reference");
        a::PatternId id = 0;
        if (kind == "linear") {
            a::LinearPatternSpec p;
            p.seed = seed;
            p.reference = reference;
            p.direction = optionalVec3(spec, "direction", a::Vec3::UnitX(), "pattern");
            p.spacing = optionalNumber(spec, "spacing", 0.0, "pattern");
            p.count = present(spec["count"]) ? requireInt(spec["count"], "pattern.count") : 2;
            p.direction2 = optionalVec3(spec, "direction2", a::Vec3::UnitY(), "pattern");
            p.spacing2 = optionalNumber(spec, "spacing2", 0.0, "pattern");
            p.count2 = present(spec["count2"]) ? requireInt(spec["count2"], "pattern.count2") : 1;
            edit(s, "Linear pattern", [&](a::AssemblyDocument& d) { id = take(d.addLinearPattern(p)); });
        } else if (kind == "circular") {
            a::CircularPatternSpec p;
            p.seed = seed;
            p.reference = reference;
            p.axisPoint = optionalVec3(spec, "axisPoint", a::Vec3::Zero(), "pattern");
            p.axisDirection = optionalVec3(spec, "axisDirection", a::Vec3::UnitZ(), "pattern");
            p.angleStep = optionalNumber(spec, "angleStep", 0.0, "pattern") * kPi / 180.0;
            p.count = present(spec["count"]) ? requireInt(spec["count"], "pattern.count") : 2;
            edit(s, "Circular pattern", [&](a::AssemblyDocument& d) { id = take(d.addCircularPattern(p)); });
        } else if (kind == "feature") {
            a::FeaturePatternSpec p;
            p.seed = seed;
            p.seedHole = refFromJs(spec["seedHole"], "pattern.seedHole");
            if (present(spec["radius"])) p.radius = requireNumber(spec["radius"], "pattern.radius");
            edit(s, "Feature pattern", [&](a::AssemblyDocument& d) { id = take(d.addFeaturePattern(p)); });
        } else {
            throw InputError("pattern.kind must be linear, circular or feature");
        }
        return success(val(static_cast<double>(id)));
    });
}

val asmRemovePattern(std::uint32_t handle, std::uint32_t id) {
    return withSession(handle, [&](Session& s) {
        edit(s, "Remove pattern", [&](a::AssemblyDocument& d) { check(d.removePattern(id)); });
        return success(val(true));
    });
}

// ---- exploded views

val asmAddExplodedView(std::uint32_t handle, val spec) {
    return withSession(handle, [&](Session& s) {
        if (!isObject(spec)) throw InputError("view must be an object");
        a::ExplodedView view;
        view.name = optionalString(spec, "name", "Exploded", "view");
        std::size_t i = 0;
        for (const val& st : arrayItems(spec["steps"], "view.steps")) {
            const std::string what = "view.steps[" + std::to_string(i++) + "]";
            a::ExplodeStep step;
            step.occurrences = idList(st["occurrences"], what + ".occurrences");
            step.direction = vec3(st["direction"], what + ".direction");
            step.distance = requireNumber(st["distance"], what + ".distance");
            if (step.occurrences.empty()) throw InputError(what + ".occurrences must not be empty");
            if (!(step.direction.norm() > 1e-12)) throw InputError(what + ".direction must not be zero");
            step.direction.normalize();
            for (a::OccurrenceId id : step.occurrences) {
                if (s.history.document().parentOf(id) != s.history.document().root())
                    throw InputError(what + ": occurrence " + std::to_string(id) + " is not a top-level component");
            }
            view.steps.push_back(std::move(step));
        }
        std::size_t index = 0;
        edit(s, "Add exploded view", [&](a::AssemblyDocument& d) { index = d.addExplodedView(view); }, false);
        return success(val(static_cast<double>(index)));
    });
}

val asmRemoveExplodedView(std::uint32_t handle, std::uint32_t index) {
    return withSession(handle, [&](Session& s) {
        edit(s, "Remove exploded view", [&](a::AssemblyDocument& d) { check(d.removeExplodedView(index)); }, false);
        return success(val(true));
    });
}

// ---- geometry

// {params: {linearDeflection, angularDeflection}, skip: [mesh keys the caller has], view, progress}
// -> {meshes: {key: mesh}, occurrences: [{path, pathName, definition, mesh, world, color}]}
val asmTessellate(std::uint32_t handle, val options) {
    return withSession(handle, [&](Session& s) {
        const a::AssemblyDocument& doc = s.active();
        k::TessellationParams p;
        p.linearDeflection = 0.05;
        p.angularDeflection = 0.25;
        std::set<std::string> skip;
        if (present(options)) {
            const val params = options["params"];
            if (present(params)) {
                p.linearDeflection = optionalNumber(params, "linearDeflection", p.linearDeflection, "params");
                p.angularDeflection = optionalNumber(params, "angularDeflection", p.angularDeflection, "params");
                p.relative = optionalBool(params, "relative", p.relative, "params");
            }
            if (present(options["skip"]))
                for (const val& key : arrayItems(options["skip"], "options.skip")) skip.insert(requireString(key, "options.skip[]"));
        }
        p.inParallel = false;
        p.requireAllFaces = false;
        const auto overrides = overridesFromJs(doc, options);
        const auto placed = take(doc.placedParts(overrides ? &*overrides : nullptr));
        val meshes = val::object();
        std::set<a::DefinitionId> done;
        for (const a::PlacedOccurrence& pl : placed) {
            if (!done.insert(pl.definition).second) continue;
            const a::Definition& def = doc.definition(pl.definition);
            if (def.shape.IsNull()) continue;
            const std::string key = meshKeyOf(def.shape);
            if (skip.count(key)) continue;
            meshes.set(key, meshToJs(take(k::tessellate(k::Shape(def.shape), p))));
        }
        val out = val::object();
        out.set("meshes", meshes);
        out.set("occurrences", placedToJs(doc, placed));
        return success(out);
    });
}

// World placements only (drag, exploded view slider): {view, progress}.
val asmPlacements(std::uint32_t handle, val options) {
    return withSession(handle, [&](Session& s) {
        const a::AssemblyDocument& doc = s.active();
        const auto overrides = overridesFromJs(doc, options);
        return success(placedToJs(doc, take(doc.placedParts(overrides ? &*overrides : nullptr))));
    });
}

// Element of a placed part: {kind, index, geometry type, name, entity (world)}.
val asmElement(std::uint32_t handle, val ref) {
    return withSession(handle, [&](Session& s) {
        const a::AssemblyDocument& doc = s.active();
        const a::GeometryRef r = refFromJs(ref, "ref");
        const a::Occurrence* leaf = doc.findOccurrence(r.path.back());
        if (!leaf) throw InputError("unknown occurrence " + std::to_string(r.path.back()));
        const a::Definition& def = doc.definition(leaf->definition);
        if (def.kind != a::DefinitionKind::Part || !def.topology) throw InputError("the path does not end at a part");
        val out = val::object();
        out.set("path", idArray(r.path));
        out.set("kind", std::string(kindName(r.element.kind)));
        std::uint32_t index = 0;
        if (r.element.index) index = *r.element.index;
        else if (def.elementMap) {
            auto i = def.elementMap->find(r.element.kind, r.element.name);
            if (!i) throw InputError("no element named '" + r.element.name + "'");
            index = static_cast<std::uint32_t>(*i);
        } else {
            throw InputError("the part has no element map: use an index");
        }
        if (index >= a::elementCount(*def.topology, r.element.kind)) throw InputError("element index out of range");
        out.set("index", index);
        if (r.element.kind == m::ElementKind::Face)
            out.set("geometry", std::string(k::toString(def.topology->faces()[index].surface)));
        else if (r.element.kind == m::ElementKind::Edge)
            out.set("geometry", std::string(k::toString(def.topology->edges()[index].curve)));
        else
            out.set("geometry", std::string("Point"));
        if (def.elementMap && index < def.elementMap->count(r.element.kind))
            out.set("name", def.elementMap->record(r.element.kind, index).name);
        else
            out.set("name", val::null());
        a::GeometryRef withIndex = r;
        withIndex.element.index = index;
        auto entity = doc.worldEntity(withIndex);
        if (entity) out.set("entity", entityToJs(entity.value()));
        else {
            out.set("entity", val::null());
            out.set("entityError", entity.error().message);
        }
        return success(out);
    });
}

val asmInterference(std::uint32_t handle, val options) {
    return withSession(handle, [&](Session& s) {
        a::InterferenceOptions o;
        if (present(options)) {
            o.reportTouching = optionalBool(options, "reportTouching", true, "options");
            o.includeSameTopLevel = optionalBool(options, "includeSameTopLevel", true, "options");
            o.volumeTolerance = optionalNumber(options, "volumeTolerance", o.volumeTolerance, "options");
            o.contactTolerance = optionalNumber(options, "contactTolerance", o.contactTolerance, "options");
        }
        const a::InterferenceReport r = take(a::checkInterference(s.active(), o));
        val parts = val::array();
        for (const a::PlacedOccurrence& p : r.parts) {
            val x = val::object();
            x.set("path", idArray(p.path));
            x.set("pathName", p.pathName);
            x.set("definition", static_cast<double>(p.definition));
            parts.call<void>("push", x);
        }
        val pairs = val::array();
        for (const a::InterferencePair& p : r.pairs) {
            val x = val::object();
            x.set("a", static_cast<double>(p.a));
            x.set("b", static_cast<double>(p.b));
            x.set("kind", std::string(p.kind == a::ContactKind::Interference ? "interference" : "touching"));
            x.set("volume", p.volume);
            x.set("distance", p.distance);
            pairs.call<void>("push", x);
        }
        val out = val::object();
        out.set("parts", parts);
        out.set("pairs", pairs);
        out.set("totalPairs", static_cast<double>(r.totalPairs));
        out.set("candidatePairs", static_cast<double>(r.candidatePairs));
        out.set("totalVolume", r.totalVolume);
        return success(out);
    });
}

val asmBom(std::uint32_t handle, std::string mode) {
    return withSession(handle, [&](Session& s) {
        a::Bom bom;
        if (mode == "flat") bom = take(a::flatBom(s.active()));
        else if (mode == "structured") bom = take(a::structuredBom(s.active()));
        else throw InputError("BOM mode must be 'flat' or 'structured'");
        val rows = val::array();
        for (const a::BomRow& r : bom.rows) rows.call<void>("push", bomRowToJs(r));
        val out = val::object();
        out.set("rows", rows);
        out.set("totalMass", bom.totalMass ? val(*bom.totalMass) : val::null());
        out.set("totalVolume", bom.totalVolume);
        out.set("totalParts", bom.totalParts);
        return success(out);
    });
}

// ---- fasteners in holes

val holeSiteToJs(const f::HoleSite& site) {
    val x = val::object();
    x.set("hole", refToJs(site.hole));
    x.set("entryFace", refToJs(site.entryFace));
    x.set("exitFace", site.exitFace ? refToJs(*site.exitFace) : val::null());
    x.set("radius", site.radius);
    x.set("depth", site.depth);
    return x;
}

// {part: path, insertion: [x, y, z] (world, head → tip), radius, tolerance}
val asmFindHoles(std::uint32_t handle, val spec) {
    return withSession(handle, [&](Session& s) {
        if (!isObject(spec)) throw InputError("spec must be an object");
        const auto sites = take(f::findHoleSites(s.active(), pathFromJs(spec["part"], "spec.part"),
                                                 vec3(spec["insertion"], "spec.insertion"),
                                                 requireNumber(spec["radius"], "spec.radius"),
                                                 optionalNumber(spec, "tolerance", 1e-6, "spec")));
        val out = val::array();
        for (const f::HoleSite& site : sites) out.call<void>("push", holeSiteToJs(site));
        return success(out);
    });
}

// {part, insertion, radius, holes?: [hole face indices], screw: {standard, size, length},
//  headWashers: [{standard, size}], nutWashers: [...], nut: {standard, size} | null}
val asmInsertFasteners(std::uint32_t handle, val spec) {
    return withSession(handle, [&](Session& s) {
        if (!isObject(spec)) throw InputError("spec must be an object");
        const a::OccurrencePath part = pathFromJs(spec["part"], "spec.part");
        const a::Vec3 insertion = vec3(spec["insertion"], "spec.insertion");
        const double radius = requireNumber(spec["radius"], "spec.radius");
        const double tolerance = optionalNumber(spec, "tolerance", 1e-6, "spec");
        std::optional<std::set<std::uint32_t>> only;
        if (present(spec["holes"])) {
            const auto list = idList(spec["holes"], "spec.holes");
            only = std::set<std::uint32_t>(list.begin(), list.end());
        }
        const f::FastenerSpec screw = fastenerSpecFromJs(spec["screw"], "spec.screw");
        std::vector<f::FastenerSpec> headWashers, nutWashers;
        if (present(spec["headWashers"]))
            for (const val& w : arrayItems(spec["headWashers"], "spec.headWashers"))
                headWashers.push_back(fastenerSpecFromJs(w, "spec.headWashers[]"));
        if (present(spec["nutWashers"]))
            for (const val& w : arrayItems(spec["nutWashers"], "spec.nutWashers"))
                nutWashers.push_back(fastenerSpecFromJs(w, "spec.nutWashers[]"));
        std::optional<f::FastenerSpec> nut;
        if (present(spec["nut"])) nut = fastenerSpecFromJs(spec["nut"], "spec.nut");

        std::vector<f::StackPlacement> placed;
        std::vector<f::HoleSite> used;
        edit(s, "Insert fasteners", [&](a::AssemblyDocument& d) {
            std::vector<f::HoleSite> sites = take(f::findHoleSites(d, part, insertion, radius, tolerance));
            if (only) {
                std::vector<f::HoleSite> kept;
                for (const f::HoleSite& site : sites)
                    if (site.hole.element.index && only->count(*site.hole.element.index)) kept.push_back(site);
                if (kept.empty()) throw InputError("none of the selected faces is a hole of radius " + std::to_string(radius) +
                                                   " along the insertion direction");
                sites = std::move(kept);
            }
            f::FastenerStack stack;
            stack.screw = fastenerDefinition(d, screw);
            for (const auto& w : headWashers) stack.headWashers.push_back(fastenerDefinition(d, w));
            for (const auto& w : nutWashers) stack.nutWashers.push_back(fastenerDefinition(d, w));
            if (nut) stack.nut = fastenerDefinition(d, *nut);
            placed = take(f::insertFastenerStacks(d, stack, sites));
            used = sites;
        });
        val out = val::array();
        for (std::size_t i = 0; i < placed.size(); ++i) {
            const f::StackPlacement& p = placed[i];
            val x = val::object();
            x.set("site", holeSiteToJs(used[i]));
            x.set("screw", static_cast<double>(p.screw));
            x.set("headWashers", idArray(p.headWashers));
            x.set("nutWashers", idArray(p.nutWashers));
            x.set("nut", p.nut ? val(static_cast<double>(*p.nut)) : val::null());
            x.set("mates", idArray(p.mates));
            x.set("grip", p.grip);
            x.set("protrusion", p.protrusion);
            out.call<void>("push", x);
        }
        return success(out);
    });
}

}  // namespace

EMSCRIPTEN_BINDINGS(bendcore_assembly) {
    emscripten::function("assemblyInfo", &assemblyInfo);
    emscripten::function("fastenerCatalog", &fastenerCatalog);
    emscripten::function("fastenerDescribe", &fastenerDescribe);
    emscripten::function("asmRegisterSource", &asmRegisterSource);
    emscripten::function("asmCreate", &asmCreate);
    emscripten::function("asmLoad", &asmLoad);
    emscripten::function("asmImportStep", &asmImportStep);
    emscripten::function("asmImportIges", &asmImportIges);
    emscripten::function("asmExport", &asmExport);
    emscripten::function("asmMassProperties", &asmMassProperties);
    emscripten::function("asmDispose", &asmDispose);
    emscripten::function("asmState", &asmState);
    emscripten::function("asmSave", &asmSave);
    emscripten::function("asmExportStep", &asmExportStep);
    emscripten::function("asmInsertPart", &asmInsertPart);
    emscripten::function("asmInsertOccurrence", &asmInsertOccurrence);
    emscripten::function("asmInsertSubassembly", &asmInsertSubassembly);
    emscripten::function("asmRemoveOccurrence", &asmRemoveOccurrence);
    emscripten::function("asmSetOccurrence", &asmSetOccurrence);
    emscripten::function("asmSetProperties", &asmSetProperties);
    emscripten::function("asmAddMate", &asmAddMate);
    emscripten::function("asmEditMate", &asmEditMate);
    emscripten::function("asmRemoveMate", &asmRemoveMate);
    emscripten::function("asmSetMateSuppressed", &asmSetMateSuppressed);
    emscripten::function("asmSolve", &asmSolve);
    emscripten::function("asmUndo", &asmUndo);
    emscripten::function("asmRedo", &asmRedo);
    emscripten::function("asmDragBegin", &asmDragBegin);
    emscripten::function("asmDragMove", &asmDragMove);
    emscripten::function("asmDragEnd", &asmDragEnd);
    emscripten::function("asmDragCancel", &asmDragCancel);
    emscripten::function("asmAddPattern", &asmAddPattern);
    emscripten::function("asmRemovePattern", &asmRemovePattern);
    emscripten::function("asmAddExplodedView", &asmAddExplodedView);
    emscripten::function("asmRemoveExplodedView", &asmRemoveExplodedView);
    emscripten::function("asmTessellate", &asmTessellate);
    emscripten::function("asmPlacements", &asmPlacements);
    emscripten::function("asmElement", &asmElement);
    emscripten::function("asmInterference", &asmInterference);
    emscripten::function("asmBom", &asmBom);
    emscripten::function("asmFindHoles", &asmFindHoles);
    emscripten::function("asmInsertFasteners", &asmInsertFasteners);
}
