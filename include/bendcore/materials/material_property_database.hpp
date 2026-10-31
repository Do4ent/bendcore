// Material Property Database - Comprehensive Material Library
// M12 Phase: Material Property Database (GAP-052)
#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cmath>
#include <memory>

namespace bendcore::materials {

enum class MaterialClass {
    Steel,
    Aluminum,
    Copper,
    Stainless,
    Titanium,
    Other
};

// Temperature-dependent property
struct TemperatureDependentProperty {
    double reference_temperature{20.0};         // °C
    double base_value{0.0};
    double temperature_coefficient{0.0};        // Change per °C

    [[nodiscard]] double getValueAtTemperature(double temp_c) const noexcept {
        double delta_t = temp_c - reference_temperature;
        return base_value * (1.0 + temperature_coefficient * delta_t);
    }
};

// Comprehensive material property structure
struct MaterialProperty {
    std::string material_id;
    std::string material_name;
    MaterialClass material_class;
    std::string grade;                          // e.g., "AISI 1045"

    // Mechanical properties
    TemperatureDependentProperty young_modulus; // Pa
    TemperatureDependentProperty yield_strength;// Pa
    TemperatureDependentProperty tensile_strength;// Pa
    TemperatureDependentProperty shear_modulus; // Pa

    // Physical properties
    double density{7850.0};                     // kg/m³
    TemperatureDependentProperty specific_heat;// J/(kg·K)
    TemperatureDependentProperty thermal_conductivity;// W/(m·K)
    TemperatureDependentProperty thermal_expansion;// 1/K

    // Hardness
    double hardness_hv{0.0};                    // Vickers hardness
    double hardness_rb{0.0};                    // Rockwell B

    // Ductility and brittleness
    double elongation_percentage{0.0};          // %
    double reduction_area_percentage{0.0};      // %
    double impact_strength{0.0};                // J

    // Fatigue properties
    double fatigue_limit{0.0};                  // Pa
    double endurance_limit{0.0};                // Pa

    // Cost (relative, 1.0 = reference)
    double relative_cost{1.0};

    // Workability
    double machinability_rating{0.0};           // 0-100
    double weldability_rating{0.0};             // 0-100
    double formability_rating{0.0};             // 0-100

    // Corrosion resistance
    double corrosion_resistance_rating{0.0};    // 0-100

    // Environmental properties
    double recyclability_percentage{0.0};       // %
};

// Material property database class
class MaterialPropertyDatabase {
public:
    MaterialPropertyDatabase() {
        initializeStandardMaterials();
    }

    // Add/update material
    bool addMaterial(const MaterialProperty& material) noexcept;

    // Query materials
    [[nodiscard]] std::optional<MaterialProperty> getMaterial(const std::string& id) const noexcept;

    [[nodiscard]] std::vector<MaterialProperty> getMaterialsByClass(MaterialClass material_class) const noexcept;

    [[nodiscard]] std::vector<MaterialProperty> getMaterialsByName(const std::string& name_pattern) const noexcept;

    // Get all materials
    [[nodiscard]] std::vector<MaterialProperty> getAllMaterials() const noexcept;

    // Material count
    [[nodiscard]] size_t getMaterialCount() const noexcept {
        return materials_.size();
    }

    // Search by property range
    [[nodiscard]] std::vector<MaterialProperty> searchByYieldStrength(
        double min_strength, double max_strength, double temp_c = 20.0) const noexcept;

    [[nodiscard]] std::vector<MaterialProperty> searchByDensity(
        double min_density, double max_density) const noexcept;

    [[nodiscard]] std::vector<MaterialProperty> searchByThermalConductivity(
        double min_conductivity, double max_conductivity, double temp_c = 20.0) const noexcept;

    // Delete material
    bool removeMaterial(const std::string& id) noexcept;

    // Clear database
    void clear() noexcept {
        materials_.clear();
    }

    // Update properties at specific temperature
    [[nodiscard]] MaterialProperty getMaterialAtTemperature(
        const std::string& id, double temp_c) const noexcept;

    // Get comparison of properties
    [[nodiscard]] std::map<std::string, double> compareProperties(
        const std::vector<std::string>& material_ids,
        const std::string& property_name,
        double temp_c = 20.0) const noexcept;

private:
    std::map<std::string, MaterialProperty> materials_;

    void initializeStandardMaterials() noexcept;

    // Helper to match property name
    [[nodiscard]] double getPropertyValue(
        const MaterialProperty& mat,
        const std::string& property_name,
        double temp_c = 20.0) const noexcept;
};

// Factory functions for common materials
namespace factory {
    [[nodiscard]] MaterialProperty createSteel_AISI1045() noexcept;
    [[nodiscard]] MaterialProperty createSteel_Stainless304() noexcept;
    [[nodiscard]] MaterialProperty createAluminum_6061() noexcept;
    [[nodiscard]] MaterialProperty createAluminum_2024() noexcept;
    [[nodiscard]] MaterialProperty createCopper_Pure() noexcept;
    [[nodiscard]] MaterialProperty createTitanium_Grade5() noexcept;
}

}  // namespace bendcore::materials
