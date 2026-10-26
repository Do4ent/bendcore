// Material Library - Comprehensive Material Management System
// M8 Phase: Material Library (GAP-033)
#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace bendcore::materials {

// Material categories
enum class MaterialCategory {
    Steel,
    Aluminum,
    Titanium,
    Copper,
    Nickel,
    Magnesium,
    Composite,
    Ceramic,
    Polymer,
    Other
};

// Manufacturing properties
struct ManufacturingProperties {
    double machinability_rating;      // 0-100, higher is easier to machine
    double weldability_rating;        // 0-100
    double formability_rating;        // 0-100
    double heat_treatability_rating;  // 0-100
    std::string primary_joining_method; // "welding", "bolting", "adhesive", etc.
};

// Sustainability properties
struct SustainabilityProperties {
    double carbon_footprint;          // kg CO2-eq per kg
    double recyclability_percentage;  // 0-100
    std::string environmental_impact; // "low", "medium", "high"
    bool is_recyclable;
    double recycled_content_percentage;
};

// Material composition
struct CompositionInfo {
    std::string primary_element;
    std::map<std::string, double> elements; // element name -> percentage
};

// Material grade/variant
struct MaterialGrade {
    std::string name;
    std::string specification;  // e.g., "ASTM A36", "EN 10025-2", etc.
    int32_t id;
    std::map<std::string, double> property_modifications; // property -> modification factor
};

// Temperature range properties
struct TemperatureRangeProperty {
    double min_temperature;
    double max_temperature;
    double value_at_min;
    double value_at_max;
};

// Comprehensive material specification
class Material {
public:
    Material(int32_t id, const std::string& name, MaterialCategory category);

    // Basic properties
    int32_t get_id() const { return id_; }
    const std::string& get_name() const { return name_; }
    MaterialCategory get_category() const { return category_; }
    const std::string& get_category_name() const { return category_name_; }

    // Density and cost
    void set_density(double rho) { density_ = rho; }
    double get_density() const { return density_; }

    void set_cost_per_kg(double cost) { cost_per_kg_ = cost; }
    double get_cost_per_kg() const { return cost_per_kg_; }

    // Mechanical properties (isotropic)
    void set_youngs_modulus(double E) { youngs_modulus_ = E; }
    double get_youngs_modulus() const { return youngs_modulus_; }

    void set_poissons_ratio(double nu) { poissons_ratio_ = nu; }
    double get_poissons_ratio() const { return poissons_ratio_; }

    void set_shear_modulus(double G) { shear_modulus_ = G; }
    double get_shear_modulus() const { return shear_modulus_; }

    void set_yield_strength(double sigma_y) { yield_strength_ = sigma_y; }
    double get_yield_strength() const { return yield_strength_; }

    void set_tensile_strength(double sigma_u) { tensile_strength_ = sigma_u; }
    double get_tensile_strength() const { return tensile_strength_; }

    void set_compressive_strength(double sigma_c) { compressive_strength_ = sigma_c; }
    double get_compressive_strength() const { return compressive_strength_; }

    void set_elongation_at_break(double elongation) { elongation_at_break_ = elongation; }
    double get_elongation_at_break() const { return elongation_at_break_; }

    // Fatigue properties
    void set_fatigue_limit(double sigma_f) { fatigue_limit_ = sigma_f; }
    double get_fatigue_limit() const { return fatigue_limit_; }

    void set_fatigue_strength_coefficient(double sigma_f_prime) {
        fatigue_strength_coefficient_ = sigma_f_prime;
    }
    double get_fatigue_strength_coefficient() const { return fatigue_strength_coefficient_; }

    // Thermal properties
    void set_thermal_conductivity(double k) { thermal_conductivity_ = k; }
    double get_thermal_conductivity() const { return thermal_conductivity_; }

    void set_specific_heat(double c) { specific_heat_ = c; }
    double get_specific_heat() const { return specific_heat_; }

    void set_thermal_expansion(double alpha) { thermal_expansion_ = alpha; }
    double get_thermal_expansion() const { return thermal_expansion_; }

    void set_melting_point(double T_m) { melting_point_ = T_m; }
    double get_melting_point() const { return melting_point_; }

    // Manufacturing properties
    void set_manufacturing_properties(const ManufacturingProperties& props) {
        manufacturing_properties_ = props;
    }
    const ManufacturingProperties& get_manufacturing_properties() const {
        return manufacturing_properties_;
    }

    // Sustainability properties
    void set_sustainability_properties(const SustainabilityProperties& props) {
        sustainability_properties_ = props;
    }
    const SustainabilityProperties& get_sustainability_properties() const {
        return sustainability_properties_;
    }

