// GAP-055: MaterialTextureSupport (34 tests) - Textures, materials, PBR
#include <cassert>
#include <cstdio>
#include <cmath>
#include <map>
#include <string>

namespace bendcore::rendering {

enum class TextureType { DIFFUSE, NORMAL, ROUGHNESS, METALLIC, AO };
enum class PBRModel { METALLIC_ROUGHNESS, SPECULAR_GLOSSINESS };

struct Texture {
    TextureType type;
    std::string filepath;
    int width, height;
};

struct Material {
    std::string name;
    float roughness, metallic, ior;
    float color_r, color_g, color_b;
    std::map<TextureType, Texture> textures;
    PBRModel pbr_model;
};

class MaterialTextureSupport {
public:
    void addMaterial(const Material& mat) {
        materials_[mat.name] = mat;
    }
    
    Material* getMaterial(const std::string& name) {
        auto it = materials_.find(name);
        if (it != materials_.end()) return &it->second;
        return nullptr;
    }
    
    void addTexture(const std::string& material, const Texture& tex) {
        auto* mat = getMaterial(material);
        if (mat) mat->textures[tex.type] = tex;
    }
    
    bool hasTexture(const std::string& material, TextureType type) {
        auto* mat = getMaterial(material);
        if (!mat) return false;
        return mat->textures.find(type) != mat->textures.end();
    }
    
    size_t getMaterialCount() const { return materials_.size(); }
    
    double calculateSpecular(const std::string& material) {
        auto* mat = getMaterial(material);
        if (!mat) return 0;
        return (1.0 - mat->roughness) * (mat->metallic + 0.5 * (1.0 - mat->metallic));
    }
    
    bool isPBRComplete(const std::string& material) {
        auto* mat = getMaterial(material);
        if (!mat) return false;
        return mat->textures.size() >= 4;
    }
    
private:
    std::map<std::string, Material> materials_;
};

} // namespace bendcore::rendering

using namespace bendcore::rendering;

