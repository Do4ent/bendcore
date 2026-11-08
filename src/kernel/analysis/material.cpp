#include <bendcore/kernel/material.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <utility>

namespace bendcore::kernel {

namespace {

using nlohmann::json;

constexpr const char* kFormat = "bendcore-materials";
constexpr int kVersion = 1;

// Source strings of the built-in table (docs/kernel/ANALYSIS.md, "Материалы"). None of the
// values was compared with the printed edition inside the project, hence verified = false
// everywhere; docs/STANDARDS_VERIFICATION.md lists what to check.
constexpr const char* kSrcEc3 =
    "EN 1993-1-1:2005 3.2.6 (E, nu, density); EN 10025-2:2004 (ReH, t <= 16 mm); "
    "EN 1993-1-2:2005 3.4.1.3 (lambda at 20 C)";
constexpr const char* kSrcSp16 =
    "SP 16.13330.2017 (E, nu, density); GOST 27772-2015 (Ryn, t 2-20 mm)";
constexpr const char* kSrcSteel45 =
    "GOST 1050-2013 (yield, normalised); density and E: Marochnik staley i splavov "
    "(ed. Zubchenko), steel 45";
constexpr const char* kSrc12x18 =
    "Marochnik staley i splavov (ed. Zubchenko) (density); GOST 5949 (0.2 % proof stress, bars)";
constexpr const char* kSrc1_4301 =
    "EN 10088-1:2014 (density, E, lambda at 20 C); EN 10088-2:2014 (Rp0.2, hot rolled plate P); "
    "EN 1993-1-4:2006 (nu)";
constexpr const char* kSrc1_4404 = kSrc1_4301;
constexpr const char* kSrcEc9 =
    "EN 1999-1-1:2007 3.2.5 (E, nu); EN 1999-1-1:2007 table 3.2a (f0); density: "
    "ASM Handbook vol. 2 (1990)";
constexpr const char* kSrcAsm = "ASM Handbook vol. 2 (1990), typical room-temperature values";
constexpr const char* kSrcAsmTi = "ASM Handbook vol. 2 (1990), Ti-6Al-4V annealed, typical values";
constexpr const char* kSrcAmg6 = "GOST 4784-2019 (grade); density: Spravochnik po alyuminievym splavam";
constexpr const char* kSrcPlastic =
    "Typical values of unfilled grades (manufacturer datasheets vary by grade)";

Material make(std::string id, std::string name, MaterialCategory category, double density,
              std::optional<double> e, std::optional<double> nu, std::optional<double> yield,
              std::optional<double> lambda, Rgb color, const char* source) {
    Material m;
    m.id = std::move(id);
    m.name = std::move(name);
    m.category = category;
    m.density = density;
    m.youngModulus = e;
    m.poissonRatio = nu;
    m.yieldStrength = yield;
    m.thermalConductivity = lambda;
    m.color = color;
    m.source = source;
    m.verified = false;
    m.builtIn = true;
    return m;
}

std::vector<Material> makeBuiltIns() {
    constexpr auto S = MaterialCategory::Steel;
    constexpr auto SS = MaterialCategory::StainlessSteel;
    constexpr auto AL = MaterialCategory::Aluminum;
    constexpr auto TI = MaterialCategory::Titanium;
    constexpr auto PL = MaterialCategory::Plastic;
    const std::nullopt_t none = std::nullopt;
    const Rgb steel{125, 130, 138};
    const Rgb stainless{190, 192, 196};
    const Rgb aluminium{208, 212, 218};
    return {
        make("steel_s235", "S235 (EN 10025-2)", S, 7850, 210000, 0.3, 235, 53.3, steel, kSrcEc3),
        make("steel_s355", "S355 (EN 10025-2)", S, 7850, 210000, 0.3, 355, 53.3, steel, kSrcEc3),
        make("steel_c245", "C245 (GOST 27772)", S, 7850, 206000, 0.3, 245, none, steel, kSrcSp16),
        make("steel_45", "Steel 45 (GOST 1050)", S, 7826, 200000, none, 355, none, steel,
             kSrcSteel45),
        make("stainless_12x18n10t", "12Kh18N10T (GOST 5632)", SS, 7920, none, none, 196, none,
             stainless, kSrc12x18),
        make("stainless_1_4301", "1.4301 / AISI 304 (EN 10088)", SS, 7900, 200000, 0.3, 210, 15,
             stainless, kSrc1_4301),
        make("stainless_1_4404", "1.4404 / AISI 316L (EN 10088)", SS, 8000, 200000, 0.3, 220, 15,
             stainless, kSrc1_4404),
        make("al_6082_t6", "EN AW-6082 T6", AL, 2700, 70000, 0.3, 260, none, aluminium, kSrcEc9),
        make("al_5083_o", "EN AW-5083 O/H111", AL, 2660, 70000, 0.3, 125, none, aluminium,
             kSrcEc9),
        make("al_6061_t6", "6061-T6", AL, 2700, 68900, 0.33, 276, 167, aluminium, kSrcAsm),
        make("al_2024_t3", "2024-T3 (D16T analogue)", AL, 2780, 73100, 0.33, 345, 121, aluminium,
             kSrcAsm),
        make("al_7075_t6", "7075-T6", AL, 2810, 71700, 0.33, 503, 130, aluminium, kSrcAsm),
        make("al_amg6", "AMg6 (GOST 4784)", AL, 2640, none, none, none, none, aluminium, kSrcAmg6),
        make("ti_6al4v", "Ti-6Al-4V (Grade 5)", TI, 4430, 113800, 0.342, 880, 6.7, {150, 150, 160},
             kSrcAsmTi),
        make("plastic_abs", "ABS", PL, 1040, 2300, none, 40, 0.17, {230, 226, 210}, kSrcPlastic),
        make("plastic_pa6", "PA6 (dry)", PL, 1130, 3000, none, none, 0.24, {235, 232, 220},
             kSrcPlastic),
        make("plastic_pom_h", "POM-H", PL, 1420, 3100, none, none, 0.31, {245, 245, 240},
             kSrcPlastic),
        make("plastic_pc", "PC", PL, 1200, 2350, 0.37, 65, 0.20, {200, 215, 230}, kSrcPlastic),
        make("plastic_pmma", "PMMA", PL, 1190, 3200, 0.37, none, 0.19, {215, 235, 240}, kSrcPlastic),
        make("plastic_ptfe", "PTFE", PL, 2170, none, none, none, 0.25, {250, 250, 250}, kSrcPlastic),
        make("plastic_pp", "PP (homopolymer)", PL, 905, none, none, none, none, {240, 240, 232},
             kSrcPlastic),
    };
}

std::string hexColor(Rgb c) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "#%02X%02X%02X", c.r, c.g, c.b);
    return buf;
}

