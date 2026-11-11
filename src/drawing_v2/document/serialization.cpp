// Формат файла документа чертежа ("bendcore-drawing", версия 2, миграция с версии 1) и
// документа спецификации ("bendcore-specification", версия 1). docs/drawing/DRAWING_DOCUMENT.md,
// раздел «Формат файла».

#include <bendcore/drawing_v2/drawing_document.hpp>
#include <bendcore/drawing_v2/export.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace bendcore::drawing_v2::document {

using nlohmann::json;
using namespace views;

namespace {

struct ParseFailure {
    std::string message;
};

[[noreturn]] void fail(const std::string& m) { throw ParseFailure{m}; }

template <typename E>
std::string enumName(E v, const std::vector<std::pair<E, const char*>>& table) {
    for (const auto& [e, n] : table)
        if (e == v) return n;
    return table.front().second;
}
template <typename E>
E enumValue(const json& j, const std::vector<std::pair<E, const char*>>& table, const char* what) {
    if (!j.is_string()) fail(std::string("'") + what + "' must be a string");
    const std::string s = j.get<std::string>();
    for (const auto& [e, n] : table)
        if (s == n) return e;
    fail(std::string("unknown ") + what + " '" + s + "'");
}

const std::vector<std::pair<ModelKind, const char*>> kModelKinds = {{ModelKind::Part, "part"}, {ModelKind::Assembly, "assembly"}};
const std::vector<std::pair<GostFormat, const char*>> kFormats = {
    {GostFormat::A0, "A0"}, {GostFormat::A1, "A1"}, {GostFormat::A2, "A2"}, {GostFormat::A3, "A3"}, {GostFormat::A4, "A4"}};
const std::vector<std::pair<Orientation, const char*>> kOrient = {{Orientation::Portrait, "portrait"},
                                                                  {Orientation::Landscape, "landscape"}};
const std::vector<std::pair<ViewKind, const char*>> kViewKinds = {{ViewKind::Base, "base"},
                                                                  {ViewKind::Projected, "projected"},
                                                                  {ViewKind::Axonometric, "axonometric"},
                                                                  {ViewKind::Section, "section"},
                                                                  {ViewKind::Detail, "detail"}};
const std::vector<std::pair<SheetSide, const char*>> kSides = {
    {SheetSide::Above, "above"}, {SheetSide::Below, "below"}, {SheetSide::LeftOf, "leftOf"}, {SheetSide::RightOf, "rightOf"}};
const std::vector<std::pair<Axonometry, const char*>> kAxo = {{Axonometry::Isometric, "isometric"},
                                                              {Axonometry::Dimetric, "dimetric"}};
const std::vector<std::pair<SectionKind, const char*>> kSecKinds = {{SectionKind::Cut, "cut"}, {SectionKind::Slice, "slice"}};
const std::vector<std::pair<HlrMethod, const char*>> kHlr = {{HlrMethod::Exact, "exact"}, {HlrMethod::Polygonal, "polygonal"}};
const std::vector<std::pair<ProjectionMethod, const char*>> kProj = {{ProjectionMethod::FirstAngle, "firstAngle"},
                                                                     {ProjectionMethod::ThirdAngle, "thirdAngle"}};
const std::vector<std::pair<annotations::DimensionType, const char*>> kDimTypes = {
    {annotations::DimensionType::Horizontal, "horizontal"}, {annotations::DimensionType::Vertical, "vertical"},
    {annotations::DimensionType::Aligned, "aligned"},       {annotations::DimensionType::Radius, "radius"},
    {annotations::DimensionType::Diameter, "diameter"},     {annotations::DimensionType::Angle, "angle"}};
const std::vector<std::pair<annotations::AnchorKind, const char*>> kAnchorKinds = {
    {annotations::AnchorKind::Start, "start"}, {annotations::AnchorKind::End, "end"},
    {annotations::AnchorKind::Center, "center"}, {annotations::AnchorKind::Point, "point"}};
const std::vector<std::pair<annotations::ToleranceDisplay, const char*>> kTolDisp = {
    {annotations::ToleranceDisplay::ClassOnly, "classOnly"},
    {annotations::ToleranceDisplay::Deviations, "deviations"},
    {annotations::ToleranceDisplay::ClassAndDeviations, "classAndDeviations"}};
const std::vector<std::pair<annotations::RoughnessMethod, const char*>> kRoughMethods = {
    {annotations::RoughnessMethod::Unspecified, "unspecified"},
    {annotations::RoughnessMethod::MaterialRemoval, "materialRemoval"},
    {annotations::RoughnessMethod::NoMaterialRemoval, "noMaterialRemoval"}};
const std::vector<std::pair<modeling::ElementKind, const char*>> kElemKinds = {
    {modeling::ElementKind::Face, "face"}, {modeling::ElementKind::Edge, "edge"}, {modeling::ElementKind::Vertex, "vertex"}};
const std::vector<std::pair<annotations::SpecSection, const char*>> kSpecSections = {
    {annotations::SpecSection::Documentation, "documentation"}, {annotations::SpecSection::Complexes, "complexes"},
    {annotations::SpecSection::AssemblyUnits, "assemblyUnits"}, {annotations::SpecSection::Parts, "parts"},
    {annotations::SpecSection::StandardProducts, "standardProducts"}, {annotations::SpecSection::OtherProducts, "otherProducts"},
    {annotations::SpecSection::Materials, "materials"},         {annotations::SpecSection::Kits, "kits"}};

const json& field(const json& j, const char* name) {
    if (!j.is_object() || !j.contains(name)) fail(std::string("missing field '") + name + "'");
    return j.at(name);
}
std::string str(const json& j, const char* name, const std::string& def = {}) {
    if (!j.contains(name) || j.at(name).is_null()) return def;
    if (!j.at(name).is_string()) fail(std::string("'") + name + "' must be a string");
    return j.at(name).get<std::string>();
}
double num(const json& j, const char* name, double def) {
    if (!j.contains(name) || j.at(name).is_null()) return def;
    if (!j.at(name).is_number()) fail(std::string("'") + name + "' must be a number");
    return j.at(name).get<double>();
}
bool flag(const json& j, const char* name, bool def) {
    if (!j.contains(name) || j.at(name).is_null()) return def;
    if (!j.at(name).is_boolean()) fail(std::string("'") + name + "' must be a boolean");
    return j.at(name).get<bool>();
}

json vec(const Vec3& v) { return json::array({v.x, v.y, v.z}); }
Vec3 vecFrom(const json& j) {
    if (!j.is_array() || j.size() != 3) fail("a vector must be an array of 3 numbers");
    return {j[0].get<double>(), j[1].get<double>(), j[2].get<double>()};
}
json pt(const Point2& p) { return json::array({p.x, p.y}); }
Point2 ptFrom(const json& j) {
    if (!j.is_array() || j.size() != 2) fail("a point must be an array of 2 numbers");
    return {j[0].get<double>(), j[1].get<double>()};
}
json point3(const kernel::Point3& p) { return json::array({p[0], p[1], p[2]}); }
kernel::Point3 point3From(const json& j) {
    if (!j.is_array() || j.size() != 3) fail("a point must be an array of 3 numbers");
    return {j[0].get<double>(), j[1].get<double>(), j[2].get<double>()};
}

json scaleJson(const std::optional<ScaleRatio>& s) {
    if (!s) return nullptr;
    return json::array({s->num, s->den});
}
std::optional<ScaleRatio> scaleFrom(const json& j) {
    if (j.is_null()) return std::nullopt;
    if (!j.is_array() || j.size() != 2) fail("a scale must be [num, den]");
    ScaleRatio s{j[0].get<int>(), j[1].get<int>()};
    for (const ScaleRatio& g : gostScales())
        if (g.num * s.den == s.num * g.den) return g;
    fail("scale " + std::to_string(s.num) + ":" + std::to_string(s.den) + " is not in the GOST 2.302 series");
}

json refJson(const modeling::TopoReference& r) {
    json j{{"name", r.name}, {"kind", enumName(r.kind, kElemKinds)}};
    if (r.signature.valid) {
        const auto& s = r.signature;
        j["signature"] = {{"geometry", s.geometry},           {"center", point3(s.center)}, {"hasDirection", s.hasDirection},
                          {"direction", point3(s.direction)}, {"radius", s.radius},         {"size", s.size}};
    } else {
        j["signature"] = nullptr;
    }
    return j;
}
modeling::TopoReference refFrom(const json& j) {
    modeling::TopoReference r;
    r.name = str(j, "name");
    r.kind = enumValue(field(j, "kind"), kElemKinds, "element kind");
    if (j.contains("signature") && !j.at("signature").is_null()) {
        const json& s = j.at("signature");
        r.signature.valid = true;
        r.signature.kind = r.kind;
        r.signature.geometry = str(s, "geometry");
        r.signature.center = point3From(field(s, "center"));
        r.signature.hasDirection = flag(s, "hasDirection", false);
        r.signature.direction = point3From(field(s, "direction"));
        r.signature.radius = num(s, "radius", 0);
        r.signature.size = num(s, "size", 0);
    }
    if (r.name.empty() && !r.signature.valid) fail("a reference needs a name or a signature");
    return r;
}

annotations::RefKind refKind(modeling::ElementKind k) {
    switch (k) {
        case modeling::ElementKind::Face: return annotations::RefKind::Face;
        case modeling::ElementKind::Edge: return annotations::RefKind::Edge;
        case modeling::ElementKind::Vertex: return annotations::RefKind::Vertex;
    }
    return annotations::RefKind::Edge;
}

json anchorsJson(const std::vector<annotations::Anchor>& as) {
    json a = json::array();
    for (const auto& x : as) a.push_back({{"anchor", enumName(x.kind, kAnchorKinds)}, {"ref", refJson(x.ref.persistent)}});
    return a;
}
std::vector<annotations::Anchor> anchorsFrom(const json& j, int version) {
    std::vector<annotations::Anchor> out;
    if (!j.is_array()) fail("anchors must be an array");
    for (const json& x : j) {
        annotations::Anchor a;
        a.kind = enumValue(field(x, "anchor"), kAnchorKinds, "anchor kind");
        if (version == 1) {
            // v1: ссылка — только имя элемента и вид («edge»/«face»/«vertex»).
            a.ref.persistent.name = str(x, "ref");
            a.ref.persistent.kind = enumValue(field(x, "refKind"), kElemKinds, "element kind");
            if (a.ref.persistent.name.empty()) fail("v1 reference without a name");
        } else {
            a.ref.persistent = refFrom(field(x, "ref"));
        }
        a.ref.kind = refKind(a.ref.persistent.kind);
        out.push_back(std::move(a));
    }
    return out;
}

json signatureRow(const SignatureRow& r) { return {{"surname", r.surname}, {"date", r.date}}; }
SignatureRow signatureRowFrom(const json& j, const char* name) {
    SignatureRow r;
    if (!j.contains(name)) return r;
    r.surname = str(j.at(name), "surname");
    r.date = str(j.at(name), "date");
    return r;
}
json titleJson(const TitleFields& t) {
    return {{"designation", t.designation}, {"name", t.name},           {"material", t.material},
            {"mass", t.mass},               {"litera", t.litera},       {"organization", t.organization},
            {"developer", signatureRow(t.developer)}, {"checker", signatureRow(t.checker)},
            {"techControl", signatureRow(t.techControl)}, {"normControl", signatureRow(t.normControl)},
            {"approver", signatureRow(t.approver)}};
}
TitleFields titleFrom(const json& j) {
    TitleFields t;
    if (j.is_null()) return t;
    t.designation = str(j, "designation");
    t.name = str(j, "name");
    t.material = str(j, "material");
    t.mass = str(j, "mass");
    t.litera = str(j, "litera");
    t.organization = str(j, "organization");
    t.developer = signatureRowFrom(j, "developer");
    t.checker = signatureRowFrom(j, "checker");
    t.techControl = signatureRowFrom(j, "techControl");
    t.normControl = signatureRowFrom(j, "normControl");
    t.approver = signatureRowFrom(j, "approver");
    return t;
}

const std::vector<std::pair<annotations::GeometricCharacteristic, const char*>> kGeomChars = {
    {annotations::GeometricCharacteristic::Straightness, "straightness"},
    {annotations::GeometricCharacteristic::Flatness, "flatness"},
    {annotations::GeometricCharacteristic::Roundness, "roundness"},
    {annotations::GeometricCharacteristic::Cylindricity, "cylindricity"},
    {annotations::GeometricCharacteristic::LineProfile, "lineProfile"},
    {annotations::GeometricCharacteristic::SurfaceProfile, "surfaceProfile"},
    {annotations::GeometricCharacteristic::Parallelism, "parallelism"},
    {annotations::GeometricCharacteristic::Perpendicularity, "perpendicularity"},
    {annotations::GeometricCharacteristic::Angularity, "angularity"},
    {annotations::GeometricCharacteristic::Position, "position"},
    {annotations::GeometricCharacteristic::Coaxiality, "coaxiality"},
    {annotations::GeometricCharacteristic::Symmetry, "symmetry"},
    {annotations::GeometricCharacteristic::CircularRunout, "circularRunout"},
    {annotations::GeometricCharacteristic::TotalRunout, "totalRunout"}};

json roughJson(const annotations::RoughnessSpec& r) {
    return {{"method", enumName(r.method, kRoughMethods)}, {"parameter", r.parameter}, {"value", r.value},
            {"process", r.process}, {"heightFactor", r.heightFactor}};
}
annotations::RoughnessSpec roughFrom(const json& j) {
    annotations::RoughnessSpec r;
    r.method = enumValue(field(j, "method"), kRoughMethods, "roughness method");
    r.parameter = str(j, "parameter", "Ra");
    r.value = str(j, "value");
    r.process = str(j, "process");
    r.heightFactor = num(j, "heightFactor", 2.5);
    return r;
}

json linkJson(const ModelLink& m) {
    json p{{"designation", m.properties.designation}, {"name", m.properties.name}, {"material", m.properties.material},
           {"density", m.properties.density ? json(*m.properties.density) : json(nullptr)}};
    return {{"kind", enumName(m.kind, kModelKinds)}, {"path", m.path}, {"resultHash", m.resultHash},
            {"revision", m.revision}, {"properties", p}};
}
ModelLink linkFrom(const json& j) {
    ModelLink m;
    m.kind = enumValue(field(j, "kind"), kModelKinds, "model kind");
    m.path = str(j, "path");
    m.resultHash = str(j, "resultHash");
    m.revision = std::uint32_t(num(j, "revision", 0));
    if (j.contains("properties") && !j.at("properties").is_null()) {
        const json& p = j.at("properties");
        m.properties.designation = str(p, "designation");
        m.properties.name = str(p, "name");
        m.properties.material = str(p, "material");
        if (p.contains("density") && !p.at("density").is_null()) m.properties.density = p.at("density").get<double>();
    }
    return m;
}

// v1: формат листа строкой «A3L» / «A4P» / «A4x3P» (основной формат, кратность, ориентация).
SheetFormat formatFromV1(const std::string& s) {
    SheetFormat f;
    if (s.size() < 3 || s[0] != 'A') fail("v1 sheet format '" + s + "' must look like A3L or A4x3P");
    const char o = s.back();
    if (o != 'L' && o != 'P') fail("v1 sheet format '" + s + "': orientation must be L or P");
    f.orientation = o == 'L' ? Orientation::Landscape : Orientation::Portrait;
    const std::string body = s.substr(0, s.size() - 1);
    const auto x = body.find('x');
    f.base = enumValue(json(body.substr(0, x)), kFormats, "sheet format");
    f.multiplier = x == std::string::npos ? 1 : std::stoi(body.substr(x + 1));
    return f;
}

json specItemJson(const annotations::SpecItem& it) {
    return {{"format", it.format}, {"designation", it.designation}, {"name", it.name}, {"note", it.note}};
}

}  // namespace

