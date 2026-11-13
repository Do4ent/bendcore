// M13 Phase: GAP-055 - MaterialTextureSupport
// PBR material and texture management system
// Supports metallic-roughness and specular-glossiness PBR workflows

#include "bendcore/rendering/material_system.hpp"

namespace bendcore::rendering {

void MaterialTextureSupport::addMaterial(const Material& mat) {
    materials_[mat.name] = mat;
}

Material* MaterialTextureSupport::getMaterial(const std::string& name) {
    auto it = materials_.find(name);
    if (it != materials_.end()) return &it->second;
    return nullptr;
}

void MaterialTextureSupport::addTexture(const std::string& material, const Texture& tex) {
    auto* mat = getMaterial(material);
    if (mat) mat->textures[tex.type] = tex;
}

bool MaterialTextureSupport::hasTexture(const std::string& material, TextureType type) {
    auto* mat = getMaterial(material);
    if (!mat) return false;
    return mat->textures.find(type) != mat->textures.end();
}

size_t MaterialTextureSupport::getMaterialCount() const {
    return materials_.size();
}

double MaterialTextureSupport::calculateSpecular(const std::string& material) {
    auto* mat = getMaterial(material);
    if (!mat) return 0;
    return (1.0 - mat->roughness) * (mat->metallic + 0.5 * (1.0 - mat->metallic));
}

bool MaterialTextureSupport::isPBRComplete(const std::string& material) {
    auto* mat = getMaterial(material);
    if (!mat) return false;
    return mat->textures.size() >= 4;
}

} // namespace bendcore::rendering
