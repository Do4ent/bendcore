// Material library: built-in table invariants, user materials, JSON round trip and errors.
// The reference numbers below are copied from the cited sources (docs/kernel/ANALYSIS.md),
// not from the code; they all still require a check against the printed editions.

#include <bendcore/kernel/material.hpp>

#include <gtest/gtest.h>

#include <set>

using namespace bendcore::kernel;

TEST(Materials, BuiltInTableIsConsistent) {
    const auto& all = builtInMaterials();
    ASSERT_GE(all.size(), 15u);
    std::set<std::string> ids;
    std::set<MaterialCategory> categories;
    for (const Material& m : all) {
        EXPECT_TRUE(ids.insert(m.id).second) << "duplicate id " << m.id;
        categories.insert(m.category);
        EXPECT_TRUE(validateMaterial(m).ok()) << m.id;
        EXPECT_TRUE(m.builtIn) << m.id;
        EXPECT_FALSE(m.source.empty()) << m.id << " has no source";
        // Nothing has been compared with a printed edition yet (docs/STANDARDS_VERIFICATION.md).
        EXPECT_FALSE(m.verified) << m.id;
    }
    EXPECT_TRUE(categories.count(MaterialCategory::Steel));
    EXPECT_TRUE(categories.count(MaterialCategory::StainlessSteel));
    EXPECT_TRUE(categories.count(MaterialCategory::Aluminum));
    EXPECT_TRUE(categories.count(MaterialCategory::Plastic));
}

TEST(Materials, ReferenceValuesOfStructuralGrades) {
    const MaterialLibrary lib = MaterialLibrary::withBuiltIns();
    // EN 1993-1-1 3.2.6: E = 210000 MPa, nu = 0.3, rho = 7850 kg/m^3; EN 10025-2: ReH 235/355.
    const Material* s235 = lib.find("steel_s235");
    ASSERT_NE(s235, nullptr);
    EXPECT_DOUBLE_EQ(s235->density, 7850.0);
    EXPECT_DOUBLE_EQ(*s235->youngModulus, 210000.0);
    EXPECT_DOUBLE_EQ(*s235->poissonRatio, 0.3);
    EXPECT_DOUBLE_EQ(*s235->yieldStrength, 235.0);
    ASSERT_NE(lib.find("steel_s355"), nullptr);
    EXPECT_DOUBLE_EQ(*lib.find("steel_s355")->yieldStrength, 355.0);
    // EN 1999-1-1 3.2.5: E = 70000 MPa, nu = 0.3 for all aluminium alloys.
    const Material* al = lib.find("al_6082_t6");
    ASSERT_NE(al, nullptr);
    EXPECT_DOUBLE_EQ(*al->youngModulus, 70000.0);
    EXPECT_DOUBLE_EQ(*al->poissonRatio, 0.3);
    // Values the sources do not give are absent rather than guessed.
    const Material* amg6 = lib.find("al_amg6");
    ASSERT_NE(amg6, nullptr);
    EXPECT_FALSE(amg6->youngModulus.has_value());
    EXPECT_FALSE(amg6->yieldStrength.has_value());
    EXPECT_EQ(lib.find("no_such_material"), nullptr);
}

