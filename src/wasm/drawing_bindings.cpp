// WebAssembly module of the drawing document (bendcore_drawing_wasm, docs/ui/DRAWING_UI.md).
//
// A drawing session holds the model document (a part document rebuilt from its JSON, or an
// assembly document with embedded geometry), the drawing document
// (bendcore::drawing_v2::document::DrawingDocument), the specification document of an assembly
// and a view cache. Sessions live in a registry and are addressed from JS by an integer handle.
//
// Arguments and results cross the boundary as JSON text: every entry point takes JSON strings
// and returns {"ok":true,"value":...} or {"ok":false,"code":...,"message":...} with the kernel
// ErrorCode; binary exports return a JS object {ok, value: Uint8Array}. The ES wrapper
// (bendcore_drawing_api.mjs) parses the text and throws KernelError on failure.
//
// Every edit of the drawing is a transaction: the sheet definitions are copied, the edit is
// applied and the document is updated against the model snapshot the drawing currently shows;
// if the update fails, the copy is restored and the error is returned. Changing the model (a
// part variable) does not touch the drawing: updateFromModel() takes a fresh snapshot and
// returns the update report (changed and dangling dimensions).
//
// Text needs a TrueType font (metrics for dimensions and title block, embedding into PDF). The
// module has no file system: JS passes the bytes of the font file with setFont() before the
// first session is created (docs/ui/DRAWING_UI.md, section "Font").

#include <bendcore/assembly_v2/assembly_document.hpp>
#include <bendcore/drawing_v2/drawing_document.hpp>
#include <bendcore/drawing_v2/export.hpp>
#include <bendcore/modeling/part_document.hpp>
#include <bendcore/modeling/part_features.hpp>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <Standard_Failure.hxx>
#include <Standard_Version.hxx>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using emscripten::val;
using nlohmann::json;
namespace k = bendcore::kernel;
namespace m = bendcore::modeling;
namespace dv = bendcore::drawing_v2;
namespace dd = bendcore::drawing_v2::document;
namespace vw = bendcore::drawing_v2::views;
namespace an = bendcore::drawing_v2::annotations;
namespace asmv = bendcore::assembly_v2;

