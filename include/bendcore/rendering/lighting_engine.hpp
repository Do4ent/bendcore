#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace bendcore::rendering {

// Lighting model types
enum class LightingModel {
    PHONG,      // Traditional Phong lighting
    PBR,        // Physically-based rendering
    BLINN_PHONG // Improved Phong variant
};

// Shadow mapping techniques
enum class ShadowTechnique {
    NONE,
    SHADOW_MAP,           // Simple shadow mapping
    VARIANCE_SHADOW_MAP,  // VSM for softer shadows
    PERCENTAGE_CLOSER,    // PCF for smoother transitions
    CASCADE_SHADOW_MAP    // CSM for large scenes
};

// Light types
enum class LightType {
    DIRECTIONAL,
    POINT,
    SPOT
};

// Light structure
struct Light {
    uint32_t id = 0;
    LightType type = LightType::DIRECTIONAL;

    glm::vec3 position{0.0f, 1.0f, 0.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};

    glm::vec3 ambient{0.1f, 0.1f, 0.1f};
    glm::vec3 diffuse{1.0f, 1.0f, 1.0f};
    glm::vec3 specular{1.0f, 1.0f, 1.0f};

    float intensity = 1.0f;
    float constant_attenuation = 1.0f;
    float linear_attenuation = 0.09f;
    float quadratic_attenuation = 0.032f;

    float cutoff_angle = 45.0f; // For spot lights
    float outer_cutoff = 65.0f;
};

// Shadow mapping configuration
struct ShadowConfig {
    ShadowTechnique technique = ShadowTechnique::SHADOW_MAP;
    int shadow_map_size = 2048;
    bool enable_pcf = true;
    int pcf_samples = 9;
    float bias = 0.0005f;
    bool enable_self_shadowing = true;
    int cascade_levels = 4; // For CSM
};

// Lighting engine for advanced illumination and shadows
class LightingEngine {
public:
    LightingEngine();
    ~LightingEngine();

    // Initialization
    void initialize(const ShadowConfig& config);
    void shutdown();

    // Lighting model control
    void set_lighting_model(LightingModel model);
    LightingModel get_lighting_model() const;

    // Light management
    uint32_t add_light(const Light& light);
    void update_light(uint32_t light_id, const Light& light);
    void remove_light(uint32_t light_id);
    Light get_light(uint32_t light_id) const;
    std::vector<Light> get_all_lights() const;
    int get_light_count() const;

    // Shadow mapping
    void set_shadow_config(const ShadowConfig& config);
    ShadowConfig get_shadow_config() const;
    void enable_shadows(bool enabled);
    bool shadows_enabled() const;
    void update_shadow_maps();

    // Lighting computation
    glm::vec3 compute_phong_lighting(const glm::vec3& position,
                                     const glm::vec3& normal,
                                     const glm::vec3& view_dir,
                                     const glm::vec3& albedo,
                                     const Light& light) const;

    glm::vec3 compute_pbr_lighting(const glm::vec3& position,
                                   const glm::vec3& normal,
                                   const glm::vec3& view_dir,
                                   const glm::vec3& albedo,
                                   float roughness,
                                   float metallic,
                                   const Light& light) const;

    // Global illumination
    void set_ambient_light(const glm::vec3& color);
    glm::vec3 get_ambient_light() const;

    // Shadow queries
    float compute_shadow_factor(const glm::vec3& position,
                               const glm::vec3& normal,
                               const Light& light) const;

    // Performance
    void set_max_lights(int max_count);
    int get_max_lights() const;
    void optimize_lights();
    float get_lighting_update_time() const;

    // Real-time shadow computation
    void compute_cascade_matrices(const glm::mat4& view_matrix,
                                 const glm::mat4& proj_matrix);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    LightingModel lighting_model_ = LightingModel::PBR;
    ShadowConfig shadow_config_;
    glm::vec3 ambient_light_{0.2f, 0.2f, 0.2f};
    bool shadows_enabled_ = true;
    int max_lights_ = 16;

    uint32_t next_light_id_ = 1;
    std::vector<Light> lights_;

    float lighting_update_time_ = 0.0f;

    void update_lighting_matrices();
    void compute_shadow_bias();
    glm::vec3 fresnel_schlick(float cos_theta, const glm::vec3& F0) const;
    float distribution_ggx(const glm::vec3& N, const glm::vec3& H, float roughness) const;
    float geometry_schlick_ggx(float NdotV, float roughness) const;
};

} // namespace bendcore::rendering
