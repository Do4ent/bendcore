#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>

namespace bendcore::rendering {

// PBR material properties
struct PBRMaterial {
    glm::vec3 albedo{0.5f, 0.5f, 0.5f};
    float roughness = 0.5f;
    float metallic = 0.0f;
    float ambient_occlusion = 1.0f;
    glm::vec3 emission{0.0f, 0.0f, 0.0f};
};

// Texture mapping modes
enum class TextureWrapMode {
    CLAMP,
    REPEAT,
    MIRROR
};

enum class TextureFilterMode {
    NEAREST,
    LINEAR,
    ANISOTROPIC
};

// Texture representation
struct Texture {
    uint32_t id = 0;
    std::string name;
    std::string filepath;
    int width = 0;
    int height = 0;
    int channels = 3;
    std::vector<uint8_t> data;
    TextureWrapMode wrap_mode = TextureWrapMode::REPEAT;
    TextureFilterMode filter_mode = TextureFilterMode::LINEAR;
};

// Material definition with textures
struct Material {
    uint32_t id = 0;
    std::string name;

    // Color properties
    PBRMaterial pbr_properties;

    // Textures
    std::shared_ptr<Texture> albedo_texture;
    std::shared_ptr<Texture> normal_map;
    std::shared_ptr<Texture> roughness_texture;
    std::shared_ptr<Texture> metallic_texture;
    std::shared_ptr<Texture> ambient_occlusion_texture;
    std::shared_ptr<Texture> displacement_map;

    // Material flags
    bool use_normal_mapping = false;
    bool use_parallax_mapping = false;
    bool is_transparent = false;
    float transparency = 1.0f;
    bool is_double_sided = false;
};

// Material database
class MaterialSystem {
public:
    MaterialSystem();
    ~MaterialSystem();

    // Initialization
    void initialize();
    void shutdown();

    // Material management
    uint32_t create_material(const std::string& name);
    void delete_material(uint32_t material_id);
    std::shared_ptr<Material> get_material(uint32_t material_id);
    std::shared_ptr<Material> get_material_by_name(const std::string& name);
    std::vector<std::shared_ptr<Material>> get_all_materials() const;

    // Texture management
    uint32_t load_texture(const std::string& filepath);
    uint32_t create_texture(const std::string& name, int width, int height,
                           const std::vector<uint8_t>& data);
    void delete_texture(uint32_t texture_id);
    std::shared_ptr<Texture> get_texture(uint32_t texture_id);
    std::shared_ptr<Texture> get_texture_by_name(const std::string& name);

    // Material properties
    void set_material_pbr(uint32_t material_id, const PBRMaterial& pbr);
    PBRMaterial get_material_pbr(uint32_t material_id) const;

    // Texture mapping
    void set_albedo_texture(uint32_t material_id, uint32_t texture_id);
    void set_normal_map(uint32_t material_id, uint32_t texture_id);
    void set_roughness_texture(uint32_t material_id, uint32_t texture_id);
    void set_metallic_texture(uint32_t material_id, uint32_t texture_id);
    void set_ambient_occlusion_texture(uint32_t material_id, uint32_t texture_id);
    void set_displacement_map(uint32_t material_id, uint32_t texture_id);

    // Texture properties
    void set_texture_wrap_mode(uint32_t texture_id, TextureWrapMode mode);
    void set_texture_filter_mode(uint32_t texture_id, TextureFilterMode mode);

    // Material flags
    void set_normal_mapping(uint32_t material_id, bool enabled);
    void set_parallax_mapping(uint32_t material_id, bool enabled);
    void set_transparency(uint32_t material_id, float alpha);
    void set_double_sided(uint32_t material_id, bool enabled);

    // Predefined materials
    void create_standard_materials();
    uint32_t get_default_material_id() const;
    uint32_t get_metal_material_id() const;
    uint32_t get_plastic_material_id() const;
    uint32_t get_glass_material_id() const;

    // Statistics
    int get_material_count() const;
    int get_texture_count() const;
    size_t get_memory_usage() const;

    // PBR validation
    bool validate_material(uint32_t material_id) const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    uint32_t next_material_id_ = 1;
    uint32_t next_texture_id_ = 1;

    std::unordered_map<uint32_t, std::shared_ptr<Material>> materials_;
    std::unordered_map<uint32_t, std::shared_ptr<Texture>> textures_;

    uint32_t default_material_id_ = 0;

    void create_default_material();
    std::vector<uint8_t> load_texture_data(const std::string& filepath);
};

} // namespace bendcore::rendering
