#include <gtest/gtest.h>
#include "bendcore/rendering/material_system.hpp"
#include <glm/glm.hpp>

using namespace bendcore::rendering;

class MaterialSystemTest : public ::testing::Test {
protected:
    MaterialSystem system;

    void SetUp() override {
        system.initialize();
    }

    void TearDown() override {
        system.shutdown();
    }
};

// Initialization tests
TEST_F(MaterialSystemTest, Initialize) {
    MaterialSystem sys;
    sys.initialize();
    EXPECT_GT(sys.get_material_count(), 0); // Should have default material
    sys.shutdown();
}

TEST_F(MaterialSystemTest, DoubleInitialize) {
    MaterialSystem sys;
    sys.initialize();
    sys.initialize(); // Should not crash
    sys.shutdown();
}

TEST_F(MaterialSystemTest, Shutdown) {
    MaterialSystem sys;
    sys.initialize();
    sys.shutdown();
    sys.shutdown(); // Should be safe
}

// Material creation and management
TEST_F(MaterialSystemTest, CreateMaterial) {
    uint32_t id = system.create_material("TestMaterial");
    EXPECT_GT(id, 0);

    auto mat = system.get_material(id);
    EXPECT_NE(mat, nullptr);
    EXPECT_EQ(mat->name, "TestMaterial");
}

