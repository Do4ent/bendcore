#include "bendcore/rendering/material_system.hpp"
#include <algorithm>
#include <fstream>
#include <cstring>

namespace bendcore::rendering {

struct MaterialSystem::Impl {
    bool is_initialized = false;
};

MaterialSystem::MaterialSystem() : pimpl_(std::make_unique<Impl>()) {}

MaterialSystem::~MaterialSystem() {
    shutdown();
}

void MaterialSystem::initialize() {
    if (pimpl_->is_initialized) return;

    pimpl_->is_initialized = true;
    create_default_material();
    create_standard_materials();
}

void MaterialSystem::shutdown() {
    if (!pimpl_->is_initialized) return;

    materials_.clear();
    textures_.clear();
    pimpl_->is_initialized = false;
}

uint32_t MaterialSystem::create_material(const std::string& name) {
    uint32_t id = next_material_id_++;
    auto material = std::make_shared<Material>();
    material->id = id;
    material->name = name;
    material->pbr_properties = PBRMaterial();

    materials_[id] = material;
    return id;
}

void MaterialSystem::delete_material(uint32_t material_id) {
    materials_.erase(material_id);
}

std::shared_ptr<Material> MaterialSystem::get_material(uint32_t material_id) {
    auto it = materials_.find(material_id);
    return it != materials_.end() ? it->second : nullptr;
}

std::shared_ptr<Material> MaterialSystem::get_material_by_name(const std::string& name) {
    for (const auto& pair : materials_) {
        if (pair.second->name == name) {
            return pair.second;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<Material>> MaterialSystem::get_all_materials() const {
    std::vector<std::shared_ptr<Material>> result;
    for (const auto& pair : materials_) {
        result.push_back(pair.second);
    }
    return result;
}

uint32_t MaterialSystem::load_texture(const std::string& filepath) {
    uint32_t id = next_texture_id_++;
    auto texture = std::make_shared<Texture>();
    texture->id = id;
    texture->filepath = filepath;
    texture->name = filepath.substr(filepath.find_last_of("/\\") + 1);

    // In a real implementation, load texture data from file
    texture->data = load_texture_data(filepath);

    textures_[id] = texture;
    return id;
}

uint32_t MaterialSystem::create_texture(const std::string& name, int width, int height,
                                       const std::vector<uint8_t>& data) {
    uint32_t id = next_texture_id_++;
    auto texture = std::make_shared<Texture>();
    texture->id = id;
    texture->name = name;
    texture->width = width;
    texture->height = height;
    texture->channels = data.size() / (width * height);
    texture->data = data;

    textures_[id] = texture;
    return id;
}

void MaterialSystem::delete_texture(uint32_t texture_id) {
    textures_.erase(texture_id);
}

std::shared_ptr<Texture> MaterialSystem::get_texture(uint32_t texture_id) {
    auto it = textures_.find(texture_id);
    return it != textures_.end() ? it->second : nullptr;
}

std::shared_ptr<Texture> MaterialSystem::get_texture_by_name(const std::string& name) {
    for (const auto& pair : textures_) {
        if (pair.second->name == name) {
            return pair.second;
        }
    }
    return nullptr;
}

void MaterialSystem::set_material_pbr(uint32_t material_id, const PBRMaterial& pbr) {
    auto mat = get_material(material_id);
    if (mat) {
        mat->pbr_properties = pbr;
    }
}

PBRMaterial MaterialSystem::get_material_pbr(uint32_t material_id) const {
    auto mat = get_material(material_id);
    return mat ? mat->pbr_properties : PBRMaterial();
}

void MaterialSystem::set_albedo_texture(uint32_t material_id, uint32_t texture_id) {
    auto mat = get_material(material_id);
    if (mat) {
        mat->albedo_texture = get_texture(texture_id);
    }
}

void MaterialSystem::set_normal_map(uint32_t material_id, uint32_t texture_id) {
    auto mat = get_material(material_id);
    if (mat) {
        mat->normal_map = get_texture(texture_id);
        mat->use_normal_mapping = true;
    }
}

void MaterialSystem::set_roughness_texture(uint32_t material_id, uint32_t texture_id) {
    auto mat = get_material(material_id);
    if (mat) {
        mat->roughness_texture = get_texture(texture_id);
    }
}

void MaterialSystem::set_metallic_texture(uint32_t material_id, uint32_t texture_id) {
    auto mat = get_material(material_id);
    if (mat) {
        mat->metallic_texture = get_texture(texture_id);
    }
}

void MaterialSystem::set_ambient_occlusion_texture(uint32_t material_id, uint32_t texture_id) {
    auto mat = get_material(material_id);
    if (mat) {
        mat->ambient_occlusion_texture = get_texture(texture_id);
    }
}

void MaterialSystem::set_displacement_map(uint32_t material_id, uint32_t texture_id) {
    auto mat = get_material(material_id);
    if (mat) {
        mat->displacement_map = get_texture(texture_id);
    }
}

void MaterialSystem::set_texture_wrap_mode(uint32_t texture_id, TextureWrapMode mode) {
    auto tex = get_texture(texture_id);
    if (tex) {
        tex->wrap_mode = mode;
    }
}

void MaterialSystem::set_texture_filter_mode(uint32_t texture_id, TextureFilterMode mode) {
    auto tex = get_texture(texture_id);
    if (tex) {
        tex->filter_mode = mode;
    }
}

void MaterialSystem::set_normal_mapping(uint32_t material_id, bool enabled) {
    auto mat = get_material(material_id);
    if (mat) {
        mat->use_normal_mapping = enabled;
    }
}

void MaterialSystem::set_parallax_mapping(uint32_t material_id, bool enabled) {
    auto mat = get_material(material_id);
    if (mat) {
        mat->use_parallax_mapping = enabled;
    }
}

void MaterialSystem::set_transparency(uint32_t material_id, float alpha) {
    auto mat = get_material(material_id);
    if (mat) {
        mat->transparency = glm::clamp(alpha, 0.0f, 1.0f);
        mat->is_transparent = alpha < 1.0f;
    }
}

void MaterialSystem::set_double_sided(uint32_t material_id, bool enabled) {
    auto mat = get_material(material_id);
    if (mat) {
        mat->is_double_sided = enabled;
    }
}

void MaterialSystem::create_standard_materials() {
    // Metal material
    uint32_t metal_id = create_material("Metal");
    PBRMaterial metal_pbr;
    metal_pbr.albedo = {0.5f, 0.5f, 0.5f};
    metal_pbr.metallic = 1.0f;
    metal_pbr.roughness = 0.2f;
    set_material_pbr(metal_id, metal_pbr);

    // Plastic material
    uint32_t plastic_id = create_material("Plastic");
    PBRMaterial plastic_pbr;
    plastic_pbr.albedo = {0.8f, 0.8f, 0.8f};
    plastic_pbr.metallic = 0.0f;
    plastic_pbr.roughness = 0.5f;
    set_material_pbr(plastic_id, plastic_pbr);

    // Glass material
    uint32_t glass_id = create_material("Glass");
    PBRMaterial glass_pbr;
    glass_pbr.albedo = {1.0f, 1.0f, 1.0f};
    glass_pbr.metallic = 0.0f;
    glass_pbr.roughness = 0.0f;
    set_material_pbr(glass_id, glass_pbr);
    set_transparency(glass_id, 0.3f);
}

uint32_t MaterialSystem::get_default_material_id() const {
    return default_material_id_;
}

uint32_t MaterialSystem::get_metal_material_id() const {
    auto mat = get_material_by_name("Metal");
    return mat ? mat->id : 0;
}

uint32_t MaterialSystem::get_plastic_material_id() const {
    auto mat = get_material_by_name("Plastic");
    return mat ? mat->id : 0;
}

uint32_t MaterialSystem::get_glass_material_id() const {
    auto mat = get_material_by_name("Glass");
    return mat ? mat->id : 0;
}

int MaterialSystem::get_material_count() const {
    return static_cast<int>(materials_.size());
}

int MaterialSystem::get_texture_count() const {
    return static_cast<int>(textures_.size());
}

size_t MaterialSystem::get_memory_usage() const {
    size_t total = 0;

    for (const auto& pair : materials_) {
        total += sizeof(Material);
    }

    for (const auto& pair : textures_) {
        if (pair.second) {
            total += sizeof(Texture) + pair.second->data.size();
        }
    }

    return total;
}

bool MaterialSystem::validate_material(uint32_t material_id) const {
    auto mat = get_material(material_id);
    if (!mat) return false;

    // Validate PBR properties
    const auto& pbr = mat->pbr_properties;
    if (pbr.roughness < 0.0f || pbr.roughness > 1.0f) return false;
    if (pbr.metallic < 0.0f || pbr.metallic > 1.0f) return false;
    if (pbr.ambient_occlusion < 0.0f || pbr.ambient_occlusion > 1.0f) return false;

    if (mat->transparency < 0.0f || mat->transparency > 1.0f) return false;

    return true;
}

void MaterialSystem::create_default_material() {
    default_material_id_ = create_material("Default");
    auto mat = get_material(default_material_id_);
    if (mat) {
        mat->pbr_properties.albedo = {0.7f, 0.7f, 0.7f};
        mat->pbr_properties.roughness = 0.5f;
        mat->pbr_properties.metallic = 0.0f;
    }
}

std::vector<uint8_t> MaterialSystem::load_texture_data(const std::string& filepath) {
    // In a real implementation, this would load texture from file
    // For now, return empty vector
    std::vector<uint8_t> data;

    // Try to read file
    std::ifstream file(filepath, std::ios::binary);
    if (file.good()) {
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        data.resize(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        file.close();
    }

    return data;
}

} // namespace bendcore::rendering