std::optional<Rgb> parseHexColor(const std::string& text) {
    if (text.size() != 7 || text[0] != '#') return std::nullopt;
    auto nibble = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    int v[6];
    for (int i = 0; i < 6; ++i) {
        v[i] = nibble(text[1 + i]);
        if (v[i] < 0) return std::nullopt;
    }
    return Rgb{static_cast<std::uint8_t>(v[0] * 16 + v[1]), static_cast<std::uint8_t>(v[2] * 16 + v[3]),
               static_cast<std::uint8_t>(v[4] * 16 + v[5])};
}

json toJsonValue(const Material& m) {
    json j;
    j["id"] = m.id;
    j["name"] = m.name;
    j["category"] = toString(m.category);
    j["density_kg_m3"] = m.density;
    if (m.youngModulus) j["young_modulus_mpa"] = *m.youngModulus;
    if (m.poissonRatio) j["poisson_ratio"] = *m.poissonRatio;
    if (m.yieldStrength) j["yield_strength_mpa"] = *m.yieldStrength;
    if (m.thermalConductivity) j["thermal_conductivity_w_mk"] = *m.thermalConductivity;
    j["color"] = hexColor(m.color);
    j["source"] = m.source;
    j["verified"] = m.verified;
    return j;
}

Result<Material> fromJsonValue(const json& j) {
    if (!j.is_object()) return Result<Material>::failure(ErrorCode::ParseError, "material is not a JSON object");
    Material m;
    auto needString = [&](const char* key, std::string& out) -> std::optional<Error> {
        auto it = j.find(key);
        if (it == j.end() || !it->is_string())
            return Error{ErrorCode::ParseError, std::string("material: '") + key + "' must be a string"};
        out = it->get<std::string>();
        return std::nullopt;
    };
    auto optNumber = [&](const char* key, std::optional<double>& out) -> std::optional<Error> {
        auto it = j.find(key);
        if (it == j.end() || it->is_null()) return std::nullopt;
        if (!it->is_number())
            return Error{ErrorCode::ParseError, std::string("material: '") + key + "' must be a number"};
        out = it->get<double>();
        return std::nullopt;
    };
    if (auto e = needString("id", m.id)) return *e;
    if (auto e = needString("name", m.name)) return *e;
    std::string category;
    if (auto e = needString("category", category)) return *e;
    auto parsedCategory = materialCategoryFromString(category);
    if (!parsedCategory)
        return Result<Material>::failure(ErrorCode::ParseError, "material: unknown category '" + category + "'");
    m.category = *parsedCategory;
    std::optional<double> density;
    if (auto e = optNumber("density_kg_m3", density)) return *e;
    if (!density) return Result<Material>::failure(ErrorCode::ParseError, "material: 'density_kg_m3' is required");
    m.density = *density;
    if (auto e = optNumber("young_modulus_mpa", m.youngModulus)) return *e;
    if (auto e = optNumber("poisson_ratio", m.poissonRatio)) return *e;
    if (auto e = optNumber("yield_strength_mpa", m.yieldStrength)) return *e;
    if (auto e = optNumber("thermal_conductivity_w_mk", m.thermalConductivity)) return *e;
    if (auto it = j.find("color"); it != j.end()) {
        if (!it->is_string()) return Result<Material>::failure(ErrorCode::ParseError, "material: 'color' must be \"#RRGGBB\"");
        auto color = parseHexColor(it->get<std::string>());
        if (!color) return Result<Material>::failure(ErrorCode::ParseError, "material: 'color' must be \"#RRGGBB\"");
        m.color = *color;
    }
    if (auto it = j.find("source"); it != j.end() && it->is_string()) m.source = it->get<std::string>();
    if (auto it = j.find("verified"); it != j.end()) {
        if (!it->is_boolean()) return Result<Material>::failure(ErrorCode::ParseError, "material: 'verified' must be a boolean");
        m.verified = it->get<bool>();
    }
    m.builtIn = false;
    return validateMaterial(std::move(m));
}

}  // namespace

