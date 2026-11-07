// JSON persistence of PartDocument: format "bendcore-part" v2, migration from v1,
// optional BRep cache keyed by input hash (docs/modeling/PART_DOCUMENT.md).

#include "part_document_internal.hpp"

#include <bendcore/kernel/brep_io.hpp>

#include <nlohmann/json.hpp>

#include <cstdio>
#include <stdexcept>

namespace bendcore::modeling {

using nlohmann::json;

namespace {

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
            arr.push_back(json{{"name", r.name},
                               {"origin", toString(r.origin)},
                               {"feature", r.feature},
                               {"aliases", r.aliases},
                               {"signature", signatureToJson(r.signature)}});
        }
        kinds[toString(kind)] = std::move(arr);
    }
    json del = json::object();
    for (const auto& [name, d] : shape.deletions()) {
        del[name] = json{{"feature", d.feature}, {"successors", d.successors}};
    }
    return json{{"elements", kinds}, {"deletions", del}};
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
            records[static_cast<std::size_t>(kind)].push_back(std::move(r));
        }
    }
    std::map<std::string, DeletionRecord> deletions;
    for (const auto& [name, d] : j.at("deletions").items()) {
        deletions[name] = DeletionRecord{d.at("feature").get<std::string>(),
                                         d.at("successors").get<std::vector<std::string>>()};
    }
    return NamedShape::fromRecords(shape.value().occt(), std::move(records), std::move(deletions));
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
        auto feature = registry.create(type);
        if (!feature) throw std::domain_error("unknown feature type '" + type + "'");
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
                for (const auto& s : feature->referenceSlots()) {
                    if (s.name == slot) {
                        kind = s.kind;
                        known = true;
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
            features.push_back(json{{"id", n->id},
                                    {"type", n->type},
                                    {"label", n->label},
                                    {"suppressed", n->suppressed},
                                    {"parameters", n->parameters},
                                    {"options", n->options},
                                    {"references", refs}});
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
        const int version = doc.at("version").get<int>();
        if (version > kFormatVersion) {
            return R::failure(ErrorCode::Unsupported, "format version " + std::to_string(version) +
                                                          " is newer than the supported " +
                                                          std::to_string(kFormatVersion));
        }
        if (version < 1) return R::failure(ErrorCode::ParseError, "invalid format version " + std::to_string(version));
        if (version == 1) doc = migrateV1(doc, *registry);

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
            if (!registry->contains(spec.type)) {
                return R::failure(ErrorCode::Unsupported, "unknown feature type '" + spec.type + "'");
            }
            auto id = part.addFeature(std::move(spec), part.size());
            if (!id) return R::failure(ErrorCode::ParseError, "feature: " + id.error().message);
        }
        const json& rb = doc.value("rollback", json());
        if (!rb.is_null()) {
            auto st = part.setRollback(rb.get<std::size_t>());
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
                n->cache = std::move(c);
            }
        }
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