TEST(Materials, UserMaterialsAndJsonRoundTrip) {
    MaterialLibrary lib = MaterialLibrary::withBuiltIns();
    const std::size_t builtIns = lib.materials().size();
    Material bronze;
    bronze.id = "user_bronze";
    bronze.name = "Bronze \"BrAZh\" 9-4";
    bronze.category = MaterialCategory::Other;
    bronze.density = 7600;
    bronze.youngModulus = 110000;
    bronze.color = {180, 120, 60};
    bronze.source = "workshop sample, weighed";
    auto added = lib.add(bronze);
    ASSERT_TRUE(added.ok()) << added.error().message;
    EXPECT_FALSE(added.value()->builtIn);
    EXPECT_EQ(lib.materials().size(), builtIns + 1);

    Material pom = bronze;
    pom.id = "user_pom";
    pom.name = "POM-C";
    pom.category = MaterialCategory::Plastic;
    pom.density = 1410;
    pom.youngModulus.reset();
    pom.poissonRatio = 0.35;
    pom.thermalConductivity = 0.31;
    ASSERT_TRUE(lib.add(pom).ok());

    const std::string json = lib.userMaterialsToJson();
    EXPECT_NE(json.find("\"bendcore-materials\""), std::string::npos);
    EXPECT_EQ(json.find("steel_s235"), std::string::npos) << "built-ins are not written";

    MaterialLibrary other = MaterialLibrary::withBuiltIns();
    auto count = other.addUserMaterialsFromJson(json);
    ASSERT_TRUE(count.ok()) << count.error().message;
    EXPECT_EQ(count.value(), 2u);
    const Material* back = other.find("user_bronze");
    ASSERT_NE(back, nullptr);
    EXPECT_EQ(*back, *lib.find("user_bronze"));
    EXPECT_EQ(*other.find("user_pom"), *lib.find("user_pom"));
    EXPECT_FALSE(other.find("user_pom")->youngModulus.has_value());

    // Single material round trip.
    auto single = materialFromJson(materialToJson(bronze));
    ASSERT_TRUE(single.ok());
    EXPECT_EQ(single.value().color, (Rgb{180, 120, 60}));
    EXPECT_DOUBLE_EQ(single.value().density, 7600);

    // Update and remove user materials; built-ins are protected.
    bronze.density = 7650;
    ASSERT_TRUE(lib.update(bronze).ok());
    EXPECT_DOUBLE_EQ(lib.find("user_bronze")->density, 7650);
    Material fakeS235 = *lib.find("steel_s235");
    fakeS235.density = 1;
    EXPECT_EQ(lib.update(fakeS235).error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(lib.remove("steel_s235").error().code, ErrorCode::InvalidInput);
    EXPECT_TRUE(lib.remove("user_bronze").ok());
    EXPECT_EQ(lib.find("user_bronze"), nullptr);
}

TEST(Materials, ValidationAndParseErrors) {
    MaterialLibrary lib = MaterialLibrary::withBuiltIns();
    Material m;
    m.id = "x";
    m.name = "X";
    m.density = 0;
    EXPECT_EQ(lib.add(m).error().code, ErrorCode::InvalidInput);
    m.density = 1000;
    m.poissonRatio = 0.5;
    EXPECT_EQ(lib.add(m).error().code, ErrorCode::InvalidInput);
    m.poissonRatio = 0.3;
    m.id = "steel_s235";
    EXPECT_EQ(lib.add(m).error().code, ErrorCode::InvalidInput) << "duplicate id";

    EXPECT_EQ(materialFromJson("{not json").error().code, ErrorCode::ParseError);
    EXPECT_EQ(materialFromJson(R"({"id":"a","name":"A","category":"wood","density_kg_m3":600})").error().code,
              ErrorCode::ParseError);
    EXPECT_EQ(materialFromJson(R"({"id":"a","name":"A","category":"other"})").error().code, ErrorCode::ParseError);
    EXPECT_EQ(materialFromJson(R"({"id":"a","name":"A","category":"other","density_kg_m3":600,"color":"red"})")
                  .error()
                  .code,
              ErrorCode::ParseError);
    EXPECT_EQ(materialFromJson(R"({"id":"a","name":"A","category":"other","density_kg_m3":-1})").error().code,
              ErrorCode::InvalidInput);

    // A document with one bad entry adds nothing.
    const std::size_t before = lib.materials().size();
    auto r = lib.addUserMaterialsFromJson(
        R"({"format":"bendcore-materials","version":1,"materials":[)"
        R"({"id":"ok","name":"OK","category":"other","density_kg_m3":1000},)"
        R"({"id":"bad","name":"Bad","category":"other","density_kg_m3":0}]})");
    EXPECT_FALSE(r.ok());
    EXPECT_EQ(lib.materials().size(), before);
    EXPECT_EQ(lib.addUserMaterialsFromJson(R"({"format":"bendcore-materials","version":2,"materials":[]})")
                  .error()
                  .code,
              ErrorCode::Unsupported);
}