    // Composition information
    void set_composition(const CompositionInfo& comp) { composition_ = comp; }
    const CompositionInfo& get_composition() const { return composition_; }

    // Temperature-dependent properties
    void add_temperature_dependent_youngs_modulus(double temp, double value);
    void add_temperature_dependent_yield_strength(double temp, double value);
    void add_temperature_dependent_thermal_conductivity(double temp, double value);

    double get_youngs_modulus_at_temperature(double temperature) const;
    double get_yield_strength_at_temperature(double temperature) const;
    double get_thermal_conductivity_at_temperature(double temperature) const;

    // Strain rate effects
    void set_strain_rate_sensitivity(double m) { strain_rate_sensitivity_ = m; }
    double get_strain_rate_sensitivity() const { return strain_rate_sensitivity_; }

    // Material grades
    void add_grade(const MaterialGrade& grade);
    const std::vector<MaterialGrade>& get_grades() const { return grades_; }
    const MaterialGrade* get_grade(const std::string& grade_name) const;

    // Description and notes
    void set_description(const std::string& desc) { description_ = desc; }
    const std::string& get_description() const { return description_; }

    void set_application_notes(const std::string& notes) { application_notes_ = notes; }
    const std::string& get_application_notes() const { return application_notes_; }

    // Validation
    bool is_valid() const;
    std::vector<std::string> validate() const;

private:
    int32_t id_;
    std::string name_;
    std::string category_name_;
    MaterialCategory category_;

    // Density and cost
    double density_;
    double cost_per_kg_;

    // Elastic properties
    double youngs_modulus_;
    double poissons_ratio_;
    double shear_modulus_;

    // Strength properties
    double yield_strength_;
    double tensile_strength_;
    double compressive_strength_;
    double elongation_at_break_;

    // Fatigue properties
    double fatigue_limit_;
    double fatigue_strength_coefficient_;

    // Thermal properties
    double thermal_conductivity_;
    double specific_heat_;
    double thermal_expansion_;
    double melting_point_;

    // Manufacturing and sustainability
    ManufacturingProperties manufacturing_properties_;
    SustainabilityProperties sustainability_properties_;
    CompositionInfo composition_;

    // Temperature-dependent properties
    std::vector<TemperatureRangeProperty> temp_dependent_E_;
    std::vector<TemperatureRangeProperty> temp_dependent_yield_;
    std::vector<TemperatureRangeProperty> temp_dependent_k_;

    // Strain rate sensitivity
    double strain_rate_sensitivity_;

    // Material grades
    std::vector<MaterialGrade> grades_;

    // Description
    std::string description_;
    std::string application_notes_;

    // Helper function to interpolate temperature-dependent properties
    double interpolate_property(const std::vector<TemperatureRangeProperty>& props, double temperature) const;
};

// Search and filtering criteria
struct MaterialSearchCriteria {
    std::optional<MaterialCategory> category;
    std::optional<double> min_youngs_modulus;
    std::optional<double> max_youngs_modulus;
    std::optional<double> min_yield_strength;
    std::optional<double> max_yield_strength;
    std::optional<double> max_density;
    std::optional<double> min_density;
    std::optional<double> max_cost_per_kg;
    std::optional<std::string> name_contains;
    std::optional<double> min_machinability;
    std::optional<double> min_weldability;
    std::optional<double> min_thermal_conductivity;
};

// Material comparison result
struct MaterialComparison {
    const Material* material1;
    const Material* material2;

    double cost_ratio;              // material1_cost / material2_cost
    double strength_ratio;          // material1_yield / material2_yield
    double weight_ratio;            // material1_density / material2_density
    double stiffness_ratio;         // material1_E / material2_E
    double thermal_conductivity_ratio;

    double strength_to_weight;      // yield_strength / density for material1
    double strength_to_weight_2;    // yield_strength / density for material2
    double stiffness_to_weight;     // youngs_modulus / density for material1
    double stiffness_to_weight_2;   // youngs_modulus / density for material2
};

// Comprehensive material database with 50+ materials
class MaterialLibrary {
public:
    MaterialLibrary();
    ~MaterialLibrary();

    // Add/get materials
    void add_material(std::unique_ptr<Material> material);
    Material* get_material(int32_t id);
    const Material* get_material(int32_t id) const;
    Material* get_material_by_name(const std::string& name);
    const Material* get_material_by_name(const std::string& name) const;

    // List operations
    const std::vector<std::unique_ptr<Material>>& get_all_materials() const {
        return materials_;
    }

    std::vector<Material*> get_materials_by_category(MaterialCategory category);
    std::vector<const Material*> get_materials_by_category(MaterialCategory category) const;

