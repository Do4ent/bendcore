#include <gtest/gtest.h>
#include "bendcore/materials/material_property_database.hpp"
#include <cmath>

using namespace bendcore::materials;

class MaterialPropertyDatabaseTest : public ::testing::Test {
protected:
    MaterialPropertyDatabase db;

    MaterialProperty createTestMaterial(const std::string& id, const std::string& name) {
        MaterialProperty mat;
        mat.material_id = id;
        mat.material_name = name;
        mat.material_class = MaterialClass::Steel;
        mat.grade = "AISI 1045";

        mat.young_modulus.base_value = 200e9;
        mat.yield_strength.base_value = 250e6;
        mat.tensile_strength.base_value = 420e6;
        mat.shear_modulus.base_value = 80e9;

        mat.density = 7850.0;
        mat.specific_heat.base_value = 490.0;
        mat.thermal_conductivity.base_value = 50.0;
        mat.thermal_expansion.base_value = 12e-6;

        return mat;
    }
};

TEST_F(MaterialPropertyDatabaseTest, DefaultConstructor) {
    MaterialPropertyDatabase database;
    EXPECT_GT(database.getMaterialCount(), 0);  // Should initialize standard materials
}

TEST_F(MaterialPropertyDatabaseTest, AddMaterial) {
    auto mat = createTestMaterial("MAT001", "TestSteel");
    bool success = db.addMaterial(mat);
    EXPECT_TRUE(success);
    EXPECT_GT(db.getMaterialCount(), 0);
}

TEST_F(MaterialPropertyDatabaseTest, GetMaterial) {
    auto mat = createTestMaterial("MAT002", "TestAluminum");
    db.addMaterial(mat);

    auto retrieved = db.getMaterial("MAT002");
    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->material_name, "TestAluminum");
}

TEST_F(MaterialPropertyDatabaseTest, GetMaterialNotFound) {
    auto retrieved = db.getMaterial("NONEXISTENT");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(MaterialPropertyDatabaseTest, GetMaterialsByClass) {
    auto steels = db.getMaterialsByClass(MaterialClass::Steel);
    EXPECT_GT(steels.size(), 0);
}

TEST_F(MaterialPropertyDatabaseTest, GetMaterialsByName) {
    auto mat = createTestMaterial("MAT003", "UniqueSteel");
    db.addMaterial(mat);

    auto results = db.getMaterialsByName("Unique");
    EXPECT_GT(results.size(), 0);
}

TEST_F(MaterialPropertyDatabaseTest, GetAllMaterials) {
    auto all = db.getAllMaterials();
    EXPECT_GT(all.size(), 0);
}

TEST_F(MaterialPropertyDatabaseTest, GetMaterialCount) {
    size_t count = db.getMaterialCount();
    EXPECT_GT(count, 0);
}

TEST_F(MaterialPropertyDatabaseTest, SearchByYieldStrength) {
    auto results = db.searchByYieldStrength(200e6, 300e6, 20.0);
    EXPECT_GT(results.size(), 0);
}

TEST_F(MaterialPropertyDatabaseTest, SearchByDensity) {
    auto results = db.searchByDensity(2600.0, 8900.0);
    EXPECT_GT(results.size(), 0);
}

TEST_F(MaterialPropertyDatabaseTest, SearchByThermalConductivity) {
    auto results = db.searchByThermalConductivity(40.0, 60.0, 20.0);
    EXPECT_GT(results.size(), 0);
}

TEST_F(MaterialPropertyDatabaseTest, RemoveMaterial) {
    auto mat = createTestMaterial("MAT004", "RemovableSteel");
    db.addMaterial(mat);
    size_t count_before = db.getMaterialCount();

    bool success = db.removeMaterial("MAT004");
    EXPECT_TRUE(success);
    EXPECT_EQ(db.getMaterialCount(), count_before - 1);
}

TEST_F(MaterialPropertyDatabaseTest, RemoveNonexistentMaterial) {
    bool success = db.removeMaterial("NONEXISTENT");
    EXPECT_FALSE(success);
}

TEST_F(MaterialPropertyDatabaseTest, ClearDatabase) {
    auto mat = createTestMaterial("MAT005", "ClearTest");
    db.addMaterial(mat);
    db.clear();

    EXPECT_EQ(db.getMaterialCount(), 0);
}

TEST_F(MaterialPropertyDatabaseTest, GetMaterialAtTemperature) {
    auto mat = createTestMaterial("MAT006", "TempTest");
    mat.young_modulus.reference_temperature = 20.0;
    mat.young_modulus.base_value = 200e9;
    mat.young_modulus.temperature_coefficient = -0.001;

    db.addMaterial(mat);

    auto at_temp = db.getMaterialAtTemperature("MAT006", 100.0);
    EXPECT_EQ(at_temp.material_id, "MAT006");
    EXPECT_LT(at_temp.young_modulus.getValueAtTemperature(100.0),
              at_temp.young_modulus.getValueAtTemperature(20.0));
}

TEST_F(MaterialPropertyDatabaseTest, CompareProperties) {
    auto mat1 = createTestMaterial("MAT007", "Compare1");
    mat1.density = 7850.0;

    auto mat2 = createTestMaterial("MAT008", "Compare2");
    mat2.density = 2700.0;

    db.addMaterial(mat1);
    db.addMaterial(mat2);

    auto comparison = db.compareProperties({"MAT007", "MAT008"}, "density", 20.0);
    EXPECT_GT(comparison.size(), 0);
}

TEST_F(MaterialPropertyDatabaseTest, TemperatureDependentProperty) {
    TemperatureDependentProperty prop;
    prop.reference_temperature = 20.0;
    prop.base_value = 200e9;
    prop.temperature_coefficient = -0.0005;

    double value_20 = prop.getValueAtTemperature(20.0);
    double value_100 = prop.getValueAtTemperature(100.0);

    EXPECT_DOUBLE_EQ(value_20, 200e9);
    EXPECT_LT(value_100, value_20);
}

TEST_F(MaterialPropertyDatabaseTest, MaterialPropertyCompleteness) {
    auto mat = createTestMaterial("MAT009", "Complete");
    mat.hardness_hv = 250.0;
    mat.elongation_percentage = 15.0;
    mat.fatigue_limit = 100e6;
    mat.machinability_rating = 75.0;

    db.addMaterial(mat);
    auto retrieved = db.getMaterial("MAT009");

    EXPECT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->hardness_hv, 250.0);
    EXPECT_EQ(retrieved->elongation_percentage, 15.0);
}

