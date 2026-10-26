// FEA Material Database - Implementation
// M6 Phase: FEA Integration (GAP-025)
#include "bendcore/fea/material_database.hpp"
#include <algorithm>

namespace bendcore::fea {

Material::Material(int32_t id, const std::string& name, MaterialType type)
    : id_(id), name_(name), type_(type),
      youngs_modulus_(0.0), poissons_ratio_(0.0), shear_modulus_(0.0),
      density_(0.0), thermal_conductivity_(0.0), specific_heat_(0.0),
      thermal_expansion_(0.0), yield_strength_(0.0), tensile_strength_(0.0),
      compressive_strength_(0.0), fatigue_limit_(0.0),
      fatigue_strength_coefficient_(0.0),
      youngs_modulus_x_(0.0), youngs_modulus_y_(0.0), youngs_modulus_z_(0.0),
      shear_modulus_xy_(0.0), shear_modulus_yz_(0.0), shear_modulus_zx_(0.0),
      poissons_ratio_xy_(0.0), poissons_ratio_yz_(0.0), poissons_ratio_zx_(0.0) {}

void Material::add_temperature_dependent_youngs_modulus(double temperature, double value) {
    temperature_dependent_E_.push_back({temperature, value});
    std::sort(temperature_dependent_E_.begin(), temperature_dependent_E_.end(),
        [](const TemperatureDependentProperty& a, const TemperatureDependentProperty& b) {
            return a.temperature < b.temperature;
        });
}

void Material::add_temperature_dependent_thermal_conductivity(double temperature, double value) {
    temperature_dependent_k_.push_back({temperature, value});
    std::sort(temperature_dependent_k_.begin(), temperature_dependent_k_.end(),
        [](const TemperatureDependentProperty& a, const TemperatureDependentProperty& b) {
            return a.temperature < b.temperature;
        });
}

double Material::get_youngs_modulus_at_temperature(double temperature) const {
    if (temperature_dependent_E_.empty()) return youngs_modulus_;

    if (temperature <= temperature_dependent_E_.front().temperature) {
        return temperature_dependent_E_.front().value;
    }
    if (temperature >= temperature_dependent_E_.back().temperature) {
        return temperature_dependent_E_.back().value;
    }

    for (size_t i = 0; i < temperature_dependent_E_.size() - 1; ++i) {
        if (temperature >= temperature_dependent_E_[i].temperature &&
            temperature <= temperature_dependent_E_[i+1].temperature) {
            double t0 = temperature_dependent_E_[i].temperature;
            double t1 = temperature_dependent_E_[i+1].temperature;
            double v0 = temperature_dependent_E_[i].value;
            double v1 = temperature_dependent_E_[i+1].value;

            double alpha = (temperature - t0) / (t1 - t0);
            return v0 + alpha * (v1 - v0);
        }
    }

    return youngs_modulus_;
}

double Material::get_thermal_conductivity_at_temperature(double temperature) const {
    if (temperature_dependent_k_.empty()) return thermal_conductivity_;

    if (temperature <= temperature_dependent_k_.front().temperature) {
        return temperature_dependent_k_.front().value;
    }
    if (temperature >= temperature_dependent_k_.back().temperature) {
        return temperature_dependent_k_.back().value;
    }

    for (size_t i = 0; i < temperature_dependent_k_.size() - 1; ++i) {
        if (temperature >= temperature_dependent_k_[i].temperature &&
            temperature <= temperature_dependent_k_[i+1].temperature) {
            double t0 = temperature_dependent_k_[i].temperature;
            double t1 = temperature_dependent_k_[i+1].temperature;
            double v0 = temperature_dependent_k_[i].value;
            double v1 = temperature_dependent_k_[i+1].value;

            double alpha = (temperature - t0) / (t1 - t0);
            return v0 + alpha * (v1 - v0);
        }
    }

    return thermal_conductivity_;
}

bool Material::is_valid() const {
    if (type_ == MaterialType::Isotropic) {
        return youngs_modulus_ > 0.0 && poissons_ratio_ >= -1.0 && poissons_ratio_ <= 0.5;
    }
    return true;
}

MaterialDatabase::MaterialDatabase() = default;
MaterialDatabase::~MaterialDatabase() = default;

void MaterialDatabase::add_material(std::unique_ptr<Material> material) {
    int32_t id = material->get_id();
    materials_by_id_[id] = material.get();
    materials_.push_back(std::move(material));
}

Material* MaterialDatabase::get_material(int32_t id) {
    auto it = materials_by_id_.find(id);
    if (it != materials_by_id_.end()) return it->second;
    return nullptr;
}

const Material* MaterialDatabase::get_material(int32_t id) const {
    auto it = materials_by_id_.find(id);
    if (it != materials_by_id_.end()) return it->second;
    return nullptr;
}

Material* MaterialDatabase::get_material_by_name(const std::string& name) {
    for (auto& mat : materials_) {
        if (mat->get_name() == name) return mat.get();
    }
    return nullptr;
}

const Material* MaterialDatabase::get_material_by_name(const std::string& name) const {
    for (const auto& mat : materials_) {
        if (mat->get_name() == name) return mat.get();
    }
    return nullptr;
}

void MaterialDatabase::clear() {
    materials_.clear();
    materials_by_id_.clear();
}

void MaterialDatabase::load_default_materials() {
    add_material(create_mild_steel());
    add_material(create_stainless_steel_304());
    add_material(create_aluminum_6061());
    add_material(create_copper());
}

std::unique_ptr<Material> MaterialDatabase::create_mild_steel() {
    auto mat = std::make_unique<Material>(1, "Mild Steel", MaterialType::Isotropic);
    mat->set_youngs_modulus(200e9);
    mat->set_poissons_ratio(0.3);
    mat->set_density(7850.0);
    mat->set_yield_strength(250e6);
    mat->set_tensile_strength(400e6);
    mat->set_thermal_conductivity(50.0);
    mat->set_specific_heat(500.0);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_stainless_steel_304() {
    auto mat = std::make_unique<Material>(2, "Stainless Steel 304", MaterialType::Isotropic);
    mat->set_youngs_modulus(193e9);
    mat->set_poissons_ratio(0.3);
    mat->set_density(8000.0);
    mat->set_yield_strength(210e6);
    mat->set_tensile_strength(515e6);
    mat->set_thermal_conductivity(16.0);
    mat->set_specific_heat(500.0);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_stainless_steel_316() {
    auto mat = std::make_unique<Material>(3, "Stainless Steel 316", MaterialType::Isotropic);
    mat->set_youngs_modulus(193e9);
    mat->set_poissons_ratio(0.3);
    mat->set_density(8000.0);
    mat->set_yield_strength(170e6);
    mat->set_tensile_strength(485e6);
    mat->set_thermal_conductivity(16.0);
    mat->set_specific_heat(500.0);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_high_carbon_steel() {
    auto mat = std::make_unique<Material>(4, "High Carbon Steel", MaterialType::Isotropic);
    mat->set_youngs_modulus(200e9);
    mat->set_poissons_ratio(0.3);
    mat->set_density(7850.0);
    mat->set_yield_strength(700e6);
    mat->set_tensile_strength(1000e6);
    mat->set_thermal_conductivity(50.0);
    mat->set_specific_heat(500.0);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_aluminum_6061() {
    auto mat = std::make_unique<Material>(5, "Aluminum 6061", MaterialType::Isotropic);
    mat->set_youngs_modulus(69e9);
    mat->set_poissons_ratio(0.33);
    mat->set_density(2700.0);
    mat->set_yield_strength(275e6);
    mat->set_tensile_strength(310e6);
    mat->set_thermal_conductivity(167.0);
    mat->set_specific_heat(896.0);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_aluminum_7075() {
    auto mat = std::make_unique<Material>(6, "Aluminum 7075", MaterialType::Isotropic);
    mat->set_youngs_modulus(72e9);
    mat->set_poissons_ratio(0.33);
    mat->set_density(2810.0);
    mat->set_yield_strength(505e6);
    mat->set_tensile_strength(570e6);
    mat->set_thermal_conductivity(130.0);
    mat->set_specific_heat(960.0);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_aluminum_5083() {
    auto mat = std::make_unique<Material>(7, "Aluminum 5083", MaterialType::Isotropic);
    mat->set_youngs_modulus(70e9);
    mat->set_poissons_ratio(0.33);
    mat->set_density(2650.0);
    mat->set_yield_strength(215e6);
    mat->set_tensile_strength(305e6);
    mat->set_thermal_conductivity(121.0);
    mat->set_specific_heat(897.0);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_copper() {
    auto mat = std::make_unique<Material>(8, "Copper", MaterialType::Isotropic);
    mat->set_youngs_modulus(128e9);
    mat->set_poissons_ratio(0.34);
    mat->set_density(8960.0);
    mat->set_yield_strength(70e6);
    mat->set_tensile_strength(220e6);
    mat->set_thermal_conductivity(398.0);
    mat->set_specific_heat(385.0);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_brass() {
    auto mat = std::make_unique<Material>(9, "Brass", MaterialType::Isotropic);
    mat->set_youngs_modulus(97e9);
    mat->set_poissons_ratio(0.34);
    mat->set_density(8440.0);
    mat->set_yield_strength(200e6);
    mat->set_tensile_strength(350e6);
    mat->set_thermal_conductivity(120.0);
    mat->set_specific_heat(380.0);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_bronze() {
    auto mat = std::make_unique<Material>(10, "Bronze", MaterialType::Isotropic);
    mat->set_youngs_modulus(116e9);
    mat->set_poissons_ratio(0.34);
    mat->set_density(8750.0);
    mat->set_yield_strength(300e6);
    mat->set_tensile_strength(350e6);
    mat->set_thermal_conductivity(60.0);
    mat->set_specific_heat(380.0);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_titanium_ti6al4v() {
    auto mat = std::make_unique<Material>(11, "Titanium Ti6Al4V", MaterialType::Isotropic);
    mat->set_youngs_modulus(103e9);
    mat->set_poissons_ratio(0.342);
    mat->set_density(4430.0);
    mat->set_yield_strength(880e6);
    mat->set_tensile_strength(950e6);
    mat->set_thermal_conductivity(7.0);
    mat->set_specific_heat(560.0);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_titanium_pure() {
    auto mat = std::make_unique<Material>(12, "Pure Titanium", MaterialType::Isotropic);
    mat->set_youngs_modulus(102e9);
    mat->set_poissons_ratio(0.3);
    mat->set_density(4506.0);
    mat->set_yield_strength(170e6);
    mat->set_tensile_strength(340e6);
    mat->set_thermal_conductivity(22.0);
    mat->set_specific_heat(523.0);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_carbon_fiber_epoxy() {
    auto mat = std::make_unique<Material>(13, "Carbon Fiber Epoxy", MaterialType::Orthotropic);
    mat->set_density(1550.0);
    mat->set_youngs_modulus_x(140e9);
    mat->set_youngs_modulus_y(10e9);
    mat->set_youngs_modulus_z(10e9);
    mat->set_shear_modulus_xy(7e9);
    mat->set_shear_modulus_yz(7e9);
    mat->set_shear_modulus_zx(7e9);
    return mat;
}

std::unique_ptr<Material> MaterialDatabase::create_glass_fiber_epoxy() {
    auto mat = std::make_unique<Material>(14, "Glass Fiber Epoxy", MaterialType::Orthotropic);
    mat->set_density(1850.0);
    mat->set_youngs_modulus_x(45e9);
    mat->set_youngs_modulus_y(12e9);
    mat->set_youngs_modulus_z(12e9);
    mat->set_shear_modulus_xy(4e9);
    mat->set_shear_modulus_yz(4e9);
    mat->set_shear_modulus_zx(4e9);
    return mat;
}

}  // namespace bendcore::fea
