// JSON persistence of PartDocument: format "bendcore-part" v3 (links, sketches), migration
// v1 -> v2 -> v3, optional BRep cache keyed by input hash (docs/modeling/PART_DOCUMENT.md).

#include "part_document_internal.hpp"

#include <bendcore/kernel/brep_io.hpp>

#include <TopExp_Explorer.hxx>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace bendcore::modeling {

using nlohmann::json;

namespace {

// Целые поля файла: только целое число JSON в диапазоне T. Вещественное значение вне диапазона
// (1e308) при get<int>() — неопределённое преобразование double → int; здесь — ошибка разбора.
template <typename T>
T checkedInteger(const json& v, const char* what) {
    if (v.is_number_unsigned()) {
        const auto x = v.get<std::uint64_t>();
        if (x <= static_cast<std::uint64_t>(std::numeric_limits<T>::max())) return static_cast<T>(x);
    } else if (v.is_number_integer()) {
        const auto x = v.get<std::int64_t>();
        if (x >= static_cast<std::int64_t>(std::numeric_limits<T>::min()) &&
            (x < 0 || static_cast<std::uint64_t>(x) <= static_cast<std::uint64_t>(std::numeric_limits<T>::max())))
            return static_cast<T>(x);
    }
    throw std::invalid_argument(std::string(what) + ": expected an integer in range, got " + v.dump().substr(0, 40));
}

template <typename T>
T integerValue(const json& object, const char* key, T fallback) {
    const auto it = object.find(key);
    return it == object.end() ? fallback : checkedInteger<T>(*it, key);
}

template <typename T>
std::vector<T> integerArray(const json& object, const char* key) {
    std::vector<T> out;
    const auto it = object.find(key);
    if (it == object.end()) return out;
    if (!it->is_array()) throw std::invalid_argument(std::string(key) + ": expected an array");
    for (const json& v : *it) out.push_back(checkedInteger<T>(v, key));
    return out;
}

constexpr const char* kFormatName = "bendcore-part";

const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::vector<std::uint8_t>& data) {
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        std::uint32_t v = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        out += kB64[(v >> 6) & 63];
        out += kB64[v & 63];
    }
    if (i + 1 == data.size()) {
        std::uint32_t v = data[i] << 16;
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        out += "==";
    } else if (i + 2 == data.size()) {
        std::uint32_t v = (data[i] << 16) | (data[i + 1] << 8);
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        out += kB64[(v >> 6) & 63];
        out += '=';
    }
    return out;
}

std::optional<std::vector<std::uint8_t>> base64Decode(const std::string& text) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    if (text.size() % 4 != 0) return std::nullopt;
    std::vector<std::uint8_t> out;
    out.reserve(text.size() / 4 * 3);
    for (std::size_t i = 0; i < text.size(); i += 4) {
        int a = val(text[i]), b = val(text[i + 1]);
        if (a < 0 || b < 0) return std::nullopt;
        std::uint32_t v = (static_cast<std::uint32_t>(a) << 18) | (static_cast<std::uint32_t>(b) << 12);
        out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
        if (text[i + 2] == '=') {
            if (text[i + 3] != '=' || i + 4 != text.size()) return std::nullopt;
            break;
        }
        int c = val(text[i + 2]);
        if (c < 0) return std::nullopt;
        v |= static_cast<std::uint32_t>(c) << 6;
        out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
        if (text[i + 3] == '=') {
            if (i + 4 != text.size()) return std::nullopt;
            break;
        }
        int d = val(text[i + 3]);
        if (d < 0) return std::nullopt;
        v |= static_cast<std::uint32_t>(d);
        out.push_back(static_cast<std::uint8_t>(v & 0xff));
    }
    return out;
}

std::string hex64(std::uint64_t v) {
    char buf[20];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
    return buf;
}

std::uint64_t parseHex64(const std::string& s) {
    if (s.size() != 16) throw std::invalid_argument("hash must have 16 hex digits");
    return std::stoull(s, nullptr, 16);
}

json point(const kernel::Point3& p) { return json::array({p[0], p[1], p[2]}); }
kernel::Point3 point(const json& j) {
    if (!j.is_array() || j.size() != 3) throw std::invalid_argument("point must be an array of 3 numbers");
    return {j.at(0).get<double>(), j.at(1).get<double>(), j.at(2).get<double>()};
}