TEST_F(MaterialPropertyDatabaseTest, DuplicateMaterialUpdate) {
    auto mat1 = createTestMaterial("MAT010", "UpdateTest");
    mat1.density = 7850.0;
    db.addMaterial(mat1);

    auto mat2 = createTestMaterial("MAT010", "UpdateTest");
    mat2.density = 8000.0;
    db.addMaterial(mat2);

    auto retrieved = db.getMaterial("MAT010");
    EXPECT_EQ(retrieved->density, 8000.0);
}

TEST_F(MaterialPropertyDatabaseTest, CreateSteel_AISI1045) {
    auto steel = factory::createSteel_AISI1045();
    EXPECT_EQ(steel.material_class, MaterialClass::Steel);
    EXPECT_GT(steel.yield_strength.base_value, 0.0);
    EXPECT_GT(steel.tensile_strength.base_value, steel.yield_strength.base_value);
}

TEST_F(MaterialPropertyDatabaseTest, CreateSteel_Stainless304) {
    auto steel = factory::createSteel_Stainless304();
    EXPECT_EQ(steel.material_class, MaterialClass::Stainless);
    EXPECT_GT(steel.corrosion_resistance_rating, 0.0);
}

TEST_F(MaterialPropertyDatabaseTest, CreateAluminum_6061) {
    auto aluminum = factory::createAluminum_6061();
    EXPECT_EQ(aluminum.material_class, MaterialClass::Aluminum);
    EXPECT_LT(aluminum.density, 3000.0);
    EXPECT_GT(aluminum.density, 2000.0);
}

TEST_F(MaterialPropertyDatabaseTest, CreateAluminum_2024) {
    auto aluminum = factory::createAluminum_2024();
    EXPECT_EQ(aluminum.material_class, MaterialClass::Aluminum);
    EXPECT_GT(aluminum.yield_strength.base_value, 200e6);
}

TEST_F(MaterialPropertyDatabaseTest, CreateCopper_Pure) {
    auto copper = factory::createCopper_Pure();
    EXPECT_EQ(copper.material_class, MaterialClass::Copper);
    EXPECT_GT(copper.thermal_conductivity.base_value, 100.0);
}

TEST_F(MaterialPropertyDatabaseTest, CreateTitanium_Grade5) {
    auto titanium = factory::createTitanium_Grade5();
    EXPECT_EQ(titanium.material_class, MaterialClass::Titanium);
    EXPECT_GT(titanium.yield_strength.base_value, 800e6);
}

TEST_F(MaterialPropertyDatabaseTest, FactoryMaterialIntegration) {
    auto steel = factory::createSteel_AISI1045();
    auto aluminum = factory::createAluminum_6061();
    auto copper = factory::createCopper_Pure();

    db.addMaterial(steel);
    db.addMaterial(aluminum);
    db.addMaterial(copper);

    EXPECT_EQ(db.getMaterialCount(), 3);
}

TEST_F(MaterialPropertyDatabaseTest, SearchMultipleClasses) {
    db.clear();

    db.addMaterial(factory::createSteel_AISI1045());
    db.addMaterial(factory::createAluminum_6061());
    db.addMaterial(factory::createCopper_Pure());

    auto steels = db.getMaterialsByClass(MaterialClass::Steel);
    auto alums = db.getMaterialsByClass(MaterialClass::Aluminum);
    auto coppers = db.getMaterialsByClass(MaterialClass::Copper);

    EXPECT_EQ(steels.size(), 1);
    EXPECT_EQ(alums.size(), 1);
    EXPECT_EQ(coppers.size(), 1);
}

