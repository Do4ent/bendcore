#pragma once

// Engineering materials for mass properties and display: density, elastic constants, yield
// strength, thermal conductivity and a display colour. Built-in grades carry their source and
// a verification flag (docs/kernel/ANALYSIS.md, docs/STANDARDS_VERIFICATION.md); user
// materials are added at run time and round-trip through JSON.

#include <bendcore/kernel/result.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bendcore::kernel {

enum class MaterialCategory { Steel, StainlessSteel, Aluminum, Titanium, Plastic, Other };

const char* toString(MaterialCategory category) noexcept;
std::optional<MaterialCategory> materialCategoryFromString(std::string_view text) noexcept;

struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    bool operator==(const Rgb&) const = default;
};

// Units: density kg/m^3, moduli and strengths MPa, thermal conductivity W/(m*K), all at
// room temperature (20 C). Properties the source does not give with confidence are empty.
struct Material {
    std::string id;           // unique key, e.g. "steel_s235"
    std::string name;         // display name, e.g. "S235 (EN 10025-2)"
    MaterialCategory category = MaterialCategory::Other;
    double density = 0.0;     // kg/m^3, > 0
    std::optional<double> youngModulus;         // MPa
    std::optional<double> poissonRatio;         // dimensionless, 0 < nu < 0.5
    std::optional<double> yieldStrength;        // MPa (0.2 % proof stress where no yield point)
    std::optional<double> thermalConductivity;  // W/(m*K)
    Rgb color{160, 160, 160};
    std::string source;       // where the numbers come from
    bool verified = false;    // true only after a check against the cited edition
    bool builtIn = false;     // part of builtInMaterials(); false for user materials

    bool operator==(const Material&) const = default;
};

// Range checks: non-empty id and name, density in (0, 30000], E > 0, 0 < nu < 0.5,
// yield > 0, conductivity > 0. Returns InvalidInput naming the offending field.
Result<Material> validateMaterial(Material material);

// The built-in grades (steels, stainless steels, aluminium alloys, titanium, plastics).
const std::vector<Material>& builtInMaterials();

class MaterialLibrary {
public:
    // Library pre-filled with builtInMaterials().
    static MaterialLibrary withBuiltIns();

    const std::vector<Material>& materials() const noexcept { return materials_; }
    const Material* find(std::string_view id) const noexcept;

    // Adds a user material (builtIn is forced to false). InvalidInput on validation failure
    // or when the id is already taken.
    Result<const Material*> add(Material material);
    // Replaces a user material with the same id; built-in materials cannot be replaced.
    Result<const Material*> update(Material material);
    // Removes a user material; built-in materials cannot be removed.
    Result<bool> remove(std::string_view id);

    // {"format":"bendcore-materials","version":1,"materials":[...]} with only user materials.
    std::string userMaterialsToJson() const;
    // Adds every material of such a document; nothing is added when any entry is invalid.
    Result<std::size_t> addUserMaterialsFromJson(std::string_view json);

private:
    std::vector<Material> materials_;
};

std::string materialToJson(const Material& material);
Result<Material> materialFromJson(std::string_view json);

}  // namespace bendcore::kernel