json signatureToJson(const GeometricSignature& s) {
    if (!s.valid) return nullptr;
    json j{{"geometry", s.geometry}, {"center", point(s.center)}, {"size", s.size}};
    if (s.hasDirection) j["direction"] = point(s.direction);
    if (s.radius != 0.0) j["radius"] = s.radius;
    return j;
}

GeometricSignature signatureFromJson(const json& j, ElementKind kind) {
    GeometricSignature s;
    s.kind = kind;
    if (j.is_null()) return s;
    s.valid = true;
    s.geometry = j.at("geometry").get<std::string>();
    s.center = point(j.at("center"));
    s.size = j.value("size", 0.0);
    s.radius = j.value("radius", 0.0);
    if (j.contains("direction")) {
        s.hasDirection = true;
        s.direction = point(j.at("direction"));
    }
    return s;
}

json referenceToJson(const TopoReference& r) {
    return json{{"name", r.name}, {"kind", toString(r.kind)}, {"signature", signatureToJson(r.signature)}};
}

TopoReference referenceFromJson(const json& j) {
    TopoReference r;
    r.name = j.at("name").get<std::string>();
    auto kind = elementKindFromString(j.at("kind").get<std::string>());
    if (!kind) throw std::invalid_argument("unknown element kind '" + j.at("kind").get<std::string>() + "'");
    r.kind = *kind;
    r.signature = signatureFromJson(j.value("signature", json()), r.kind);
    return r;
}

json resolutionToJson(const ReferenceReport& rep) {
    return json{{"slot", rep.slot},
                {"reference", referenceToJson(rep.reference)},
                {"method", toString(rep.resolution.method)},
                {"indices", rep.resolution.indices},
                {"names", rep.resolution.names},
                {"detail", rep.resolution.detail}};
}

ReferenceReport resolutionFromJson(const json& j) {
    ReferenceReport rep;
    rep.slot = j.at("slot").get<std::string>();
    rep.reference = referenceFromJson(j.at("reference"));
    auto m = resolveMethodFromString(j.at("method").get<std::string>());
    if (!m) throw std::invalid_argument("unknown resolve method");
    rep.resolution.method = *m;
    rep.resolution.indices = j.at("indices").get<std::vector<std::size_t>>();
    rep.resolution.names = j.at("names").get<std::vector<std::string>>();
    rep.resolution.detail = j.at("detail").get<std::string>();
    return rep;
}

json elementsToJson(const NamedShape& shape) {
    json kinds = json::object();
    for (ElementKind kind : {ElementKind::Face, ElementKind::Edge, ElementKind::Vertex}) {
        json arr = json::array();
        for (const auto& r : shape.records(kind)) {
            json e{{"name", r.name},
                   {"origin", toString(r.origin)},
                   {"feature", r.feature},
                   {"aliases", r.aliases},
                   {"signature", signatureToJson(r.signature)}};
            if (!r.attributes.empty()) {
                json attrs = json::array();
                for (const auto& [k, v] : r.attributes) attrs.push_back(json::array({k, v}));
                e["attributes"] = std::move(attrs);
            }
            arr.push_back(std::move(e));
        }
        kinds[toString(kind)] = std::move(arr);
    }
    json del = json::object();
    for (const auto& [name, d] : shape.deletions()) {
        del[name] = json{{"feature", d.feature}, {"successors", d.successors}};
    }
    return json{{"elements", kinds}, {"deletions", del}, {"bodies", shape.bodyNames()}};
}