// ------------------------------------------------------------------ чертёж

Result<std::string> DrawingDocument::toJson() const {
    json j;
    j["format"] = "bendcore-drawing";
    j["version"] = kFormatVersion;
    j["model"] = linkJson(model_);
    j["settings"] = {{"lineThickness", settings_.lineThickness},
                     {"projection", enumName(settings_.projection, kProj)},
                     {"viewGap", settings_.viewGap},
                     {"margin", settings_.margin},
                     {"maxScale", scaleJson(settings_.maxScale)},
                     {"reducedAxonometry", settings_.reducedAxonometry}};
    json sheets = json::array();
    for (const SheetDef& s : sheets_) {
        json js;
        js["id"] = s.id;
        js["format"] = {{"base", enumName(s.format.base, kFormats)},
                        {"multiplier", s.format.multiplier},
                        {"orientation", enumName(s.format.orientation, kOrient)}};
        js["scale"] = scaleJson(s.scale);
        js["title"] = titleJson(s.title);
        json vs = json::array();
        for (const ViewDef& v : s.views) {
            json jv{{"id", v.id},
                    {"kind", enumName(v.kind, kViewKinds)},
                    {"toward", vec(v.toward)},
                    {"right", vec(v.right)},
                    {"parent", v.parent},
                    {"side", v.side ? json(enumName(*v.side, kSides)) : json(nullptr)},
                    {"axonometry", enumName(v.axonometry, kAxo)},
                    {"planePoint", vec(v.planePoint)},
                    {"sectionKind", enumName(v.sectionKind, kSecKinds)},
                    {"markView", v.markView},
                    {"detailCenter", pt(v.detailCenter)},
                    {"detailRadius", v.detailRadius},
                    {"letter", v.letter},
                    {"scale", scaleJson(v.scale)},
                    {"position", v.position ? pt(*v.position) : json(nullptr)},
                    {"hidden", v.hidden},
                    {"centerLines", v.centerLines},
                    {"autoBalloons", v.autoBalloons},
                    {"method", enumName(v.method, kHlr)},
                    {"autoMethod", v.autoMethod}};
            vs.push_back(std::move(jv));
        }
        js["views"] = std::move(vs);
        json ds = json::array();
        for (const DimensionItem& d : s.dimensions) {
            const auto& def = d.def;
            ds.push_back({{"id", d.id},
                          {"view", d.view},
                          {"type", enumName(def.type, kDimTypes)},
                          {"anchors", anchorsJson(def.anchors)},
                          {"offsetFrom", anchorsJson(def.offsetFrom)},
                          {"offset", def.offset},
                          {"leaderAngleDeg", def.leaderAngleDeg},
                          {"textShift", def.textShift},
                          {"textOverride", def.textOverride ? json(*def.textOverride) : json(nullptr)},
                          {"tolerance", def.tolerance ? json(def.tolerance->label()) : json(nullptr)},
                          {"toleranceDisplay", enumName(def.toleranceDisplay, kTolDisp)},
                          {"prefix", def.prefix},
                          {"suffix", def.suffix},
                          {"decimals", def.decimals},
                          {"lastValue", d.lastValue ? json(*d.lastValue) : json(nullptr)}});
        }
        js["dimensions"] = std::move(ds);
        json rs = json::array();
        for (const RoughnessItem& r : s.roughness)
            rs.push_back({{"id", r.id}, {"view", r.view}, {"edge", refJson(r.edge)}, {"at", r.at}, {"spec", roughJson(r.spec)}});
        js["roughness"] = std::move(rs);
        json ts = json::array();
        for (const ToleranceItem& t : s.tolerances)
            ts.push_back({{"id", t.id}, {"view", t.view}, {"edge", refJson(t.edge)}, {"at", t.at},
                          {"characteristic", enumName(t.frame.characteristic, kGeomChars)},
                          {"tolerance", t.frame.tolerance}, {"diameterZone", t.frame.diameterZone},
                          {"datums", t.frame.datums}});
        js["tolerances"] = std::move(ts);
        json dms = json::array();
        for (const DatumItem& dm : s.datums)
            dms.push_back({{"id", dm.id}, {"view", dm.view}, {"edge", refJson(dm.edge)}, {"at", dm.at}, {"letter", dm.letter}});
        js["datums"] = std::move(dms);
        js["roughnessRemainder"] = s.roughnessRemainder ? roughJson(*s.roughnessRemainder) : json(nullptr);
        js["technicalRequirements"] = s.technicalRequirements;
        js["partsList"] = s.partsList;
        sheets.push_back(std::move(js));
    }
    j["sheets"] = std::move(sheets);
    return j.dump(1);
}