namespace {

// Malformed argument from JS (wrong type, missing field, unknown value) -> InvalidInput.
struct InputError : std::runtime_error {
    using std::runtime_error::runtime_error;
};
// Failure of a kernel call that carries its own code.
struct KernelFailure {
    k::Error error;
};

[[noreturn]] void raise(const k::Error& e) { throw KernelFailure{e}; }

template <typename T>
T take(k::Result<T> r) {
    if (!r) raise(r.error());
    return std::move(r).value();
}

std::string okText(const json& value) { return json{{"ok", true}, {"value", value}}.dump(); }
std::string failText(const std::string& code, const std::string& message) {
    return json{{"ok", false}, {"code", code}, {"message", message}}.dump();
}

template <typename Body>
std::string guarded(Body&& body) {
    try {
        return okText(body());
    } catch (const InputError& e) {
        return failText("InvalidInput", e.what());
    } catch (const KernelFailure& f) {
        return failText(k::toString(f.error.code), f.error.message);
    } catch (const json::exception& e) {
        return failText("InvalidInput", std::string("JSON: ") + e.what());
    } catch (const Standard_Failure& e) {
        return failText("OperationFailed", std::string("OCCT: ") + e.what());
    } catch (const std::exception& e) {
        return failText("OperationFailed", e.what());
    }
}

json parseArg(const std::string& text, const char* what) {
    try {
        return json::parse(text);
    } catch (const json::parse_error& e) {
        throw InputError(std::string(what) + ": not JSON: " + e.what());
    }
}

const std::shared_ptr<const m::FeatureRegistry>& registry() {
    static const std::shared_ptr<const m::FeatureRegistry> r = m::partRegistry();
    return r;
}

std::optional<dv::TrueTypeFont>& font() {
    static std::optional<dv::TrueTypeFont> f;
    return f;
}

const dv::TrueTypeFont& requireFont() {
    if (!font()) raise({k::ErrorCode::InvalidInput, "drawing: no font - call setFont() with the bytes of a TrueType font first"});
    return *font();
}

// ------------------------------------------------------------------ enums

template <typename E>
using Table = std::vector<std::pair<E, const char*>>;

template <typename E>
E enumFrom(const json& j, const Table<E>& table, const char* what) {
    if (!j.is_string()) throw InputError(std::string(what) + " must be a string");
    const std::string s = j.get<std::string>();
    for (const auto& [e, n] : table)
        if (s == n) return e;
    throw InputError(std::string("unknown ") + what + " '" + s + "'");
}
template <typename E>
std::string enumName(E v, const Table<E>& table) {
    for (const auto& [e, n] : table)
        if (e == v) return n;
    return table.front().second;
}

const Table<dv::GostFormat> kFormats = {{dv::GostFormat::A0, "A0"}, {dv::GostFormat::A1, "A1"}, {dv::GostFormat::A2, "A2"},
                                        {dv::GostFormat::A3, "A3"}, {dv::GostFormat::A4, "A4"}};
const Table<dv::Orientation> kOrient = {{dv::Orientation::Portrait, "portrait"}, {dv::Orientation::Landscape, "landscape"}};
const Table<dd::ViewKind> kViewKinds = {{dd::ViewKind::Base, "base"},
                                        {dd::ViewKind::Projected, "projected"},
                                        {dd::ViewKind::Axonometric, "axonometric"},
                                        {dd::ViewKind::Section, "section"},
                                        {dd::ViewKind::Detail, "detail"}};
const Table<vw::SheetSide> kSides = {{vw::SheetSide::Above, "above"},
                                     {vw::SheetSide::Below, "below"},
                                     {vw::SheetSide::LeftOf, "leftOf"},
                                     {vw::SheetSide::RightOf, "rightOf"}};
const Table<vw::StandardView> kStandard = {{vw::StandardView::Front, "front"}, {vw::StandardView::Top, "top"},
                                           {vw::StandardView::Left, "left"},   {vw::StandardView::Right, "right"},
                                           {vw::StandardView::Bottom, "bottom"}, {vw::StandardView::Rear, "rear"}};
const Table<vw::Axonometry> kAxo = {{vw::Axonometry::Isometric, "isometric"}, {vw::Axonometry::Dimetric, "dimetric"}};
const Table<vw::SectionKind> kSecKinds = {{vw::SectionKind::Cut, "cut"}, {vw::SectionKind::Slice, "slice"}};
const Table<an::DimensionType> kDimTypes = {{an::DimensionType::Horizontal, "horizontal"}, {an::DimensionType::Vertical, "vertical"},
                                            {an::DimensionType::Aligned, "aligned"},       {an::DimensionType::Radius, "radius"},
                                            {an::DimensionType::Diameter, "diameter"},     {an::DimensionType::Angle, "angle"}};
const Table<an::ToleranceDisplay> kTolDisp = {{an::ToleranceDisplay::ClassOnly, "classOnly"},
                                              {an::ToleranceDisplay::Deviations, "deviations"},
                                              {an::ToleranceDisplay::ClassAndDeviations, "classAndDeviations"}};
const Table<an::RoughnessMethod> kRoughMethods = {{an::RoughnessMethod::Unspecified, "unspecified"},
                                                  {an::RoughnessMethod::MaterialRemoval, "materialRemoval"},
                                                  {an::RoughnessMethod::NoMaterialRemoval, "noMaterialRemoval"}};
const Table<an::GeometricCharacteristic> kGeomChars = {
    {an::GeometricCharacteristic::Straightness, "straightness"},
    {an::GeometricCharacteristic::Flatness, "flatness"},
    {an::GeometricCharacteristic::Roundness, "roundness"},
    {an::GeometricCharacteristic::Cylindricity, "cylindricity"},
    {an::GeometricCharacteristic::LineProfile, "lineProfile"},
    {an::GeometricCharacteristic::SurfaceProfile, "surfaceProfile"},
    {an::GeometricCharacteristic::Parallelism, "parallelism"},
    {an::GeometricCharacteristic::Perpendicularity, "perpendicularity"},
    {an::GeometricCharacteristic::Angularity, "angularity"},
    {an::GeometricCharacteristic::Position, "position"},
    {an::GeometricCharacteristic::Coaxiality, "coaxiality"},
    {an::GeometricCharacteristic::Symmetry, "symmetry"},
    {an::GeometricCharacteristic::CircularRunout, "circularRunout"},
    {an::GeometricCharacteristic::TotalRunout, "totalRunout"}};
const Table<vw::EdgeClass> kEdgeClasses = {{vw::EdgeClass::Sharp, "sharp"},
                                           {vw::EdgeClass::Smooth, "smooth"},
                                           {vw::EdgeClass::Silhouette, "silhouette"},
                                           {vw::EdgeClass::SectionCut, "sectionCut"}};
const Table<an::SpecSection> kSpecSections = {{an::SpecSection::Documentation, "documentation"},
                                              {an::SpecSection::Complexes, "complexes"},
                                              {an::SpecSection::AssemblyUnits, "assemblyUnits"},
                                              {an::SpecSection::Parts, "parts"},
                                              {an::SpecSection::StandardProducts, "standardProducts"},
                                              {an::SpecSection::OtherProducts, "otherProducts"},
                                              {an::SpecSection::Materials, "materials"},
                                              {an::SpecSection::Kits, "kits"}};

// ------------------------------------------------------------------ JSON helpers

std::string strOr(const json& j, const char* name, const std::string& def = {}) {
    if (!j.is_object() || !j.contains(name) || j.at(name).is_null()) return def;
    if (!j.at(name).is_string()) throw InputError(std::string(name) + " must be a string");
    return j.at(name).get<std::string>();
}
double numOr(const json& j, const char* name, double def) {
    if (!j.is_object() || !j.contains(name) || j.at(name).is_null()) return def;
    if (!j.at(name).is_number()) throw InputError(std::string(name) + " must be a number");
    const double v = j.at(name).get<double>();
    if (!std::isfinite(v)) throw InputError(std::string(name) + " must be finite");
    return v;
}
bool boolOr(const json& j, const char* name, bool def) {
    if (!j.is_object() || !j.contains(name) || j.at(name).is_null()) return def;
    if (!j.at(name).is_boolean()) throw InputError(std::string(name) + " must be a boolean");
    return j.at(name).get<bool>();
}
const json& need(const json& j, const char* name) {
    if (!j.is_object() || !j.contains(name) || j.at(name).is_null()) throw InputError(std::string("missing field '") + name + "'");
    return j.at(name);
}
vw::Vec3 vec3From(const json& j, const char* what) {
    if (!j.is_array() || j.size() != 3) throw InputError(std::string(what) + " must be [x, y, z]");
    return {j[0].get<double>(), j[1].get<double>(), j[2].get<double>()};
}
dv::Point2 point2From(const json& j, const char* what) {
    if (j.is_array() && j.size() == 2) return {j[0].get<double>(), j[1].get<double>()};
    if (j.is_object()) return {numOr(j, "x", 0.0), numOr(j, "y", 0.0)};
    throw InputError(std::string(what) + " must be [x, y] or {x, y}");
}
json vec3Json(const vw::Vec3& v) { return json::array({v.x, v.y, v.z}); }
json pointJson(dv::Point2 p) { return json::array({p.x, p.y}); }

std::optional<vw::ScaleRatio> scaleFrom(const json& j) {
    if (j.is_null()) return std::nullopt;
    if (!j.is_array() || j.size() != 2 || !j[0].is_number_integer() || !j[1].is_number_integer())
        throw InputError("scale must be [num, den] or null");
    const vw::ScaleRatio s{j[0].get<int>(), j[1].get<int>()};
    for (const vw::ScaleRatio& g : vw::gostScales())
        if (g.num == s.num && g.den == s.den) return s;
    throw InputError("scale " + std::to_string(s.num) + ":" + std::to_string(s.den) + " is not in the GOST 2.302 series");
}
json scaleJson(const std::optional<vw::ScaleRatio>& s) {
    if (!s) return nullptr;
    return {{"ratio", json::array({s->num, s->den})}, {"label", s->label()}};
}

dd::SheetFormat formatFrom(const json& j, const dd::SheetFormat& base = {}) {
    dd::SheetFormat f = base;
    if (j.contains("format")) f.base = enumFrom(j.at("format"), kFormats, "sheet format");
    if (j.contains("multiplier")) f.multiplier = j.at("multiplier").get<int>();
    if (j.contains("orientation")) f.orientation = enumFrom(j.at("orientation"), kOrient, "orientation");
    take(dv::gostSheet(f.base, f.multiplier, f.orientation));  // checks the combination
    return f;
}

json titleJson(const dd::TitleFields& t) {
    auto row = [](const dv::SignatureRow& r) { return json{{"surname", r.surname}, {"date", r.date}}; };
    return {{"designation", t.designation}, {"name", t.name},         {"material", t.material},
            {"mass", t.mass},               {"litera", t.litera},     {"organization", t.organization},
            {"developer", row(t.developer)}, {"checker", row(t.checker)}, {"techControl", row(t.techControl)},
            {"normControl", row(t.normControl)}, {"approver", row(t.approver)}};
}
// Only the fields present in j are changed.
void titleApply(dd::TitleFields& t, const json& j) {
    if (!j.is_object()) throw InputError("title fields must be an object");
    for (auto [name, field] : std::initializer_list<std::pair<const char*, std::string*>>{
             {"designation", &t.designation}, {"name", &t.name}, {"material", &t.material},
             {"mass", &t.mass}, {"litera", &t.litera}, {"organization", &t.organization}}) {
        if (j.contains(name)) *field = strOr(j, name);
    }
    for (auto [name, row] : std::initializer_list<std::pair<const char*, dv::SignatureRow*>>{
             {"developer", &t.developer}, {"checker", &t.checker}, {"techControl", &t.techControl},
             {"normControl", &t.normControl}, {"approver", &t.approver}}) {
        if (!j.contains(name)) continue;
        const json& r = j.at(name);
        if (r.contains("surname")) row->surname = strOr(r, "surname");
        if (r.contains("date")) row->date = strOr(r, "date");
    }
}

an::RoughnessSpec roughFrom(const json& j) {
    an::RoughnessSpec r;
    if (j.contains("method")) r.method = enumFrom(j.at("method"), kRoughMethods, "roughness method");
    r.parameter = strOr(j, "parameter", "Ra");
    r.value = strOr(j, "value");
    r.process = strOr(j, "process");
    r.heightFactor = numOr(j, "heightFactor", 2.5);
    return r;
}
json roughJson(const an::RoughnessSpec& r) {
    return {{"method", enumName(r.method, kRoughMethods)}, {"parameter", r.parameter}, {"value", r.value}, {"process", r.process}};
}

json propertiesJson(const dd::ModelProperties& p) {
    return {{"designation", p.designation}, {"name", p.name}, {"material", p.material},
            {"density", p.density ? json(*p.density) : json(nullptr)}};
}
void propertiesApply(dd::ModelProperties& p, const json& j) {
    if (!j.is_object()) throw InputError("model properties must be an object");
    if (j.contains("designation")) p.designation = strOr(j, "designation");
    if (j.contains("name")) p.name = strOr(j, "name");
    if (j.contains("material")) p.material = strOr(j, "material");
    if (j.contains("density")) {
        if (j.at("density").is_null()) {
            p.density.reset();
        } else {
            const double d = numOr(j, "density", 0.0);
            if (d <= 0) throw InputError("density must be > 0 kg/m3");
            p.density = d;
        }
    }
}

// ------------------------------------------------------------------ sessions

struct Session {
    dd::ModelKind kind = dd::ModelKind::Part;
    std::unique_ptr<m::PartDocument> part;
    std::shared_ptr<asmv::AssemblyDocument> assembly;
    dd::DrawingDocument doc;
    std::optional<dd::SpecificationDocument> spec;
    std::unique_ptr<vw::ViewCache> cache = std::make_unique<vw::ViewCache>(64, 0);
    std::optional<dd::ModelSnapshot> shown;  // the model state the drawing shows (last update)
    dd::UpdateReport lastReport;
};

std::map<std::uint32_t, std::unique_ptr<Session>>& sessions() {
    static std::map<std::uint32_t, std::unique_ptr<Session>> s;
    return s;
}
std::uint32_t& nextHandle() {
    static std::uint32_t n = 1;
    return n;
}

Session& session(std::uint32_t handle) {
    auto it = sessions().find(handle);
    if (it == sessions().end())
        throw InputError("unknown drawing handle " + std::to_string(handle) + " (never created or already disposed)");
    return *it->second;
}

dd::ModelSnapshot currentSnapshot(Session& s) {
    if (s.kind == dd::ModelKind::Part) {
        const m::RebuildReport rep = s.part->rebuild();
        if (!rep.failed.empty())
            raise({k::ErrorCode::OperationFailed, "part rebuild failed: feature '" + rep.failed.front() + "'"});
        return take(dd::snapshotPart(*s.part, s.doc.model().properties));
    }
    return take(dd::snapshotAssembly(s.assembly));
}

// Updates the drawing against snapshot; the specification document of an assembly first.
dd::UpdateReport runUpdate(Session& s, const dd::ModelSnapshot& snap) {
    const an::Specification* spec = nullptr;
    if (s.spec) {
        take(s.spec->update(snap));
        spec = s.spec->specification();
    }
    dd::UpdateReport rep = take(s.doc.update(snap, *s.cache, spec));
    s.shown = snap;
    s.lastReport = rep;
    return rep;
}

json updateReportJson(const dd::UpdateReport& r) {
    json dims = json::array();
    for (const dd::DimensionReport& d : r.dimensions) {
        dims.push_back({{"sheet", d.sheet},
                        {"id", d.id},
                        {"state", d.state == an::DimensionState::Ok ? "ok" : "dangling"},
                        {"oldValue", d.oldValue ? json(*d.oldValue) : json(nullptr)},
                        {"newValue", d.newValue},
                        {"changed", d.changed},
                        {"method", m::toString(d.method)},
                        {"reason", d.reason}});
    }
    json balloons = json::array();
    for (const dd::BalloonReport& b : r.balloons) {
        json placed = json::array();
        for (const auto& [pos, def] : b.placed) placed.push_back({{"position", pos}, {"definition", def}});
        balloons.push_back({{"sheet", b.sheet}, {"view", b.view}, {"placed", placed}, {"withoutBalloon", b.withoutBalloon}});
    }
    return {{"modelChanged", r.modelChanged}, {"previousHash", r.previousHash}, {"currentHash", r.currentHash},
            {"revision", r.revision},         {"viewsComputed", r.viewsComputed}, {"viewsFromCache", r.viewsFromCache},
            {"dimensions", dims},             {"stale", r.stale},               {"dangling", r.dangling},
            {"balloons", balloons},           {"warnings", r.warnings}};
}

// Applies edit to the sheet definitions and updates against the shown snapshot; restores the
// definitions if the edit or the update fails. Returns the warnings of the update.
template <typename Edit>
json transaction(Session& s, Edit&& edit) {
    if (!s.shown) raise({k::ErrorCode::InvalidInput, "drawing: the session has no model snapshot"});
    const std::vector<dd::SheetDef> backup = s.doc.sheets();
    json result;
    try {
        result = edit();
        dd::UpdateReport rep = runUpdate(s, *s.shown);
        if (result.is_null()) result = json::object();
        result["warnings"] = rep.warnings;
        result["dangling"] = rep.dangling;
    } catch (...) {
        s.doc.sheets() = backup;
        // The restored definitions were valid: recompute them so that the computed state matches.
        if (auto r = s.doc.update(*s.shown, *s.cache, s.spec ? s.spec->specification() : nullptr); r) s.lastReport = r.value();
        throw;
    }
    return result;
}

dd::SheetDef& sheetOf(Session& s, const std::string& id) {
    dd::SheetDef* sh = s.doc.findSheet(id);
    if (!sh) throw InputError("unknown sheet '" + id + "'");
    return *sh;
}
const dd::ComputedSheet& computedSheetOf(Session& s, const std::string& id) {
    for (const dd::ComputedSheet& cs : s.doc.computedSheets())
        if (cs.id == id) return cs;
    throw InputError("sheet '" + id + "' is not computed");
}
const dd::ComputedView& computedViewOf(const dd::ComputedSheet& cs, const std::string& id) {
    for (const dd::ComputedView& v : cs.views)
        if (v.id == id) return v;
    throw InputError("unknown view '" + id + "' on sheet '" + cs.id + "'");
}

// Identifier not used by any sheet, view or annotation of the document: prefix + number.
std::string freshId(const dd::DrawingDocument& doc, const std::string& prefix) {
    std::set<std::string> used;
    for (const dd::SheetDef& s : doc.sheets()) {
        used.insert(s.id);
        for (const auto& v : s.views) used.insert(v.id);
        for (const auto& d : s.dimensions) used.insert(d.id);
        for (const auto& r : s.roughness) used.insert(r.id);
        for (const auto& t : s.tolerances) used.insert(t.id);
        for (const auto& d : s.datums) used.insert(d.id);
    }
    for (int i = 1;; ++i) {
        std::string id = prefix + std::to_string(i);
        if (!used.count(id)) return id;
    }
}

// ------------------------------------------------------------------ description of the document

json viewDefJson(const dd::ViewDef& v) {
    json j = {{"id", v.id},
              {"kind", enumName(v.kind, kViewKinds)},
              {"parent", v.parent},
              {"side", v.side ? json(enumName(*v.side, kSides)) : json(nullptr)},
              {"toward", vec3Json(v.toward)},
              {"right", vec3Json(v.right)},
              {"scale", scaleJson(v.scale)},
              {"position", v.position ? pointJson(*v.position) : json(nullptr)},
              {"hidden", v.hidden},
              {"centerLines", v.centerLines},
              {"autoBalloons", v.autoBalloons}};
    if (v.kind == dd::ViewKind::Axonometric) j["axonometry"] = enumName(v.axonometry, kAxo);
    if (v.kind == dd::ViewKind::Section) {
        j["planePoint"] = vec3Json(v.planePoint);
        j["sectionKind"] = enumName(v.sectionKind, kSecKinds);
        j["markView"] = v.markView;
    }
    if (v.kind == dd::ViewKind::Detail) {
        j["detailCenter"] = pointJson(v.detailCenter);
        j["detailRadius"] = v.detailRadius;
    }
    return j;
}

bool inGroup(const dd::ViewDef& v) { return v.kind == dd::ViewKind::Base || v.side.has_value(); }

json describe(Session& s) {
    json sheets = json::array();
    const auto& computed = s.doc.computedSheets();
    for (std::size_t si = 0; si < s.doc.sheets().size(); ++si) {
        const dd::SheetDef& sd = s.doc.sheets()[si];
        const dd::ComputedSheet* cs = si < computed.size() ? &computed[si] : nullptr;
        json views = json::array();
        for (std::size_t vi = 0; vi < sd.views.size(); ++vi) {
            json v = viewDefJson(sd.views[vi]);
            v["inGroup"] = inGroup(sd.views[vi]);
            if (cs && vi < cs->views.size()) {
                const dd::ComputedView& cv = cs->views[vi];
                v["computedScale"] = scaleJson(cv.scale);
                v["label"] = cv.label;
                const vw::Box2 b = cv.sheetView.sheetBounds();
                v["bounds"] = {{"minX", b.minX}, {"minY", b.minY}, {"maxX", b.maxX}, {"maxY", b.maxY}};
                v["fromCache"] = cv.fromCache;
            }
            views.push_back(std::move(v));
        }
        json dims = json::array();
        for (std::size_t di = 0; di < sd.dimensions.size(); ++di) {
            const dd::DimensionItem& d = sd.dimensions[di];
            json j = {{"id", d.id},
                      {"view", d.view},
                      {"type", enumName(d.def.type, kDimTypes)},
                      {"offset", d.def.offset},
                      {"tolerance", d.def.tolerance ? json(d.def.tolerance->label()) : json(nullptr)},
                      {"toleranceDisplay", enumName(d.def.toleranceDisplay, kTolDisp)},
                      {"textOverride", d.def.textOverride ? json(*d.def.textOverride) : json(nullptr)},
                      {"decimals", d.def.decimals},
                      {"lastValue", d.lastValue ? json(*d.lastValue) : json(nullptr)}};
            if (cs && di < cs->dimensions.size()) {
                const an::DimensionResult& r = cs->dimensions[di];
                j["state"] = r.state == an::DimensionState::Ok ? "ok" : "dangling";
                j["value"] = r.value;
                j["text"] = r.text;
                j["reason"] = r.reason;
                if (!r.geometry.labels.empty()) j["labelPosition"] = pointJson(r.geometry.labels.front().position);
            }
            dims.push_back(std::move(j));
        }
        auto symbolState = [&](const std::vector<std::optional<an::SymbolGeometry>>* list, std::size_t i) -> json {
            if (!cs || i >= list->size()) return nullptr;
            return (*list)[i] ? "ok" : "dangling";
        };
        json rough = json::array();
        for (std::size_t i = 0; i < sd.roughness.size(); ++i) {
            const auto& r = sd.roughness[i];
            rough.push_back({{"id", r.id}, {"view", r.view}, {"at", r.at}, {"spec", roughJson(r.spec)},
                             {"state", symbolState(cs ? &cs->roughness : nullptr, i)}});
        }
        json tols = json::array();
        for (std::size_t i = 0; i < sd.tolerances.size(); ++i) {
            const auto& t = sd.tolerances[i];
            tols.push_back({{"id", t.id}, {"view", t.view}, {"at", t.at},
                            {"characteristic", enumName(t.frame.characteristic, kGeomChars)},
                            {"tolerance", t.frame.tolerance}, {"diameterZone", t.frame.diameterZone},
                            {"datums", t.frame.datums}, {"state", symbolState(cs ? &cs->tolerances : nullptr, i)}});
        }
        json datums = json::array();
        for (std::size_t i = 0; i < sd.datums.size(); ++i) {
            const auto& d = sd.datums[i];
            datums.push_back({{"id", d.id}, {"view", d.view}, {"at", d.at}, {"letter", d.letter},
                              {"state", symbolState(cs ? &cs->datums : nullptr, i)}});
        }
        json balloons = json::array();
        if (cs)
            for (const an::Balloon& b : cs->balloons)
                balloons.push_back({{"definition", b.definition}, {"text", b.text}, {"at", pointJson(b.at)}, {"target", pointJson(b.target)}});
        json sheet = {{"id", sd.id},
                      {"format", enumName(sd.format.base, kFormats)},
                      {"multiplier", sd.format.multiplier},
                      {"orientation", enumName(sd.format.orientation, kOrient)},
                      {"scale", scaleJson(sd.scale)},
                      {"title", titleJson(sd.title)},
                      {"views", views},
                      {"dimensions", dims},
                      {"roughness", rough},
                      {"tolerances", tols},
                      {"datums", datums},
                      {"roughnessRemainder", sd.roughnessRemainder ? roughJson(*sd.roughnessRemainder) : json(nullptr)},
                      {"technicalRequirements", sd.technicalRequirements},
                      {"partsList", sd.partsList},
                      {"balloons", balloons}};
        if (cs) {
            sheet["name"] = cs->sheet.name;
            sheet["width"] = cs->sheet.widthMm;
            sheet["height"] = cs->sheet.heightMm;
            sheet["computedScale"] = scaleJson(cs->scale);
            const dv::TitleBlockData& td = cs->title;
            sheet["titleShown"] = {{"designation", td.designation}, {"name", td.name}, {"material", td.material},
                                   {"mass", td.mass}, {"scale", td.scale}, {"sheet", td.sheet}, {"sheets", td.sheets}};
        }
        sheets.push_back(std::move(sheet));
    }
    const dd::ModelLink& ml = s.doc.model();
    return {{"model", {{"kind", dd::toString(ml.kind)}, {"path", ml.path}, {"resultHash", ml.resultHash},
                       {"revision", ml.revision}, {"properties", propertiesJson(ml.properties)}}},
            {"projection", s.doc.settings().projection == vw::ProjectionMethod::FirstAngle ? "firstAngle" : "thirdAngle"},
            {"sheets", sheets}};
}

// ------------------------------------------------------------------ creation

dd::SheetDef newSheet(const dd::DrawingDocument& doc, const json& opts) {
    dd::SheetDef sd;
    sd.id = strOr(opts, "id");
    if (sd.id.empty()) sd.id = freshId(doc, "S");
    dd::SheetFormat f;
    f.base = dv::GostFormat::A3;
    f.orientation = dv::Orientation::Landscape;
    sd.format = formatFrom(opts, f);
    if (opts.contains("scale")) sd.scale = scaleFrom(opts.at("scale"));
    if (opts.contains("title")) titleApply(sd.title, opts.at("title"));
    return sd;
}

std::uint32_t registerSession(std::unique_ptr<Session> s) {
    const std::uint32_t h = nextHandle()++;
    sessions()[h] = std::move(s);
    return h;
}

// Common part of create and load: snapshot, specification document of an assembly, first update.
void initialise(Session& s, bool forceSpec) {
    s.doc.setFont(&requireFont());
    if (s.kind == dd::ModelKind::Assembly || forceSpec) {
        if (!s.spec) {
            s.spec.emplace();
            s.spec->model() = s.doc.model();
        }
    }
    const dd::ModelSnapshot snap = currentSnapshot(s);
    runUpdate(s, snap);
}

std::unique_ptr<Session> sessionForModel(const std::string& kind, const std::string& modelJson, const json& opts) {
    auto s = std::make_unique<Session>();
    if (kind == "part") {
        s->kind = dd::ModelKind::Part;
        m::RebuildReport rep;
        s->part = std::make_unique<m::PartDocument>(take(m::PartDocument::fromJson(modelJson, registry(), &rep)));
        if (!rep.failed.empty()) raise({k::ErrorCode::OperationFailed, "part rebuild failed: feature '" + rep.failed.front() + "'"});
    } else if (kind == "assembly") {
        s->kind = dd::ModelKind::Assembly;
        asmv::LoadedAssembly loaded = take(asmv::AssemblyDocument::fromJson(modelJson, asmv::LoadOptions{}));
        s->assembly = std::make_shared<asmv::AssemblyDocument>(std::move(loaded.document));
    } else {
        throw InputError("model kind must be 'part' or 'assembly'");
    }
    (void)opts;
    return s;
}

std::string createDrawing(std::string kind, std::string modelJson, std::string optionsText) {
    return guarded([&]() -> json {
        const json opts = optionsText.empty() ? json::object() : parseArg(optionsText, "options");
        auto s = sessionForModel(kind, modelJson, opts);
        dd::ModelLink& link = s->doc.model();
        link.kind = s->kind;
        link.path = strOr(opts, "path", s->kind == dd::ModelKind::Part ? "part.json" : "assembly.json");
        if (opts.contains("properties")) propertiesApply(link.properties, opts.at("properties"));
        s->doc.sheets().push_back(newSheet(s->doc, opts.contains("sheet") ? opts.at("sheet") : json::object()));
        initialise(*s, false);
        const std::uint32_t h = registerSession(std::move(s));
        return {{"handle", h}};
    });
}

std::string loadDrawing(std::string drawingJson, std::string modelJson) {
    return guarded([&]() -> json {
        dd::DrawingDocument doc = take(dd::DrawingDocument::fromJson(drawingJson));
        const std::string kind = doc.model().kind == dd::ModelKind::Part ? "part" : "assembly";
        auto s = sessionForModel(kind, modelJson, json::object());
        s->doc = std::move(doc);
        initialise(*s, false);
        const json report = updateReportJson(s->lastReport);
        const std::uint32_t h = registerSession(std::move(s));
        return {{"handle", h}, {"report", report}};
    });
}

// ------------------------------------------------------------------ entry points

std::string drawingInfo() {
    return guarded([]() -> json {
        return {{"occtVersion", OCC_VERSION_COMPLETE},
                {"liveDrawings", sessions().size()},
                {"formatVersion", dd::DrawingDocument::kFormatVersion},
                {"font", font() ? json({{"postScriptName", font()->postScriptName()}, {"source", font()->source()}}) : json(nullptr)}};
    });
}

std::string setFont(std::string bytes, std::string source) {
    return guarded([&]() -> json {
        std::vector<std::uint8_t> data(bytes.begin(), bytes.end());
        dv::TrueTypeFont f = take(dv::TrueTypeFont::fromBytes(std::move(data), source));
        font() = std::move(f);
        // Sessions keep a pointer to the font: rebind them to the new object.
        for (auto& [h, s] : sessions()) s->doc.setFont(&*font());
        return {{"postScriptName", font()->postScriptName()}, {"glyphs", font()->numGlyphs()}};
    });
}

std::string dispose(std::uint32_t handle) {
    return guarded([&]() -> json {
        session(handle);
        sessions().erase(handle);
        return nullptr;
    });
}

std::string documentDescribe(std::uint32_t handle) {
    return guarded([&]() -> json { return describe(session(handle)); });
}

std::string documentSave(std::uint32_t handle) {
    return guarded([&]() -> json { return take(session(handle).doc.toJson()); });
}

std::string modelSave(std::uint32_t handle) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        if (s.kind == dd::ModelKind::Part) return take(s.part->toJson());
        return take(s.assembly->toJson());
    });
}