Result<NamedShape> namedShapeFromJson(const std::string& brep64, const json& j) {
    auto bytes = base64Decode(brep64);
    if (!bytes) return Result<NamedShape>::failure(ErrorCode::ParseError, "cache: invalid base64");
    auto shape = kernel::readBrepBinary(*bytes);
    if (!shape) return Result<NamedShape>::failure(shape.error().code, "cache: " + shape.error().message);
    std::array<std::vector<ElementRecord>, 3> records;
    for (ElementKind kind : {ElementKind::Face, ElementKind::Edge, ElementKind::Vertex}) {
        for (const auto& e : j.at("elements").at(toString(kind))) {
            ElementRecord r;
            r.name = e.at("name").get<std::string>();
            auto origin = nameOriginFromString(e.at("origin").get<std::string>());
            if (!origin) return Result<NamedShape>::failure(ErrorCode::ParseError, "cache: unknown name origin");
            r.origin = *origin;
            r.feature = e.at("feature").get<std::string>();
            r.aliases = e.at("aliases").get<std::vector<std::string>>();
            r.signature = signatureFromJson(e.at("signature"), kind);
            if (e.contains("attributes")) {
                for (const auto& kv : e.at("attributes"))
                    r.attributes.emplace_back(kv.at(0).get<std::string>(), kv.at(1).get<std::string>());
            }
            records[static_cast<std::size_t>(kind)].push_back(std::move(r));
        }
    }
    std::map<std::string, DeletionRecord> deletions;
    for (const auto& [name, d] : j.at("deletions").items()) {
        deletions[name] = DeletionRecord{d.at("feature").get<std::string>(),
                                         d.at("successors").get<std::vector<std::string>>()};
    }
    // Body names are part of the cache since the multibody part; an entry without them is
    // recomputed (fromRecords would invent names that differ from the rebuilt ones).
    if (!j.contains("bodies")) return Result<NamedShape>::failure(ErrorCode::ParseError, "cache: no body names");
    auto bodies = j.at("bodies").get<std::vector<std::string>>();
    if (bodies.empty() && TopExp_Explorer(shape.value().occt(), TopAbs_SOLID).More())
        return Result<NamedShape>::failure(ErrorCode::ParseError, "cache: no body names");
    return NamedShape::fromRecords(shape.value().occt(), std::move(records), std::move(deletions), std::move(bodies));
}