const char* toString(MaterialCategory category) noexcept {
    switch (category) {
        case MaterialCategory::Steel: return "steel";
        case MaterialCategory::StainlessSteel: return "stainless_steel";
        case MaterialCategory::Aluminum: return "aluminum";
        case MaterialCategory::Titanium: return "titanium";
        case MaterialCategory::Plastic: return "plastic";
        case MaterialCategory::Other: return "other";
    }
    return "other";
}

std::optional<MaterialCategory> materialCategoryFromString(std::string_view text) noexcept {
    for (MaterialCategory c : {MaterialCategory::Steel, MaterialCategory::StainlessSteel, MaterialCategory::Aluminum,
                               MaterialCategory::Titanium, MaterialCategory::Plastic, MaterialCategory::Other}) {
        if (text == toString(c)) return c;
    }
    return std::nullopt;
}

Result<Material> validateMaterial(Material m) {
    auto bad = [&](const std::string& what) {
        return Result<Material>::failure(ErrorCode::InvalidInput, "material '" + m.id + "': " + what);
    };
    if (m.id.empty()) return bad("id is empty");
    if (m.name.empty()) return bad("name is empty");
    if (!std::isfinite(m.density) || m.density <= 0.0 || m.density > 30000.0)
        return bad("density must be in (0, 30000] kg/m^3");
    if (m.youngModulus && (!std::isfinite(*m.youngModulus) || *m.youngModulus <= 0.0))
        return bad("Young's modulus must be positive");
    if (m.poissonRatio && (!std::isfinite(*m.poissonRatio) || *m.poissonRatio <= 0.0 || *m.poissonRatio >= 0.5))
        return bad("Poisson's ratio must be in (0, 0.5)");
    if (m.yieldStrength && (!std::isfinite(*m.yieldStrength) || *m.yieldStrength <= 0.0))
        return bad("yield strength must be positive");
    if (m.thermalConductivity && (!std::isfinite(*m.thermalConductivity) || *m.thermalConductivity <= 0.0))
        return bad("thermal conductivity must be positive");
    return m;
}

