// FEA Material Database
// M6 Phase: FEA Integration (GAP-025)
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>

namespace bendcore::fea {

// Material property types
enum class MaterialType {
    Isotropic,      // Same properties in all directions
    Orthotropic,    // Different properties in three perpendicular directions
    Anisotropic     // General anisotropic material
};

// Temperature-dependent material property
struct TemperatureDependentProperty {
    double temperature;   // Temperature in Kelvin
    double value;         // Property value at this temperature
};

// Material specification
class Material {
public:
    Material(int32_t id, const std::string& name, MaterialType type);

    // Basic properties
    int32_t get_id() const { return id_; }
    const std::string& get_name() const { return name_; }
    MaterialType get_type() const { return type_; }

    // Isotropic properties
    void set_youngs_modulus(double E) { youngs_modulus_ = E; }
    double get_youngs_modulus() const { return youngs_modulus_; }

    void set_poissons_ratio(double nu) { poissons_ratio_ = nu; }
    double get_poissons_ratio() const { return poissons_ratio_; }

    void set_shear_modulus(double G) { shear_modulus_ = G; }
    double get_shear_modulus() const { return shear_modulus_; }

    // Density
    void set_density(double rho) { density_ = rho; }
    double get_density() const { return density_; }

    // Thermal properties
    void set_thermal_conductivity(double k) { thermal_conductivity_ = k; }
    double get_thermal_conductivity() const { return thermal_conductivity_; }

    void set_specific_heat(double c) { specific_heat_ = c; }
    double get_specific_heat() const { return specific_heat_; }

    void set_thermal_expansion(double alpha) { thermal_expansion_ = alpha; }
    double get_thermal_expansion() const { return thermal_expansion_; }

    // Yield and failure properties
    void set_yield_strength(double sigma_y) { yield_strength_ = sigma_y; }
    double get_yield_strength() const { return yield_strength_; }

    void set_tensile_strength(double sigma_u) { tensile_strength_ = sigma_u; }
    double get_tensile_strength() const { return tensile_strength_; }

    void set_compressive_strength(double sigma_c) { compressive_strength_ = sigma_c; }
    double get_compressive_strength() const { return compressive_strength_; }

    // Fatigue properties
    void set_fatigue_limit(double sigma_f) { fatigue_limit_ = sigma_f; }
    double get_fatigue_limit() const { return fatigue_limit_; }

    void set_fatigue_strength_coefficient(double sigma_f_prime) {
        fatigue_strength_coefficient_ = sigma_f_prime;
    }
    double get_fatigue_strength_coefficient() const { return fatigue_strength_coefficient_; }

    // Orthotropic properties
    void set_youngs_modulus_x(double E_x) { youngs_modulus_x_ = E_x; }
    void set_youngs_modulus_y(double E_y) { youngs_modulus_y_ = E_y; }
    void set_youngs_modulus_z(double E_z) { youngs_modulus_z_ = E_z; }

    double get_youngs_modulus_x() const { return youngs_modulus_x_; }
    double get_youngs_modulus_y() const { return youngs_modulus_y_; }
    double get_youngs_modulus_z() const { return youngs_modulus_z_; }

    void set_shear_modulus_xy(double G_xy) { shear_modulus_xy_ = G_xy; }
    void set_shear_modulus_yz(double G_yz) { shear_modulus_yz_ = G_yz; }
    void set_shear_modulus_zx(double G_zx) { shear_modulus_zx_ = G_zx; }

    double get_shear_modulus_xy() const { return shear_modulus_xy_; }
    double get_shear_modulus_yz() const { return shear_modulus_yz_; }
    double get_shear_modulus_zx() const { return shear_modulus_zx_; }

    // Poisson's ratios
    void set_poissons_ratio_xy(double nu_xy) { poissons_ratio_xy_ = nu_xy; }
    void set_poissons_ratio_yz(double nu_yz) { poissons_ratio_yz_ = nu_yz; }
    void set_poissons_ratio_zx(double nu_zx) { poissons_ratio_zx_ = nu_zx; }

    // Temperature-dependent properties
    void add_temperature_dependent_youngs_modulus(double temperature, double value);
    void add_temperature_dependent_thermal_conductivity(double temperature, double value);
    double get_youngs_modulus_at_temperature(double temperature) const;
    double get_thermal_conductivity_at_temperature(double temperature) const;

    // Validation
    bool is_valid() const;

private:
    int32_t id_;
    std::string name_;
    MaterialType type_;

    // Isotropic elastic properties
    double youngs_modulus_;
    double poissons_ratio_;
    double shear_modulus_;

    // Density
    double density_;

    // Thermal properties
    double thermal_conductivity_;
    double specific_heat_;
    double thermal_expansion_;

    // Strength properties
    double yield_strength_;
    double tensile_strength_;
    double compressive_strength_;
    double fatigue_limit_;
    double fatigue_strength_coefficient_;

    // Orthotropic properties
    double youngs_modulus_x_, youngs_modulus_y_, youngs_modulus_z_;
    double shear_modulus_xy_, shear_modulus_yz_, shear_modulus_zx_;
    double poissons_ratio_xy_, poissons_ratio_yz_, poissons_ratio_zx_;

    // Temperature-dependent properties
    std::vector<TemperatureDependentProperty> temperature_dependent_E_;
    std::vector<TemperatureDependentProperty> temperature_dependent_k_;
};

// Material database
class MaterialDatabase {
public:
    MaterialDatabase();
    ~MaterialDatabase();

    // Add/get materials
    void add_material(std::unique_ptr<Material> material);
    Material* get_material(int32_t id);
    const Material* get_material(int32_t id) const;
    Material* get_material_by_name(const std::string& name);
    const Material* get_material_by_name(const std::string& name) const;

    // List all materials
    const std::vector<std::unique_ptr<Material>>& get_all_materials() const {
        return materials_;
    }

    // Clear database
    void clear();

    // Predefined materials
    void load_default_materials();

    // Steel materials
    static std::unique_ptr<Material> create_mild_steel();
    static std::unique_ptr<Material> create_stainless_steel_304();
    static std::unique_ptr<Material> create_stainless_steel_316();
    static std::unique_ptr<Material> create_high_carbon_steel();

    // Aluminum materials
    static std::unique_ptr<Material> create_aluminum_6061();
    static std::unique_ptr<Material> create_aluminum_7075();
    static std::unique_ptr<Material> create_aluminum_5083();

    // Copper materials
    static std::unique_ptr<Material> create_copper();
    static std::unique_ptr<Material> create_brass();
    static std::unique_ptr<Material> create_bronze();

    // Titanium materials
    static std::unique_ptr<Material> create_titanium_ti6al4v();
    static std::unique_ptr<Material> create_titanium_pure();

    // Composite materials
    static std::unique_ptr<Material> create_carbon_fiber_epoxy();
    static std::unique_ptr<Material> create_glass_fiber_epoxy();

private:
    std::vector<std::unique_ptr<Material>> materials_;
    std::map<int32_t, Material*> materials_by_id_;
};

}  // namespace bendcore::fea