std::string modelVariables(std::uint32_t handle) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        if (s.kind != dd::ModelKind::Part) raise({k::ErrorCode::Unsupported, "variables: the drawing model is an assembly"});
        json out = json::array();
        for (const auto& [name, expr] : s.part->variables()) {
            auto v = s.part->variableValue(name);
            out.push_back({{"name", name}, {"expression", expr}, {"value", v ? json(v.value()) : json(nullptr)},
                           {"error", v ? json(nullptr) : json(v.error().message)}});
        }
        return out;
    });
}

json stalenessJson(const dd::StalenessReport& r) {
    return {{"modelChanged", r.modelChanged}, {"storedHash", r.storedHash}, {"currentHash", r.currentHash},
            {"staleViews", r.staleViews}, {"dimensions", r.dimensions}};
}

// Sets a part variable and rebuilds the part; the drawing is not updated (see checkModel).
std::string modelSetVariable(std::uint32_t handle, std::string name, std::string expression) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        if (s.kind != dd::ModelKind::Part) raise({k::ErrorCode::Unsupported, "variables: the drawing model is an assembly"});
        std::string previous;
        for (const auto& [n, e] : s.part->variables())
            if (n == name) previous = e;
        if (previous.empty()) throw InputError("unknown variable '" + name + "'");
        if (auto st = s.part->setVariable(name, expression); !st) raise(st.error());
        const m::RebuildReport rep = s.part->rebuild();
        if (!rep.failed.empty()) {
            // Restore the variable: a drawing needs a valid model.
            s.part->setVariable(name, previous);
            s.part->rebuild();
            raise({k::ErrorCode::OperationFailed,
                   "variable " + name + " = " + expression + ": the part does not rebuild (feature '" + rep.failed.front() + "')"});
        }
        const dd::ModelSnapshot snap = take(dd::snapshotPart(*s.part, s.doc.model().properties));
        return stalenessJson(s.doc.checkModel(snap));
    });
}