Result<DrawingDocument> DrawingDocument::fromJson(std::string_view text, int* loadedVersion) {
    json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded()) return Error{ErrorCode::ParseError, "drawing: not a JSON document"};
    try {
        if (str(j, "format") != "bendcore-drawing") return Error{ErrorCode::ParseError, "drawing: not a bendcore-drawing file"};
        const json& ver = field(j, "version");
        if (!ver.is_number_integer()) fail("'version' must be an integer");
        const int version = ver.get<int>();
        if (version > kFormatVersion)
            return Error{ErrorCode::Unsupported, "drawing: file version " + std::to_string(version) + " is newer than " +
                                                     std::to_string(kFormatVersion)};
        if (version < 1) fail("unknown version " + std::to_string(version));
        if (loadedVersion) *loadedVersion = version;
        DrawingDocument d;
        if (version == 1) {
            // Миграция v1 → v2: модель — строка пути и "modelKind".
            d.model_.path = str(j, "model");
            d.model_.kind = enumValue(field(j, "modelKind"), kModelKinds, "model kind");
            d.model_.resultHash = str(j, "modelHash");
        } else {
            d.model_ = linkFrom(field(j, "model"));
        }
        if (j.contains("settings")) {
            const json& s = j.at("settings");
            d.settings_.lineThickness = num(s, "lineThickness", 0.5);
            if (s.contains("projection")) d.settings_.projection = enumValue(s.at("projection"), kProj, "projection");
            d.settings_.viewGap = num(s, "viewGap", 30);
            d.settings_.margin = num(s, "margin", 15);
            if (s.contains("maxScale") && !s.at("maxScale").is_null()) d.settings_.maxScale = *scaleFrom(s.at("maxScale"));
            d.settings_.reducedAxonometry = flag(s, "reducedAxonometry", true);
        }
        for (const json& js : field(j, "sheets")) {
            SheetDef s;
            s.id = str(js, "id");
            if (version == 1) {
                s.format = formatFromV1(str(js, "format"));
            } else {
                const json& f = field(js, "format");
                s.format.base = enumValue(field(f, "base"), kFormats, "sheet format");
                s.format.multiplier = int(num(f, "multiplier", 1));
                s.format.orientation = enumValue(field(f, "orientation"), kOrient, "orientation");
            }
            if (js.contains("scale")) s.scale = scaleFrom(js.at("scale"));
            if (js.contains("title")) s.title = titleFrom(js.at("title"));
            for (const json& jv : field(js, "views")) {
                ViewDef v;
                v.id = str(jv, "id");
                v.kind = enumValue(field(jv, "kind"), kViewKinds, "view kind");
                if (jv.contains("toward")) v.toward = vecFrom(jv.at("toward"));
                if (jv.contains("right")) v.right = vecFrom(jv.at("right"));
                v.parent = str(jv, "parent");
                if (jv.contains("side") && !jv.at("side").is_null()) v.side = enumValue(jv.at("side"), kSides, "side");
                if (jv.contains("axonometry")) v.axonometry = enumValue(jv.at("axonometry"), kAxo, "axonometry");
                if (jv.contains("planePoint")) v.planePoint = vecFrom(jv.at("planePoint"));
                if (jv.contains("sectionKind")) v.sectionKind = enumValue(jv.at("sectionKind"), kSecKinds, "section kind");
                v.markView = str(jv, "markView");
                if (jv.contains("detailCenter")) v.detailCenter = ptFrom(jv.at("detailCenter"));
                v.detailRadius = num(jv, "detailRadius", 0);
                v.letter = str(jv, "letter");
                if (jv.contains("scale")) {
                    // v1: масштаб вида — число (масштабный коэффициент).
                    if (version == 1 && jv.at("scale").is_number()) {
                        const double k = jv.at("scale").get<double>();
                        bool found = false;
                        for (const ScaleRatio& g : gostScales())
                            if (std::abs(g.value() - k) < 1e-9) v.scale = g, found = true;
                        if (!found) fail("v1 view scale is not in the GOST 2.302 series");
                    } else {
                        v.scale = scaleFrom(jv.at("scale"));
                    }
                }
                if (jv.contains("position") && !jv.at("position").is_null()) v.position = ptFrom(jv.at("position"));
                v.hidden = flag(jv, "hidden", true);
                v.centerLines = flag(jv, "centerLines", true);
                v.autoBalloons = flag(jv, "autoBalloons", false);
                if (jv.contains("method")) v.method = enumValue(jv.at("method"), kHlr, "HLR method");
                v.autoMethod = flag(jv, "autoMethod", false);
                s.views.push_back(std::move(v));
            }
            if (js.contains("dimensions")) {
                for (const json& jd : js.at("dimensions")) {
                    DimensionItem di;
                    di.id = str(jd, "id");
                    di.view = str(jd, "view");
                    auto& def = di.def;
                    def.type = enumValue(field(jd, "type"), kDimTypes, "dimension type");
                    def.anchors = anchorsFrom(field(jd, "anchors"), version);
                    if (jd.contains("offsetFrom")) def.offsetFrom = anchorsFrom(jd.at("offsetFrom"), version);
                    def.offset = num(jd, "offset", 10);
                    def.leaderAngleDeg = num(jd, "leaderAngleDeg", 45);
                    def.textShift = num(jd, "textShift", 0);
                    if (jd.contains("textOverride") && !jd.at("textOverride").is_null())
                        def.textOverride = jd.at("textOverride").get<std::string>();
                    if (jd.contains("tolerance") && !jd.at("tolerance").is_null()) {
                        auto t = annotations::parseToleranceClass(jd.at("tolerance").get<std::string>());
                        if (!t) fail(t.error().message);
                        def.tolerance = t.value();
                    }
                    if (jd.contains("toleranceDisplay"))
                        def.toleranceDisplay = enumValue(jd.at("toleranceDisplay"), kTolDisp, "tolerance display");
                    def.prefix = str(jd, "prefix");
                    def.suffix = str(jd, "suffix");
                    def.decimals = int(num(jd, "decimals", 2));
                    if (jd.contains("lastValue") && !jd.at("lastValue").is_null()) di.lastValue = jd.at("lastValue").get<double>();
                    s.dimensions.push_back(std::move(di));
                }
            }
            if (js.contains("roughness")) {
                for (const json& jr : js.at("roughness")) {
                    RoughnessItem r;
                    r.id = str(jr, "id");
                    r.view = str(jr, "view");
                    if (version == 1) {
                        r.edge.name = str(jr, "edge");
                        r.edge.kind = modeling::ElementKind::Edge;
                    } else {
                        r.edge = refFrom(field(jr, "edge"));
                    }
                    r.at = num(jr, "at", 0.5);
                    r.spec = roughFrom(field(jr, "spec"));
                    s.roughness.push_back(std::move(r));
                }
            }
            // Допуски формы и базы (необязательны: файлы версии 2 без них читаются).
            if (js.contains("tolerances")) {
                for (const json& jt : js.at("tolerances")) {
                    ToleranceItem t;
                    t.id = str(jt, "id");
                    t.view = str(jt, "view");
                    t.edge = refFrom(field(jt, "edge"));
                    t.at = num(jt, "at", 0.5);
                    t.frame.characteristic = enumValue(field(jt, "characteristic"), kGeomChars, "geometric characteristic");
                    t.frame.tolerance = str(jt, "tolerance");
                    t.frame.diameterZone = flag(jt, "diameterZone", false);
                    if (jt.contains("datums")) t.frame.datums = jt.at("datums").get<std::vector<std::string>>();
                    s.tolerances.push_back(std::move(t));
                }
            }
            if (js.contains("datums")) {
                for (const json& jdm : js.at("datums")) {
                    DatumItem dm;
                    dm.id = str(jdm, "id");
                    dm.view = str(jdm, "view");
                    dm.edge = refFrom(field(jdm, "edge"));
                    dm.at = num(jdm, "at", 0.5);
                    dm.letter = str(jdm, "letter");
                    s.datums.push_back(std::move(dm));
                }
            }
            if (js.contains("roughnessRemainder") && !js.at("roughnessRemainder").is_null())
                s.roughnessRemainder = roughFrom(js.at("roughnessRemainder"));
            if (js.contains("technicalRequirements"))
                s.technicalRequirements = js.at("technicalRequirements").get<std::vector<std::string>>();
            s.partsList = flag(js, "partsList", false);
            d.sheets_.push_back(std::move(s));
        }
        if (auto st = d.validate(); !st) return Error{ErrorCode::ParseError, st.error().message};
        return d;
    } catch (const ParseFailure& e) {
        return Error{ErrorCode::ParseError, "drawing: " + e.message};
    } catch (const json::exception& e) {
        return Error{ErrorCode::ParseError, std::string("drawing: ") + e.what()};
    } catch (const std::exception& e) {
        return Error{ErrorCode::ParseError, std::string("drawing: ") + e.what()};
    }
}

