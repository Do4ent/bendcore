#pragma once

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
    void addMaterial(const Material& mat);
    Material* getMaterial(const std::string& name);
    void addTexture(const std::string& material, const Texture& tex);
    bool hasTexture(const std::string& material, TextureType type);
    size_t getMaterialCount() const;
    double calculateSpecular(const std::string& material);
    bool isPBRComplete(const std::string& material);

private:
    std::map<std::string, Material> materials_;
};

} // namespace bendcore::rendering
