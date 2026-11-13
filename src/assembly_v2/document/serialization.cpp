// Собственный формат документа сборки (JSON, docs/assembly/ASSEMBLY_DOCUMENT.md, раздел 10):
// сохранение, загрузка с миграцией v1 → v2, внешние подсборки, диагностика перемещённых и
// изменённых источников по каждому затронутому сопряжению.

#include <bendcore/assembly_v2/assembly_document.hpp>

#include <bendcore/kernel/brep_io.hpp>

#include <Standard_Failure.hxx>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace bendcore::assembly_v2 {

using kernel::Error;
using kernel::ErrorCode;
using kernel::Result;
using json = nlohmann::json;

namespace {

constexpr const char* kFormatName = "bendcore-assembly";
// Допуск совпадения сигнатуры элемента при загрузке (signatureDistance, безразмерный).
constexpr double kSignatureTolerance = 1e-6;
// Относительный допуск отпечатка геометрии детали.
constexpr double kFingerprintTolerance = 1e-6;

// Ошибка разбора внутри загрузчика; на границе превращается в ParseError, а аппаратный сбой
// читателя ядра (ErrorCode::Fault, docs/kernel/KERNEL_CORE.md) — в Fault: код не теряется.
struct FormatError : std::runtime_error {
    explicit FormatError(const std::string& message, ErrorCode errorCode = ErrorCode::ParseError)
        : std::runtime_error(message), code(errorCode) {}
    ErrorCode code;
};

[[noreturn]] void bad(const std::string& message) { throw FormatError(message); }
[[noreturn]] void bad(ErrorCode code, const std::string& message) {
    throw FormatError(message, code == ErrorCode::Fault ? ErrorCode::Fault : ErrorCode::ParseError);
}

// ---------------------------------------------------------------- base64

constexpr char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const std::vector<std::uint8_t>& data) {
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 2 < data.size(); i += 3) {
        const std::uint32_t v = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        out += kB64[(v >> 6) & 63];
        out += kB64[v & 63];
    }
    if (i + 1 == data.size()) {
        const std::uint32_t v = data[i] << 16;
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        out += "==";
    } else if (i + 2 == data.size()) {
        const std::uint32_t v = (data[i] << 16) | (data[i + 1] << 8);
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        out += kB64[(v >> 6) & 63];
        out += '=';
    }
    return out;
}

std::vector<std::uint8_t> base64Decode(const std::string& text) {
    int table[256];
    std::fill(std::begin(table), std::end(table), -1);
    for (int k = 0; k < 64; ++k) table[static_cast<unsigned char>(kB64[k])] = k;
    std::vector<std::uint8_t> out;
    out.reserve(text.size() / 4 * 3);
    std::uint32_t acc = 0;
    int bits = 0;
    std::size_t pad = 0;
    for (char ch : text) {
        if (ch == '=') {
            ++pad;
            continue;
        }
        if (pad > 0) bad("base64: data after padding");
        const int v = table[static_cast<unsigned char>(ch)];
        if (v < 0) bad("base64: invalid character");
        acc = (acc << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((acc >> bits) & 0xFF));
        }
    }
    if (pad > 2 || (text.size() % 4) != 0) bad("base64: invalid length");
    return out;
}

// ---------------------------------------------------------------- значения

const json& field(const json& j, const char* key) {
    if (!j.is_object() || !j.contains(key)) bad(std::string("missing field '") + key + "'");
    return j.at(key);
}

double number(const json& j, const char* key) {
    const json& v = field(j, key);
    if (!v.is_number()) bad(std::string("field '") + key + "' must be a number");
    return v.get<double>();
}

std::uint32_t uid(const json& v, const char* what) {
    if (!v.is_number_unsigned() && !(v.is_number_integer() && v.get<long long>() >= 0))
        bad(std::string(what) + " must be a non-negative integer");
    const auto x = v.get<unsigned long long>();
    if (x > std::numeric_limits<std::uint32_t>::max()) bad(std::string(what) + " is out of range");
    return static_cast<std::uint32_t>(x);
}

std::string text(const json& j, const char* key, const std::string& fallback = {}) {
    if (!j.contains(key) || j.at(key).is_null()) return fallback;
    if (!j.at(key).is_string()) bad(std::string("field '") + key + "' must be a string");
    return j.at(key).get<std::string>();
}

bool flag(const json& j, const char* key) {
    if (!j.contains(key)) return false;
    if (!j.at(key).is_boolean()) bad(std::string("field '") + key + "' must be true or false");
    return j.at(key).get<bool>();
}

json vec(const Vec3& v) { return json::array({v.x(), v.y(), v.z()}); }

Vec3 vecFrom(const json& j) {
    if (!j.is_array() || j.size() != 3) bad("vector must have 3 numbers");
    for (const json& x : j)
        if (!x.is_number()) bad("vector must have 3 numbers");
    return {j[0].get<double>(), j[1].get<double>(), j[2].get<double>()};
}

json point(const kernel::Point3& p) { return json::array({p[0], p[1], p[2]}); }

kernel::Point3 pointFrom(const json& j) {
    const Vec3 v = vecFrom(j);
    return {v.x(), v.y(), v.z()};
}

json placementJson(const Transform& t) {
    return {{"position", vec(t.position)},
            {"rotation", json::array({t.rotation.w(), t.rotation.x(), t.rotation.y(), t.rotation.z()})}};
}

// v2: {"position": [x, y, z], "rotation": [w, x, y, z]};
// v1: {"matrix": [r00, r01, r02, tx, r10, r11, r12, ty, r20, r21, r22, tz]}.
Transform placementFrom(const json& j, int version) {
    Transform t;
    if (version == 1) {
        const json& m = field(j, "matrix");
        if (!m.is_array() || m.size() != 12) bad("v1 placement matrix must have 12 numbers");
        Mat3 r;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) r(row, col) = m[row * 4 + col].get<double>();
            t.position[row] = m[row * 4 + 3].get<double>();
        }
        if ((r.transpose() * r - Mat3::Identity()).norm() > 1e-9 || r.determinant() < 0.0)
            bad("v1 placement matrix is not a rotation");
        t.rotation = Quat(r).normalized();
        return t;
    }
    t.position = vecFrom(field(j, "position"));
    const json& q = field(j, "rotation");
    if (!q.is_array() || q.size() != 4) bad("rotation must be a quaternion [w, x, y, z]");
    t.rotation = Quat(q[0].get<double>(), q[1].get<double>(), q[2].get<double>(), q[3].get<double>());
    if (!(t.rotation.norm() > 0.0)) bad("rotation quaternion is zero");
    // Сохранённый кватернион уже нормирован; повторная нормировка не меняет его более чем на ulp.
    t.rotation.normalize();
    return t;
}

