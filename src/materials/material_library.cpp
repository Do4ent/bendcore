// Material Library - Implementation
// M8 Phase: Material Library (GAP-033)
#include "bendcore/materials/material_library.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <memory>

namespace bendcore::materials {

// Material class implementation
Material::Material(int32_t id, const std::string& name, MaterialCategory category)
    : id_(id), name_(name), category_(category),
      density_(0.0), cost_per_kg_(0.0),
      youngs_modulus_(0.0), poissons_ratio_(0.0), shear_modulus_(0.0),
      yield_strength_(0.0), tensile_strength_(0.0), compressive_strength_(0.0),
      elongation_at_break_(0.0), fatigue_limit_(0.0),
      fatigue_strength_coefficient_(0.0),
      thermal_conductivity_(0.0), specific_heat_(0.0),
      thermal_expansion_(0.0), melting_point_(0.0),
      strain_rate_sensitivity_(0.0) {

    // Set category name
    switch (category) {
        case MaterialCategory::Steel: category_name_ = "Steel"; break;
        case MaterialCategory::Aluminum: category_name_ = "Aluminum"; break;
        case MaterialCategory::Titanium: category_name_ = "Titanium"; break;
        case MaterialCategory::Copper: category_name_ = "Copper"; break;
        case MaterialCategory::Nickel: category_name_ = "Nickel"; break;
        case MaterialCategory::Magnesium: category_name_ = "Magnesium"; break;
        case MaterialCategory::Composite: category_name_ = "Composite"; break;
        case MaterialCategory::Ceramic: category_name_ = "Ceramic"; break;
        case MaterialCategory::Polymer: category_name_ = "Polymer"; break;
        default: category_name_ = "Other"; break;
    }

    // Initialize manufacturing properties
    manufacturing_properties_.machinability_rating = 0.0;
    manufacturing_properties_.weldability_rating = 0.0;
    manufacturing_properties_.formability_rating = 0.0;
    manufacturing_properties_.heat_treatability_rating = 0.0;

    // Initialize sustainability properties
    sustainability_properties_.carbon_footprint = 0.0;
    sustainability_properties_.recyclability_percentage = 0.0;
    sustainability_properties_.environmental_impact = "unknown";
    sustainability_properties_.is_recyclable = false;
    sustainability_properties_.recycled_content_percentage = 0.0;
}

void Material::add_temperature_dependent_youngs_modulus(double temp, double value) {
    temp_dependent_E_.push_back({temp, 0, value, 0});
}

void Material::add_temperature_dependent_yield_strength(double temp, double value) {
    temp_dependent_yield_.push_back({temp, 0, value, 0});
}

void Material::add_temperature_dependent_thermal_conductivity(double temp, double value) {
    temp_dependent_k_.push_back({temp, 0, value, 0});
}

double Material::interpolate_property(const std::vector<TemperatureRangeProperty>& props,
                                     double temperature) const {
    if (props.empty()) return 0.0;

    // Find the range containing the temperature
    for (size_t i = 0; i < props.size() - 1; ++i) {
        if (temperature >= props[i].min_temperature &&
            temperature <= props[i].max_temperature) {
            double t0 = props[i].min_temperature;
            double t1 = props[i].max_temperature;
            double v0 = props[i].value_at_min;
            double v1 = props[i].value_at_max;

            if (std::abs(t1 - t0) < 1e-6) return v0;
            double alpha = (temperature - t0) / (t1 - t0);
            return v0 + alpha * (v1 - v0);
        }
    }

    // Extrapolate if outside range
    if (temperature < props.front().min_temperature) {
        return props.front().value_at_min;
    }
    return props.back().value_at_max;
}

double Material::get_youngs_modulus_at_temperature(double temperature) const {
    if (temp_dependent_E_.empty()) return youngs_modulus_;
    return interpolate_property(temp_dependent_E_, temperature);
}

double Material::get_yield_strength_at_temperature(double temperature) const {
    if (temp_dependent_yield_.empty()) return yield_strength_;
    return interpolate_property(temp_dependent_yield_, temperature);
}

double Material::get_thermal_conductivity_at_temperature(double temperature) const {
    if (temp_dependent_k_.empty()) return thermal_conductivity_;
    return interpolate_property(temp_dependent_k_, temperature);
}

void Material::add_grade(const MaterialGrade& grade) {
    grades_.push_back(grade);
}

const MaterialGrade* Material::get_grade(const std::string& grade_name) const {
    for (const auto& grade : grades_) {
        if (grade.name == grade_name) {
            return &grade;
        }
    }
    return nullptr;
}

bool Material::is_valid() const {
    // Basic validation
    if (youngs_modulus_ > 0.0 && poissons_ratio_ >= -1.0 && poissons_ratio_ <= 0.5) {
        return true;
    }
    if (yield_strength_ > 0.0 && tensile_strength_ > 0.0) {
        return true;
    }
    return false;
}

std::vector<std::string> Material::validate() const {
    std::vector<std::string> errors;

    if (name_.empty()) {
        errors.push_back("Material name is empty");
    }

    if (youngs_modulus_ > 0.0 && (poissons_ratio_ < -1.0 || poissons_ratio_ > 0.5)) {
        errors.push_back("Poisson's ratio out of valid range [-1.0, 0.5]");
    }

    if (yield_strength_ > 0.0 && tensile_strength_ > 0.0) {
        if (yield_strength_ > tensile_strength_) {
            errors.push_back("Yield strength cannot exceed tensile strength");
        }
    }

    if (density_ <= 0.0 && density_ != 0.0) {
        errors.push_back("Density must be positive");
    }

    if (thermal_expansion_ < 0.0) {
        errors.push_back("Thermal expansion coefficient must be non-negative");
    }

    return errors;
}

// MaterialLibrary class implementation
MaterialLibrary::MaterialLibrary() : next_custom_id_(10000) {}
MaterialLibrary::~MaterialLibrary() = default;

void MaterialLibrary::add_material(std::unique_ptr<Material> material) {
    int32_t id = material->get_id();
    materials_by_id_[id] = material.get();
    materials_.push_back(std::move(material));
}

Material* MaterialLibrary::get_material(int32_t id) {
    auto it = materials_by_id_.find(id);
    if (it != materials_by_id_.end()) return it->second;
    return nullptr;
}

const Material* MaterialLibrary::get_material(int32_t id) const {
    auto it = materials_by_id_.find(id);
    if (it != materials_by_id_.end()) return it->second;
    return nullptr;
}

Material* MaterialLibrary::get_material_by_name(const std::string& name) {
    for (auto& mat : materials_) {
        if (mat->get_name() == name) return mat.get();
    }
    return nullptr;
}

const Material* MaterialLibrary::get_material_by_name(const std::string& name) const {
    for (const auto& mat : materials_) {
        if (mat->get_name() == name) return mat.get();
    }
    return nullptr;
}

std::vector<Material*> MaterialLibrary::get_materials_by_category(MaterialCategory category) {
    std::vector<Material*> result;
    for (auto& mat : materials_) {
        if (mat->get_category() == category) {
            result.push_back(mat.get());
        }
    }
    return result;
}

std::vector<const Material*> MaterialLibrary::get_materials_by_category(MaterialCategory category) const {
    std::vector<const Material*> result;
    for (const auto& mat : materials_) {
        if (mat->get_category() == category) {
            result.push_back(mat.get());
        }
    }
    return result;
}

std::vector<Material*> MaterialLibrary::search(const MaterialSearchCriteria& criteria) {
    std::vector<Material*> result;

    for (auto& mat : materials_) {
        bool matches = true;

        if (criteria.category && mat->get_category() != criteria.category.value()) {
            matches = false;
        }
        if (matches && criteria.min_youngs_modulus &&
            mat->get_youngs_modulus() < criteria.min_youngs_modulus.value()) {
            matches = false;
        }
        if (matches && criteria.max_youngs_modulus &&
            mat->get_youngs_modulus() > criteria.max_youngs_modulus.value()) {
            matches = false;
        }
        if (matches && criteria.min_yield_strength &&
            mat->get_yield_strength() < criteria.min_yield_strength.value()) {
            matches = false;
        }
        if (matches && criteria.max_yield_strength &&
            mat->get_yield_strength() > criteria.max_yield_strength.value()) {
            matches = false;
        }
        if (matches && criteria.min_density &&
            mat->get_density() < criteria.min_density.value()) {
            matches = false;
        }
        if (matches && criteria.max_density &&
            mat->get_density() > criteria.max_density.value()) {
            matches = false;
        }
        if (matches && criteria.max_cost_per_kg &&
            mat->get_cost_per_kg() > criteria.max_cost_per_kg.value()) {
            matches = false;
        }
        if (matches && criteria.name_contains) {
            if (mat->get_name().find(criteria.name_contains.value()) == std::string::npos) {
                matches = false;
            }
        }
        if (matches && criteria.min_machinability &&
            mat->get_manufacturing_properties().machinability_rating < criteria.min_machinability.value()) {
            matches = false;
        }
        if (matches && criteria.min_weldability &&
            mat->get_manufacturing_properties().weldability_rating < criteria.min_weldability.value()) {
            matches = false;
        }
        if (matches && criteria.min_thermal_conductivity &&
            mat->get_thermal_conductivity() < criteria.min_thermal_conductivity.value()) {
            matches = false;
        }

        if (matches) {
            result.push_back(mat.get());
        }
    }

    return result;
}

std::vector<const Material*> MaterialLibrary::search(const MaterialSearchCriteria& criteria) const {
    std::vector<const Material*> result;

    for (const auto& mat : materials_) {
        bool matches = true;

        if (criteria.category && mat->get_category() != criteria.category.value()) {
            matches = false;
        }
        if (matches && criteria.min_youngs_modulus &&
            mat->get_youngs_modulus() < criteria.min_youngs_modulus.value()) {
            matches = false;
        }
        if (matches && criteria.max_youngs_modulus &&
            mat->get_youngs_modulus() > criteria.max_youngs_modulus.value()) {
            matches = false;
        }
        if (matches && criteria.min_yield_strength &&
            mat->get_yield_strength() < criteria.min_yield_strength.value()) {
            matches = false;
        }
        if (matches && criteria.max_yield_strength &&
            mat->get_yield_strength() > criteria.max_yield_strength.value()) {
            matches = false;
        }
        if (matches && criteria.min_density &&
            mat->get_density() < criteria.min_density.value()) {
            matches = false;
        }
        if (matches && criteria.max_density &&
            mat->get_density() > criteria.max_density.value()) {
            matches = false;
        }
        if (matches && criteria.max_cost_per_kg &&
            mat->get_cost_per_kg() > criteria.max_cost_per_kg.value()) {
            matches = false;
        }
        if (matches && criteria.name_contains) {
            if (mat->get_name().find(criteria.name_contains.value()) == std::string::npos) {
                matches = false;
            }
        }
        if (matches && criteria.min_machinability &&
            mat->get_manufacturing_properties().machinability_rating < criteria.min_machinability.value()) {
            matches = false;
        }
        if (matches && criteria.min_weldability &&
            mat->get_manufacturing_properties().weldability_rating < criteria.min_weldability.value()) {
            matches = false;
        }
        if (matches && criteria.min_thermal_conductivity &&
            mat->get_thermal_conductivity() < criteria.min_thermal_conductivity.value()) {
            matches = false;
        }

        if (matches) {
            result.push_back(mat.get());
        }
    }

    return result;
}

MaterialComparison MaterialLibrary::compare(int32_t id1, int32_t id2) const {
    return compare(get_material(id1), get_material(id2));
}

MaterialComparison MaterialLibrary::compare(const Material* mat1, const Material* mat2) const {
    MaterialComparison comp{};
    comp.material1 = mat1;
    comp.material2 = mat2;

    if (mat1 && mat2) {
        if (mat2->get_cost_per_kg() > 0) {
            comp.cost_ratio = mat1->get_cost_per_kg() / mat2->get_cost_per_kg();
        }
        if (mat2->get_yield_strength() > 0) {
            comp.strength_ratio = mat1->get_yield_strength() / mat2->get_yield_strength();
        }
        if (mat2->get_density() > 0) {
            comp.weight_ratio = mat1->get_density() / mat2->get_density();
        }
        if (mat2->get_youngs_modulus() > 0) {
            comp.stiffness_ratio = mat1->get_youngs_modulus() / mat2->get_youngs_modulus();
        }
        if (mat2->get_thermal_conductivity() > 0) {
            comp.thermal_conductivity_ratio = mat1->get_thermal_conductivity() / mat2->get_thermal_conductivity();
        }

        if (mat1->get_density() > 0) {
            comp.strength_to_weight = mat1->get_yield_strength() / mat1->get_density();
        }
        if (mat2->get_density() > 0) {
            comp.strength_to_weight_2 = mat2->get_yield_strength() / mat2->get_density();
        }
        if (mat1->get_density() > 0) {
            comp.stiffness_to_weight = mat1->get_youngs_modulus() / mat1->get_density();
        }
        if (mat2->get_density() > 0) {
            comp.stiffness_to_weight_2 = mat2->get_youngs_modulus() / mat2->get_density();
        }
    }

    return comp;
}

double MaterialLibrary::calculate_total_cost(int32_t material_id, double mass_kg) const {
    const auto* mat = get_material(material_id);
    if (mat) {
        return mat->get_cost_per_kg() * mass_kg;
    }
    return 0.0;
}

std::vector<std::pair<int32_t, double>> MaterialLibrary::rank_by_cost_performance(
    const MaterialSearchCriteria& criteria) const {

    auto matches = search(criteria);
    std::vector<std::pair<int32_t, double>> ranking;

    for (const auto* mat : matches) {
        if (mat->get_cost_per_kg() > 0 && mat->get_yield_strength() > 0) {
            double cost_performance = mat->get_yield_strength() / mat->get_cost_per_kg();
            ranking.push_back({mat->get_id(), cost_performance});
        }
    }

    std::sort(ranking.rbegin(), ranking.rend(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    return ranking;
}

std::vector<std::pair<int32_t, double>> MaterialLibrary::rank_by_strength_to_weight(
    const MaterialSearchCriteria& criteria) const {

    auto matches = search(criteria);
    std::vector<std::pair<int32_t, double>> ranking;

    for (const auto* mat : matches) {
        if (mat->get_density() > 0 && mat->get_yield_strength() > 0) {
            double strength_to_weight = mat->get_yield_strength() / mat->get_density();
            ranking.push_back({mat->get_id(), strength_to_weight});
        }
    }

    std::sort(ranking.rbegin(), ranking.rend(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    return ranking;
}

std::vector<std::pair<int32_t, double>> MaterialLibrary::rank_by_stiffness_to_weight(
    const MaterialSearchCriteria& criteria) const {

    auto matches = search(criteria);
    std::vector<std::pair<int32_t, double>> ranking;

    for (const auto* mat : matches) {
        if (mat->get_density() > 0 && mat->get_youngs_modulus() > 0) {
            double stiffness_to_weight = mat->get_youngs_modulus() / mat->get_density();
            ranking.push_back({mat->get_id(), stiffness_to_weight});
        }
    }

    std::sort(ranking.rbegin(), ranking.rend(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    return ranking;
}

int32_t MaterialLibrary::create_custom_material(const std::string& name, MaterialCategory category) {
    int32_t id = next_custom_id_++;
    auto mat = std::make_unique<Material>(id, name, category);
    custom_material_ids_.insert(id);
    add_material(std::move(mat));
    return id;
}

bool MaterialLibrary::delete_custom_material(int32_t id) {
    if (custom_material_ids_.find(id) == custom_material_ids_.end()) {
        return false; // Not a custom material
    }

    auto it = std::find_if(materials_.begin(), materials_.end(),
        [id](const auto& m) { return m->get_id() == id; });

    if (it != materials_.end()) {
        materials_by_id_.erase(id);
        custom_material_ids_.erase(id);
        materials_.erase(it);
        return true;
    }
    return false;
}

bool MaterialLibrary::is_custom_material(int32_t id) const {
    return custom_material_ids_.find(id) != custom_material_ids_.end();
}

void MaterialLibrary::clear() {
    materials_.clear();
    materials_by_id_.clear();
    custom_material_ids_.clear();
    next_custom_id_ = 10000;
}

// Factory methods for 50+ predefined materials

std::unique_ptr<Material> MaterialLibrary::create_mild_steel() {
    auto mat = std::make_unique<Material>(1, "Mild Steel", MaterialCategory::Steel);
    mat->set_youngs_modulus(200e9);
    mat->set_poissons_ratio(0.3);
    mat->set_shear_modulus(80e9);
    mat->set_density(7850.0);
    mat->set_yield_strength(250e6);
    mat->set_tensile_strength(400e6);
    mat->set_elongation_at_break(0.25);
    mat->set_thermal_conductivity(50.0);
    mat->set_specific_heat(500.0);
    mat->set_thermal_expansion(12e-6);
    mat->set_melting_point(1540.0);
    mat->set_cost_per_kg(0.5);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 75.0;
    mfg.weldability_rating = 90.0;
    mfg.formability_rating = 80.0;
    mfg.heat_treatability_rating = 70.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 2.0;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 20.0;
    mat->set_sustainability_properties(sust);

    mat->set_description("General-purpose structural steel for construction");
    mat->set_application_notes("Good weldability, suitable for structural applications");

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_structural_steel_s235() {
    auto mat = std::make_unique<Material>(2, "Structural Steel S235", MaterialCategory::Steel);
    mat->set_youngs_modulus(210e9);
    mat->set_poissons_ratio(0.3);
    mat->set_shear_modulus(81e9);
    mat->set_density(7850.0);
    mat->set_yield_strength(235e6);
    mat->set_tensile_strength(360e6);
    mat->set_elongation_at_break(0.26);
    mat->set_thermal_conductivity(50.0);
    mat->set_specific_heat(500.0);
    mat->set_thermal_expansion(12e-6);
    mat->set_melting_point(1540.0);
    mat->set_cost_per_kg(0.48);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 70.0;
    mfg.weldability_rating = 95.0;
    mfg.formability_rating = 85.0;
    mfg.heat_treatability_rating = 65.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 1.95;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 25.0;
    mat->set_sustainability_properties(sust);

    mat->set_description("European standard structural steel");

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_structural_steel_s355() {
    auto mat = std::make_unique<Material>(3, "Structural Steel S355", MaterialCategory::Steel);
    mat->set_youngs_modulus(210e9);
    mat->set_poissons_ratio(0.3);
    mat->set_shear_modulus(81e9);
    mat->set_density(7850.0);
    mat->set_yield_strength(355e6);
    mat->set_tensile_strength(510e6);
    mat->set_elongation_at_break(0.22);
    mat->set_thermal_conductivity(50.0);
    mat->set_specific_heat(500.0);
    mat->set_thermal_expansion(12e-6);
    mat->set_melting_point(1540.0);
    mat->set_cost_per_kg(0.55);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 65.0;
    mfg.weldability_rating = 90.0;
    mfg.formability_rating = 75.0;
    mfg.heat_treatability_rating = 70.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 2.05;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 20.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_carbon_steel_ck45() {
    auto mat = std::make_unique<Material>(4, "Carbon Steel CK45", MaterialCategory::Steel);
    mat->set_youngs_modulus(210e9);
    mat->set_poissons_ratio(0.3);
    mat->set_shear_modulus(81e9);
    mat->set_density(7850.0);
    mat->set_yield_strength(395e6);
    mat->set_tensile_strength(630e6);
    mat->set_elongation_at_break(0.16);
    mat->set_thermal_conductivity(51.0);
    mat->set_specific_heat(490.0);
    mat->set_thermal_expansion(12e-6);
    mat->set_melting_point(1540.0);
    mat->set_cost_per_kg(0.65);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 60.0;
    mfg.weldability_rating = 70.0;
    mfg.formability_rating = 60.0;
    mfg.heat_treatability_rating = 85.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 2.1;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 15.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_stainless_steel_304() {
    auto mat = std::make_unique<Material>(5, "Stainless Steel 304", MaterialCategory::Steel);
    mat->set_youngs_modulus(193e9);
    mat->set_poissons_ratio(0.30);
    mat->set_shear_modulus(77e9);
    mat->set_density(8000.0);
    mat->set_yield_strength(210e6);
    mat->set_tensile_strength(515e6);
    mat->set_elongation_at_break(0.70);
    mat->set_thermal_conductivity(16.3);
    mat->set_specific_heat(500.0);
    mat->set_thermal_expansion(16e-6);
    mat->set_melting_point(1454.0);
    mat->set_cost_per_kg(5.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 35.0;
    mfg.weldability_rating = 80.0;
    mfg.formability_rating = 85.0;
    mfg.heat_treatability_rating = 10.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 6.5;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 30.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_stainless_steel_316() {
    auto mat = std::make_unique<Material>(6, "Stainless Steel 316", MaterialCategory::Steel);
    mat->set_youngs_modulus(193e9);
    mat->set_poissons_ratio(0.30);
    mat->set_shear_modulus(77e9);
    mat->set_density(8000.0);
    mat->set_yield_strength(170e6);
    mat->set_tensile_strength(485e6);
    mat->set_elongation_at_break(0.80);
    mat->set_thermal_conductivity(16.3);
    mat->set_specific_heat(500.0);
    mat->set_thermal_expansion(16e-6);
    mat->set_melting_point(1425.0);
    mat->set_cost_per_kg(6.5);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 30.0;
    mfg.weldability_rating = 80.0;
    mfg.formability_rating = 85.0;
    mfg.heat_treatability_rating = 10.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 7.2;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 25.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_stainless_steel_430() {
    auto mat = std::make_unique<Material>(7, "Stainless Steel 430", MaterialCategory::Steel);
    mat->set_youngs_modulus(200e9);
    mat->set_poissons_ratio(0.28);
    mat->set_shear_modulus(77e9);
    mat->set_density(7700.0);
    mat->set_yield_strength(170e6);
    mat->set_tensile_strength(310e6);
    mat->set_elongation_at_break(0.22);
    mat->set_thermal_conductivity(26.0);
    mat->set_specific_heat(500.0);
    mat->set_thermal_expansion(10e-6);
    mat->set_melting_point(1500.0);
    mat->set_cost_per_kg(3.5);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 55.0;
    mfg.weldability_rating = 50.0;
    mfg.formability_rating = 60.0;
    mfg.heat_treatability_rating = 20.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 5.8;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "low";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 40.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_tool_steel_a2() {
    auto mat = std::make_unique<Material>(8, "Tool Steel A2", MaterialCategory::Steel);
    mat->set_youngs_modulus(210e9);
    mat->set_poissons_ratio(0.3);
    mat->set_shear_modulus(81e9);
    mat->set_density(7860.0);
    mat->set_yield_strength(1700e6);
    mat->set_tensile_strength(1900e6);
    mat->set_elongation_at_break(0.05);
    mat->set_thermal_conductivity(33.0);
    mat->set_specific_heat(460.0);
    mat->set_thermal_expansion(11.4e-6);
    mat->set_melting_point(1450.0);
    mat->set_cost_per_kg(8.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 45.0;
    mfg.weldability_rating = 40.0;
    mfg.formability_rating = 20.0;
    mfg.heat_treatability_rating = 95.0;
    mfg.primary_joining_method = "none";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 3.5;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 10.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_high_speed_steel_m2() {
    auto mat = std::make_unique<Material>(9, "High Speed Steel M2", MaterialCategory::Steel);
    mat->set_youngs_modulus(210e9);
    mat->set_poissons_ratio(0.3);
    mat->set_shear_modulus(81e9);
    mat->set_density(8800.0);
    mat->set_yield_strength(1230e6);
    mat->set_tensile_strength(1800e6);
    mat->set_thermal_conductivity(25.0);
    mat->set_specific_heat(460.0);
    mat->set_thermal_expansion(11e-6);
    mat->set_melting_point(1525.0);
    mat->set_cost_per_kg(12.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 40.0;
    mfg.weldability_rating = 20.0;
    mfg.formability_rating = 10.0;
    mfg.heat_treatability_rating = 98.0;
    mfg.primary_joining_method = "none";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 4.2;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 5.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_aluminum_1100() {
    auto mat = std::make_unique<Material>(10, "Aluminum 1100", MaterialCategory::Aluminum);
    mat->set_youngs_modulus(69e9);
    mat->set_poissons_ratio(0.33);
    mat->set_shear_modulus(26e9);
    mat->set_density(2710.0);
    mat->set_yield_strength(35e6);
    mat->set_tensile_strength(90e6);
    mat->set_elongation_at_break(0.40);
    mat->set_thermal_conductivity(222.0);
    mat->set_specific_heat(896.0);
    mat->set_thermal_expansion(23.1e-6);
    mat->set_melting_point(933.0);
    mat->set_cost_per_kg(2.5);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 70.0;
    mfg.weldability_rating = 95.0;
    mfg.formability_rating = 100.0;
    mfg.heat_treatability_rating = 20.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 12.0;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 35.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_aluminum_2024() {
    auto mat = std::make_unique<Material>(11, "Aluminum 2024", MaterialCategory::Aluminum);
    mat->set_youngs_modulus(73e9);
    mat->set_poissons_ratio(0.33);
    mat->set_shear_modulus(28e9);
    mat->set_density(2780.0);
    mat->set_yield_strength(276e6);
    mat->set_tensile_strength(428e6);
    mat->set_elongation_at_break(0.19);
    mat->set_thermal_conductivity(121.0);
    mat->set_specific_heat(875.0);
    mat->set_thermal_expansion(23.6e-6);
    mat->set_melting_point(935.0);
    mat->set_cost_per_kg(3.8);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 70.0;
    mfg.weldability_rating = 30.0;
    mfg.formability_rating = 50.0;
    mfg.heat_treatability_rating = 95.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 13.5;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 25.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_aluminum_5083() {
    auto mat = std::make_unique<Material>(12, "Aluminum 5083", MaterialCategory::Aluminum);
    mat->set_youngs_modulus(70e9);
    mat->set_poissons_ratio(0.33);
    mat->set_shear_modulus(27e9);
    mat->set_density(2650.0);
    mat->set_yield_strength(215e6);
    mat->set_tensile_strength(305e6);
    mat->set_elongation_at_break(0.22);
    mat->set_thermal_conductivity(121.0);
    mat->set_specific_heat(897.0);
    mat->set_thermal_expansion(23.7e-6);
    mat->set_melting_point(933.0);
    mat->set_cost_per_kg(2.8);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 65.0;
    mfg.weldability_rating = 90.0;
    mfg.formability_rating = 95.0;
    mfg.heat_treatability_rating = 25.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 11.8;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 40.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_aluminum_6061() {
    auto mat = std::make_unique<Material>(13, "Aluminum 6061", MaterialCategory::Aluminum);
    mat->set_youngs_modulus(69e9);
    mat->set_poissons_ratio(0.33);
    mat->set_shear_modulus(26e9);
    mat->set_density(2700.0);
    mat->set_yield_strength(275e6);
    mat->set_tensile_strength(310e6);
    mat->set_elongation_at_break(0.17);
    mat->set_thermal_conductivity(167.0);
    mat->set_specific_heat(896.0);
    mat->set_thermal_expansion(23.6e-6);
    mat->set_melting_point(933.0);
    mat->set_cost_per_kg(3.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 80.0;
    mfg.weldability_rating = 85.0;
    mfg.formability_rating = 80.0;
    mfg.heat_treatability_rating = 90.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 12.1;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 35.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_aluminum_7075() {
    auto mat = std::make_unique<Material>(14, "Aluminum 7075", MaterialCategory::Aluminum);
    mat->set_youngs_modulus(72e9);
    mat->set_poissons_ratio(0.33);
    mat->set_shear_modulus(27e9);
    mat->set_density(2810.0);
    mat->set_yield_strength(505e6);
    mat->set_tensile_strength(570e6);
    mat->set_elongation_at_break(0.11);
    mat->set_thermal_conductivity(130.0);
    mat->set_specific_heat(960.0);
    mat->set_thermal_expansion(23.4e-6);
    mat->set_melting_point(943.0);
    mat->set_cost_per_kg(5.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 60.0;
    mfg.weldability_rating = 20.0;
    mfg.formability_rating = 35.0;
    mfg.heat_treatability_rating = 95.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 13.8;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 20.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_aluminum_7178() {
    auto mat = std::make_unique<Material>(15, "Aluminum 7178", MaterialCategory::Aluminum);
    mat->set_youngs_modulus(72e9);
    mat->set_poissons_ratio(0.33);
    mat->set_shear_modulus(27e9);
    mat->set_density(2870.0);
    mat->set_yield_strength(570e6);
    mat->set_tensile_strength(620e6);
    mat->set_elongation_at_break(0.10);
    mat->set_thermal_conductivity(121.0);
    mat->set_specific_heat(960.0);
    mat->set_thermal_expansion(23.2e-6);
    mat->set_melting_point(943.0);
    mat->set_cost_per_kg(6.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 55.0;
    mfg.weldability_rating = 15.0;
    mfg.formability_rating = 30.0;
    mfg.heat_treatability_rating = 98.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 14.2;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 15.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_aluminum_3003() {
    auto mat = std::make_unique<Material>(16, "Aluminum 3003", MaterialCategory::Aluminum);
    mat->set_youngs_modulus(69e9);
    mat->set_poissons_ratio(0.33);
    mat->set_shear_modulus(26e9);
    mat->set_density(2730.0);
    mat->set_yield_strength(170e6);
    mat->set_tensile_strength(240e6);
    mat->set_elongation_at_break(0.30);
    mat->set_thermal_conductivity(200.0);
    mat->set_specific_heat(900.0);
    mat->set_thermal_expansion(23.8e-6);
    mat->set_melting_point(933.0);
    mat->set_cost_per_kg(2.6);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 70.0;
    mfg.weldability_rating = 90.0;
    mfg.formability_rating = 98.0;
    mfg.heat_treatability_rating = 30.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 11.9;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 45.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_aluminum_5052() {
    auto mat = std::make_unique<Material>(17, "Aluminum 5052", MaterialCategory::Aluminum);
    mat->set_youngs_modulus(70e9);
    mat->set_poissons_ratio(0.33);
    mat->set_shear_modulus(27e9);
    mat->set_density(2680.0);
    mat->set_yield_strength(193e6);
    mat->set_tensile_strength(228e6);
    mat->set_elongation_at_break(0.25);
    mat->set_thermal_conductivity(138.0);
    mat->set_specific_heat(895.0);
    mat->set_thermal_expansion(23.7e-6);
    mat->set_melting_point(934.0);
    mat->set_cost_per_kg(2.7);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 65.0;
    mfg.weldability_rating = 90.0;
    mfg.formability_rating = 95.0;
    mfg.heat_treatability_rating = 25.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 11.7;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 42.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_copper() {
    auto mat = std::make_unique<Material>(18, "Copper", MaterialCategory::Copper);
    mat->set_youngs_modulus(128e9);
    mat->set_poissons_ratio(0.34);
    mat->set_shear_modulus(48e9);
    mat->set_density(8960.0);
    mat->set_yield_strength(70e6);
    mat->set_tensile_strength(220e6);
    mat->set_elongation_at_break(0.45);
    mat->set_thermal_conductivity(398.0);
    mat->set_specific_heat(385.0);
    mat->set_thermal_expansion(16.5e-6);
    mat->set_melting_point(1358.0);
    mat->set_cost_per_kg(7.5);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 60.0;
    mfg.weldability_rating = 60.0;
    mfg.formability_rating = 95.0;
    mfg.heat_treatability_rating = 40.0;
    mfg.primary_joining_method = "brazing";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 3.5;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "low";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 50.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_copper_beryllium() {
    auto mat = std::make_unique<Material>(19, "Copper Beryllium", MaterialCategory::Copper);
    mat->set_youngs_modulus(131e9);
    mat->set_poissons_ratio(0.34);
    mat->set_shear_modulus(50e9);
    mat->set_density(8250.0);
    mat->set_yield_strength(1380e6);
    mat->set_tensile_strength(1550e6);
    mat->set_thermal_conductivity(110.0);
    mat->set_specific_heat(415.0);
    mat->set_thermal_expansion(11.8e-6);
    mat->set_melting_point(1350.0);
    mat->set_cost_per_kg(120.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 50.0;
    mfg.weldability_rating = 30.0;
    mfg.formability_rating = 40.0;
    mfg.heat_treatability_rating = 90.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 15.0;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 30.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_brass_c360() {
    auto mat = std::make_unique<Material>(20, "Brass C360", MaterialCategory::Copper);
    mat->set_youngs_modulus(97e9);
    mat->set_poissons_ratio(0.34);
    mat->set_shear_modulus(37e9);
    mat->set_density(8440.0);
    mat->set_yield_strength(200e6);
    mat->set_tensile_strength(350e6);
    mat->set_elongation_at_break(0.55);
    mat->set_thermal_conductivity(120.0);
    mat->set_specific_heat(380.0);
    mat->set_thermal_expansion(20.3e-6);
    mat->set_melting_point(1170.0);
    mat->set_cost_per_kg(8.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 85.0;
    mfg.weldability_rating = 50.0;
    mfg.formability_rating = 70.0;
    mfg.heat_treatability_rating = 50.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 4.8;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "low";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 55.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_bronze_tin() {
    auto mat = std::make_unique<Material>(21, "Bronze (Tin)", MaterialCategory::Copper);
    mat->set_youngs_modulus(116e9);
    mat->set_poissons_ratio(0.34);
    mat->set_shear_modulus(44e9);
    mat->set_density(8750.0);
    mat->set_yield_strength(300e6);
    mat->set_tensile_strength(350e6);
    mat->set_elongation_at_break(0.25);
    mat->set_thermal_conductivity(60.0);
    mat->set_specific_heat(380.0);
    mat->set_thermal_expansion(17.0e-6);
    mat->set_melting_point(1140.0);
    mat->set_cost_per_kg(9.5);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 55.0;
    mfg.weldability_rating = 35.0;
    mfg.formability_rating = 45.0;
    mfg.heat_treatability_rating = 60.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 5.2;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "low";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 50.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_aluminum_bronze() {
    auto mat = std::make_unique<Material>(22, "Aluminum Bronze", MaterialCategory::Copper);
    mat->set_youngs_modulus(103e9);
    mat->set_poissons_ratio(0.33);
    mat->set_shear_modulus(39e9);
    mat->set_density(7700.0);
    mat->set_yield_strength(380e6);
    mat->set_tensile_strength(580e6);
    mat->set_elongation_at_break(0.15);
    mat->set_thermal_conductivity(60.0);
    mat->set_specific_heat(420.0);
    mat->set_thermal_expansion(17e-6);
    mat->set_melting_point(1050.0);
    mat->set_cost_per_kg(10.5);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 50.0;
    mfg.weldability_rating = 55.0;
    mfg.formability_rating = 40.0;
    mfg.heat_treatability_rating = 85.0;
    mfg.primary_joining_method = "brazing";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 5.8;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "low";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 45.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_titanium_grade_2() {
    auto mat = std::make_unique<Material>(23, "Titanium Grade 2", MaterialCategory::Titanium);
    mat->set_youngs_modulus(102e9);
    mat->set_poissons_ratio(0.3);
    mat->set_shear_modulus(40e9);
    mat->set_density(4506.0);
    mat->set_yield_strength(275e6);
    mat->set_tensile_strength(345e6);
    mat->set_elongation_at_break(0.24);
    mat->set_thermal_conductivity(21.9);
    mat->set_specific_heat(523.0);
    mat->set_thermal_expansion(8.6e-6);
    mat->set_melting_point(1941.0);
    mat->set_cost_per_kg(13.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 35.0;
    mfg.weldability_rating = 90.0;
    mfg.formability_rating = 75.0;
    mfg.heat_treatability_rating = 50.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 18.0;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 20.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_titanium_grade_5_ti6al4v() {
    auto mat = std::make_unique<Material>(24, "Titanium Grade 5 (Ti6Al4V)", MaterialCategory::Titanium);
    mat->set_youngs_modulus(103e9);
    mat->set_poissons_ratio(0.342);
    mat->set_shear_modulus(41e9);
    mat->set_density(4430.0);
    mat->set_yield_strength(880e6);
    mat->set_tensile_strength(950e6);
    mat->set_elongation_at_break(0.14);
    mat->set_thermal_conductivity(7.4);
    mat->set_specific_heat(560.0);
    mat->set_thermal_expansion(8.6e-6);
    mat->set_melting_point(1925.0);
    mat->set_cost_per_kg(15.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 30.0;
    mfg.weldability_rating = 80.0;
    mfg.formability_rating = 55.0;
    mfg.heat_treatability_rating = 85.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 19.5;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 15.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_titanium_grade_9_ti3al2_5v() {
    auto mat = std::make_unique<Material>(25, "Titanium Grade 9 (Ti3Al2.5V)", MaterialCategory::Titanium);
    mat->set_youngs_modulus(96e9);
    mat->set_poissons_ratio(0.32);
    mat->set_shear_modulus(37e9);
    mat->set_density(4810.0);
    mat->set_yield_strength(450e6);
    mat->set_tensile_strength(550e6);
    mat->set_thermal_conductivity(11.0);
    mat->set_specific_heat(544.0);
    mat->set_thermal_expansion(8.4e-6);
    mat->set_melting_point(1815.0);
    mat->set_cost_per_kg(14.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 40.0;
    mfg.weldability_rating = 85.0;
    mfg.formability_rating = 70.0;
    mfg.heat_treatability_rating = 75.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 18.5;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 22.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_nickel() {
    auto mat = std::make_unique<Material>(26, "Nickel", MaterialCategory::Nickel);
    mat->set_youngs_modulus(207e9);
    mat->set_poissons_ratio(0.31);
    mat->set_shear_modulus(79e9);
    mat->set_density(8900.0);
    mat->set_yield_strength(170e6);
    mat->set_tensile_strength(330e6);
    mat->set_elongation_at_break(0.30);
    mat->set_thermal_conductivity(91.0);
    mat->set_specific_heat(440.0);
    mat->set_thermal_expansion(13.3e-6);
    mat->set_melting_point(1728.0);
    mat->set_cost_per_kg(20.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 40.0;
    mfg.weldability_rating = 50.0;
    mfg.formability_rating = 70.0;
    mfg.heat_treatability_rating = 60.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 6.5;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 35.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_inconel_625() {
    auto mat = std::make_unique<Material>(27, "Inconel 625", MaterialCategory::Nickel);
    mat->set_youngs_modulus(207e9);
    mat->set_poissons_ratio(0.28);
    mat->set_shear_modulus(81e9);
    mat->set_density(8440.0);
    mat->set_yield_strength(413e6);
    mat->set_tensile_strength(1004e6);
    mat->set_thermal_conductivity(9.8);
    mat->set_specific_heat(410.0);
    mat->set_thermal_expansion(12.3e-6);
    mat->set_melting_point(1538.0);
    mat->set_cost_per_kg(45.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 25.0;
    mfg.weldability_rating = 90.0;
    mfg.formability_rating = 65.0;
    mfg.heat_treatability_rating = 85.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 25.0;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 20.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_inconel_718() {
    auto mat = std::make_unique<Material>(28, "Inconel 718", MaterialCategory::Nickel);
    mat->set_youngs_modulus(204e9);
    mat->set_poissons_ratio(0.28);
    mat->set_shear_modulus(80e9);
    mat->set_density(8190.0);
    mat->set_yield_strength(1172e6);
    mat->set_tensile_strength(1379e6);
    mat->set_thermal_conductivity(11.4);
    mat->set_specific_heat(435.0);
    mat->set_thermal_expansion(11.3e-6);
    mat->set_melting_point(1533.0);
    mat->set_cost_per_kg(40.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 20.0;
    mfg.weldability_rating = 85.0;
    mfg.formability_rating = 45.0;
    mfg.heat_treatability_rating = 98.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 26.5;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 18.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_monel_400() {
    auto mat = std::make_unique<Material>(29, "Monel 400", MaterialCategory::Nickel);
    mat->set_youngs_modulus(179e9);
    mat->set_poissons_ratio(0.32);
    mat->set_shear_modulus(67e9);
    mat->set_density(8800.0);
    mat->set_yield_strength(276e6);
    mat->set_tensile_strength(483e6);
    mat->set_thermal_conductivity(21.8);
    mat->set_specific_heat(427.0);
    mat->set_thermal_expansion(13.3e-6);
    mat->set_melting_point(1610.0);
    mat->set_cost_per_kg(35.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 30.0;
    mfg.weldability_rating = 85.0;
    mfg.formability_rating = 75.0;
    mfg.heat_treatability_rating = 70.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 22.0;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 25.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_hastelloy_c276() {
    auto mat = std::make_unique<Material>(30, "Hastelloy C276", MaterialCategory::Nickel);
    mat->set_youngs_modulus(210e9);
    mat->set_poissons_ratio(0.27);
    mat->set_shear_modulus(83e9);
    mat->set_density(8890.0);
    mat->set_yield_strength(450e6);
    mat->set_tensile_strength(795e6);
    mat->set_thermal_conductivity(9.3);
    mat->set_specific_heat(395.0);
    mat->set_thermal_expansion(12.6e-6);
    mat->set_melting_point(1520.0);
    mat->set_cost_per_kg(65.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 20.0;
    mfg.weldability_rating = 85.0;
    mfg.formability_rating = 50.0;
    mfg.heat_treatability_rating = 90.0;
    mfg.primary_joining_method = "welding";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 28.5;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 15.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_magnesium_az91d() {
    auto mat = std::make_unique<Material>(31, "Magnesium AZ91D", MaterialCategory::Magnesium);
    mat->set_youngs_modulus(41e9);
    mat->set_poissons_ratio(0.35);
    mat->set_shear_modulus(15e9);
    mat->set_density(1810.0);
    mat->set_yield_strength(105e6);
    mat->set_tensile_strength(230e6);
    mat->set_elongation_at_break(0.03);
    mat->set_thermal_conductivity(72.0);
    mat->set_specific_heat(1050.0);
    mat->set_thermal_expansion(26e-6);
    mat->set_melting_point(893.0);
    mat->set_cost_per_kg(4.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 65.0;
    mfg.weldability_rating = 50.0;
    mfg.formability_rating = 30.0;
    mfg.heat_treatability_rating = 80.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 8.5;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 30.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_magnesium_am60b() {
    auto mat = std::make_unique<Material>(32, "Magnesium AM60B", MaterialCategory::Magnesium);
    mat->set_youngs_modulus(41e9);
    mat->set_poissons_ratio(0.35);
    mat->set_shear_modulus(15e9);
    mat->set_density(1800.0);
    mat->set_yield_strength(160e6);
    mat->set_tensile_strength(240e6);
    mat->set_elongation_at_break(0.06);
    mat->set_thermal_conductivity(68.0);
    mat->set_specific_heat(1050.0);
    mat->set_thermal_expansion(26e-6);
    mat->set_melting_point(877.0);
    mat->set_cost_per_kg(3.8);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 70.0;
    mfg.weldability_rating = 55.0;
    mfg.formability_rating = 35.0;
    mfg.heat_treatability_rating = 85.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 8.2;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 35.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_carbon_fiber_epoxy() {
    auto mat = std::make_unique<Material>(33, "Carbon Fiber Epoxy", MaterialCategory::Composite);
    mat->set_density(1550.0);
    mat->set_youngs_modulus(140e9);
    mat->set_tensile_strength(1500e6);
    mat->set_thermal_conductivity(4.0);
    mat->set_specific_heat(1200.0);
    mat->set_thermal_expansion(0.5e-6);
    mat->set_melting_point(500.0);
    mat->set_cost_per_kg(25.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 40.0;
    mfg.weldability_rating = 0.0;
    mfg.formability_rating = 30.0;
    mfg.heat_treatability_rating = 20.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 14.5;
    sust.recyclability_percentage = 30.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 5.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_glass_fiber_epoxy() {
    auto mat = std::make_unique<Material>(34, "Glass Fiber Epoxy", MaterialCategory::Composite);
    mat->set_density(1850.0);
    mat->set_youngs_modulus(45e9);
    mat->set_tensile_strength(600e6);
    mat->set_thermal_conductivity(0.25);
    mat->set_specific_heat(1200.0);
    mat->set_thermal_expansion(20e-6);
    mat->set_melting_point(400.0);
    mat->set_cost_per_kg(8.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 50.0;
    mfg.weldability_rating = 0.0;
    mfg.formability_rating = 60.0;
    mfg.heat_treatability_rating = 15.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 7.5;
    sust.recyclability_percentage = 20.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 3.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_aramid_fiber_epoxy() {
    auto mat = std::make_unique<Material>(35, "Aramid Fiber Epoxy", MaterialCategory::Composite);
    mat->set_density(1450.0);
    mat->set_youngs_modulus(76e9);
    mat->set_tensile_strength(1380e6);
    mat->set_thermal_conductivity(0.05);
    mat->set_specific_heat(1200.0);
    mat->set_thermal_expansion(3e-6);
    mat->set_melting_point(480.0);
    mat->set_cost_per_kg(45.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 35.0;
    mfg.weldability_rating = 0.0;
    mfg.formability_rating = 40.0;
    mfg.heat_treatability_rating = 25.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 12.0;
    sust.recyclability_percentage = 25.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 8.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_carbon_fiber_polyester() {
    auto mat = std::make_unique<Material>(36, "Carbon Fiber Polyester", MaterialCategory::Composite);
    mat->set_density(1600.0);
    mat->set_youngs_modulus(110e9);
    mat->set_tensile_strength(1000e6);
    mat->set_thermal_conductivity(3.0);
    mat->set_specific_heat(1100.0);
    mat->set_thermal_expansion(2e-6);
    mat->set_melting_point(350.0);
    mat->set_cost_per_kg(18.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 45.0;
    mfg.weldability_rating = 0.0;
    mfg.formability_rating = 40.0;
    mfg.heat_treatability_rating = 20.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 10.5;
    sust.recyclability_percentage = 25.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 4.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_glass_mat_polyester() {
    auto mat = std::make_unique<Material>(37, "Glass Mat Polyester", MaterialCategory::Composite);
    mat->set_density(1800.0);
    mat->set_youngs_modulus(10e9);
    mat->set_tensile_strength(100e6);
    mat->set_thermal_conductivity(0.2);
    mat->set_specific_heat(1200.0);
    mat->set_thermal_expansion(50e-6);
    mat->set_melting_point(300.0);
    mat->set_cost_per_kg(4.5);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 55.0;
    mfg.weldability_rating = 0.0;
    mfg.formability_rating = 70.0;
    mfg.heat_treatability_rating = 10.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 6.0;
    sust.recyclability_percentage = 15.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = false;
    sust.recycled_content_percentage = 2.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_aluminum_oxide_ceramic() {
    auto mat = std::make_unique<Material>(38, "Aluminum Oxide (Al2O3) Ceramic", MaterialCategory::Ceramic);
    mat->set_density(3950.0);
    mat->set_youngs_modulus(345e9);
    mat->set_tensile_strength(400e6);
    mat->set_thermal_conductivity(30.0);
    mat->set_specific_heat(880.0);
    mat->set_thermal_expansion(8.1e-6);
    mat->set_melting_point(2327.0);
    mat->set_cost_per_kg(5.5);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 20.0;
    mfg.weldability_rating = 0.0;
    mfg.formability_rating = 0.0;
    mfg.heat_treatability_rating = 30.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 8.5;
    sust.recyclability_percentage = 0.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = false;
    sust.recycled_content_percentage = 0.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_silicon_carbide_ceramic() {
    auto mat = std::make_unique<Material>(39, "Silicon Carbide (SiC) Ceramic", MaterialCategory::Ceramic);
    mat->set_density(3210.0);
    mat->set_youngs_modulus(440e9);
    mat->set_tensile_strength(550e6);
    mat->set_thermal_conductivity(120.0);
    mat->set_specific_heat(750.0);
    mat->set_thermal_expansion(3.3e-6);
    mat->set_melting_point(2700.0);
    mat->set_cost_per_kg(12.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 15.0;
    mfg.weldability_rating = 0.0;
    mfg.formability_rating = 0.0;
    mfg.heat_treatability_rating = 40.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 15.0;
    sust.recyclability_percentage = 0.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = false;
    sust.recycled_content_percentage = 0.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_zirconia_ceramic() {
    auto mat = std::make_unique<Material>(40, "Zirconia (ZrO2) Ceramic", MaterialCategory::Ceramic);
    mat->set_density(6100.0);
    mat->set_youngs_modulus(210e9);
    mat->set_tensile_strength(900e6);
    mat->set_thermal_conductivity(2.4);
    mat->set_specific_heat(560.0);
    mat->set_thermal_expansion(10.5e-6);
    mat->set_melting_point(2715.0);
    mat->set_cost_per_kg(25.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 10.0;
    mfg.weldability_rating = 0.0;
    mfg.formability_rating = 0.0;
    mfg.heat_treatability_rating = 35.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 20.0;
    sust.recyclability_percentage = 0.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = false;
    sust.recycled_content_percentage = 0.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_polycarbonate() {
    auto mat = std::make_unique<Material>(41, "Polycarbonate", MaterialCategory::Polymer);
    mat->set_density(1200.0);
    mat->set_youngs_modulus(2.3e9);
    mat->set_tensile_strength(60e6);
    mat->set_thermal_conductivity(0.19);
    mat->set_specific_heat(1200.0);
    mat->set_thermal_expansion(70e-6);
    mat->set_melting_point(503.0);
    mat->set_cost_per_kg(3.5);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 60.0;
    mfg.weldability_rating = 30.0;
    mfg.formability_rating = 80.0;
    mfg.heat_treatability_rating = 50.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 5.0;
    sust.recyclability_percentage = 40.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 10.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_polyetherimide() {
    auto mat = std::make_unique<Material>(42, "Polyetherimide (PEI)", MaterialCategory::Polymer);
    mat->set_density(1270.0);
    mat->set_youngs_modulus(3.6e9);
    mat->set_tensile_strength(110e6);
    mat->set_thermal_conductivity(0.22);
    mat->set_specific_heat(1300.0);
    mat->set_thermal_expansion(47e-6);
    mat->set_melting_point(588.0);
    mat->set_cost_per_kg(8.5);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 65.0;
    mfg.weldability_rating = 25.0;
    mfg.formability_rating = 70.0;
    mfg.heat_treatability_rating = 75.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 6.5;
    sust.recyclability_percentage = 35.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 8.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_peek() {
    auto mat = std::make_unique<Material>(43, "PEEK", MaterialCategory::Polymer);
    mat->set_density(1320.0);
    mat->set_youngs_modulus(3.6e9);
    mat->set_tensile_strength(100e6);
    mat->set_thermal_conductivity(0.25);
    mat->set_specific_heat(1330.0);
    mat->set_thermal_expansion(47e-6);
    mat->set_melting_point(623.0);
    mat->set_cost_per_kg(35.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 70.0;
    mfg.weldability_rating = 20.0;
    mfg.formability_rating = 60.0;
    mfg.heat_treatability_rating = 80.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 8.0;
    sust.recyclability_percentage = 30.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 5.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_acetal_copolymer() {
    auto mat = std::make_unique<Material>(44, "Acetal Copolymer (POM)", MaterialCategory::Polymer);
    mat->set_density(1410.0);
    mat->set_youngs_modulus(2.9e9);
    mat->set_tensile_strength(70e6);
    mat->set_thermal_conductivity(0.23);
    mat->set_specific_heat(1500.0);
    mat->set_thermal_expansion(110e-6);
    mat->set_melting_point(498.0);
    mat->set_cost_per_kg(2.8);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 85.0;
    mfg.weldability_rating = 15.0;
    mfg.formability_rating = 50.0;
    mfg.heat_treatability_rating = 40.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 4.2;
    sust.recyclability_percentage = 50.0;
    sust.environmental_impact = "low";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 15.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_nylon_6() {
    auto mat = std::make_unique<Material>(45, "Nylon 6", MaterialCategory::Polymer);
    mat->set_density(1140.0);
    mat->set_youngs_modulus(2.8e9);
    mat->set_tensile_strength(80e6);
    mat->set_thermal_conductivity(0.25);
    mat->set_specific_heat(1700.0);
    mat->set_thermal_expansion(80e-6);
    mat->set_melting_point(498.0);
    mat->set_cost_per_kg(2.2);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 75.0;
    mfg.weldability_rating = 40.0;
    mfg.formability_rating = 75.0;
    mfg.heat_treatability_rating = 60.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 6.0;
    sust.recyclability_percentage = 55.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 20.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_polyetheretherketone_peek() {
    auto mat = std::make_unique<Material>(46, "Polyetheretherketone (PEEK)", MaterialCategory::Polymer);
    mat->set_density(1320.0);
    mat->set_youngs_modulus(3.6e9);
    mat->set_tensile_strength(100e6);
    mat->set_thermal_conductivity(0.25);
    mat->set_specific_heat(1330.0);
    mat->set_thermal_expansion(47e-6);
    mat->set_melting_point(623.0);
    mat->set_cost_per_kg(35.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 70.0;
    mfg.weldability_rating = 20.0;
    mfg.formability_rating = 60.0;
    mfg.heat_treatability_rating = 80.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 8.0;
    sust.recyclability_percentage = 30.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 5.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_cast_iron_ductile() {
    auto mat = std::make_unique<Material>(47, "Cast Iron (Ductile)", MaterialCategory::Steel);
    mat->set_youngs_modulus(165e9);
    mat->set_poissons_ratio(0.3);
    mat->set_shear_modulus(65e9);
    mat->set_density(7100.0);
    mat->set_yield_strength(300e6);
    mat->set_tensile_strength(400e6);
    mat->set_elongation_at_break(0.07);
    mat->set_thermal_conductivity(38.0);
    mat->set_specific_heat(460.0);
    mat->set_thermal_expansion(11e-6);
    mat->set_melting_point(1560.0);
    mat->set_cost_per_kg(0.8);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 80.0;
    mfg.weldability_rating = 45.0;
    mfg.formability_rating = 20.0;
    mfg.heat_treatability_rating = 75.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 1.8;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 40.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_cast_iron_gray() {
    auto mat = std::make_unique<Material>(48, "Cast Iron (Gray)", MaterialCategory::Steel);
    mat->set_youngs_modulus(110e9);
    mat->set_poissons_ratio(0.3);
    mat->set_shear_modulus(44e9);
    mat->set_density(7250.0);
    mat->set_yield_strength(140e6);
    mat->set_tensile_strength(200e6);
    mat->set_thermal_conductivity(55.0);
    mat->set_specific_heat(450.0);
    mat->set_thermal_expansion(11e-6);
    mat->set_melting_point(1370.0);
    mat->set_cost_per_kg(0.7);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 90.0;
    mfg.weldability_rating = 35.0;
    mfg.formability_rating = 10.0;
    mfg.heat_treatability_rating = 65.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 1.7;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "low";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 50.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_aluminum_composite_sic() {
    auto mat = std::make_unique<Material>(49, "Aluminum Composite (SiC)", MaterialCategory::Composite);
    mat->set_density(2900.0);
    mat->set_youngs_modulus(200e9);
    mat->set_tensile_strength(600e6);
    mat->set_thermal_conductivity(180.0);
    mat->set_specific_heat(800.0);
    mat->set_thermal_expansion(7.5e-6);
    mat->set_melting_point(900.0);
    mat->set_cost_per_kg(15.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 40.0;
    mfg.weldability_rating = 20.0;
    mfg.formability_rating = 25.0;
    mfg.heat_treatability_rating = 60.0;
    mfg.primary_joining_method = "adhesive";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 11.0;
    sust.recyclability_percentage = 50.0;
    sust.environmental_impact = "medium";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 25.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_beryllium_copper_c17200() {
    auto mat = std::make_unique<Material>(50, "Beryllium Copper C17200", MaterialCategory::Copper);
    mat->set_youngs_modulus(131e9);
    mat->set_poissons_ratio(0.34);
    mat->set_shear_modulus(50e9);
    mat->set_density(8250.0);
    mat->set_yield_strength(1280e6);
    mat->set_tensile_strength(1520e6);
    mat->set_thermal_conductivity(110.0);
    mat->set_specific_heat(415.0);
    mat->set_thermal_expansion(11.8e-6);
    mat->set_melting_point(1350.0);
    mat->set_cost_per_kg(125.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 45.0;
    mfg.weldability_rating = 25.0;
    mfg.formability_rating = 35.0;
    mfg.heat_treatability_rating = 95.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 16.0;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 25.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

std::unique_ptr<Material> MaterialLibrary::create_tungsten_heavy_alloy() {
    auto mat = std::make_unique<Material>(51, "Tungsten Heavy Alloy", MaterialCategory::Other);
    mat->set_youngs_modulus(360e9);
    mat->set_poissons_ratio(0.28);
    mat->set_shear_modulus(140e9);
    mat->set_density(17600.0);
    mat->set_yield_strength(1100e6);
    mat->set_tensile_strength(1200e6);
    mat->set_thermal_conductivity(75.0);
    mat->set_specific_heat(200.0);
    mat->set_thermal_expansion(4.5e-6);
    mat->set_melting_point(2970.0);
    mat->set_cost_per_kg(50.0);

    ManufacturingProperties mfg{};
    mfg.machinability_rating = 35.0;
    mfg.weldability_rating = 30.0;
    mfg.formability_rating = 20.0;
    mfg.heat_treatability_rating = 50.0;
    mfg.primary_joining_method = "bolting";
    mat->set_manufacturing_properties(mfg);

    SustainabilityProperties sust{};
    sust.carbon_footprint = 18.0;
    sust.recyclability_percentage = 100.0;
    sust.environmental_impact = "high";
    sust.is_recyclable = true;
    sust.recycled_content_percentage = 10.0;
    mat->set_sustainability_properties(sust);

    return mat;
}

void MaterialLibrary::load_default_materials() {
    add_material(create_mild_steel());
    add_material(create_structural_steel_s235());
    add_material(create_structural_steel_s355());
    add_material(create_carbon_steel_ck45());
    add_material(create_stainless_steel_304());
    add_material(create_stainless_steel_316());
    add_material(create_stainless_steel_430());
    add_material(create_tool_steel_a2());
    add_material(create_high_speed_steel_m2());
    add_material(create_aluminum_1100());
    add_material(create_aluminum_2024());
    add_material(create_aluminum_5083());
    add_material(create_aluminum_6061());
    add_material(create_aluminum_7075());
    add_material(create_aluminum_7178());
    add_material(create_aluminum_3003());
    add_material(create_aluminum_5052());
    add_material(create_copper());
    add_material(create_copper_beryllium());
    add_material(create_brass_c360());
    add_material(create_bronze_tin());
    add_material(create_aluminum_bronze());
    add_material(create_titanium_grade_2());
    add_material(create_titanium_grade_5_ti6al4v());
    add_material(create_titanium_grade_9_ti3al2_5v());
    add_material(create_nickel());
    add_material(create_inconel_625());
    add_material(create_inconel_718());
    add_material(create_monel_400());
    add_material(create_hastelloy_c276());
    add_material(create_magnesium_az91d());
    add_material(create_magnesium_am60b());
    add_material(create_carbon_fiber_epoxy());
    add_material(create_glass_fiber_epoxy());
    add_material(create_aramid_fiber_epoxy());
    add_material(create_carbon_fiber_polyester());
    add_material(create_glass_mat_polyester());
    add_material(create_aluminum_oxide_ceramic());
    add_material(create_silicon_carbide_ceramic());
    add_material(create_zirconia_ceramic());
    add_material(create_polycarbonate());
    add_material(create_polyetherimide());
    add_material(create_peek());
    add_material(create_acetal_copolymer());
    add_material(create_nylon_6());
    add_material(create_polyetheretherketone_peek());
    add_material(create_cast_iron_ductile());
    add_material(create_cast_iron_gray());
    add_material(create_aluminum_composite_sic());
    add_material(create_beryllium_copper_c17200());
    add_material(create_tungsten_heavy_alloy());
}

}  // namespace bendcore::materials