Status DrawingDocument::saveFile(const std::string& path) const {
    auto text = toJson();
    if (!text) return text.error();
    auto n = saveToFile(path, text.value());
    if (!n) return n.error();
    return okStatus();
}

Result<DrawingDocument> DrawingDocument::loadFile(const std::string& path, int* loadedVersion) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return Error{ErrorCode::IoError, "drawing: cannot open '" + path + "'"};
    std::ostringstream s;
    s << in.rdbuf();
    return fromJson(s.str(), loadedVersion);
}

// ------------------------------------------------------------------ спецификация

Result<std::string> SpecificationDocument::toJson() const {
    json j;
    j["format"] = "bendcore-specification";
    j["version"] = kFormatVersion;
    j["model"] = linkJson(model_);
    j["title"] = titleJson(title_);
    json sections = json::object();
    for (const auto& [k, v] : settings_.sections) sections[k] = enumName(v, kSpecSections);
    json docs = json::array();
    for (const auto& d : settings_.documents) docs.push_back(specItemJson(d));
    j["settings"] = {{"formats", settings_.formats},
                     {"notes", settings_.notes},
                     {"sections", sections},
                     {"documents", docs},
                     {"addAssemblyDrawing", settings_.addAssemblyDrawing},
                     {"assemblyDrawingFormat", settings_.assemblyDrawingFormat},
                     {"reservePositions", settings_.reservePositions}};
    return j.dump(1);
}

