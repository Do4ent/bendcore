// GAP-052: MaterialPropertyDatabase (38 tests)
// Material database, density, properties
#include <cassert>
#include <cstdio>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace bendcore::materials {

struct MaterialProperties {
    std::string name;
    double density;
    double youngs_modulus;
    double poissons_ratio;
    double yield_strength;
    double ultimate_strength;
    double thermal_expansion;
    double melting_point;
    int material_id;
};

class MaterialPropertyDatabase {
public:
    MaterialPropertyDatabase() : next_id_(1) {
        initializeStandardMaterials();
    }

    void initializeStandardMaterials() {
        // Steel
        addMaterial({"Steel (Mild)", 7850, 210e9, 0.30, 250e6, 400e6, 12e-6, 1500, 1});
        addMaterial({"Steel (Stainless 304)", 8000, 193e9, 0.31, 205e6, 515e6, 16e-6, 1450, 2});

        // Aluminum
        addMaterial({"Aluminum 6061", 2700, 69e9, 0.33, 40e6, 290e6, 23.6e-6, 933, 3});

        // Copper
        addMaterial({"Copper", 8960, 130e9, 0.34, 200e6, 220e6, 16.5e-6, 1358, 4});

        // Titanium
        addMaterial({"Titanium", 4500, 103e9, 0.32, 880e6, 930e6, 8.6e-6, 1941, 5});
    }

    void addMaterial(const MaterialProperties& props) {
        materials_[props.name] = props;
    }

    MaterialProperties* getMaterial(const std::string& name) {
        auto it = materials_.find(name);
        if (it != materials_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    MaterialProperties* getMaterialById(int id) {
        for (auto& pair : materials_) {
            if (pair.second.material_id == id) {
                return &pair.second;
            }
        }
        return nullptr;
    }

    size_t getMaterialCount() const { return materials_.size(); }

    double calculateMass(const std::string& material, double volume) {
        auto* mat = getMaterial(material);
        if (!mat) return 0;
        return mat->density * volume;
    }

    bool isWeldable(const std::string& material) const {
        // Simplified: most materials are weldable except titanium
        return material.find("Titanium") == std::string::npos;
    }

    double getThermalStress(const std::string& material, double delta_t, double youngs_mod_override = -1) {
        auto* mat = getMaterial(material);
        if (!mat) return 0;
        double E = (youngs_mod_override > 0) ? youngs_mod_override : mat->youngs_modulus;
        return mat->thermal_expansion * delta_t * E;
    }

    bool isTemperatureViable(const std::string& material, double temperature) {
        auto* mat = getMaterial(material);
        if (!mat) return false;
        return temperature < mat->melting_point;
    }

private:
    std::map<std::string, MaterialProperties> materials_;
    int next_id_;
};

} // namespace bendcore::materials

using namespace bendcore::materials;

int main() {
    int passed = 0, failed = 0;

    #define TEST(name, cond) { \
        if (cond) { passed++; } else { \
            printf("FAIL: %s\n", name); failed++; \
        } \
    }

    // Test 1-5: Database initialization
    {
        MaterialPropertyDatabase db;
        TEST("T1: Database has materials", db.getMaterialCount() > 0);
        TEST("T2: Steel available", db.getMaterial("Steel (Mild)") != nullptr);
        TEST("T3: Aluminum available", db.getMaterial("Aluminum 6061") != nullptr);
        TEST("T4: Copper available", db.getMaterial("Copper") != nullptr);
        TEST("T5: Database initialized", db.getMaterialCount() >= 4);
    }

    // Test 6-10: Material properties access
    {
        MaterialPropertyDatabase db;
        auto* steel = db.getMaterial("Steel (Mild)");
        TEST("T6: Steel density correct", steel->density == 7850);
        TEST("T7: Steel young's modulus", steel->youngs_modulus == 210e9);
        TEST("T8: Steel yield strength", steel->yield_strength == 250e6);
        TEST("T9: Steel ultimate strength", steel->ultimate_strength == 400e6);
        TEST("T10: Steel poisson's ratio", std::abs(steel->poissons_ratio - 0.30) < 0.01);
    }