std::string numberText(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

// v1 (first layout): numeric document "parameters", "operations" with numeric or textual
// "params" and references as plain names without signatures. v2 renames the sections,
// stores every parameter as an expression and references with kind and signature.
json migrateV1(const json& v1, const FeatureRegistry& registry) {
    json v2{{"format", kFormatName}, {"version", 2}, {"variables", json::array()}, {"features", json::array()}};
    if (v1.contains("parameters")) {
        for (const auto& [name, value] : v1.at("parameters").items()) {
            v2["variables"].push_back(json{{"name", name},
                                           {"expression", value.is_number() ? numberText(value.get<double>())
                                                                            : value.get<std::string>()}});
        }
    }
    for (const auto& op : v1.at("operations")) {
        const std::string type = op.at("type").get<std::string>();
        // v1 knew the v2 types; PrismCut is migrated further by migrateV2 and has one slot "face".
        auto feature = registry.create(type);
        if (!feature && type != "PrismCut") throw std::domain_error("unknown feature type '" + type + "'");
        json f{{"id", op.at("id")}, {"type", type}, {"suppressed", op.value("suppressed", false)}};
        json params = json::object();
        if (op.contains("params")) {
            for (const auto& [name, value] : op.at("params").items()) {
                params[name] = value.is_number() ? numberText(value.get<double>()) : value.get<std::string>();
            }
        }
        f["parameters"] = params;
        f["options"] = op.value("options", json::object());
        json refs = json::object();
        if (op.contains("refs")) {
            for (const auto& [slot, names] : op.at("refs").items()) {
                ElementKind kind = ElementKind::Edge;
                bool known = false;
                if (!feature) {
                    kind = ElementKind::Face;
                    known = slot == "face";
                } else {
                    for (const auto& s : feature->referenceSlots()) {
                        if (s.name == slot) {
                            kind = s.kind;
                            known = true;
                        }
                    }
                }
                if (!known) throw std::invalid_argument(type + " has no reference slot '" + slot + "'");
                json arr = json::array();
                for (const auto& n : names) {
                    arr.push_back(json{{"name", n.get<std::string>()}, {"kind", toString(kind)}, {"signature", nullptr}});
                }
                refs[slot] = arr;
            }
        }
        f["references"] = refs;
        v2["features"].push_back(f);
    }
    v2["rollback"] = v1.value("rollback", json());
    return v2;
}

json sketchToJson(const SketchData& s) {
    json points = json::array();
    for (const auto& p : s.points) points.push_back(json{{"x", p.x}, {"y", p.y}, {"fixed", p.fixed}});
    json geometry = json::array();
    for (const auto& g : s.geometry) {
        json e{{"type", toString(g.type)},
               {"start", g.start},
               {"end", g.end},
               {"center", g.center},
               {"radius", g.radius},
               {"counterClockwise", g.counterClockwise},
               {"construction", g.construction},
               {"projection", g.projection}};
        // Fields of the conics and splines are written only for them: files of lines, circles and
        // arcs stay as before (format v3 readers ignore unknown fields).
        if (g.type == SketchGeometryType::Ellipse || g.type == SketchGeometryType::EllipticalArc) e["focus"] = g.focus;
        if (g.type == SketchGeometryType::BSpline) {
            e["poles"] = g.poles;
            e["degree"] = g.degree;
            e["knots"] = g.knots;
            e["multiplicities"] = g.multiplicities;
            e["weights"] = g.weights;
            e["periodic"] = g.periodic;
        }
        geometry.push_back(std::move(e));
    }
    json constraints = json::array();
    for (const auto& c : s.constraints) {
        json e{{"type", toString(c.type)}, {"a", c.a}, {"b", c.b}, {"c", c.c}, {"value", c.value}};
        if (c.driven) e["driven"] = true;
        constraints.push_back(std::move(e));
    }
    return json{{"points", points}, {"geometry", geometry}, {"constraints", constraints}, {"regions", s.regions}};
}

SketchData sketchFromJson(const json& j) {
    SketchData s;
    for (const auto& p : j.at("points")) {
        s.points.push_back({p.at("x").get<std::string>(), p.at("y").get<std::string>(), p.value("fixed", false)});
    }
    for (const auto& g : j.at("geometry")) {
        SketchGeometryDef d;
        auto type = sketchGeometryTypeFromString(g.at("type").get<std::string>());
        if (!type) throw std::invalid_argument("unknown sketch geometry '" + g.at("type").get<std::string>() + "'");
        d.type = *type;
        d.start = integerValue<int>(g, "start", -1);
        d.end = integerValue<int>(g, "end", -1);
        d.center = integerValue<int>(g, "center", -1);
        d.radius = g.value("radius", std::string("1"));
        d.counterClockwise = g.value("counterClockwise", true);
        d.construction = g.value("construction", false);
        d.projection = integerValue<int>(g, "projection", -1);
        d.focus = integerValue<int>(g, "focus", -1);
        d.poles = integerArray<int>(g, "poles");
        d.degree = integerValue<int>(g, "degree", 3);
        d.knots = g.value("knots", std::vector<double>());
        d.multiplicities = integerArray<int>(g, "multiplicities");
        d.weights = g.value("weights", std::vector<double>());
        d.periodic = g.value("periodic", false);
        s.geometry.push_back(std::move(d));
    }
    for (const auto& c : j.at("constraints")) {
        SketchConstraintDef d;
        auto type = sketchConstraintTypeFromString(c.at("type").get<std::string>());
        if (!type) throw std::invalid_argument("unknown sketch constraint '" + c.at("type").get<std::string>() + "'");
        d.type = *type;
        d.a = integerValue<int>(c, "a", -1);
        d.b = integerValue<int>(c, "b", -1);
        d.c = integerValue<int>(c, "c", -1);
        d.value = c.value("value", std::string("0"));
        d.driven = c.value("driven", false);
        s.constraints.push_back(std::move(d));
    }
    s.regions = integerArray<SketchEntityId>(j, "regions");
    return s;
}

// Splits "<a>|<b>|..." at the top level (outside brackets and parentheses).
std::vector<std::string> splitTopLevel(const std::string& text) {
    std::vector<std::string> out;
    int depth = 0;
    std::string cur;
    for (char c : text) {
        if (c == '(' || c == '[') ++depth;
        if (c == ')' || c == ']') --depth;
        if (c == '|' && depth == 0) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

// Derived names list their components in sorted order (element_map.cpp, joinSorted); after a
// component is renamed the order is restored so that the name stays the one the map builds.
std::string resortDerived(const std::string& name) {
    if (name.size() < 4 || (name[0] != 'E' && name[0] != 'V' && name[0] != 'F') || name[1] != '(') return name;
    int depth = 0;
    std::size_t close = std::string::npos;
    for (std::size_t i = 1; i < name.size(); ++i) {
        if (name[i] == '(' || name[i] == '[') ++depth;
        if (name[i] == ')' || name[i] == ']') {
            if (--depth == 0 && name[i] == ')') {
                close = i;
                break;
            }
        }
    }
    if (close == std::string::npos) return name;
    auto parts = splitTopLevel(name.substr(2, close - 2));
    std::sort(parts.begin(), parts.end());
    std::string joined;
    for (const auto& p : parts) joined += (joined.empty() ? "" : "|") + p;
    return name.substr(0, 2) + joined + name.substr(close);
}

std::string replaceAll(std::string text, const std::string& from, const std::string& to) {
    for (std::size_t pos = text.find(from); pos != std::string::npos; pos = text.find(from, pos + to.size())) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

// v2 had a parametric prism cut ("PrismCut": rectangle or circle on a planar face). v3 builds
// it from a sketch on that face and a cut extrusion; the extrusion keeps the id, the sketch is
// "<id>Sketch", and element names of the old roles are rewritten to the names the extrusion
// gives the same faces.
json migrateV2(json doc) {
    json features = json::array();
    std::vector<std::pair<std::string, std::string>> renames;
    for (const auto& f : doc.at("features")) {
        if (f.at("type").get<std::string>() != "PrismCut") {
            features.push_back(f);
            continue;
        }
        const std::string id = f.at("id").get<std::string>();
        const std::string sid = id + "Sketch";
        const auto params = f.value("parameters", std::map<std::string, std::string>());
        auto param = [&](const char* name, const char* def) {
            auto it = params.find(name);
            return "(" + (it == params.end() ? std::string(def) : it->second) + ")";
        };
        const auto options = f.value("options", std::map<std::string, std::string>());
        const bool circle = options.count("profile") && options.at("profile") == "circle";
        const bool through = options.count("extent") && options.at("extent") == "through";
        SketchBuilder b;
        if (circle) {
            b.fixedCircle("0", "0", param("radius", "2"));
            renames.emplace_back(id + ":Side", "G[" + id + "](" + sid + ":E1)");
        } else {
            const std::string u = param("sizeU", "5") + " / 2", v = param("sizeV", "5") + " / 2";
            b.fixedPolygon({{"-" + u, "-" + v}, {u, "-" + v}, {u, v}, {"-" + u, v}});
            const char* sides[4] = {"Side-V", "Side+U", "Side+V", "Side-U"};
            for (int k = 0; k < 4; ++k) {
                renames.emplace_back(id + ":" + sides[k], "G[" + id + "](" + sid + ":E" + std::to_string(k + 1) + ")");
            }
        }
        renames.emplace_back(id + ":Start", sid + ":R1");
        renames.emplace_back(id + ":Floor", "G[" + id + "](" + sid + ":R1)");
        json sketch{{"id", sid},
                    {"type", "Sketch"},
                    {"label", sid},
                    {"suppressed", f.value("suppressed", false)},
                    {"parameters",
                     {{"originX", param("centerX", "0")}, {"originY", param("centerY", "0")}, {"originZ", param("centerZ", "0")}}},
                    {"options", {{"plane", "face"}}},
                    {"references", {{"face", f.value("references", json::object()).value("face", json::array())}}},
                    {"links", json::object()},
                    {"sketch", sketchToJson(b.build())}};
        json extrude{{"id", id},
                     {"type", "Extrude"},
                     {"label", f.value("label", id)},
                     {"suppressed", f.value("suppressed", false)},
                     {"parameters", {{"distance", params.count("depth") ? params.at("depth") : std::string("5")}}},
                     {"options", {{"mode", "cut"}, {"extent", through ? "throughAll" : "blind"}, {"reversed", "true"}}},
                     {"references", json::object()},
                     {"links", {{"sketch", sid}}}};
        features.push_back(sketch);
        features.push_back(extrude);
    }
    // Rewrite names that referred to the old roles; a reference whose name no longer matches
    // falls back to its stored signature at the first rebuild.
    for (auto& f : features) {
        if (!f.contains("references")) continue;
        for (auto& [slot, arr] : f.at("references").items()) {
            for (auto& r : arr) {
                std::string name = r.at("name").get<std::string>();
                const std::string before = name;
                for (const auto& [from, to] : renames) name = replaceAll(name, from, to);
                if (name != before) r["name"] = resortDerived(name);
            }
        }
    }
    // The rollback marker counted the old features: each PrismCut became two nodes.
    if (doc.contains("rollback") && !doc.at("rollback").is_null()) {
        const std::size_t rb = checkedInteger<std::size_t>(doc.at("rollback"), "rollback");
        std::size_t extra = 0, k = 0;
        for (const auto& f : doc.at("features")) {
            if (k++ >= rb) break;
            if (f.at("type").get<std::string>() == "PrismCut") ++extra;
        }
        doc["rollback"] = rb + extra;
    }
    doc["features"] = features;
    doc["version"] = 3;
    doc.erase("cache");  // cached shapes of v2 carry the old names
    return doc;
}

}  // namespace

Result<std::string> PartDocument::toJson(const SaveOptions& options) const {
    try {
        json doc{{"format", kFormatName}, {"version", kFormatVersion}};
        json vars = json::array();
        for (const auto& [name, expr] : variables_) vars.push_back(json{{"name", name}, {"expression", expr}});
        doc["variables"] = vars;
        doc["rollback"] = rollback_ ? json(*rollback_) : json();
        json features = json::array();
        json cache = json::object();
        for (const auto& n : nodes_) {
            json refs = json::object();
            for (const auto& [slot, list] : n->references) {
                json arr = json::array();
                for (const auto& r : list) arr.push_back(referenceToJson(r));
                refs[slot] = arr;
            }
            json f{{"id", n->id},
                   {"type", n->type},
                   {"label", n->label},
                   {"suppressed", n->suppressed},
                   {"parameters", n->parameters},
                   {"options", n->options},
                   {"references", refs},
                   {"links", n->links}};
            if (n->sketch) f["sketch"] = sketchToJson(*n->sketch);
            features.push_back(std::move(f));
            if (options.includeCache && n->output && n->hash) {
                auto bytes = kernel::writeBrepBinary(kernel::Shape(n->output->shape()));
                if (!bytes) return Result<std::string>::failure(bytes.error().code, "cache of " + n->id + ": " + bytes.error().message);
                json reports = json::array();
                for (const auto& rep : n->reports) reports.push_back(resolutionToJson(rep));
                json entry = elementsToJson(*n->output);
                entry["hash"] = hex64(*n->hash);
                entry["state"] = toString(n->state);
                entry["messages"] = n->messages;
                entry["values"] = n->values;
                entry["reports"] = reports;
                entry["brep"] = base64Encode(bytes.value());
                if (!n->tool.IsNull() && n->toolMode) {
                    auto toolBytes = kernel::writeBrepBinary(kernel::Shape(n->tool));
                    if (!toolBytes) {
                        return Result<std::string>::failure(toolBytes.error().code,
                                                            "tool cache of " + n->id + ": " + toolBytes.error().message);
                    }
                    entry["tool"] = base64Encode(toolBytes.value());
                    entry["toolMode"] = toString(*n->toolMode);
                }
                cache[n->id] = std::move(entry);
            }
        }
        doc["features"] = features;
        if (options.includeCache) doc["cache"] = cache;
        return doc.dump(1);
    } catch (const std::exception& e) {
        return Result<std::string>::failure(ErrorCode::OperationFailed, std::string("save failed: ") + e.what());
    }
}

Result<PartDocument> PartDocument::fromJson(std::string_view text, std::shared_ptr<const FeatureRegistry> registry,
                                            RebuildReport* report) {
    using R = Result<PartDocument>;
    if (!registry) return R::failure(ErrorCode::InvalidInput, "registry is null");
    json doc;
    try {
        doc = json::parse(text);
    } catch (const std::exception& e) {
        return R::failure(ErrorCode::ParseError, std::string("not JSON: ") + e.what());
    }
    try {
        if (!doc.is_object() || doc.value("format", std::string()) != kFormatName) {
            return R::failure(ErrorCode::ParseError, std::string("not a ") + kFormatName + " document");
        }
        const int version = checkedInteger<int>(doc.at("version"), "version");
        if (version > kFormatVersion) {
            return R::failure(ErrorCode::Unsupported, "format version " + std::to_string(version) +
                                                          " is newer than the supported " +
                                                          std::to_string(kFormatVersion));
        }
        if (version < 1) return R::failure(ErrorCode::ParseError, "invalid format version " + std::to_string(version));
        if (version == 1) doc = migrateV1(doc, *registry);
        if (checkedInteger<int>(doc.at("version"), "version") == 2) doc = migrateV2(std::move(doc));

        PartDocument part(registry);
        for (const auto& v : doc.at("variables")) {
            auto st = part.setVariable(v.at("name").get<std::string>(), v.at("expression").get<std::string>());
            if (!st) return R::failure(ErrorCode::ParseError, "variable: " + st.error().message);
        }
        for (const auto& f : doc.at("features")) {
            FeatureSpec spec;
            spec.type = f.at("type").get<std::string>();
            spec.id = f.at("id").get<std::string>();
            spec.label = f.value("label", std::string());
            spec.suppressed = f.value("suppressed", false);
            spec.parameters = f.value("parameters", std::map<std::string, std::string>());
            spec.options = f.value("options", std::map<std::string, std::string>());
            if (f.contains("references")) {
                for (const auto& [slot, arr] : f.at("references").items()) {
                    auto& list = spec.references[slot];
                    for (const auto& r : arr) list.push_back(referenceFromJson(r));
                }
            }
            spec.links = f.value("links", std::map<std::string, std::string>());
            if (f.contains("sketch")) spec.sketch = sketchFromJson(f.at("sketch"));
            if (!registry->contains(spec.type)) {
                return R::failure(ErrorCode::Unsupported, "unknown feature type '" + spec.type + "'");
            }
            auto id = part.addFeature(std::move(spec), part.size());
            if (!id) return R::failure(ErrorCode::ParseError, "feature: " + id.error().message);
        }
        const json& rb = doc.value("rollback", json());
        if (!rb.is_null()) {
            auto st = part.setRollback(checkedInteger<std::size_t>(rb, "rollback"));
            if (!st) return R::failure(ErrorCode::ParseError, st.error().message);
        }
        if (doc.contains("cache")) {
            for (const auto& [id, entry] : doc.at("cache").items()) {
                Node* n = part.node(id);
                if (!n) continue;
                auto named = namedShapeFromJson(entry.at("brep").get<std::string>(), entry);
                if (!named) continue;  // unusable cache entry: the feature is recomputed
                auto state = featureStateFromString(entry.at("state").get<std::string>());
                if (!state) continue;
                CachedResult c;
                c.hash = parseHex64(entry.at("hash").get<std::string>());
                c.state = *state;
                c.messages = entry.at("messages").get<std::vector<std::string>>();
                c.values = entry.at("values").get<std::map<std::string, double>>();
                for (const auto& rep : entry.at("reports")) c.reports.push_back(resolutionFromJson(rep));
                c.output = std::make_shared<const NamedShape>(std::move(named).value());
                if (entry.contains("tool")) {
                    auto toolBytes = base64Decode(entry.at("tool").get<std::string>());
                    if (!toolBytes) continue;
                    auto tool = kernel::readBrepBinary(*toolBytes);
                    if (!tool) continue;
                    auto toolNamed = nameTool(n->id, tool.value().occt());
                    if (!toolNamed) continue;
                    c.tool = tool.value().occt();
                    // toString(FeatureMode): "Boss", "Cut", "NewBody".
                    auto mode = featureModeFromString(entry.at("toolMode").get<std::string>());
                    if (!mode) continue;
                    c.toolMode = *mode;
                    c.toolNamed = std::make_shared<const NamedShape>(std::move(toolNamed).value());
                }
                n->cache = std::move(c);
            }
        }
        part.clearHistory();
        RebuildReport r = part.rebuild();
        if (report) *report = std::move(r);
        return R(std::move(part));
    } catch (const std::domain_error& e) {
        return R::failure(ErrorCode::Unsupported, e.what());
    } catch (const std::exception& e) {
        return R::failure(ErrorCode::ParseError, std::string("malformed document: ") + e.what());
    }
}

}  // namespace bendcore::modeling