std::string checkModel(std::uint32_t handle) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        return stalenessJson(s.doc.checkModel(currentSnapshot(s)));
    });
}

std::string updateFromModel(std::uint32_t handle) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        const dd::ModelSnapshot snap = currentSnapshot(s);
        return updateReportJson(runUpdate(s, snap));
    });
}

std::string setModelProperties(std::uint32_t handle, std::string text) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        const json j = parseArg(text, "properties");
        if (s.kind != dd::ModelKind::Part)
            raise({k::ErrorCode::Unsupported, "model properties of an assembly come from its root definition"});
        const dd::ModelProperties backup = s.doc.model().properties;
        try {
            propertiesApply(s.doc.model().properties, j);
            const dd::ModelSnapshot snap = take(dd::snapshotPart(*s.part, s.doc.model().properties));
            return updateReportJson(runUpdate(s, snap));
        } catch (...) {
            s.doc.model().properties = backup;
            throw;
        }
    });
}

// ---- sheets

std::string addSheet(std::uint32_t handle, std::string text) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        const json j = text.empty() ? json::object() : parseArg(text, "sheet");
        return transaction(s, [&]() -> json {
            dd::SheetDef sd = newSheet(s.doc, j);
            const std::string id = sd.id;
            if (s.doc.findSheet(id)) throw InputError("sheet '" + id + "' already exists");
            s.doc.sheets().push_back(std::move(sd));
            return {{"id", id}};
        });
    });
}

