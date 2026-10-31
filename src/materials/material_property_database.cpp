#include "bendcore/materials/material_property_database.hpp"
#include <algorithm>
#include <cctype>

namespace bendcore::materials {

bool MaterialPropertyDatabase::addMaterial(const MaterialProperty& material) noexcept {
    if (material.material_id.empty()) return false;
    materials_[material.material_id] = material;
    return true;
}

std::optional<MaterialProperty> MaterialPropertyDatabase::getMaterial(const std::string& id) const noexcept {
    auto it = materials_.find(id);
    if (it != materials_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<MaterialProperty> MaterialPropertyDatabase::getMaterialsByClass(
    MaterialClass material_class) const noexcept {
    std::vector<MaterialProperty> results;
    for (const auto& [id, material] : materials_) {
        if (material.material_class == material_class) {
            results.push_back(material);
        }
    }
    return results;
}

std::vector<MaterialProperty> MaterialPropertyDatabase::getMaterialsByName(
    const std::string& name_pattern) const noexcept {
    std::vector<MaterialProperty> results;

    // Convert pattern to lowercase for case-insensitive search
    std::string pattern_lower = name_pattern;
    std::transform(pattern_lower.begin(), pattern_lower.end(), pattern_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto& [id, material] : materials_) {
        std::string name_lower = material.material_name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (name_lower.find(pattern_lower) != std::string::npos) {
            results.push_back(material);
        }
    }
    return results;
}

std::vector<MaterialProperty> MaterialPropertyDatabase::getAllMaterials() const noexcept {
    std::vector<MaterialProperty> results;
    for (const auto& [id, material] : materials_) {
        results.push_back(material);
    }
    return results;
}

std::vector<MaterialProperty> MaterialPropertyDatabase::searchByYieldStrength(
    double min_strength, double max_strength, double temp_c) const noexcept {
    std::vector<MaterialProperty> results;

    for (const auto& [id, material] : materials_) {
        double yield = material.yield_strength.getValueAtTemperature(temp_c);
        if (yield >= min_strength && yield <= max_strength) {
            results.push_back(material);
        }
    }
    return results;
}

std::vector<MaterialProperty> MaterialPropertyDatabase::searchByDensity(
    double min_density, double max_density) const noexcept {
    std::vector<MaterialProperty> results;

    for (const auto& [id, material] : materials_) {
        if (material.density >= min_density && material.density <= max_density) {
            results.push_back(material);
        }
    }
    return results;
}

std::vector<MaterialProperty> MaterialPropertyDatabase::searchByThermalConductivity(
    double min_conductivity, double max_conductivity, double temp_c) const noexcept {
    std::vector<MaterialProperty> results;

    for (const auto& [id, material] : materials_) {
        double conductivity = material.thermal_conductivity.getValueAtTemperature(temp_c);
        if (conductivity >= min_conductivity && conductivity <= max_conductivity) {
            results.push_back(material);
        }
    }
    return results;
}

bool MaterialPropertyDatabase::removeMaterial(const std::string& id) noexcept {
    return materials_.erase(id) > 0;
}

MaterialProperty MaterialPropertyDatabase::getMaterialAtTemperature(
    const std::string& id, double temp_c) const noexcept {
    auto opt = getMaterial(id);
    if (!opt) return MaterialProperty();

    MaterialProperty mat = *opt;
    // Update temperature-dependent properties
    mat.young_modulus.base_value = mat.young_modulus.getValueAtTemperature(temp_c);
    mat.yield_strength.base_value = mat.yield_strength.getValueAtTemperature(temp_c);
    mat.tensile_strength.base_value = mat.tensile_strength.getValueAtTemperature(temp_c);
    mat.thermal_conductivity.base_value = mat.thermal_conductivity.getValueAtTemperature(temp_c);

    return mat;
}

std::map<std::string, double> MaterialPropertyDatabase::compareProperties(
    const std::vector<std::string>& material_ids,
    const std::string& property_name,
    double temp_c) const noexcept {
    std::map<std::string, double> comparison;

    for (const auto& id : material_ids) {
        auto opt = getMaterial(id);
        if (opt) {
            double value = getPropertyValue(*opt, property_name, temp_c);
            comparison[id] = value;
        }
    }
    return comparison;
}

double MaterialPropertyDatabase::getPropertyValue(
    const MaterialProperty& mat,
    const std::string& property_name,
    double temp_c) const noexcept {
    // Map property names to values
    if (property_name == "density") return mat.density;
    if (property_name == "hardness_hv") return mat.hardness_hv;
    if (property_name == "elongation") return mat.elongation_percentage;
    if (property_name == "machinability") return mat.machinability_rating;
    if (property_name == "weldability") return mat.weldability_rating;

    // Temperature-dependent properties
    if (property_name == "young_modulus") return mat.young_modulus.getValueAtTemperature(temp_c);
    if (property_name == "yield_strength") return mat.yield_strength.getValueAtTemperature(temp_c);
    if (property_name == "tensile_strength") return mat.tensile_strength.getValueAtTemperature(temp_c);
    if (property_name == "thermal_conductivity") return mat.thermal_conductivity.getValueAtTemperature(temp_c);

    return 0.0;
}

void MaterialPropertyDatabase::initializeStandardMaterials() noexcept {
    addMaterial(factory::createSteel_AISI1045());
    addMaterial(factory::createSteel_Stainless304());
    addMaterial(factory::createAluminum_6061());
    addMaterial(factory::createAluminum_2024());
    addMaterial(factory::createCopper_Pure());
    addMaterial(factory::createTitanium_Grade5());
}

// Factory implementations
namespace factory {

MaterialProperty createSteel_AISI1045() noexcept {
    MaterialProperty mat;
    mat.material_id = "steel_aisi1045";
    mat.material_name = "AISI 1045 Steel";
    mat.material_class = MaterialClass::Steel;
    mat.grade = "AISI 1045";

    mat.young_modulus = {20.0, 200e9, -4e-4};  // ~4% loss per 100°C
    mat.yield_strength = {20.0, 310e6, -1e-2};  // Decreases with temp
    mat.tensile_strength = {20.0, 565e6, -1.5e-2};
    mat.shear_modulus = {20.0, 80e9, 0};

    mat.density = 7850.0;
    mat.specific_heat = {20.0, 486, 0};
    mat.thermal_conductivity = {20.0, 49.8, 0.01};
    mat.thermal_expansion = {20.0, 11.6e-6, 0};

    mat.hardness_hv = 163;
    mat.elongation_percentage = 16.0;
    mat.fatigue_limit = 240e6;
    mat.machinability_rating = 65.0;
    mat.weldability_rating = 70.0;

    return mat;
}

MaterialProperty createSteel_Stainless304() noexcept {
    MaterialProperty mat;
    mat.material_id = "steel_stainless304";
    mat.material_name = "Stainless Steel 304";
    mat.material_class = MaterialClass::Stainless;
    mat.grade = "ASTM A240 304";

    mat.young_modulus = {20.0, 193e9, -4e-4};
    mat.yield_strength = {20.0, 210e6, -1.5e-2};
    mat.tensile_strength = {20.0, 515e6, -2e-2};
    mat.shear_modulus = {20.0, 77e9, 0};

    mat.density = 8000.0;
    mat.specific_heat = {20.0, 500, 0};
    mat.thermal_conductivity = {20.0, 16.3, 0.005};
    mat.thermal_expansion = {20.0, 16e-6, 0};

    mat.hardness_hv = 217;
    mat.elongation_percentage = 40.0;
    mat.fatigue_limit = 170e6;
    mat.machinability_rating = 35.0;
    mat.weldability_rating = 85.0;
    mat.corrosion_resistance_rating = 95.0;

    return mat;
}

MaterialProperty createAluminum_6061() noexcept {
    MaterialProperty mat;
    mat.material_id = "aluminum_6061";
    mat.material_name = "Aluminum Alloy 6061";
    mat.material_class = MaterialClass::Aluminum;
    mat.grade = "ASTM B308 6061";

    mat.young_modulus = {20.0, 69e9, -3e-4};
    mat.yield_strength = {20.0, 275e6, -1e-2};
    mat.tensile_strength = {20.0, 310e6, -1.2e-2};
    mat.shear_modulus = {20.0, 26e9, 0};

    mat.density = 2700.0;
    mat.specific_heat = {20.0, 896, 0};
    mat.thermal_conductivity = {20.0, 167, 0.01};
    mat.thermal_expansion = {20.0, 23.6e-6, 0};

    mat.hardness_hv = 95;
    mat.elongation_percentage = 12.0;
    mat.fatigue_limit = 96e6;
    mat.machinability_rating = 70.0;
    mat.weldability_rating = 80.0;
    mat.corrosion_resistance_rating = 80.0;
    mat.relative_cost = 0.4;

    return mat;
}

MaterialProperty createAluminum_2024() noexcept {
    MaterialProperty mat;
    mat.material_id = "aluminum_2024";
    mat.material_name = "Aluminum Alloy 2024";
    mat.material_class = MaterialClass::Aluminum;
    mat.grade = "ASTM B308 2024";

    mat.young_modulus = {20.0, 73e9, -3.5e-4};
    mat.yield_strength = {20.0, 345e6, -1.5e-2};
    mat.tensile_strength = {20.0, 485e6, -2e-2};
    mat.shear_modulus = {20.0, 28e9, 0};

    mat.density = 2780.0;
    mat.specific_heat = {20.0, 875, 0};
    mat.thermal_conductivity = {20.0, 121, 0.008};
    mat.thermal_expansion = {20.0, 23.4e-6, 0};

    mat.hardness_hv = 120;
    mat.elongation_percentage = 19.0;
    mat.fatigue_limit = 160e6;
    mat.machinability_rating = 55.0;
    mat.weldability_rating = 40.0;
    mat.corrosion_resistance_rating = 30.0;
    mat.relative_cost = 0.6;

    return mat;
}

MaterialProperty createCopper_Pure() noexcept {
    MaterialProperty mat;
    mat.material_id = "copper_pure";
    mat.material_name = "Pure Copper";
    mat.material_class = MaterialClass::Copper;
    mat.grade = "ASTM B32 Electrolytic";

    mat.young_modulus = {20.0, 130e9, -3e-4};
    mat.yield_strength = {20.0, 70e6, -5e-3};
    mat.tensile_strength = {20.0, 220e6, -8e-3};
    mat.shear_modulus = {20.0, 48e9, 0};

    mat.density = 8960.0;
    mat.specific_heat = {20.0, 385, 0};
    mat.thermal_conductivity = {20.0, 398, 0.02};
    mat.thermal_expansion = {20.0, 16.5e-6, 0};

    mat.hardness_hv = 60;
    mat.elongation_percentage = 50.0;
    mat.fatigue_limit = 40e6;
    mat.machinability_rating = 40.0;
    mat.weldability_rating = 50.0;
    mat.corrosion_resistance_rating = 85.0;
    mat.relative_cost = 2.0;

    return mat;
}

MaterialProperty createTitanium_Grade5() noexcept {
    MaterialProperty mat;
    mat.material_id = "titanium_grade5";
    mat.material_name = "Titanium Grade 5 (Ti-6Al-4V)";
    mat.material_class = MaterialClass::Titanium;
    mat.grade = "ASTM B348 Grade 5";

    mat.young_modulus = {20.0, 103e9, -3.5e-4};
    mat.yield_strength = {20.0, 880e6, -5e-2};
    mat.tensile_strength = {20.0, 950e6, -6e-2};
    mat.shear_modulus = {20.0, 41e9, 0};

    mat.density = 4506.0;
    mat.specific_heat = {20.0, 526, 0};
    mat.thermal_conductivity = {20.0, 7.4, 0.002};
    mat.thermal_expansion = {20.0, 8.6e-6, 0};

    mat.hardness_hv = 334;
    mat.elongation_percentage = 10.0;
    mat.fatigue_limit = 400e6;
    mat.machinability_rating = 30.0;
    mat.weldability_rating = 70.0;
    mat.corrosion_resistance_rating = 98.0;
    mat.relative_cost = 5.0;

    return mat;
}

}  // namespace factory

}  // namespace bendcore::materials