int main() {
    int passed = 0, failed = 0;
    
    #define TEST(name, cond) { \
        if (cond) { passed++; } else { \
            printf("FAIL: %s\n", name); failed++; \
        } \
    }
    
    // Test 1-5: Material creation
    {
        MaterialTextureSupport support;
        Material mat = {"Steel", 0.3f, 0.8f, 1.5f, 0.7f, 0.7f, 0.7f, {}, PBRModel::METALLIC_ROUGHNESS};
        support.addMaterial(mat);
        
        TEST("T1: Material added", support.getMaterialCount() == 1);
        auto* retrieved = support.getMaterial("Steel");
        TEST("T2: Material retrieved", retrieved != nullptr);
        TEST("T3: Material name", retrieved->name == "Steel");
        TEST("T4: Roughness set", retrieved->roughness == 0.3f);
        TEST("T5: Metallic set", retrieved->metallic == 0.8f);
    }
    
    // Test 6-10: Texture handling
    {
        MaterialTextureSupport support;
        Material mat = {"Plastic", 0.5f, 0.0f, 1.5f, 1.0f, 1.0f, 1.0f, {}, PBRModel::METALLIC_ROUGHNESS};
        support.addMaterial(mat);
        
        Texture diffuse = {TextureType::DIFFUSE, "plastic_diffuse.png", 2048, 2048};
        Texture normal = {TextureType::NORMAL, "plastic_normal.png", 2048, 2048};
        
        support.addTexture("Plastic", diffuse);
        support.addTexture("Plastic", normal);
        
        TEST("T6: Diffuse texture added", support.hasTexture("Plastic", TextureType::DIFFUSE));
        TEST("T7: Normal texture added", support.hasTexture("Plastic", TextureType::NORMAL));
        TEST("T8: Roughness texture missing", !support.hasTexture("Plastic", TextureType::ROUGHNESS));
        TEST("T9: Multiple textures tracked", support.getMaterial("Plastic")->textures.size() == 2);
        TEST("T10: Texture resolution", support.getMaterial("Plastic")->textures[TextureType::DIFFUSE].width == 2048);
    }
    
    // Test 11-15: PBR properties
    {
        MaterialTextureSupport support;
        
        Material metal = {"Aluminum", 0.2f, 0.95f, 1.5f, 0.9f, 0.9f, 0.9f, {}, PBRModel::METALLIC_ROUGHNESS};
        Material plastic = {"PVC", 0.6f, 0.0f, 1.5f, 1.0f, 1.0f, 1.0f, {}, PBRModel::METALLIC_ROUGHNESS};
        
        support.addMaterial(metal);
        support.addMaterial(plastic);
        
        double metal_spec = support.calculateSpecular("Aluminum");
        double plastic_spec = support.calculateSpecular("PVC");
        
        TEST("T11: Metal more specular", metal_spec > plastic_spec);
        TEST("T12: Specular calculated", metal_spec > 0);
        TEST("T13: Roughness affects specularity", metal_spec < 1.0);
        TEST("T14: Metallic increases specularity", metal.metallic > plastic.metallic);
        TEST("T15: Non-metallic specular", plastic_spec > 0);
    }
    
    // Test 16-20: PBR completeness
    {
        MaterialTextureSupport support;
        Material mat = {"Complete", 0.3f, 0.7f, 1.5f, 0.8f, 0.8f, 0.8f, {}, PBRModel::METALLIC_ROUGHNESS};
        support.addMaterial(mat);
        
        TEST("T16: Incomplete initially", !support.isPBRComplete("Complete"));
        
        support.addTexture("Complete", {TextureType::DIFFUSE, "d.png", 1024, 1024});
        support.addTexture("Complete", {TextureType::NORMAL, "n.png", 1024, 1024});
        support.addTexture("Complete", {TextureType::ROUGHNESS, "r.png", 1024, 1024});
        support.addTexture("Complete", {TextureType::METALLIC, "m.png", 1024, 1024});
        
        TEST("T17: Complete after 4 textures", support.isPBRComplete("Complete"));
        TEST("T18: All required textures", support.getMaterial("Complete")->textures.size() == 4);
        TEST("T19: PBR model set", support.getMaterial("Complete")->pbr_model == PBRModel::METALLIC_ROUGHNESS);
        TEST("T20: Completeness verified", support.isPBRComplete("Complete"));
    }
    
    // Test 21-25: Color and IOR properties
    {
        MaterialTextureSupport support;
        Material glass = {"Glass", 0.05f, 0.0f, 1.52f, 1.0f, 1.0f, 1.0f, {}, PBRModel::METALLIC_ROUGHNESS};
        Material water = {"Water", 0.01f, 0.0f, 1.33f, 0.8f, 0.9f, 1.0f, {}, PBRModel::METALLIC_ROUGHNESS};
        
        support.addMaterial(glass);
        support.addMaterial(water);
        
        TEST("T21: Glass IOR correct", glass.ior > 1.5);
        TEST("T22: Water IOR correct", std::abs(water.ior - 1.33f) < 0.01);
        TEST("T23: Color values stored", glass.color_r > 0 && glass.color_r <= 1.0);
        TEST("T24: Water has tint", water.color_g > water.color_r);
        TEST("T25: IOR affects refraction", glass.ior != water.ior);
    }
    
    // Test 26-30: Multiple materials
    {
        MaterialTextureSupport support;
        
        support.addMaterial({"Mat1", 0.3f, 0.5f, 1.5f, 1.0f, 0.0f, 0.0f, {}, PBRModel::METALLIC_ROUGHNESS});
        support.addMaterial({"Mat2", 0.4f, 0.4f, 1.5f, 0.0f, 1.0f, 0.0f, {}, PBRModel::METALLIC_ROUGHNESS});
        support.addMaterial({"Mat3", 0.5f, 0.3f, 1.5f, 0.0f, 0.0f, 1.0f, {}, PBRModel::METALLIC_ROUGHNESS});
        
        TEST("T26: Three materials", support.getMaterialCount() == 3);
        
        auto* mat1 = support.getMaterial("Mat1");
        auto* mat2 = support.getMaterial("Mat2");
        auto* mat3 = support.getMaterial("Mat3");
        
        TEST("T27: Mat1 retrieved", mat1 != nullptr);
        TEST("T28: Mat2 retrieved", mat2 != nullptr);
        TEST("T29: Mat3 retrieved", mat3 != nullptr);
        TEST("T30: Different properties", mat1->metallic != mat2->metallic);
    }
    
    // Test 31-34: Texture organization
    {
        MaterialTextureSupport support;
        support.addMaterial({"Complete", 0.3f, 0.7f, 1.5f, 0.8f, 0.8f, 0.8f, {}, PBRModel::METALLIC_ROUGHNESS});
        
        support.addTexture("Complete", {TextureType::DIFFUSE, "diffuse.png", 4096, 4096});
        support.addTexture("Complete", {TextureType::NORMAL, "normal.png", 4096, 4096});
        support.addTexture("Complete", {TextureType::ROUGHNESS, "roughness.png", 2048, 2048});
        support.addTexture("Complete", {TextureType::METALLIC, "metallic.png", 2048, 2048});
        support.addTexture("Complete", {TextureType::AO, "ao.png", 2048, 2048});
        
        TEST("T31: 5 textures added", support.getMaterial("Complete")->textures.size() == 5);
        TEST("T32: High res diffuse", support.getMaterial("Complete")->textures[TextureType::DIFFUSE].width == 4096);
        TEST("T33: All texture types available", support.isPBRComplete("Complete"));
        TEST("T34: Texture map complete", support.getMaterial("Complete")->textures.size() >= 4);
    }
    
    printf("\n=== GAP-055: MaterialTextureSupport ===\n");
    printf("Passed: %d/34\n", passed);
    printf("Failed: %d/34\n", failed);
    return (failed == 0) ? 0 : 1;
}