std::string removeSheet(std::uint32_t handle, std::string id) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        return transaction(s, [&]() -> json {
            auto& sh = s.doc.sheets();
            if (sh.size() == 1) throw InputError("the last sheet of a drawing cannot be removed");
            for (auto it = sh.begin(); it != sh.end(); ++it) {
                if (it->id == id) {
                    sh.erase(it);
                    return nullptr;
                }
            }
            throw InputError("unknown sheet '" + id + "'");
        });
    });
}

std::string setSheet(std::uint32_t handle, std::string id, std::string text) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        const json j = parseArg(text, "sheet");
        return transaction(s, [&]() -> json {
            dd::SheetDef& sd = sheetOf(s, id);
            if (j.contains("format") || j.contains("multiplier") || j.contains("orientation")) sd.format = formatFrom(j, sd.format);
            if (j.contains("scale")) sd.scale = scaleFrom(j.at("scale"));
            if (j.contains("title")) titleApply(sd.title, j.at("title"));
            if (j.contains("technicalRequirements")) {
                std::vector<std::string> items;
                for (const json& t : j.at("technicalRequirements")) {
                    if (!t.is_string()) throw InputError("technical requirements must be strings");
                    if (!t.get<std::string>().empty()) items.push_back(t.get<std::string>());
                }
                sd.technicalRequirements = std::move(items);
            }
            if (j.contains("roughnessRemainder"))
                sd.roughnessRemainder = j.at("roughnessRemainder").is_null() ? std::nullopt
                                                                             : std::optional(roughFrom(j.at("roughnessRemainder")));
            if (j.contains("partsList")) sd.partsList = boolOr(j, "partsList", false);
            return nullptr;
        });
    });
}

// ---- views

// Orientation of a base view from a standard view name relative to the default front view.
void orientBase(dd::ViewDef& v, const json& j) {
    if (j.contains("toward") || j.contains("right")) {
        v.toward = vec3From(need(j, "toward"), "toward");
        v.right = vec3From(need(j, "right"), "right");
        return;
    }
    const vw::StandardView sv = j.contains("standard") ? enumFrom(j.at("standard"), kStandard, "standard view") : vw::StandardView::Front;
    const vw::ViewFrame f = vw::standardFrame(vw::defaultFrontFrame(), sv);
    v.toward = f.toward;
    v.right = f.right;
}

std::string addView(std::uint32_t handle, std::string sheetId, std::string text) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        const json j = parseArg(text, "view");
        return transaction(s, [&]() -> json {
            dd::SheetDef& sd = sheetOf(s, sheetId);
            dd::ViewDef v;
            v.kind = enumFrom(need(j, "kind"), kViewKinds, "view kind");
            v.id = strOr(j, "id");
            if (v.id.empty()) v.id = freshId(s.doc, "V");
            v.parent = strOr(j, "parent");
            v.hidden = boolOr(j, "hidden", true);
            v.centerLines = boolOr(j, "centerLines", true);
            v.autoBalloons = boolOr(j, "autoBalloons", false);
            if (j.contains("scale")) v.scale = scaleFrom(j.at("scale"));
            if (j.contains("position") && !j.at("position").is_null()) v.position = point2From(j.at("position"), "position");
            if (v.parent.empty() && v.kind != dd::ViewKind::Base) {
                // Default parent: the base view of the sheet.
                for (const auto& o : sd.views)
                    if (o.kind == dd::ViewKind::Base) v.parent = o.id;
                if (v.parent.empty()) throw InputError("add a base view first");
            }
            switch (v.kind) {
                case dd::ViewKind::Base:
                    for (const auto& o : sd.views)
                        if (o.kind == dd::ViewKind::Base) throw InputError("sheet '" + sd.id + "' already has a base view");
                    orientBase(v, j);
                    break;
                case dd::ViewKind::Projected:
                    if (j.contains("standard")) {
                        // Standard view relative to the base view: side by GOST 2.305 placement.
                        const vw::StandardView st = enumFrom(j.at("standard"), kStandard, "standard view");
                        auto side = vw::standardPlacement(st, s.doc.settings().projection);
                        if (!side) throw InputError("the front view is the base view, not a projected one");
                        std::string base;
                        for (const auto& o : sd.views)
                            if (o.kind == dd::ViewKind::Base) base = o.id;
                        if (base.empty()) throw InputError("add a base view first");
                        if (st == vw::StandardView::Rear) {
                            // Beside the left (first angle) or right (third angle) view.
                            const vw::StandardView next = s.doc.settings().projection == vw::ProjectionMethod::FirstAngle
                                                              ? vw::StandardView::Left : vw::StandardView::Right;
                            const vw::SheetSide ns = *vw::standardPlacement(next, s.doc.settings().projection);
                            std::string neighbour;
                            for (const auto& o : sd.views)
                                if (o.kind == dd::ViewKind::Projected && o.parent == base && o.side && *o.side == ns) neighbour = o.id;
                            if (neighbour.empty()) throw InputError("the rear view is placed beside the " + enumName(next, kStandard) + " view: add it first");
                            v.parent = neighbour;
                        } else {
                            v.parent = base;
                        }
                        v.side = *side;
                    } else {
                        v.side = enumFrom(need(j, "side"), kSides, "side");
                    }
                    for (const auto& o : sd.views)
                        if (o.parent == v.parent && o.side && *o.side == *v.side && inGroup(o))
                            throw InputError("view '" + o.id + "' already stands " + enumName(*v.side, kSides) + " '" + v.parent + "'");
                    break;
                case dd::ViewKind::Axonometric:
                    if (j.contains("axonometry")) v.axonometry = enumFrom(j.at("axonometry"), kAxo, "axonometry");
                    break;
                case dd::ViewKind::Section: {
                    v.planePoint = vec3From(need(j, "planePoint"), "planePoint");
                    if (j.contains("sectionKind")) v.sectionKind = enumFrom(j.at("sectionKind"), kSecKinds, "section kind");
                    v.markView = strOr(j, "markView", v.parent);
                    if (j.contains("side") && !j.at("side").is_null()) v.side = enumFrom(j.at("side"), kSides, "side");
                    else orientBase(v, j);
                    break;
                }
                case dd::ViewKind::Detail:
                    v.detailCenter = point2From(need(j, "detailCenter"), "detailCenter");
                    v.detailRadius = numOr(j, "detailRadius", 0.0);
                    if (v.detailRadius <= 0) throw InputError("detailRadius must be > 0");
                    break;
            }
            const std::string id = v.id;
            sd.views.push_back(std::move(v));
            return {{"id", id}};
        });
    });
}

// Views and annotations that depend on view id (children, section marks, annotations).
void collectDependents(const dd::SheetDef& sd, const std::string& id, std::set<std::string>& out) {
    out.insert(id);
    for (const auto& v : sd.views)
        if (!out.count(v.id) && (v.parent == id || v.markView == id)) collectDependents(sd, v.id, out);
}