Result<SpecificationDocument> SpecificationDocument::fromJson(std::string_view text) {
    json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded()) return Error{ErrorCode::ParseError, "specification: not a JSON document"};
    try {
        if (str(j, "format") != "bendcore-specification")
            return Error{ErrorCode::ParseError, "specification: not a bendcore-specification file"};
        const int version = field(j, "version").get<int>();
        if (version > kFormatVersion)
            return Error{ErrorCode::Unsupported, "specification: file version " + std::to_string(version) + " is newer"};
        SpecificationDocument d;
        d.model_ = linkFrom(field(j, "model"));
        if (d.model_.kind != ModelKind::Assembly) fail("a specification references an assembly");
        if (j.contains("title")) d.title_ = titleFrom(j.at("title"));
        const json& s = field(j, "settings");
        d.settings_.formats = s.value("formats", std::map<std::string, std::string>{});
        d.settings_.notes = s.value("notes", std::map<std::string, std::string>{});
        if (s.contains("sections"))
            for (const auto& [k, v] : s.at("sections").items()) d.settings_.sections[k] = enumValue(v, kSpecSections, "section");
        if (s.contains("documents")) {
            for (const json& x : s.at("documents")) {
                annotations::SpecItem it;
                it.format = str(x, "format");
                it.designation = str(x, "designation");
                it.name = str(x, "name");
                it.note = str(x, "note");
                d.settings_.documents.push_back(std::move(it));
            }
        }
        d.settings_.addAssemblyDrawing = flag(s, "addAssemblyDrawing", true);
        d.settings_.assemblyDrawingFormat = str(s, "assemblyDrawingFormat", "А3");
        d.settings_.reservePositions = int(num(s, "reservePositions", 0));
        return d;
    } catch (const ParseFailure& e) {
        return Error{ErrorCode::ParseError, "specification: " + e.message};
    } catch (const std::exception& e) {
        return Error{ErrorCode::ParseError, std::string("specification: ") + e.what()};
    }
}

}  // namespace bendcore::drawing_v2::document