json colorJson(const std::optional<kernel::AssemblyColor>& c) {
    if (!c) return nullptr;
    return json::array({c->r, c->g, c->b, c->alpha});
}

std::optional<kernel::AssemblyColor> colorFrom(const json& j) {
    if (j.is_null()) return std::nullopt;
    if (!j.is_array() || j.size() != 4) bad("colour must be [r, g, b, alpha]");
    kernel::AssemblyColor c{j[0].get<double>(), j[1].get<double>(), j[2].get<double>(), j[3].get<double>()};
    for (double v : {c.r, c.g, c.b, c.alpha})
        if (!(v >= 0.0 && v <= 1.0)) bad("colour component outside [0, 1]");
    return c;
}

json propertiesJson(const PartProperties& p) {
    json j = {{"designation", p.designation}, {"name", p.name}, {"material", p.material}};
    j["density"] = p.density ? json(*p.density) : json(nullptr);
    return j;
}

PartProperties propertiesFrom(const json& j) {
    PartProperties p;
    if (j.is_null()) return p;
    p.designation = text(j, "designation");
    p.name = text(j, "name");
    p.material = text(j, "material");
    if (j.contains("density") && !j.at("density").is_null()) {
        p.density = number(j, "density");
        if (!(*p.density >= 0.0)) bad("density must be >= 0");
    }
    return p;
}

ElementKind kindFrom(const std::string& s) {
    auto k = modeling::elementKindFromString(s);
    if (!k) bad("unknown element kind '" + s + "'");
    return *k;
}

json signatureJson(const modeling::GeometricSignature& s) {
    return {{"valid", s.valid},           {"kind", modeling::toString(s.kind)},
            {"geometry", s.geometry},     {"center", point(s.center)},
            {"hasDirection", s.hasDirection}, {"direction", point(s.direction)},
            {"radius", s.radius},         {"size", s.size}};
}

modeling::GeometricSignature signatureFrom(const json& j) {
    modeling::GeometricSignature s;
    s.valid = flag(j, "valid");
    s.kind = kindFrom(text(j, "kind", "face"));
    s.geometry = text(j, "geometry");
    s.center = pointFrom(field(j, "center"));
    s.hasDirection = flag(j, "hasDirection");
    s.direction = pointFrom(field(j, "direction"));
    s.radius = number(j, "radius");
    s.size = number(j, "size");
    return s;
}

template <typename E, std::size_t N>
E enumFrom(const std::string& s, const E (&values)[N], const char* what) {
    for (E v : values)
        if (s == toString(v)) return v;
    bad(std::string("unknown ") + what + " '" + s + "'");
}

constexpr MateType kMateTypes[] = {MateType::Coincident, MateType::Concentric, MateType::Distance,
                                   MateType::Angle,      MateType::Parallel,   MateType::Perpendicular,
                                   MateType::Tangent,    MateType::Rigid};
constexpr EntityUse kUses[] = {EntityUse::Auto, EntityUse::Axis, EntityUse::Point};
constexpr PatternKind kPatternKinds[] = {PatternKind::Linear, PatternKind::Circular, PatternKind::Feature};

const char* alignmentName(Alignment a) {
    switch (a) {
        case Alignment::Aligned: return "Aligned";
        case Alignment::AntiAligned: return "AntiAligned";
        case Alignment::Closest: return "Closest";
    }
    return "Closest";
}

Alignment alignmentFrom(const std::string& s) {
    for (Alignment a : {Alignment::Aligned, Alignment::AntiAligned, Alignment::Closest})
        if (s == alignmentName(a)) return a;
    bad("unknown alignment '" + s + "'");
}

// ---------------------------------------------------------------- карта имён

json elementMapJson(const modeling::NamedShape& map) {
    json j;
    for (ElementKind kind : {ElementKind::Face, ElementKind::Edge, ElementKind::Vertex}) {
        json list = json::array();
        for (const modeling::ElementRecord& r : map.records(kind))
            list.push_back({{"name", r.name},
                            {"origin", modeling::toString(r.origin)},
                            {"feature", r.feature},
                            {"aliases", r.aliases},
                            {"signature", signatureJson(r.signature)}});
        j[modeling::toString(kind)] = std::move(list);
    }
    json del = json::object();
    for (const auto& [name, d] : map.deletions()) del[name] = {{"feature", d.feature}, {"successors", d.successors}};
    j["deletions"] = std::move(del);
    return j;
}

Result<modeling::NamedShape> elementMapFrom(const json& j, const TopoDS_Shape& shape) {
    std::array<std::vector<modeling::ElementRecord>, 3> records;
    for (ElementKind kind : {ElementKind::Face, ElementKind::Edge, ElementKind::Vertex}) {
        for (const json& r : field(j, modeling::toString(kind))) {
            modeling::ElementRecord rec;
            rec.name = text(r, "name");
            auto origin = modeling::nameOriginFromString(text(r, "origin"));
            if (!origin) bad("unknown name origin '" + text(r, "origin") + "'");
            rec.origin = *origin;
            rec.feature = text(r, "feature");
            if (r.contains("aliases")) rec.aliases = r.at("aliases").get<std::vector<std::string>>();
            rec.signature = signatureFrom(field(r, "signature"));
            records[static_cast<std::size_t>(kind)].push_back(std::move(rec));
        }
    }
    std::map<std::string, modeling::DeletionRecord> deletions;
    if (j.contains("deletions"))
        for (const auto& [name, d] : j.at("deletions").items())
            deletions[name] = {text(d, "feature"), field(d, "successors").get<std::vector<std::string>>()};
    return modeling::NamedShape::fromRecords(shape, std::move(records), std::move(deletions));
}

// ---------------------------------------------------------------- отпечаток геометрии

json fingerprintJson(const Definition& d) {
    json j;
    j["faces"] = d.topology->faces().size();
    j["edges"] = d.topology->edges().size();
    j["vertices"] = d.topology->vertices().size();
    const kernel::Shape s(d.shape);
    if (auto a = s.surfaceProperties()) j["area"] = a.value().area;
    if (auto v = s.volumeProperties()) j["volume"] = v.value().volume;
    if (auto b = s.boundingBox())
        j["box"] = {b.value().min[0], b.value().min[1], b.value().min[2],
                    b.value().max[0], b.value().max[1], b.value().max[2]};
    return j;
}