TEST_F(MaterialSystemTest, CreateMultipleMaterials) {
    std::vector<uint32_t> ids;
    for (int i = 0; i < 10; ++i) {
        uint32_t id = system.create_material("Material" + std::to_string(i));
        ids.push_back(id);
    }

    // All IDs should be unique
    for (size_t i = 0; i < ids.size(); ++i) {
        for (size_t j = i + 1; j < ids.size(); ++j) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST_F(MaterialSystemTest, GetMaterialByName) {
    uint32_t id = system.create_material("NamedMaterial");
    auto mat = system.get_material_by_name("NamedMaterial");

    EXPECT_NE(mat, nullptr);
    EXPECT_EQ(mat->id, id);
}

TEST_F(MaterialSystemTest, GetNonexistentMaterial) {
    auto mat = system.get_material(99999);
    EXPECT_EQ(mat, nullptr);
}

TEST_F(MaterialSystemTest, DeleteMaterial) {
    uint32_t id = system.create_material("ToDelete");
    system.delete_material(id);

    auto mat = system.get_material(id);
    EXPECT_EQ(mat, nullptr);
}

TEST_F(MaterialSystemTest, GetAllMaterials) {
    system.create_material("Mat1");
    system.create_material("Mat2");
    system.create_material("Mat3");

    auto all = system.get_all_materials();
    EXPECT_GE(all.size(), 3); // At least the 3 we created + default
}

// Texture creation and management
TEST_F(MaterialSystemTest, CreateTexture) {
    std::vector<uint8_t> data(100 * 100 * 3);
    uint32_t id = system.create_texture("TestTex", 100, 100, data);

    EXPECT_GT(id, 0);
    auto tex = system.get_texture(id);
    EXPECT_NE(tex, nullptr);
    EXPECT_EQ(tex->width, 100);
    EXPECT_EQ(tex->height, 100);
}

TEST_F(MaterialSystemTest, LoadTextureFile) {
    // Create a temporary texture file
    std::string temp_path = "/tmp/test_texture.raw";
    uint32_t id = system.load_texture(temp_path);

    EXPECT_GT(id, 0);
    auto tex = system.get_texture(id);
    EXPECT_NE(tex, nullptr);
}

TEST_F(MaterialSystemTest, GetTextureByName) {
    std::vector<uint8_t> data(50 * 50 * 3);
    uint32_t id = system.create_texture("NamedTexture", 50, 50, data);

    auto tex = system.get_texture_by_name("NamedTexture");
    EXPECT_NE(tex, nullptr);
    EXPECT_EQ(tex->id, id);
}

TEST_F(MaterialSystemTest, DeleteTexture) {
    std::vector<uint8_t> data(100 * 100 * 3);
    uint32_t id = system.create_texture("ToDelete", 100, 100, data);
    system.delete_texture(id);

    auto tex = system.get_texture(id);
    EXPECT_EQ(tex, nullptr);
}

// PBR material properties
TEST_F(MaterialSystemTest, SetMaterialPBR) {
    uint32_t id = system.create_material("PBRMat");

    PBRMaterial pbr;
    pbr.albedo = {1.0f, 0.0f, 0.0f};
    pbr.roughness = 0.3f;
    pbr.metallic = 0.8f;

    system.set_material_pbr(id, pbr);

    auto retrieved = system.get_material_pbr(id);
    EXPECT_EQ(retrieved.albedo, pbr.albedo);
    EXPECT_EQ(retrieved.roughness, pbr.roughness);
    EXPECT_EQ(retrieved.metallic, pbr.metallic);
}

TEST_F(MaterialSystemTest, TextureWrapMode) {
    std::vector<uint8_t> data(100 * 100 * 3);
    uint32_t id = system.create_texture("WrapTest", 100, 100, data);

    system.set_texture_wrap_mode(id, TextureWrapMode::CLAMP);
    auto tex = system.get_texture(id);
    EXPECT_EQ(tex->wrap_mode, TextureWrapMode::CLAMP);

    system.set_texture_wrap_mode(id, TextureWrapMode::MIRROR);
    EXPECT_EQ(tex->wrap_mode, TextureWrapMode::MIRROR);
}

TEST_F(MaterialSystemTest, TextureFilterMode) {
    std::vector<uint8_t> data(100 * 100 * 3);
    uint32_t id = system.create_texture("FilterTest", 100, 100, data);

    system.set_texture_filter_mode(id, TextureFilterMode::NEAREST);
    auto tex = system.get_texture(id);
    EXPECT_EQ(tex->filter_mode, TextureFilterMode::NEAREST);
}

// Texture mapping
TEST_F(MaterialSystemTest, SetAlbedoTexture) {
    uint32_t mat_id = system.create_material("Mat");
    std::vector<uint8_t> data(100 * 100 * 3);
    uint32_t tex_id = system.create_texture("Albedo", 100, 100, data);

    system.set_albedo_texture(mat_id, tex_id);
    auto mat = system.get_material(mat_id);

    EXPECT_NE(mat->albedo_texture, nullptr);
    EXPECT_EQ(mat->albedo_texture->id, tex_id);
}

TEST_F(MaterialSystemTest, SetNormalMap) {
    uint32_t mat_id = system.create_material("Mat");
    std::vector<uint8_t> data(100 * 100 * 3);
    uint32_t tex_id = system.create_texture("Normal", 100, 100, data);

    system.set_normal_map(mat_id, tex_id);
    auto mat = system.get_material(mat_id);

    EXPECT_TRUE(mat->use_normal_mapping);
    EXPECT_NE(mat->normal_map, nullptr);
}

TEST_F(MaterialSystemTest, SetRoughnessTexture) {
    uint32_t mat_id = system.create_material("Mat");
    std::vector<uint8_t> data(100 * 100 * 3);
    uint32_t tex_id = system.create_texture("Rough", 100, 100, data);

    system.set_roughness_texture(mat_id, tex_id);
    auto mat = system.get_material(mat_id);
    EXPECT_NE(mat->roughness_texture, nullptr);
}

TEST_F(MaterialSystemTest, SetMetallicTexture) {
    uint32_t mat_id = system.create_material("Mat");
    std::vector<uint8_t> data(100 * 100 * 3);
    uint32_t tex_id = system.create_texture("Metal", 100, 100, data);

    system.set_metallic_texture(mat_id, tex_id);
    auto mat = system.get_material(mat_id);
    EXPECT_NE(mat->metallic_texture, nullptr);
}

TEST_F(MaterialSystemTest, SetAOTexture) {
    uint32_t mat_id = system.create_material("Mat");
    std::vector<uint8_t> data(100 * 100 * 3);
    uint32_t tex_id = system.create_texture("AO", 100, 100, data);

    system.set_ambient_occlusion_texture(mat_id, tex_id);
    auto mat = system.get_material(mat_id);
    EXPECT_NE(mat->ambient_occlusion_texture, nullptr);
}

TEST_F(MaterialSystemTest, SetDisplacementMap) {
    uint32_t mat_id = system.create_material("Mat");
    std::vector<uint8_t> data(100 * 100 * 3);
    uint32_t tex_id = system.create_texture("Displacement", 100, 100, data);

    system.set_displacement_map(mat_id, tex_id);
    auto mat = system.get_material(mat_id);
    EXPECT_NE(mat->displacement_map, nullptr);
}

// Material flags
TEST_F(MaterialSystemTest, NormalMapping) {
    uint32_t id = system.create_material("Mat");
    system.set_normal_mapping(id, true);

    auto mat = system.get_material(id);
    EXPECT_TRUE(mat->use_normal_mapping);

    system.set_normal_mapping(id, false);
    EXPECT_FALSE(mat->use_normal_mapping);
}

TEST_F(MaterialSystemTest, ParallaxMapping) {
    uint32_t id = system.create_material("Mat");
    system.set_parallax_mapping(id, true);

    auto mat = system.get_material(id);
    EXPECT_TRUE(mat->use_parallax_mapping);
}

TEST_F(MaterialSystemTest, Transparency) {
    uint32_t id = system.create_material("Mat");
    system.set_transparency(id, 0.5f);

    auto mat = system.get_material(id);
    EXPECT_EQ(mat->transparency, 0.5f);
    EXPECT_TRUE(mat->is_transparent);

    system.set_transparency(id, 1.0f);
    EXPECT_FALSE(mat->is_transparent);
}

TEST_F(MaterialSystemTest, DoubleSided) {
    uint32_t id = system.create_material("Mat");
    system.set_double_sided(id, true);

    auto mat = system.get_material(id);
    EXPECT_TRUE(mat->is_double_sided);
}

// Predefined materials
TEST_F(MaterialSystemTest, DefaultMaterial) {
    uint32_t id = system.get_default_material_id();
    EXPECT_GT(id, 0);

    auto mat = system.get_material(id);
    EXPECT_NE(mat, nullptr);
}

TEST_F(MaterialSystemTest, MetalMaterial) {
    uint32_t id = system.get_metal_material_id();
    EXPECT_GT(id, 0);

    auto mat = system.get_material(id);
    EXPECT_EQ(mat->name, "Metal");
}

TEST_F(MaterialSystemTest, PlasticMaterial) {
    uint32_t id = system.get_plastic_material_id();
    EXPECT_GT(id, 0);

    auto mat = system.get_material(id);
    EXPECT_EQ(mat->name, "Plastic");
}

TEST_F(MaterialSystemTest, GlassMaterial) {
    uint32_t id = system.get_glass_material_id();
    EXPECT_GT(id, 0);

    auto mat = system.get_material(id);
    EXPECT_EQ(mat->name, "Glass");
}

// Statistics
TEST_F(MaterialSystemTest, MaterialCount) {
    int initial = system.get_material_count();
    system.create_material("Test1");
    system.create_material("Test2");

    int after = system.get_material_count();
    EXPECT_EQ(after, initial + 2);
}

TEST_F(MaterialSystemTest, TextureCount) {
    int initial = system.get_texture_count();
    std::vector<uint8_t> data(100 * 100 * 3);

    system.create_texture("Tex1", 100, 100, data);
    system.create_texture("Tex2", 100, 100, data);

    int after = system.get_texture_count();
    EXPECT_EQ(after, initial + 2);
}

TEST_F(MaterialSystemTest, MemoryUsage) {
    size_t usage = system.get_memory_usage();
    EXPECT_GT(usage, 0);
}

// Validation
TEST_F(MaterialSystemTest, ValidateMaterial) {
    uint32_t id = system.create_material("Mat");
    EXPECT_TRUE(system.validate_material(id));
}

TEST_F(MaterialSystemTest, ValidateInvalidRoughness) {
    uint32_t id = system.create_material("Mat");
    auto mat = system.get_material(id);
    mat->pbr_properties.roughness = 2.0f; // Invalid

    EXPECT_FALSE(system.validate_material(id));
}