    // Search and filtering
    std::vector<Material*> search(const MaterialSearchCriteria& criteria);
    std::vector<const Material*> search(const MaterialSearchCriteria& criteria) const;

    // Material comparison
    MaterialComparison compare(int32_t id1, int32_t id2) const;
    MaterialComparison compare(const Material* mat1, const Material* mat2) const;

    // Cost analysis
    double calculate_total_cost(int32_t material_id, double mass_kg) const;
    std::vector<std::pair<int32_t, double>> rank_by_cost_performance(
        const MaterialSearchCriteria& criteria) const;

    // Performance ranking
    std::vector<std::pair<int32_t, double>> rank_by_strength_to_weight(
        const MaterialSearchCriteria& criteria) const;
    std::vector<std::pair<int32_t, double>> rank_by_stiffness_to_weight(
        const MaterialSearchCriteria& criteria) const;

    // Custom material management
    int32_t create_custom_material(const std::string& name, MaterialCategory category);
    bool delete_custom_material(int32_t id);
    bool is_custom_material(int32_t id) const;

    // Predefined materials (50+ materials)
    void load_default_materials();

    // Clear database
    void clear();
    size_t get_material_count() const { return materials_.size(); }

private:
    std::vector<std::unique_ptr<Material>> materials_;
    std::map<int32_t, Material*> materials_by_id_;
    std::set<int32_t> custom_material_ids_;
    int32_t next_custom_id_;

    // Factory methods for predefined materials
    std::unique_ptr<Material> create_mild_steel();
    std::unique_ptr<Material> create_structural_steel_s235();
    std::unique_ptr<Material> create_structural_steel_s355();
    std::unique_ptr<Material> create_carbon_steel_ck45();
    std::unique_ptr<Material> create_stainless_steel_304();
    std::unique_ptr<Material> create_stainless_steel_316();
    std::unique_ptr<Material> create_stainless_steel_430();
    std::unique_ptr<Material> create_tool_steel_a2();
    std::unique_ptr<Material> create_high_speed_steel_m2();

    std::unique_ptr<Material> create_aluminum_1100();
    std::unique_ptr<Material> create_aluminum_2024();
    std::unique_ptr<Material> create_aluminum_5083();
    std::unique_ptr<Material> create_aluminum_6061();
    std::unique_ptr<Material> create_aluminum_7075();
    std::unique_ptr<Material> create_aluminum_7178();
    std::unique_ptr<Material> create_aluminum_3003();
    std::unique_ptr<Material> create_aluminum_5052();

    std::unique_ptr<Material> create_copper();
    std::unique_ptr<Material> create_copper_beryllium();
    std::unique_ptr<Material> create_brass_c360();
    std::unique_ptr<Material> create_bronze_tin();
    std::unique_ptr<Material> create_aluminum_bronze();

    std::unique_ptr<Material> create_titanium_grade_2();
    std::unique_ptr<Material> create_titanium_grade_5_ti6al4v();
    std::unique_ptr<Material> create_titanium_grade_9_ti3al2_5v();

    std::unique_ptr<Material> create_nickel();
    std::unique_ptr<Material> create_inconel_625();
    std::unique_ptr<Material> create_inconel_718();
    std::unique_ptr<Material> create_monel_400();
    std::unique_ptr<Material> create_hastelloy_c276();

    std::unique_ptr<Material> create_magnesium_az91d();
    std::unique_ptr<Material> create_magnesium_am60b();

    std::unique_ptr<Material> create_carbon_fiber_epoxy();
    std::unique_ptr<Material> create_glass_fiber_epoxy();
    std::unique_ptr<Material> create_aramid_fiber_epoxy();
    std::unique_ptr<Material> create_carbon_fiber_polyester();
    std::unique_ptr<Material> create_glass_mat_polyester();

    std::unique_ptr<Material> create_aluminum_oxide_ceramic();
    std::unique_ptr<Material> create_silicon_carbide_ceramic();
    std::unique_ptr<Material> create_zirconia_ceramic();

    std::unique_ptr<Material> create_polycarbonate();
    std::unique_ptr<Material> create_polyetherimide();
    std::unique_ptr<Material> create_peek();
    std::unique_ptr<Material> create_acetal_copolymer();
    std::unique_ptr<Material> create_nylon_6();
    std::unique_ptr<Material> create_polyetheretherketone_peek();

    std::unique_ptr<Material> create_cast_iron_ductile();
    std::unique_ptr<Material> create_cast_iron_gray();
    std::unique_ptr<Material> create_aluminum_composite_sic();
    std::unique_ptr<Material> create_beryllium_copper_c17200();
    std::unique_ptr<Material> create_tungsten_heavy_alloy();
};

}  // namespace bendcore::materials