// Пусто — геометрия совпадает с сохранённой; иначе — описание первого различия.
std::string fingerprintDifference(const json& saved, const json& current) {
    for (const char* k : {"faces", "edges", "vertices"})
        if (saved.value(k, -1LL) != current.value(k, -1LL))
            return std::string(k) + " " + std::to_string(saved.value(k, -1LL)) + " -> " +
                   std::to_string(current.value(k, -1LL));
    double scale = 1.0;
    if (saved.contains("box")) {
        const auto& b = saved.at("box");
        scale = std::max(1.0, std::hypot(b[3].get<double>() - b[0].get<double>(), b[4].get<double>() - b[1].get<double>(),
                                         b[5].get<double>() - b[2].get<double>()));
    }
    for (const char* k : {"area", "volume"}) {
        if (saved.contains(k) != current.contains(k)) return std::string(k) + " appeared or disappeared";
        if (!saved.contains(k)) continue;
        const double a = saved.at(k).get<double>(), b = current.at(k).get<double>();
        if (std::abs(a - b) > kFingerprintTolerance * std::max(1.0, std::abs(a))) {
            std::ostringstream s;
            s << k << " " << a << " -> " << b;
            return s.str();
        }
    }
    if (saved.contains("box") && current.contains("box"))
        for (int i = 0; i < 6; ++i)
            if (std::abs(saved.at("box")[i].get<double>() - current.at("box")[i].get<double>()) >
                kFingerprintTolerance * scale)
                return "bounding box changed";
    return {};
}

double modelScale(const TopoDS_Shape& shape) {
    auto b = kernel::Shape(shape).boundingBox();
    if (!b) return 1.0;
    const auto& x = b.value();
    const double d = std::hypot(x.max[0] - x.min[0], x.max[1] - x.min[1], x.max[2] - x.min[2]);
    return d > 0.0 ? d : 1.0;
}

std::shared_ptr<const kernel::TopologyIndex> topologyOf(const TopoDS_Shape& shape) {
    auto index = kernel::TopologyIndex::build(kernel::Shape(shape));
    if (!index) bad("cannot index part shape: " + index.error().message);
    return std::make_shared<const kernel::TopologyIndex>(std::move(index).value());
}

// ---------------------------------------------------------------- пути источников

std::string joinSource(const std::string& dir, const std::string& source) {
    if (dir.empty() || source.empty()) return source;
    const std::filesystem::path p(source);
    if (p.is_absolute()) return source;
    return (std::filesystem::path(dir) / p).lexically_normal().generic_string();
}

std::string directoryOf(const std::string& source) {
    return std::filesystem::path(source).parent_path().generic_string();
}

// Цепочка загружаемых внешних подсборок (пути относительно документа верхнего уровня).
thread_local std::vector<std::string> g_loadStack;
thread_local std::string g_loadPrefix;

LoadOptions nestedOptions(const LoadOptions& outer, const std::string& dir) {
    LoadOptions o;
    if (outer.parts) o.parts = [p = outer.parts, dir](const std::string& s) { return p(joinSource(dir, s)); };
    if (outer.assemblies)
        o.assemblies = [a = outer.assemblies, dir](const std::string& s) { return a(joinSource(dir, s)); };
    return o;
}

Result<LoadedAssembly> loadExternal(const std::string& source, const LoadOptions& options) {
    if (!options.assemblies) return Error{ErrorCode::InvalidInput, "no reader for subassembly documents"};
    const std::string key = joinSource(g_loadPrefix, source);
    if (std::find(g_loadStack.begin(), g_loadStack.end(), key) != g_loadStack.end()) {
        std::string chain;
        for (const std::string& s : g_loadStack) chain += s + " -> ";
        return Error{ErrorCode::InvalidInput, "cycle of external subassemblies: " + chain + key};
    }
    auto text = options.assemblies(source);
    if (!text) return Error{text.error().code, "cannot read subassembly '" + source + "': " + text.error().message};
    const std::string savedPrefix = g_loadPrefix;
    g_loadStack.push_back(key);
    g_loadPrefix = directoryOf(key);
    auto loaded = AssemblyDocument::fromJson(text.value(), nestedOptions(options, directoryOf(source)));
    g_loadStack.pop_back();
    g_loadPrefix = savedPrefix;
    if (!loaded) return Error{loaded.error().code, "subassembly '" + source + "': " + loaded.error().message};
    return loaded;
}

}  // namespace

const char* toString(SourceStatus status) noexcept {
    switch (status) {
        case SourceStatus::Embedded: return "Embedded";
        case SourceStatus::Loaded: return "Loaded";
        case SourceStatus::Changed: return "Changed";
        case SourceStatus::MissingEmbedded: return "MissingEmbedded";
        case SourceStatus::Missing: return "Missing";
        case SourceStatus::AssemblyLoaded: return "AssemblyLoaded";
        case SourceStatus::AssemblyMissing: return "AssemblyMissing";
    }
    return "Unknown";
}

const char* toString(MateOutcome outcome) noexcept {
    switch (outcome) {
        case MateOutcome::Kept: return "Kept";
        case MateOutcome::Remapped: return "Remapped";
        case MateOutcome::Broken: return "Broken";
    }
    return "Unknown";
}

// ---------------------------------------------------------------- внешние подсборки

Status AssemblyDocument::mergeExternal(DefinitionId target, const AssemblyDocument& sub, const std::string& subSource) {
    const std::string dir = directoryOf(subSource);
    std::vector<DefinitionId> map(sub.definitions_.size(), 0);
    map[0] = target;
    for (std::size_t i = 1; i < sub.definitions_.size(); ++i) {
        const Definition& sd = sub.definitions_[i];
        const std::string source = joinSource(dir, sd.source);
        if (sd.kind == DefinitionKind::Part && !source.empty()) {
            // Деталь из того же файла уже есть в документе — одно определение (BOM, замена).
            auto same = std::find_if(definitions_.begin(), definitions_.end(), [&](const Definition& d) {
                return d.kind == DefinitionKind::Part && d.source == source;
            });
            if (same != definitions_.end()) {
                map[i] = same->id;
                continue;
            }
        }
        Definition d = sd;
        d.id = static_cast<DefinitionId>(definitions_.size());
        d.source = source;
        d.external = true;
        d.occurrences.clear();
        map[i] = d.id;
        definitions_.push_back(std::move(d));
    }
    for (std::size_t i = 0; i < sub.definitions_.size(); ++i) {
        if (sub.definitions_[i].kind != DefinitionKind::Assembly) continue;
        for (const Occurrence& so : sub.definitions_[i].occurrences) {
            Occurrence o;
            o.id = nextOccurrence_++;
            o.name = so.name;
            o.definition = map[so.definition];
            o.placement = so.placement;
            o.color = so.color;
            o.suppressed = so.suppressed;
            o.designation = so.designation;
            o.sourceId = so.sourceId.value_or(so.id);
            definitions_[map[i]].occurrences.push_back(std::move(o));
        }
    }
    return std::monostate{};
}