const std::vector<Material>& builtInMaterials() {
    static const std::vector<Material> materials = makeBuiltIns();
    return materials;
}

MaterialLibrary MaterialLibrary::withBuiltIns() {
    MaterialLibrary library;
    library.materials_ = builtInMaterials();
    return library;
}

const Material* MaterialLibrary::find(std::string_view id) const noexcept {
    for (const Material& m : materials_)
        if (m.id == id) return &m;
    return nullptr;
}

Result<const Material*> MaterialLibrary::add(Material material) {
    material.builtIn = false;
    auto checked = validateMaterial(std::move(material));
    if (!checked) return checked.error();
    if (find(checked.value().id) != nullptr)
        return Result<const Material*>::failure(ErrorCode::InvalidInput,
                                                "material id '" + checked.value().id + "' already exists");
    materials_.push_back(std::move(checked).value());
    return &materials_.back();
}

Result<const Material*> MaterialLibrary::update(Material material) {
    material.builtIn = false;
    auto checked = validateMaterial(std::move(material));
    if (!checked) return checked.error();
    for (Material& m : materials_) {
        if (m.id != checked.value().id) continue;
        if (m.builtIn)
            return Result<const Material*>::failure(ErrorCode::InvalidInput,
                                                    "built-in material '" + m.id + "' cannot be changed");
        m = std::move(checked).value();
        return &m;
    }
    return Result<const Material*>::failure(ErrorCode::InvalidInput, "no material '" + checked.value().id + "'");
}

Result<bool> MaterialLibrary::remove(std::string_view id) {
    auto it = std::find_if(materials_.begin(), materials_.end(), [&](const Material& m) { return m.id == id; });
    if (it == materials_.end())
        return Result<bool>::failure(ErrorCode::InvalidInput, "no material '" + std::string(id) + "'");
    if (it->builtIn)
        return Result<bool>::failure(ErrorCode::InvalidInput, "built-in material '" + it->id + "' cannot be removed");
    materials_.erase(it);
    return true;
}

std::string MaterialLibrary::userMaterialsToJson() const {
    json doc;
    doc["format"] = kFormat;
    doc["version"] = kVersion;
    doc["materials"] = json::array();
    for (const Material& m : materials_)
        if (!m.builtIn) doc["materials"].push_back(toJsonValue(m));
    return doc.dump(2);
}

Result<std::size_t> MaterialLibrary::addUserMaterialsFromJson(std::string_view text) {
    json doc = json::parse(text.begin(), text.end(), nullptr, false);
    if (doc.is_discarded()) return Result<std::size_t>::failure(ErrorCode::ParseError, "materials: invalid JSON");
    if (!doc.is_object() || doc.value("format", std::string()) != kFormat)
        return Result<std::size_t>::failure(ErrorCode::ParseError, "materials: 'format' is not \"bendcore-materials\"");
    if (!doc.contains("version") || !doc["version"].is_number_integer() || doc["version"].get<int>() != kVersion)
        return Result<std::size_t>::failure(ErrorCode::Unsupported, "materials: only version 1 is supported");
    if (!doc.contains("materials") || !doc["materials"].is_array())
        return Result<std::size_t>::failure(ErrorCode::ParseError, "materials: 'materials' must be an array");
    std::vector<Material> parsed;
    std::set<std::string> ids;
    for (const json& entry : doc["materials"]) {
        auto m = fromJsonValue(entry);
        if (!m) return m.error();
        if (find(m.value().id) != nullptr || !ids.insert(m.value().id).second)
            return Result<std::size_t>::failure(ErrorCode::InvalidInput,
                                                "material id '" + m.value().id + "' already exists");
        parsed.push_back(std::move(m).value());
    }
    for (Material& m : parsed) materials_.push_back(std::move(m));
    return parsed.size();
}

std::string materialToJson(const Material& material) { return toJsonValue(material).dump(2); }

Result<Material> materialFromJson(std::string_view text) {
    json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded()) return Result<Material>::failure(ErrorCode::ParseError, "material: invalid JSON");
    return fromJsonValue(j);
}

}  // namespace bendcore::kernel