std::string removeView(std::uint32_t handle, std::string sheetId, std::string viewId) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        return transaction(s, [&]() -> json {
            dd::SheetDef& sd = sheetOf(s, sheetId);
            bool found = false;
            for (const auto& v : sd.views) found = found || v.id == viewId;
            if (!found) throw InputError("unknown view '" + viewId + "'");
            std::set<std::string> gone;
            collectDependents(sd, viewId, gone);
            auto erase = [&](auto& list) {
                std::erase_if(list, [&](const auto& item) { return gone.count(item.view) > 0; });
            };
            std::erase_if(sd.views, [&](const dd::ViewDef& v) { return gone.count(v.id) > 0; });
            erase(sd.dimensions);
            erase(sd.roughness);
            erase(sd.tolerances);
            erase(sd.datums);
            return {{"removed", std::vector<std::string>(gone.begin(), gone.end())}};
        });
    });
}

// Moves a view so that the centre of its sheet bounds lands at (x, y). A view in projection
// relation moves its whole group (the base view position): the relation is kept by construction.
std::string moveView(std::uint32_t handle, std::string sheetId, std::string viewId, double x, double y) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        if (!std::isfinite(x) || !std::isfinite(y)) throw InputError("view position must be finite");
        return transaction(s, [&]() -> json {
            dd::SheetDef& sd = sheetOf(s, sheetId);
            const dd::ComputedSheet& cs = computedSheetOf(s, sheetId);
            const dd::ComputedView& cv = computedViewOf(cs, viewId);
            const vw::Box2 b = cv.sheetView.sheetBounds();
            const double dx = x - 0.5 * (b.minX + b.maxX), dy = y - 0.5 * (b.minY + b.maxY);
            dd::ViewDef* target = nullptr;
            for (auto& v : sd.views)
                if (v.id == viewId) target = &v;
            std::string moved = viewId;
            if (inGroup(*target)) {
                // Centre of the group (base position): the current centre of all group views.
                vw::Box2 all;
                for (const dd::ComputedView& o : cs.views) {
                    if (!inGroup(o.def)) continue;
                    const vw::Box2 ob = o.sheetView.sheetBounds();
                    all.add({ob.minX, ob.minY});
                    all.add({ob.maxX, ob.maxY});
                }
                for (auto& v : sd.views) {
                    if (v.kind != dd::ViewKind::Base) continue;
                    const dv::Point2 c = v.position ? *v.position : dv::Point2{0.5 * (all.minX + all.maxX), 0.5 * (all.minY + all.maxY)};
                    v.position = dv::Point2{c.x + dx, c.y + dy};
                    moved = v.id;
                }
            } else {
                target->position = dv::Point2{x, y};
            }
            return {{"moved", moved}, {"group", inGroup(*target)}};
        });
    });
}

std::string setView(std::uint32_t handle, std::string sheetId, std::string viewId, std::string text) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        const json j = parseArg(text, "view");
        return transaction(s, [&]() -> json {
            dd::SheetDef& sd = sheetOf(s, sheetId);
            dd::ViewDef* v = nullptr;
            for (auto& o : sd.views)
                if (o.id == viewId) v = &o;
            if (!v) throw InputError("unknown view '" + viewId + "'");
            if (j.contains("scale")) {
                if (inGroup(*v) && !j.at("scale").is_null())
                    throw InputError("view '" + viewId + "' is in projection relation: its scale is the sheet scale (change the sheet scale)");
                v->scale = scaleFrom(j.at("scale"));
            }
            if (j.contains("hidden")) v->hidden = boolOr(j, "hidden", true);
            if (j.contains("centerLines")) v->centerLines = boolOr(j, "centerLines", true);
            if (j.contains("autoBalloons")) v->autoBalloons = boolOr(j, "autoBalloons", false);
            if (j.contains("position")) {
                if (j.at("position").is_null()) v->position.reset();
                else v->position = point2From(j.at("position"), "position");
            }
            if (j.contains("detailRadius") && v->kind == dd::ViewKind::Detail) v->detailRadius = numOr(j, "detailRadius", v->detailRadius);
            return nullptr;
        });
    });
}

// ---- annotations

an::Anchor anchorTo(const Session& s, an::RefKind kind, std::uint32_t index, an::AnchorKind ak) {
    an::Anchor a;
    a.kind = ak;
    a.ref.kind = kind;
    a.ref.index = index;
    const m::ElementKind ek = kind == an::RefKind::Face ? m::ElementKind::Face
                              : kind == an::RefKind::Edge ? m::ElementKind::Edge : m::ElementKind::Vertex;
    a.ref.persistent = take(dd::elementReference(*s.shown, ek, index));
    return a;
}

const vw::ViewEdge& pickedEdge(const dd::ComputedView& cv, const json& pick) {
    if (!pick.is_number_integer()) throw InputError("a picked edge is the integer index of a view edge");
    const long long i = pick.get<long long>();
    if (i < 0 || i >= (long long)cv.sheetView.view.edges.size())
        throw InputError("view '" + cv.id + "' has no edge " + std::to_string(i));
    return cv.sheetView.view.edges[std::size_t(i)];
}

// Direction of a straight view edge in sheet coordinates (unit), nullopt for curves.
std::optional<dv::Point2> lineDirection(const vw::ViewEdge& e) {
    dv::Point2 a, b;
    if (const auto* l = std::get_if<dv::LineSeg>(&e.curve)) {
        a = l->a;
        b = l->b;
    } else if (const auto* p = std::get_if<dv::Polyline>(&e.curve)) {
        // A polyline is straight when all its points lie on the chord.
        if (p->points.size() < 2) return std::nullopt;
        a = p->points.front();
        b = p->points.back();
        const double L = std::hypot(b.x - a.x, b.y - a.y);
        if (L < 1e-9) return std::nullopt;
        for (const dv::Point2& q : p->points)
            if (std::abs((b.x - a.x) * (q.y - a.y) - (b.y - a.y) * (q.x - a.x)) / L > 1e-6) return std::nullopt;
    } else {
        return std::nullopt;
    }
    const double L = std::hypot(b.x - a.x, b.y - a.y);
    if (L < 1e-12) return std::nullopt;
    return dv::Point2{(b.x - a.x) / L, (b.y - a.y) / L};
}

// Point of a picked edge for a linear dimension: a vertex of a straight edge, the centre of a
// circle or arc. Returns the anchor and its position in view coordinates.
std::pair<an::Anchor, dv::Point2> featurePoint(const Session& s, const vw::ViewEdge& e) {
    if (!e.edgeIndex) throw InputError("the picked line is a silhouette of a curved face, not a model edge: pick an edge or a circle");
    if (const auto* c = std::get_if<dv::Circle>(&e.curve))
        return {anchorTo(s, an::RefKind::Edge, *e.edgeIndex, an::AnchorKind::Center), c->center};
    if (const auto* a = std::get_if<dv::Arc>(&e.curve))
        return {anchorTo(s, an::RefKind::Edge, *e.edgeIndex, an::AnchorKind::Center), a->center};
    if (const auto* l = std::get_if<dv::LineSeg>(&e.curve))
        return {anchorTo(s, an::RefKind::Edge, *e.edgeIndex, an::AnchorKind::Start), l->a};
    if (const auto* p = std::get_if<dv::Polyline>(&e.curve))
        return {anchorTo(s, an::RefKind::Edge, *e.edgeIndex, an::AnchorKind::Start), p->points.front()};
    throw InputError("unsupported edge curve");
}