Result<DefinitionId> AssemblyDocument::addSubassemblyFromSource(std::string name, std::string source,
                                                                const LoadOptions& options, PartProperties properties) {
    if (source.empty()) return Error{ErrorCode::InvalidInput, "subassembly source is empty"};
    auto loaded = loadExternal(source, options);
    if (!loaded) return loaded.error();
    const AssemblyDocument& sub = loaded.value().document;
    if (properties.designation.empty() && properties.name.empty() && properties.material.empty() &&
        !properties.density)
        properties = sub.definitions_[0].properties;
    auto id = addSubassembly(name.empty() ? sub.name() : std::move(name), std::move(properties));
    if (!id) return id;
    definitions_[id.value()].source = source;
    definitions_[id.value()].color = sub.definitions_[0].color;
    if (auto m = mergeExternal(id.value(), sub, source); !m) return m.error();
    return id;
}

// ---------------------------------------------------------------- сохранение

Result<std::string> AssemblyDocument::toJson(const SaveOptions& options) const {
    try {
        // Определения, достижимые без захода внутрь внешних подсборок.
        std::set<DefinitionId> reachable;
        std::function<void(DefinitionId)> visit = [&](DefinitionId id) {
            if (!reachable.insert(id).second) return;
            if (isReadOnlyAssembly(id)) return;
            for (const Occurrence& o : definitions_[id].occurrences) visit(o.definition);
        };
        visit(root());

        // Локальный ID вхождения в пути: в составе внешней подсборки — ID из её файла.
        auto localId = [&](OccurrenceId id) -> OccurrenceId {
            const Occurrence* o = findOccurrence(id);
            return o && o->sourceId ? *o->sourceId : id;
        };
        auto refJson = [&](const GeometryRef& r) {
            json path = json::array();
            for (std::size_t k = 0; k < r.path.size(); ++k) path.push_back(k == 0 ? r.path[0] : localId(r.path[k]));
            json j = {{"path", path}, {"kind", modeling::toString(r.element.kind)}, {"use", toString(r.use)}};
            if (r.element.index) j["index"] = *r.element.index;
            if (!r.element.name.empty()) j["name"] = r.element.name;
            return j;
        };
        auto boundJson = [](const BoundElement& b) {
            return json{{"index", b.index}, {"signature", signatureJson(b.signature)}};
        };
        auto occurrenceJson = [&](const Occurrence& o) {
            json j = {{"id", o.id},           {"name", o.name},       {"definition", o.definition},
                      {"placement", placementJson(o.placement)},     {"fixed", o.fixed},
                      {"color", colorJson(o.color)}, {"suppressed", o.suppressed}, {"designation", o.designation}};
            if (o.pattern) j["pattern"] = *o.pattern;
            return j;
        };

        json doc;
        doc["format"] = kFormatName;
        doc["version"] = kFormatVersion;
        doc["name"] = name();
        doc["properties"] = propertiesJson(definitions_[0].properties);
        doc["color"] = colorJson(definitions_[0].color);
        doc["next"] = {{"occurrence", nextOccurrence_}, {"mate", nextMate_}, {"pattern", nextPattern_}};

        json defs = json::array();
        for (std::size_t i = 1; i < definitions_.size(); ++i) {
            const Definition& d = definitions_[i];
            if (d.external && !reachable.count(d.id)) continue;  // принадлежит файлу внешней подсборки
            json j = {{"id", d.id},
                      {"kind", d.kind == DefinitionKind::Part ? "part" : "assembly"},
                      {"name", d.name},
                      {"properties", propertiesJson(d.properties)},
                      {"color", colorJson(d.color)}};
            if (!d.source.empty()) j["source"] = d.source;
            if (d.kind == DefinitionKind::Part) {
                j["revision"] = d.revision;
                json fc = json::array();
                for (const kernel::FaceColor& f : d.faceColors) fc.push_back({{"face", f.faceIndex}, {"color", colorJson(f.color)}});
                j["faceColors"] = std::move(fc);
                if (!d.shape.IsNull()) {
                    j["fingerprint"] = fingerprintJson(d);
                    if (options.embedGeometry) {
                        auto bytes = kernel::writeBrepBinary(kernel::Shape(d.shape));
                        if (!bytes) return Error{bytes.error().code, "part '" + d.name + "': " + bytes.error().message};
                        j["brep"] = base64Encode(bytes.value());
                        if (d.elementMap) j["elementMap"] = elementMapJson(*d.elementMap);
                    }
                }
            } else if (!isReadOnlyAssembly(d.id) || d.source.empty()) {
                json occ = json::array();
                for (const Occurrence& o : d.occurrences) occ.push_back(occurrenceJson(o));
                j["occurrences"] = std::move(occ);
            }
            defs.push_back(std::move(j));
        }
        doc["definitions"] = std::move(defs);
        json top = json::array();
        for (const Occurrence& o : topLevel()) top.push_back(occurrenceJson(o));
        doc["occurrences"] = std::move(top);

        json mates = json::array();
        for (const DocumentMate& m : mates_) {
            json j = {{"id", m.id},
                      {"type", toString(m.spec.type)},
                      {"name", m.spec.name},
                      {"a", refJson(m.spec.a)},
                      {"b", refJson(m.spec.b)},
                      {"value", m.spec.value},
                      {"alignment", alignmentName(m.spec.alignment)},
                      {"internalTangency", m.spec.internalTangency},
                      {"suppressed", m.suppressed},
                      {"bound", json::array({boundJson(m.bound[0]), boundJson(m.bound[1])})}};
            if (m.rigidRelative) j["rigidRelative"] = placementJson(*m.rigidRelative);
            if (m.broken()) j["broken"] = m.brokenReason;
            mates.push_back(std::move(j));
        }
        doc["mates"] = std::move(mates);

        json patterns = json::array();
        for (const ComponentPattern& p : patterns_) {
            json j = {{"id", p.id}, {"kind", toString(p.kind)}, {"seed", p.seed}, {"instances", p.instances}};
            j["reference"] = p.reference ? json(*p.reference) : json(nullptr);
            if (const auto* l = std::get_if<LinearPatternSpec>(&p.spec)) {
                j["spec"] = {{"direction", vec(l->direction)}, {"spacing", l->spacing},     {"count", l->count},
                             {"direction2", vec(l->direction2)}, {"spacing2", l->spacing2}, {"count2", l->count2}};
            } else if (const auto* c = std::get_if<CircularPatternSpec>(&p.spec)) {
                j["spec"] = {{"axisPoint", vec(c->axisPoint)}, {"axisDirection", vec(c->axisDirection)},
                             {"angleStep", c->angleStep},      {"count", c->count}};
            } else {
                const auto& f = std::get<FeaturePatternSpec>(p.spec);
                j["spec"] = {{"seedHole", refJson(f.seedHole)}, {"tolerance", f.tolerance}};
                j["spec"]["radius"] = f.radius ? json(*f.radius) : json(nullptr);
                if (p.holeBound) j["holeBound"] = boundJson(*p.holeBound);
            }
            patterns.push_back(std::move(j));
        }
        doc["patterns"] = std::move(patterns);

        json views = json::array();
        for (const ExplodedView& v : views_) {
            json steps = json::array();
            for (const ExplodeStep& s : v.steps)
                steps.push_back({{"occurrences", s.occurrences}, {"direction", vec(s.direction)}, {"distance", s.distance}});
            views.push_back({{"name", v.name}, {"steps", std::move(steps)}});
        }
        doc["explodedViews"] = std::move(views);
        return doc.dump(options.indent);
    } catch (const Standard_Failure& e) {
        return Error{ErrorCode::OperationFailed, std::string("OCCT exception while saving: ") + e.what()};
    } catch (const json::exception& e) {
        return Error{ErrorCode::OperationFailed, std::string("JSON error while saving: ") + e.what()};
    }
}

