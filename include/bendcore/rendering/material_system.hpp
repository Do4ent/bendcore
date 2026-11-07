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