// Dimension definition from one or two picked view edges:
//   circle -> diameter, arc -> radius; two silhouettes of one cylinder -> diameter;
//   two straight edges: parallel and vertical on the sheet -> horizontal distance, parallel and
//   horizontal -> vertical distance, not parallel -> angle; a line and a circle / two circles ->
//   distance across the line (centre to centre along the larger offset).
an::DimensionDef dimensionFromPicks(const Session& s, const dd::ComputedView& cv, const json& picks, const json& opts) {
    if (!picks.is_array() || picks.empty() || picks.size() > 2) throw InputError("pick one or two edges of the view");
    an::DimensionDef def;
    const vw::Box2 vb = cv.sheetView.view.bounds;
    const dv::Point2 centre{0.5 * (vb.minX + vb.maxX), 0.5 * (vb.minY + vb.maxY)};
    const double offset = numOr(opts, "offset", 10.0);
    if (picks.size() == 1) {
        const vw::ViewEdge& e = pickedEdge(cv, picks[0]);
        if (!e.edgeIndex) throw InputError("the picked line is a silhouette: pick both silhouettes of the cylinder for its diameter");
        if (std::holds_alternative<dv::Circle>(e.curve)) {
            def.type = an::DimensionType::Diameter;
        } else if (std::holds_alternative<dv::Arc>(e.curve)) {
            def.type = an::DimensionType::Radius;
        } else {
            throw InputError("one picked edge gives a dimension only for a circle (diameter) or an arc (radius); pick two lines for a distance");
        }
        def.anchors.push_back(anchorTo(s, an::RefKind::Edge, *e.edgeIndex, an::AnchorKind::Center));
        def.leaderAngleDeg = numOr(opts, "leaderAngle", 45.0);
        return def;
    }
    const vw::ViewEdge& e1 = pickedEdge(cv, picks[0]);
    const vw::ViewEdge& e2 = pickedEdge(cv, picks[1]);
    if (&e1 == &e2) throw InputError("the two picked edges are the same");
    if (!e1.edgeIndex && !e2.edgeIndex && e1.faceIndex && e2.faceIndex && *e1.faceIndex == *e2.faceIndex) {
        def.type = an::DimensionType::Diameter;
        def.anchors.push_back(anchorTo(s, an::RefKind::Face, *e1.faceIndex, an::AnchorKind::Center));
        def.offset = 8.0;  // overshoot of the dimension line past the end of the silhouettes, sheet mm
        return def;
    }
    const auto d1 = lineDirection(e1), d2 = lineDirection(e2);
    if (d1 && d2) {
        const double cr = d1->x * d2->y - d1->y * d2->x;
        if (std::abs(cr) > 1e-6) {
            if (!e1.edgeIndex || !e2.edgeIndex) throw InputError("an angle is measured between model edges, not silhouettes");
            def.type = an::DimensionType::Angle;
            def.anchors.push_back(anchorTo(s, an::RefKind::Edge, *e1.edgeIndex, an::AnchorKind::Start));
            def.anchors.push_back(anchorTo(s, an::RefKind::Edge, *e2.edgeIndex, an::AnchorKind::Start));
            def.offset = std::max(std::abs(offset), 15.0);
            return def;
        }
    }
    // Distance: the measuring axis is across the lines.
    const auto [a1, p1] = featurePoint(s, e1);
    const auto [a2, p2] = featurePoint(s, e2);
    const auto dir = d1 ? d1 : d2;
    bool horizontal;  // measure along x of the sheet
    if (dir) {
        if (std::abs(dir->x) < 1e-6) horizontal = true;
        else if (std::abs(dir->y) < 1e-6) horizontal = false;
        else throw InputError("the picked lines are inclined: distances are measured between vertical or horizontal lines of the view");
    } else {
        horizontal = std::abs(p2.x - p1.x) >= std::abs(p2.y - p1.y);
    }
    def.type = horizontal ? an::DimensionType::Horizontal : an::DimensionType::Vertical;
    def.anchors = {a1, a2};
    // Outside the view: above/below (horizontal) or left/right (vertical) of the nearer side.
    if (horizontal) {
        const double my = 0.5 * (p1.y + p2.y);
        def.offset = my >= centre.y ? std::abs(offset) : -std::abs(offset);
    } else {
        const double mx = 0.5 * (p1.x + p2.x);
        def.offset = mx <= centre.x ? std::abs(offset) : -std::abs(offset);
    }
    return def;
}

std::string addDimension(std::uint32_t handle, std::string sheetId, std::string viewId, std::string picksText, std::string optsText) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        const json picks = parseArg(picksText, "picks");
        const json opts = optsText.empty() ? json::object() : parseArg(optsText, "options");
        return transaction(s, [&]() -> json {
            dd::SheetDef& sd = sheetOf(s, sheetId);
            const dd::ComputedView& cv = computedViewOf(computedSheetOf(s, sheetId), viewId);
            dd::DimensionItem item;
            item.id = strOr(opts, "id");
            if (item.id.empty()) item.id = freshId(s.doc, "D");
            item.view = viewId;
            item.def = dimensionFromPicks(s, cv, picks, opts);
            if (opts.contains("tolerance") && !opts.at("tolerance").is_null())
                item.def.tolerance = take(an::parseToleranceClass(strOr(opts, "tolerance")));
            const std::string id = item.id;
            sd.dimensions.push_back(std::move(item));
            return {{"id", id}};
        });
    });
}

std::string setDimension(std::uint32_t handle, std::string sheetId, std::string id, std::string text) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        const json j = parseArg(text, "dimension");
        return transaction(s, [&]() -> json {
            dd::SheetDef& sd = sheetOf(s, sheetId);
            for (auto& d : sd.dimensions) {
                if (d.id != id) continue;
                if (j.contains("offset")) d.def.offset = numOr(j, "offset", d.def.offset);
                if (j.contains("textShift")) d.def.textShift = numOr(j, "textShift", d.def.textShift);
                if (j.contains("decimals")) d.def.decimals = int(numOr(j, "decimals", d.def.decimals));
                if (j.contains("tolerance"))
                    d.def.tolerance = j.at("tolerance").is_null() || strOr(j, "tolerance").empty()
                                          ? std::nullopt
                                          : std::optional(take(an::parseToleranceClass(strOr(j, "tolerance"))));
                if (j.contains("toleranceDisplay")) d.def.toleranceDisplay = enumFrom(j.at("toleranceDisplay"), kTolDisp, "tolerance display");
                if (j.contains("textOverride"))
                    d.def.textOverride = j.at("textOverride").is_null() || strOr(j, "textOverride").empty()
                                             ? std::nullopt : std::optional(strOr(j, "textOverride"));
                if (j.contains("prefix")) d.def.prefix = strOr(j, "prefix");
                if (j.contains("suffix")) d.def.suffix = strOr(j, "suffix");
                return nullptr;
            }
            throw InputError("unknown dimension '" + id + "'");
        });
    });
}

// Edge reference of a picked straight model edge (roughness, tolerance, datum).
m::TopoReference edgeReferenceOfPick(const Session& s, const dd::ComputedView& cv, const json& pick) {
    const vw::ViewEdge& e = pickedEdge(cv, pick);
    if (!e.edgeIndex) throw InputError("the picked line is a silhouette: pick a model edge");
    if (!lineDirection(e)) throw InputError("pick a straight edge: the symbol stands on the line of a surface");
    return take(dd::elementReference(*s.shown, m::ElementKind::Edge, *e.edgeIndex));
}

std::string addSymbol(std::uint32_t handle, std::string sheetId, std::string viewId, std::string kind, std::string pickText,
                      std::string specText) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        const json pick = parseArg(pickText, "pick");
        const json spec = specText.empty() ? json::object() : parseArg(specText, "symbol");
        return transaction(s, [&]() -> json {
            dd::SheetDef& sd = sheetOf(s, sheetId);
            const dd::ComputedView& cv = computedViewOf(computedSheetOf(s, sheetId), viewId);
            const double at = numOr(spec, "at", 0.5);
            if (at < 0 || at > 1) throw InputError("at must be within 0..1");
            std::string id = strOr(spec, "id");
            if (kind == "roughness") {
                dd::RoughnessItem r;
                r.id = id.empty() ? freshId(s.doc, "R") : id;
                r.view = viewId;
                r.edge = edgeReferenceOfPick(s, cv, pick);
                r.at = at;
                r.spec = roughFrom(spec);
                id = r.id;
                sd.roughness.push_back(std::move(r));
            } else if (kind == "tolerance") {
                dd::ToleranceItem t;
                t.id = id.empty() ? freshId(s.doc, "T") : id;
                t.view = viewId;
                t.edge = edgeReferenceOfPick(s, cv, pick);
                t.at = at;
                t.frame.characteristic = enumFrom(need(spec, "characteristic"), kGeomChars, "geometric characteristic");
                t.frame.tolerance = strOr(spec, "tolerance");
                t.frame.diameterZone = boolOr(spec, "diameterZone", false);
                if (spec.contains("datums"))
                    for (const json& d : spec.at("datums"))
                        if (d.is_string() && !d.get<std::string>().empty()) t.frame.datums.push_back(d.get<std::string>());
                id = t.id;
                sd.tolerances.push_back(std::move(t));
            } else if (kind == "datum") {
                dd::DatumItem d;
                d.id = id.empty() ? freshId(s.doc, "B") : id;
                d.view = viewId;
                d.edge = edgeReferenceOfPick(s, cv, pick);
                d.at = at;
                d.letter = strOr(spec, "letter");
                id = d.id;
                sd.datums.push_back(std::move(d));
            } else {
                throw InputError("symbol kind must be 'roughness', 'tolerance' or 'datum'");
            }
            return {{"id", id}};
        });
    });
}

std::string removeAnnotation(std::uint32_t handle, std::string sheetId, std::string id) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        return transaction(s, [&]() -> json {
            dd::SheetDef& sd = sheetOf(s, sheetId);
            std::size_t n = 0;
            n += std::erase_if(sd.dimensions, [&](const auto& x) { return x.id == id; });
            n += std::erase_if(sd.roughness, [&](const auto& x) { return x.id == id; });
            n += std::erase_if(sd.tolerances, [&](const auto& x) { return x.id == id; });
            n += std::erase_if(sd.datums, [&](const auto& x) { return x.id == id; });
            if (n == 0) throw InputError("unknown annotation '" + id + "'");
            return nullptr;
        });
    });
}