// ---------------------------------------------------------------- загрузка

class JsonLoader {
public:
    explicit JsonLoader(const LoadOptions& options) : options_(options) {}

    Result<LoadedAssembly> load(std::string_view textIn) {
        json j;
        try {
            j = json::parse(textIn);
        } catch (const json::exception& e) {  // parse_error и out_of_range (переполнение числа: 1e999)
            return Error{ErrorCode::ParseError, std::string("assembly document is not JSON: ") + e.what()};
        }
        try {
            if (!j.is_object() || text(j, "format") != kFormatName)
                return Error{ErrorCode::ParseError, "not a bendcore assembly document (format != 'bendcore-assembly')"};
            const json& v = field(j, "version");
            if (!v.is_number_integer()) bad("version must be an integer");
            version_ = v.get<int>();
            if (version_ > AssemblyDocument::kFormatVersion)
                return Error{ErrorCode::Unsupported, "assembly document version " + std::to_string(version_) +
                                                         " is newer than supported " +
                                                         std::to_string(AssemblyDocument::kFormatVersion)};
            if (version_ < 1) bad("invalid version " + std::to_string(version_));
            LoadedAssembly out{AssemblyDocument(text(j, "name", "Assembly")), {}};
            doc_ = &out.document;
            report_ = &out.report;
            report_->fileVersion = version_;
            report_->migrated = version_ < AssemblyDocument::kFormatVersion;
            if (j.contains("properties")) doc_->definitions_[0].properties = propertiesFrom(j.at("properties"));
            if (j.contains("color")) doc_->definitions_[0].color = colorFrom(j.at("color"));
            readDefinitions(j);
            readExternals();
            readMates(j);
            readPatterns(j);
            readViews(j);
            finish();
            return out;
        } catch (const FormatError& e) {
            return Error{e.code, std::string("assembly document: ") + e.what()};
        } catch (const json::exception& e) {
            return Error{ErrorCode::ParseError, std::string("assembly document: ") + e.what()};
        } catch (const Standard_Failure& e) {
            return Error{ErrorCode::OperationFailed, std::string("OCCT exception while loading: ") + e.what()};
        }
    }

private:
    DefinitionId mapDefinition(const json& v) {
        const std::uint32_t saved = uid(v, "definition id");
        auto it = defMap_.find(saved);
        if (it == defMap_.end()) bad("unknown definition id " + std::to_string(saved));
        return it->second;
    }

    void loadPart(const json& d, Definition& def) {
        const std::uint32_t savedRevision = d.contains("revision") ? uid(d.at("revision"), "revision") : 0;
        TopoDS_Shape embedded;
        std::shared_ptr<const modeling::NamedShape> embeddedMap;
        if (d.contains("brep")) {
            const auto bytes = base64Decode(text(d, "brep"));
            auto shape = kernel::readBrepBinary(bytes);
            if (!shape) bad(shape.error().code, "part '" + def.name + "': embedded BRep: " + shape.error().message);
            embedded = shape.value().occt();
            if (d.contains("elementMap")) {
                auto map = elementMapFrom(d.at("elementMap"), embedded);
                if (map) embeddedMap = std::make_shared<const modeling::NamedShape>(std::move(map).value());
                else report_->warnings.push_back("part '" + def.name + "': element map ignored: " + map.error().message);
            }
        }
        SourceDiagnostic diag;
        diag.definition = def.id;
        diag.name = def.name;
        diag.source = def.source;
        bool changed = false;
        std::string sourceError;
        if (!def.source.empty()) {
            Result<PartData> data = options_.parts ? options_.parts(def.source)
                                                   : Result<PartData>(Error{ErrorCode::InvalidInput, "no part resolver"});
            if (data && checkShape(data.value())) {
                def.shape = data.value().shape;
                def.elementMap = data.value().elementMap;
                def.topology = topologyOf(def.shape);
                const json now = fingerprintJson(def);
                const std::string diff = d.contains("fingerprint") ? fingerprintDifference(d.at("fingerprint"), now) : "";
                changed = !diff.empty();
                diag.status = changed ? SourceStatus::Changed : SourceStatus::Loaded;
                diag.message = changed ? "geometry of '" + def.source + "' changed since the document was saved: " + diff
                                       : "loaded";
            } else {
                // A hardware fault while reading the source leaves the process state suspect:
                // the whole load fails with Fault instead of falling back to embedded geometry.
                if (!data && data.error().code == ErrorCode::Fault) {
                    bad(ErrorCode::Fault, "part '" + def.name + "': source '" + def.source + "': " + data.error().message);
                }
                sourceError = data ? "the source has no shape" : data.error().message;
            }
        }
        if (def.shape.IsNull()) {
            if (!embedded.IsNull()) {
                def.shape = embedded;
                def.elementMap = embeddedMap;
                def.topology = topologyOf(embedded);
                if (def.source.empty()) {
                    diag.status = SourceStatus::Embedded;
                    diag.message = "embedded geometry";
                } else {
                    diag.status = SourceStatus::MissingEmbedded;
                    diag.message = "cannot load '" + def.source + "' (" + sourceError + "); embedded geometry used";
                }
            } else if (!def.source.empty()) {
                diag.status = SourceStatus::Missing;
                def.loadError = "cannot load '" + def.source + "': " + sourceError;
                diag.message = def.loadError + "; no embedded geometry";
                changed = true;
            } else {
                bad("part '" + def.name + "' has neither a source nor embedded geometry");
            }
        }
        def.revision = savedRevision + (changed ? 1u : 0u);
        savedRevision_[def.id] = savedRevision;
        if (diag.status != SourceStatus::Embedded && diag.status != SourceStatus::Loaded) affected_.insert(def.id);
        if (d.contains("faceColors") && !changed) {
            for (const json& f : d.at("faceColors")) {
                const std::uint32_t face = uid(field(f, "face"), "face index");
                auto c = colorFrom(field(f, "color"));
                if (!c || face >= def.topology->faces().size()) bad("part '" + def.name + "': invalid face colour");
                def.faceColors.push_back({face, *c});
            }
        }
        report_->sources.push_back(std::move(diag));
    }