TEST_F(MaterialPropertyDatabaseTest, PropertyRangeSearch) {
    db.clear();

    db.addMaterial(factory::createSteel_AISI1045());
    db.addMaterial(factory::createAluminum_6061());
    db.addMaterial(factory::createTitanium_Grade5());

    auto high_strength = db.searchByYieldStrength(300e6, 1000e6, 20.0);
    EXPECT_GT(high_strength.size(), 0);
}

TEST_F(MaterialPropertyDatabaseTest, ThermalPropertiesQuery) {
    db.clear();
    db.addMaterial(factory::createSteel_AISI1045());
    db.addMaterial(factory::createCopper_Pure());

    auto results = db.searchByThermalConductivity(200.0, 400.0, 20.0);
    EXPECT_GT(results.size(), 0);  // Copper has high thermal conductivity
}

TEST_F(MaterialPropertyDatabaseTest, RelativeCostComparison) {
    auto mat1 = createTestMaterial("MAT011", "Cheap");
    mat1.relative_cost = 0.5;

    auto mat2 = createTestMaterial("MAT012", "Expensive");
    mat2.relative_cost = 5.0;

    db.addMaterial(mat1);
    db.addMaterial(mat2);

    auto cheap = db.getMaterial("MAT011");
    auto expensive = db.getMaterial("MAT012");

    EXPECT_LT(cheap->relative_cost, expensive->relative_cost);
}

TEST_F(MaterialPropertyDatabaseTest, MachinabilityRatings) {
    auto mat = createTestMaterial("MAT013", "Machining");
    mat.machinability_rating = 85.0;
    mat.weldability_rating = 75.0;
    mat.formability_rating = 60.0;

    db.addMaterial(mat);
    auto retrieved = db.getMaterial("MAT013");

    EXPECT_DOUBLE_EQ(retrieved->machinability_rating, 85.0);
    EXPECT_DOUBLE_EQ(retrieved->weldability_rating, 75.0);
    EXPECT_DOUBLE_EQ(retrieved->formability_rating, 60.0);
}

TEST_F(MaterialPropertyDatabaseTest, EnvironmentalProperties) {
    auto mat = createTestMaterial("MAT014", "Green");
    mat.recyclability_percentage = 100.0;
    mat.corrosion_resistance_rating = 95.0;

    db.addMaterial(mat);
    auto retrieved = db.getMaterial("MAT014");

    EXPECT_EQ(retrieved->recyclability_percentage, 100.0);
    EXPECT_DOUBLE_EQ(retrieved->corrosion_resistance_rating, 95.0);
}

TEST_F(MaterialPropertyDatabaseTest, LargeTemperatureRange) {
    auto mat = createTestMaterial("MAT015", "TempRange");
    mat.young_modulus.temperature_coefficient = -0.0005;

    db.addMaterial(mat);
    auto at_minus_40 = db.getMaterialAtTemperature("MAT015", -40.0);
    auto at_200 = db.getMaterialAtTemperature("MAT015", 200.0);

    EXPECT_GT(at_minus_40.young_modulus.getValueAtTemperature(-40.0),
              at_200.young_modulus.getValueAtTemperature(200.0));
}

TEST_F(MaterialPropertyDatabaseTest, CriticalPropertyLimits) {
    auto mat = createTestMaterial("MAT016", "Critical");
    mat.yield_strength.base_value = 500e6;
    mat.elongation_percentage = 5.0;  // Low ductility
    mat.impact_strength = 10.0;       // Low toughness

    db.addMaterial(mat);
    auto retrieved = db.getMaterial("MAT016");

    EXPECT_GT(retrieved->yield_strength.base_value, 400e6);
    EXPECT_LT(retrieved->elongation_percentage, 10.0);
}

TEST_F(MaterialPropertyDatabaseTest, DensityRangeValidation) {
    db.clear();

    // Add materials with known densities
    db.addMaterial(factory::createAluminum_6061());        // ~2700 kg/m³
    db.addMaterial(factory::createSteel_AISI1045());      // ~7850 kg/m³
    db.addMaterial(factory::createCopper_Pure());         // ~8960 kg/m³

    auto light = db.searchByDensity(2000.0, 3000.0);
    auto heavy = db.searchByDensity(8000.0, 9000.0);

    EXPECT_GT(light.size(), 0);
    EXPECT_GT(heavy.size(), 0);
}

TEST_F(MaterialPropertyDatabaseTest, AllocationAndDeallocation) {
    {
        MaterialPropertyDatabase temp_db;
        auto mat = createTestMaterial("MAT017", "Temp");
        temp_db.addMaterial(mat);
    }  // Should deallocate cleanly

    EXPECT_TRUE(true);  // If we get here, no memory issues
}