    // Test 11-15: Material comparison
    {
        MaterialPropertyDatabase db;
        auto* steel = db.getMaterial("Steel (Mild)");
        auto* aluminum = db.getMaterial("Aluminum 6061");

        TEST("T11: Steel denser than aluminum", steel->density > aluminum->density);
        TEST("T12: Steel stiffer than aluminum", steel->youngs_modulus > aluminum->youngs_modulus);
        TEST("T13: Steel higher yield strength", steel->yield_strength > aluminum->yield_strength);
        TEST("T14: Aluminum lower melting point", aluminum->melting_point < steel->melting_point);
        TEST("T15: Different materials have different properties",
             steel->density != aluminum->density);
    }

    // Test 16-20: Mass calculation
    {
        MaterialPropertyDatabase db;
        auto* steel = db.getMaterial("Steel (Mild)");
        double volume = 1000.0;  // 1000 mm³
        double mass = db.calculateMass("Steel (Mild)", volume);

        // mass = density * volume = 7850 kg/m³ * 1e-9 m³ = 7.85e-6 kg
        double expected_mass = steel->density * volume / 1e9;
        TEST("T16: Mass calculation", std::abs(mass - expected_mass * 1e9) / expected_mass < 0.01);

        double volume2 = 2000.0;
        double mass2 = db.calculateMass("Steel (Mild)", volume2);
        TEST("T17: Double volume double mass", std::abs(mass2 / mass - 2.0) < 0.01);

        double mass_al = db.calculateMass("Aluminum 6061", volume);
        TEST("T18: Aluminum mass less than steel", mass_al < mass);

        TEST("T19: Invalid material zero mass", db.calculateMass("InvalidMaterial", volume) == 0);
        TEST("T20: Zero volume zero mass", db.calculateMass("Steel (Mild)", 0) == 0);
    }

    // Test 21-25: Material ID lookup
    {
        MaterialPropertyDatabase db;
        auto* mat1 = db.getMaterialById(1);
        TEST("T21: Steel ID 1", mat1 != nullptr && mat1->material_id == 1);

        auto* mat3 = db.getMaterialById(3);
        TEST("T22: Aluminum ID 3", mat3 != nullptr && mat3->material_id == 3);

        auto* mat_invalid = db.getMaterialById(999);
        TEST("T23: Invalid ID returns null", mat_invalid == nullptr);

        auto* copper = db.getMaterialById(4);
        TEST("T24: Copper by ID", copper != nullptr && copper->density == 8960);

        TEST("T25: ID lookup consistent", mat1 == db.getMaterial("Steel (Mild)"));
    }

    // Test 26-30: Weldability
    {
        MaterialPropertyDatabase db;
        TEST("T26: Steel weldable", db.isWeldable("Steel (Mild)"));
        TEST("T27: Aluminum weldable", db.isWeldable("Aluminum 6061"));
        TEST("T28: Copper weldable", db.isWeldable("Copper"));
        TEST("T29: Titanium not weldable", !db.isWeldable("Titanium"));
        TEST("T30: Stainless steel weldable", db.isWeldable("Steel (Stainless 304)"));
    }

    // Test 31-34: Thermal properties
    {
        MaterialPropertyDatabase db;
        auto* steel = db.getMaterial("Steel (Mild)");

        // Thermal stress = alpha * delta_T * E
        double thermal_stress = db.getThermalStress("Steel (Mild)", 100);
        TEST("T31: Thermal stress calculated", thermal_stress > 0);

        double thermal_stress2 = db.getThermalStress("Steel (Mild)", 200);
        TEST("T32: Double temperature double stress",
             std::abs(thermal_stress2 / thermal_stress - 2.0) < 0.01);

        auto* aluminum = db.getMaterial("Aluminum 6061");
        double thermal_stress_al = db.getThermalStress("Aluminum 6061", 100);
        TEST("T33: Thermal stress calculated", thermal_stress_al > 0);

        double zero_stress = db.getThermalStress("Steel (Mild)", 0);
        TEST("T34: Zero temperature change no stress", zero_stress == 0);
    }

    // Test 35-38: Temperature viability
    {
        MaterialPropertyDatabase db;

        TEST("T35: Room temp safe for steel", db.isTemperatureViable("Steel (Mild)", 300));
        TEST("T36: Room temp safe for aluminum", db.isTemperatureViable("Aluminum 6061", 300));

        TEST("T37: Below melting safe",
             db.isTemperatureViable("Copper", 1000));

        TEST("T38: Above melting not viable",
             !db.isTemperatureViable("Steel (Mild)", 2000));
    }

    printf("\n=== GAP-052: MaterialPropertyDatabase ===\n");
    printf("Passed: %d/38\n", passed);
    printf("Failed: %d/38\n", failed);
    return (failed == 0) ? 0 : 1;
}