    static bool checkShape(const PartData& data) { return !data.shape.IsNull(); }

    void readOccurrences(const json& list, DefinitionId parent) {
        if (!list.is_array()) bad("occurrences must be an array");
        for (const json& o : list) {
            Occurrence occ;
            occ.id = uid(field(o, "id"), "occurrence id");
            if (occ.id == 0 || !occurrenceIds_.insert(occ.id).second) bad("duplicate occurrence id " + std::to_string(occ.id));
            occ.name = text(o, "name");
            occ.definition = mapDefinition(field(o, "definition"));
            if (occ.definition == 0) bad("occurrence " + std::to_string(occ.id) + " inserts the root assembly");
            occ.placement = placementFrom(field(o, "placement"), version_);
            occ.fixed = parent == 0 && flag(o, "fixed");
            if (o.contains("color")) occ.color = colorFrom(o.at("color"));
            if (o.contains("pattern") && !o.at("pattern").is_null()) occ.pattern = uid(o.at("pattern"), "pattern id");
            occ.suppressed = flag(o, "suppressed");
            occ.designation = text(o, "designation");
            doc_->definitions_[parent].occurrences.push_back(std::move(occ));
        }
    }

    void readDefinitions(const json& j) {
        defMap_[0] = 0;
        std::vector<std::pair<DefinitionId, const json*>> assemblies;
        for (const json& d : field(j, "definitions")) {
            const std::uint32_t saved = uid(field(d, "id"), "definition id");
            if (saved == 0 || defMap_.count(saved)) bad("duplicate or root definition id " + std::to_string(saved));
            Definition def;
            def.id = static_cast<DefinitionId>(doc_->definitions_.size());
            const std::string kind = text(d, "kind");
            if (kind != "part" && kind != "assembly") bad("unknown definition kind '" + kind + "'");
            def.kind = kind == "part" ? DefinitionKind::Part : DefinitionKind::Assembly;
            def.name = text(d, "name");
            if (d.contains("properties")) def.properties = propertiesFrom(d.at("properties"));
            if (d.contains("color")) def.color = colorFrom(d.at("color"));
            def.source = text(d, "source");
            if (def.kind == DefinitionKind::Part) loadPart(d, def);
            else assemblies.emplace_back(def.id, &d);
            defMap_[saved] = def.id;
            doc_->definitions_.push_back(std::move(def));
        }
        readOccurrences(field(j, "occurrences"), 0);
        for (const auto& [id, d] : assemblies)
            if (doc_->definitions_[id].source.empty()) readOccurrences(d->contains("occurrences") ? d->at("occurrences") : json::array(), id);
        // Циклы подсборок (повреждённый файл).
        for (const Definition& d : doc_->definitions_)
            for (const Occurrence& o : d.occurrences)
                if (doc_->definitions_[o.definition].kind == DefinitionKind::Assembly &&
                    doc_->containsDefinition(o.definition, d.id))
                    bad("subassembly cycle through '" + doc_->definitions_[o.definition].name + "'");
        OccurrenceId maxId = 0;
        for (OccurrenceId id : occurrenceIds_) maxId = std::max(maxId, id);
        const json next = j.contains("next") ? j.at("next") : json::object();
        doc_->nextOccurrence_ = std::max<OccurrenceId>(maxId + 1, next.contains("occurrence") ? uid(next.at("occurrence"), "next") : 1);
        nextMate_ = next.contains("mate") ? uid(next.at("mate"), "next") : 1;
        nextPattern_ = next.contains("pattern") ? uid(next.at("pattern"), "next") : 1;
    }

    void readExternals() {
        const std::size_t own = doc_->definitions_.size();
        for (std::size_t i = 1; i < own; ++i) {
            Definition& d = doc_->definitions_[i];
            if (d.kind != DefinitionKind::Assembly || d.source.empty()) continue;
            SourceDiagnostic diag;
            diag.definition = d.id;
            diag.name = d.name;
            diag.source = d.source;
            auto loaded = loadExternal(d.source, options_);
            if (!loaded) {
                diag.status = SourceStatus::AssemblyMissing;
                diag.message = loaded.error().message + "; the subassembly is empty";
                affected_.insert(d.id);
                report_->sources.push_back(std::move(diag));
                continue;
            }
            const std::string source = d.source;
            doc_->mergeExternal(static_cast<DefinitionId>(i), loaded.value().document, source);
            diag.status = SourceStatus::AssemblyLoaded;
            diag.message = "loaded";
            report_->sources.push_back(std::move(diag));
            // Диагностика источников внутри подсборки — с указанием файла подсборки.
            for (SourceDiagnostic s : loaded.value().report.sources) {
                if (s.status == SourceStatus::Loaded || s.status == SourceStatus::Embedded ||
                    s.status == SourceStatus::AssemblyLoaded)
                    continue;
                s.definition = static_cast<DefinitionId>(i);
                s.message = "in '" + source + "': " + s.message;
                report_->sources.push_back(std::move(s));
                affected_.insert(static_cast<DefinitionId>(i));
            }
            for (const std::string& w : loaded.value().report.warnings) report_->warnings.push_back("in '" + source + "': " + w);
        }
    }

    // Путь: path[0] — ID вхождения верхнего уровня, далее — локальные ID в составе родителя.
    OccurrencePath decodePath(const json& raw, std::string& error) {
        OccurrencePath path;
        if (!raw.is_array() || raw.empty()) bad("reference path must be a non-empty array");
        DefinitionId current = 0;
        bool lost = false;
        for (const json& v : raw) {
            const OccurrenceId local = uid(v, "path element");
            if (lost) {
                path.push_back(local);
                continue;
            }
            const auto& occs = doc_->definitions_[current].occurrences;
            auto it = std::find_if(occs.begin(), occs.end(), [&](const Occurrence& o) {
                return o.sourceId.value_or(o.id) == local;
            });
            if (it == occs.end()) {
                if (current == 0) bad("reference to unknown top-level occurrence " + std::to_string(local));
                error = "occurrence " + std::to_string(local) + " no longer exists in '" +
                        doc_->definitions_[current].name + "'";
                lost = true;
                path.push_back(local);
                continue;
            }
            path.push_back(it->id);
            current = it->definition;
        }
        return path;
    }