// ---- picture

json curveJson(const vw::Curve2& c, const vw::SheetView& sv) {
    auto P = [&](dv::Point2 p) { return pointJson(sv.toSheet(p)); };
    if (const auto* l = std::get_if<dv::LineSeg>(&c)) return {{"type", "line"}, {"points", json::array({P(l->a), P(l->b)})}};
    if (const auto* a = std::get_if<dv::Arc>(&c))
        return {{"type", "arc"}, {"center", P(a->center)}, {"radius", a->radius * sv.scale},
                {"startDeg", a->startDeg}, {"endDeg", a->endDeg}};
    if (const auto* ci = std::get_if<dv::Circle>(&c))
        return {{"type", "circle"}, {"center", P(ci->center)}, {"radius", ci->radius * sv.scale}};
    const auto& pl = std::get<dv::Polyline>(c);
    json pts = json::array();
    for (const dv::Point2& p : pl.points) pts.push_back(P(p));
    return {{"type", "polyline"}, {"points", pts}, {"closed", pl.closed}};
}

std::vector<dv::Drawing> renderAll(Session& s) { return take(s.doc.render(&requireFont())); }

std::size_t sheetIndex(Session& s, const std::string& id) {
    for (std::size_t i = 0; i < s.doc.sheets().size(); ++i)
        if (s.doc.sheets()[i].id == id) return i;
    throw InputError("unknown sheet '" + id + "'");
}

// Sheet as SVG text (the export writer) plus the view edges in sheet coordinates for picking.
std::string sheetPicture(std::uint32_t handle, std::string sheetId) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        const std::size_t si = sheetIndex(s, sheetId);
        std::vector<dv::Drawing> drawings = renderAll(s);
        dv::SvgOptions so;
        so.font = &requireFont();
        so.title = s.doc.computedSheets()[si].title.designation;
        const std::string svg = take(dv::writeSvg(drawings[si], so));
        const dd::ComputedSheet& cs = s.doc.computedSheets()[si];
        json views = json::array();
        for (const dd::ComputedView& cv : cs.views) {
            json edges = json::array();
            for (std::size_t i = 0; i < cv.sheetView.view.edges.size(); ++i) {
                const vw::ViewEdge& e = cv.sheetView.view.edges[i];
                json je = curveJson(e.curve, cv.sheetView);
                je["index"] = i;
                je["visible"] = e.visibility == vw::Visibility::Visible;
                je["class"] = enumName(e.cls, kEdgeClasses);
                je["edgeIndex"] = e.edgeIndex ? json(*e.edgeIndex) : json(nullptr);
                je["faceIndex"] = e.faceIndex ? json(*e.faceIndex) : json(nullptr);
                edges.push_back(std::move(je));
            }
            const vw::Box2 b = cv.sheetView.sheetBounds();
            views.push_back({{"id", cv.id},
                             {"kind", enumName(cv.kind, kViewKinds)},
                             {"inGroup", inGroup(cv.def)},
                             {"label", cv.label},
                             {"scale", cv.scale.label()},
                             {"bounds", {{"minX", b.minX}, {"minY", b.minY}, {"maxX", b.maxX}, {"maxY", b.maxY}}},
                             // Sheet point = anchor + factor · (view point); view point = (P·right, P·up).
                             {"anchor", pointJson(cv.sheetView.anchor)},
                             {"factor", cv.sheetView.scale},
                             {"frame", {{"toward", vec3Json(cv.sheetView.view.frame.toward)},
                                        {"right", vec3Json(cv.sheetView.view.frame.right)},
                                        {"up", vec3Json(cv.sheetView.view.frame.up())}}},
                             {"edges", edges}});
        }
        return {{"sheet", sheetId}, {"width", cs.sheet.widthMm}, {"height", cs.sheet.heightMm}, {"svg", svg}, {"views", views}};
    });
}

// ---- specification

std::string specification(std::uint32_t handle) {
    return guarded([&]() -> json {
        Session& s = session(handle);
        if (!s.spec || !s.spec->specification())
            raise({k::ErrorCode::Unsupported, "a specification (GOST 2.106) is made for an assembly, the drawing model is a part"});
        const an::Specification& sp = *s.spec->specification();
        json sections = json::array();
        for (const an::SpecSectionData& sec : sp.sections) {
            json items = json::array();
            for (const an::SpecItem& it : sec.items)
                items.push_back({{"format", it.format}, {"zone", it.zone}, {"position", it.position},
                                 {"designation", it.designation}, {"name", it.name},
                                 {"quantity", it.quantity ? json(*it.quantity) : json(nullptr)}, {"note", it.note}});
            sections.push_back({{"section", enumName(sec.section, kSpecSections)}, {"title", an::sectionTitle(sec.section)}, {"items", items}});
        }
        return {{"designation", sp.designation}, {"name", sp.name}, {"sections", sections}};
    });
}

// ---- export

val bytesResult(const std::string& bytes) {
    val out = val::object();
    out.set("ok", true);
    val view(emscripten::typed_memory_view(bytes.size(), reinterpret_cast<const unsigned char*>(bytes.data())));
    out.set("value", view.call<val>("slice"));  // copy out of the module memory
    return out;
}
val bytesFailure(const std::string& text) { return val::global("JSON").call<val>("parse", text); }

template <typename Body>
val exported(Body&& body) {
    std::string bytes;
    const std::string status = guarded([&]() -> json {
        bytes = body();
        return nullptr;
    });
    if (status.rfind("{\"ok\":true", 0) != 0) return bytesFailure(status);
    return bytesResult(bytes);
}

// PDF of all drawing sheets and, for an assembly, the specification sheets after them.
val exportPdf(std::uint32_t handle, std::string optsText) {
    return exported([&]() -> std::string {
        Session& s = session(handle);
        const json opts = optsText.empty() ? json::object() : parseArg(optsText, "options");
        std::vector<dv::Drawing> drawings = renderAll(s);
        std::optional<an::SpecDocument> spec;
        if (s.spec && boolOr(opts, "specification", true)) spec = take(s.spec->render(&requireFont()));
        std::vector<const dv::Drawing*> pages;
        for (const auto& d : drawings) pages.push_back(&d);
        if (spec)
            for (const auto& d : spec->sheets) pages.push_back(&d);
        dv::PdfOptions po;
        po.font = &requireFont();
        po.title = s.doc.computedSheets().front().title.designation;
        return take(dv::writePdfPages(pages, po));
    });
}

val exportSheet(std::uint32_t handle, std::string sheetId, std::string format) {
    return exported([&]() -> std::string {
        Session& s = session(handle);
        const std::size_t si = sheetIndex(s, sheetId);
        std::vector<dv::Drawing> drawings = renderAll(s);
        if (format == "dxf") return take(dv::writeDxf(drawings[si]));
        if (format == "svg") {
            dv::SvgOptions so;
            so.font = &requireFont();
            return take(dv::writeSvg(drawings[si], so));
        }
        if (format == "pdf") {
            dv::PdfOptions po;
            po.font = &requireFont();
            return take(dv::writePdf(drawings[si], po));
        }
        throw InputError("sheet export format must be 'pdf', 'dxf' or 'svg'");
    });
}

}  // namespace

EMSCRIPTEN_BINDINGS(bendcore_drawing) {
    emscripten::function("drawingInfo", &drawingInfo);
    emscripten::function("setFont", &setFont);
    emscripten::function("createDrawing", &createDrawing);
    emscripten::function("loadDrawing", &loadDrawing);
    emscripten::function("dispose", &dispose);
    emscripten::function("describe", &documentDescribe);
    emscripten::function("save", &documentSave);
    emscripten::function("modelSave", &modelSave);
    emscripten::function("modelVariables", &modelVariables);
    emscripten::function("modelSetVariable", &modelSetVariable);
    emscripten::function("setModelProperties", &setModelProperties);
    emscripten::function("checkModel", &checkModel);
    emscripten::function("updateFromModel", &updateFromModel);
    emscripten::function("addSheet", &addSheet);
    emscripten::function("removeSheet", &removeSheet);
    emscripten::function("setSheet", &setSheet);
    emscripten::function("addView", &addView);
    emscripten::function("removeView", &removeView);
    emscripten::function("moveView", &moveView);
    emscripten::function("setView", &setView);
    emscripten::function("addDimension", &addDimension);
    emscripten::function("setDimension", &setDimension);
    emscripten::function("addSymbol", &addSymbol);
    emscripten::function("removeAnnotation", &removeAnnotation);
    emscripten::function("sheetPicture", &sheetPicture);
    emscripten::function("specification", &specification);
    emscripten::function("exportPdf", &exportPdf);
    emscripten::function("exportSheet", &exportSheet);
}