    GeometryRef readRef(const json& r, std::string& error) {
        GeometryRef ref;
        ref.path = decodePath(field(r, "path"), error);
        ref.element.kind = kindFrom(text(r, "kind", "face"));
        if (r.contains("index") && !r.at("index").is_null()) ref.element.index = uid(r.at("index"), "element index");
        ref.element.name = text(r, "name");
        ref.use = enumFrom(text(r, "use", "Auto"), kUses, "entity use");
        return ref;
    }

    // Привязка из файла; при несовпадении сигнатуры ревизия сбрасывается, и revalidateMates
    // ищет элемент заново.
    BoundElement readBound(const json& b, const GeometryRef& ref, std::set<DefinitionId>& touched) {
        BoundElement bound;
        bound.index = uid(field(b, "index"), "bound index");
        bound.signature = signatureFrom(field(b, "signature"));
        auto def = doc_->terminalDefinition(ref.path);
        if (!def) return bound;
        const Definition& d = doc_->definitions_[def.value()];
        bound.definition = d.id;
        auto saved = savedRevision_.find(d.id);
        bound.revision = saved != savedRevision_.end() ? saved->second : d.revision;
        if (affected_.count(d.id)) touched.insert(d.id);
        if (d.kind != DefinitionKind::Part || !d.topology) return bound;
        bool same = bound.index < elementCount(*d.topology, ref.element.kind);
        if (same && bound.signature.valid) {
            auto sig = modeling::computeSignature(elementAt(*d.topology, ref.element.kind, bound.index));
            same = sig && modeling::signatureDistance(bound.signature, sig.value(), modelScale(d.shape)) <=
                              kSignatureTolerance;
        }
        if (!same) {
            bound.revision = std::numeric_limits<std::uint32_t>::max();
            touched.insert(d.id);
        }
        return bound;
    }

    void readMates(const json& j) {
        if (!j.contains("mates")) return;
        std::set<MateId> ids;
        for (const json& m : j.at("mates")) {
            DocumentMate dm;
            dm.id = uid(field(m, "id"), "mate id");
            if (dm.id == 0 || !ids.insert(dm.id).second) bad("duplicate mate id " + std::to_string(dm.id));
            std::string pathError;
            dm.spec.type = enumFrom(text(m, "type"), kMateTypes, "mate type");
            dm.spec.name = text(m, "name");
            dm.spec.a = readRef(field(m, "a"), pathError);
            dm.spec.b = readRef(field(m, "b"), pathError);
            dm.spec.value = m.contains("value") ? number(m, "value") : 0.0;
            dm.spec.alignment = alignmentFrom(text(m, "alignment", "Closest"));
            dm.spec.internalTangency = flag(m, "internalTangency");
            dm.suppressed = flag(m, "suppressed");
            dm.brokenReason = text(m, "broken");
            if (m.contains("rigidRelative")) dm.rigidRelative = placementFrom(m.at("rigidRelative"), version_);
            std::set<DefinitionId> touched;
            const bool rigid = dm.spec.type == MateType::Rigid;
            if (!pathError.empty()) {
                dm.brokenReason = pathError;
                touched.insert(0);
            } else if (version_ >= 2 && m.contains("bound") && !rigid) {
                const json& b = m.at("bound");
                if (!b.is_array() || b.size() != 2) bad("mate bound must have two sides");
                dm.bound[0] = readBound(b[0], dm.spec.a, touched);
                dm.bound[1] = readBound(b[1], dm.spec.b, touched);
            } else {
                // v1 (без сигнатур) и жёсткие сопряжения: привязка по текущей геометрии.
                for (int k = 0; k < 2; ++k) {
                    const GeometryRef& ref = k == 0 ? dm.spec.a : dm.spec.b;
                    if (auto s = doc_->bindSide(ref, dm.bound[k], !rigid); !s && dm.brokenReason.empty())
                        dm.brokenReason = s.error().message;
                    if (auto def = doc_->terminalDefinition(ref.path); def && affected_.count(def.value()))
                        touched.insert(def.value());
                }
                if (rigid && !dm.rigidRelative) {
                    const Occurrence* oa = doc_->findOccurrence(dm.spec.a.path[0]);
                    const Occurrence* ob = doc_->findOccurrence(dm.spec.b.path[0]);
                    dm.rigidRelative = oa->placement.inverse() * ob->placement;
                }
            }
            if (!touched.empty()) touched_[dm.id] = std::move(touched);
            doc_->mates_.push_back(std::move(dm));
        }
        MateId maxId = 0;
        for (MateId id : ids) maxId = std::max(maxId, id);
        doc_->nextMate_ = std::max(nextMate_, maxId + 1);
    }

    void readPatterns(const json& j) {
        PatternId maxId = 0;
        if (j.contains("patterns")) {
            for (const json& p : j.at("patterns")) {
                ComponentPattern cp;
                cp.id = uid(field(p, "id"), "pattern id");
                maxId = std::max(maxId, cp.id);
                cp.kind = enumFrom(text(p, "kind"), kPatternKinds, "pattern kind");
                cp.seed = uid(field(p, "seed"), "pattern seed");
                if (p.contains("reference") && !p.at("reference").is_null()) cp.reference = uid(p.at("reference"), "pattern reference");
                for (const json& i : field(p, "instances")) cp.instances.push_back(uid(i, "pattern instance"));
                for (OccurrenceId id : cp.instances) {
                    const Occurrence* o = doc_->findOccurrence(id);
                    if (!o || o->pattern != std::optional<PatternId>(cp.id))
                        bad("pattern " + std::to_string(cp.id) + ": instance " + std::to_string(id) + " is not its occurrence");
                }
                if (!doc_->findOccurrence(cp.seed)) bad("pattern " + std::to_string(cp.id) + ": unknown seed");
                const json& s = field(p, "spec");
                if (cp.kind == PatternKind::Linear) {
                    LinearPatternSpec l;
                    l.seed = cp.seed;
                    l.reference = cp.reference;
                    l.direction = vecFrom(field(s, "direction"));
                    l.spacing = number(s, "spacing");
                    l.count = static_cast<int>(number(s, "count"));
                    l.direction2 = vecFrom(field(s, "direction2"));
                    l.spacing2 = number(s, "spacing2");
                    l.count2 = static_cast<int>(number(s, "count2"));
                    cp.spec = l;
                } else if (cp.kind == PatternKind::Circular) {
                    CircularPatternSpec c;
                    c.seed = cp.seed;
                    c.reference = cp.reference;
                    c.axisPoint = vecFrom(field(s, "axisPoint"));
                    c.axisDirection = vecFrom(field(s, "axisDirection"));
                    c.angleStep = number(s, "angleStep");
                    c.count = static_cast<int>(number(s, "count"));
                    cp.spec = c;
                } else {
                    FeaturePatternSpec f;
                    f.seed = cp.seed;
                    std::string pathError;
                    f.seedHole = readRef(field(s, "seedHole"), pathError);
                    f.tolerance = number(s, "tolerance");
                    if (s.contains("radius") && !s.at("radius").is_null()) f.radius = number(s, "radius");
                    if (!pathError.empty()) report_->warnings.push_back("pattern " + std::to_string(cp.id) + ": " + pathError);
                    std::set<DefinitionId> touched;
                    if (p.contains("holeBound")) cp.holeBound = readBound(p.at("holeBound"), f.seedHole, touched);
                    cp.spec = f;
                }
                std::vector<Transform> transforms(cp.instances.size());
                cp.transforms = std::move(transforms);
                doc_->patterns_.push_back(std::move(cp));
            }
        }
        doc_->nextPattern_ = std::max(nextPattern_, maxId + 1);
        for (const Definition& d : doc_->definitions_)
            for (const Occurrence& o : d.occurrences)
                if (o.pattern && std::none_of(doc_->patterns_.begin(), doc_->patterns_.end(),
                                              [&](const ComponentPattern& p) { return p.id == *o.pattern; }))
                    bad("occurrence " + std::to_string(o.id) + " refers to unknown pattern " + std::to_string(*o.pattern));
    }

    void readViews(const json& j) {
        if (!j.contains("explodedViews")) return;
        for (const json& v : j.at("explodedViews")) {
            ExplodedView view;
            view.name = text(v, "name");
            for (const json& s : field(v, "steps")) {
                ExplodeStep step;
                for (const json& o : field(s, "occurrences")) step.occurrences.push_back(uid(o, "explode occurrence"));
                step.direction = vecFrom(field(s, "direction"));
                step.distance = number(s, "distance");
                view.steps.push_back(std::move(step));
            }
            doc_->views_.push_back(std::move(view));
        }
    }

    // Перепроверка сопряжений, затронутых изменёнными/отсутствующими источниками, и отчёт.
    void finish() {
        ReplaceReport rr;
        doc_->revalidateMates(rr);
        std::map<MateId, std::string> broken(rr.broken.begin(), rr.broken.end());
        std::set<MateId> remapped(rr.remapped.begin(), rr.remapped.end());
        for (const DocumentMate& m : doc_->mates_) {
            const auto t = touched_.find(m.id);
            const bool isBroken = broken.count(m.id) > 0;
            if (t == touched_.end() && !isBroken && !remapped.count(m.id)) continue;
            MateLoadDiagnostic diag;
            diag.mate = m.id;
            diag.name = m.spec.name;
            if (t != touched_.end())
                for (DefinitionId d : t->second)
                    if (d != 0) diag.definitions.push_back(d);
            std::string sources;
            for (DefinitionId d : diag.definitions) {
                if (!sources.empty()) sources += ", ";
                const Definition& def = doc_->definitions_[d];
                sources += "'" + def.name + "'" + (def.source.empty() ? std::string() : " (" + def.source + ")");
            }
            if (isBroken) {
                diag.outcome = MateOutcome::Broken;
                diag.message = "broken: " + broken.at(m.id);
            } else if (remapped.count(m.id)) {
                diag.outcome = MateOutcome::Remapped;
                diag.message = "references found again on the changed geometry of " + sources;
            } else {
                diag.outcome = MateOutcome::Kept;
                diag.message = "references unchanged; geometry of " + sources + " is the saved one";
            }
            report_->mates.push_back(std::move(diag));
        }
        if (auto u = doc_->updatePatterns(); !u) report_->warnings.push_back(u.error().message);
    }

    const LoadOptions& options_;
    AssemblyDocument* doc_ = nullptr;
    LoadReport* report_ = nullptr;
    int version_ = 0;
    std::map<std::uint32_t, DefinitionId> defMap_;
    std::map<DefinitionId, std::uint32_t> savedRevision_;
    std::set<DefinitionId> affected_;  // источник изменён или не загрузился
    std::map<MateId, std::set<DefinitionId>> touched_;
    std::set<OccurrenceId> occurrenceIds_;
    MateId nextMate_ = 1;
    PatternId nextPattern_ = 1;
};

Result<LoadedAssembly> AssemblyDocument::fromJson(std::string_view json, const LoadOptions& options) {
    JsonLoader loader(options);
    return loader.load(json);
}

Status AssemblyDocument::saveFile(const std::string& path, const SaveOptions& options) const {
    auto text = toJson(options);
    if (!text) return text.error();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return Status::failure(ErrorCode::IoError, "cannot open '" + path + "' for writing");
    out << text.value();
    out.close();
    if (!out) return Status::failure(ErrorCode::IoError, "cannot write '" + path + "'");
    return std::monostate{};
}

LoadOptions fileLoadOptions(std::shared_ptr<const modeling::FeatureRegistry> registry, std::string baseDirectory) {
    LoadOptions o;
    o.parts = filePartResolver(std::move(registry), baseDirectory);
    o.assemblies = [base = std::move(baseDirectory)](const std::string& source) -> Result<std::string> {
        std::filesystem::path p(source);
        if (p.is_relative() && !base.empty()) p = std::filesystem::path(base) / p;
        std::ifstream in(p, std::ios::binary);
        if (!in) return Error{ErrorCode::IoError, "cannot open '" + p.string() + "'"};
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (in.bad()) return Error{ErrorCode::IoError, "cannot read '" + p.string() + "'"};
        return text;
    };
    return o;
}

Result<LoadedAssembly> AssemblyDocument::loadFile(const std::string& path,
                                                  std::shared_ptr<const modeling::FeatureRegistry> registry) {
    const LoadOptions options =
        fileLoadOptions(std::move(registry), std::filesystem::path(path).parent_path().string());
    auto text = options.assemblies(std::filesystem::path(path).filename().string());
    if (!text) return text.error();
    return fromJson(text.value(), options);
}

}  // namespace bendcore::assembly_v2
