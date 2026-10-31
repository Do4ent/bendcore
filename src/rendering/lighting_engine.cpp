#include "bendcore/rendering/lighting_engine.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <cmath>
#include <chrono>

namespace bendcore::rendering {

struct LightingEngine::Impl {
    bool is_initialized = false;
    std::vector<glm::mat4> cascade_matrices;
    std::chrono::high_resolution_clock::time_point last_update;
};

LightingEngine::LightingEngine() : pimpl_(std::make_unique<Impl>()) {}

LightingEngine::~LightingEngine() {
    shutdown();
}

void LightingEngine::initialize(const ShadowConfig& config) {
    if (pimpl_->is_initialized) return;

    shadow_config_ = config;
    pimpl_->is_initialized = true;
    pimpl_->last_update = std::chrono::high_resolution_clock::now();

    // Pre-allocate cascade matrices
    pimpl_->cascade_matrices.resize(shadow_config_.cascade_levels);
}

void LightingEngine::shutdown() {
    if (!pimpl_->is_initialized) return;

    lights_.clear();
    pimpl_->cascade_matrices.clear();
    pimpl_->is_initialized = false;
}

void LightingEngine::set_lighting_model(LightingModel model) {
    lighting_model_ = model;
}

LightingModel LightingEngine::get_lighting_model() const {
    return lighting_model_;
}

uint32_t LightingEngine::add_light(const Light& light) {
    if (lights_.size() >= static_cast<size_t>(max_lights_)) {
        return 0;
    }

    uint32_t id = next_light_id_++;
    Light new_light = light;
    new_light.id = id;
    lights_.push_back(new_light);

    return id;
}

void LightingEngine::update_light(uint32_t light_id, const Light& light) {
    auto it = std::find_if(lights_.begin(), lights_.end(),
        [light_id](const Light& l) { return l.id == light_id; });

    if (it != lights_.end()) {
        *it = light;
        it->id = light_id;
    }
}

void LightingEngine::remove_light(uint32_t light_id) {
    auto it = std::find_if(lights_.begin(), lights_.end(),
        [light_id](const Light& l) { return l.id == light_id; });

    if (it != lights_.end()) {
        lights_.erase(it);
    }
}

Light LightingEngine::get_light(uint32_t light_id) const {
    for (const auto& light : lights_) {
        if (light.id == light_id) {
            return light;
        }
    }
    return Light();
}

std::vector<Light> LightingEngine::get_all_lights() const {
    return lights_;
}

int LightingEngine::get_light_count() const {
    return static_cast<int>(lights_.size());
}

void LightingEngine::set_shadow_config(const ShadowConfig& config) {
    shadow_config_ = config;
    pimpl_->cascade_matrices.resize(shadow_config_.cascade_levels);
}

ShadowConfig LightingEngine::get_shadow_config() const {
    return shadow_config_;
}

void LightingEngine::enable_shadows(bool enabled) {
    shadows_enabled_ = enabled;
}

bool LightingEngine::shadows_enabled() const {
    return shadows_enabled_;
}

void LightingEngine::update_shadow_maps() {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        now - pimpl_->last_update).count();

    lighting_update_time_ = elapsed / 1000.0f; // Convert to milliseconds
    pimpl_->last_update = now;

    if (!shadows_enabled_) return;

    // Update shadow maps based on technique
    switch (shadow_config_.technique) {
        case ShadowTechnique::CASCADE_SHADOW_MAP:
            compute_shadow_bias();
            break;
        default:
            break;
    }
}

glm::vec3 LightingEngine::compute_phong_lighting(const glm::vec3& position,
                                                const glm::vec3& normal,
                                                const glm::vec3& view_dir,
                                                const glm::vec3& albedo,
                                                const Light& light) const {
    glm::vec3 result = ambient_light_ * albedo;

    // Light direction
    glm::vec3 light_dir = glm::vec3(0.0f);
    float attenuation = 1.0f;

    if (light.type == LightType::DIRECTIONAL) {
        light_dir = glm::normalize(-light.direction);
    } else if (light.type == LightType::POINT) {
        glm::vec3 light_vec = light.position - position;
        float distance = glm::length(light_vec);
        light_dir = glm::normalize(light_vec);

        attenuation = 1.0f / (light.constant_attenuation +
                             light.linear_attenuation * distance +
                             light.quadratic_attenuation * distance * distance);
    }

    // Diffuse
    float diff = glm::max(glm::dot(normal, light_dir), 0.0f);
    glm::vec3 diffuse = diff * light.diffuse * albedo * light.intensity;

    // Specular
    glm::vec3 reflect_dir = glm::reflect(-light_dir, normal);
    float spec = glm::pow(glm::max(glm::dot(view_dir, reflect_dir), 0.0f), 32.0f);
    glm::vec3 specular = spec * light.specular * light.intensity;

    // Shadow
    float shadow = shadows_enabled_ ? compute_shadow_factor(position, normal, light) : 1.0f;

    return result + attenuation * shadow * (diffuse + specular);
}

glm::vec3 LightingEngine::compute_pbr_lighting(const glm::vec3& position,
                                              const glm::vec3& normal,
                                              const glm::vec3& view_dir,
                                              const glm::vec3& albedo,
                                              float roughness,
                                              float metallic,
                                              const Light& light) const {
    glm::vec3 result = ambient_light_ * albedo * (1.0f - metallic);

    // Light direction
    glm::vec3 light_dir = glm::vec3(0.0f);
    float attenuation = 1.0f;

    if (light.type == LightType::DIRECTIONAL) {
        light_dir = glm::normalize(-light.direction);
    } else if (light.type == LightType::POINT) {
        glm::vec3 light_vec = light.position - position;
        float distance = glm::length(light_vec);
        light_dir = glm::normalize(light_vec);

        attenuation = 1.0f / (light.constant_attenuation +
                             light.linear_attenuation * distance +
                             light.quadratic_attenuation * distance * distance);
    }

    // Half vector
    glm::vec3 H = glm::normalize(view_dir + light_dir);

    // Surface properties
    float NdotV = glm::max(glm::dot(normal, view_dir), 0.0f);
    float NdotL = glm::max(glm::dot(normal, light_dir), 0.0f);
    float HdotV = glm::max(glm::dot(H, view_dir), 0.0f);

    // Fresnel
    glm::vec3 F0 = glm::mix(glm::vec3(0.04f), albedo, metallic);
    glm::vec3 F = fresnel_schlick(HdotV, F0);

    // Normal distribution
    float NDF = distribution_ggx(normal, H, roughness);

    // Geometry
    float G = geometry_schlick_ggx(NdotV, roughness) *
              geometry_schlick_ggx(NdotL, roughness);

    // Cook-Torrance BRDF
    float kS = 1.0f;
    float kD = (1.0f - F.x) * (1.0f - metallic);

    glm::vec3 numerator = NDF * G * F;
    float denominator = glm::max(4.0f * NdotV * NdotL, 0.001f);
    glm::vec3 specular = numerator / glm::vec3(denominator);

    glm::vec3 diffuse = kD * albedo;

    // Shadow
    float shadow = shadows_enabled_ ? compute_shadow_factor(position, normal, light) : 1.0f;

    return result + attenuation * shadow * (diffuse + kS * specular) * NdotL * light.intensity;
}

void LightingEngine::set_ambient_light(const glm::vec3& color) {
    ambient_light_ = color;
}

glm::vec3 LightingEngine::get_ambient_light() const {
    return ambient_light_;
}

float LightingEngine::compute_shadow_factor(const glm::vec3& position,
                                           const glm::vec3& normal,
                                           const Light& light) const {
    if (!shadows_enabled_) return 1.0f;

    // Simple shadow factor based on light direction
    glm::vec3 light_dir = glm::vec3(0.0f);

    if (light.type == LightType::DIRECTIONAL) {
        light_dir = glm::normalize(-light.direction);
    } else if (light.type == LightType::POINT) {
        light_dir = glm::normalize(light.position - position);
    }

    // Basic shadow: front-facing surfaces are lit
    float NdotL = glm::max(glm::dot(normal, light_dir), 0.0f);

    // Add small bias to reduce shadow acne
    return glm::mix(0.5f, 1.0f, NdotL);
}

void LightingEngine::set_max_lights(int max_count) {
    max_lights_ = glm::max(1, max_count);
}

int LightingEngine::get_max_lights() const {
    return max_lights_;
}

void LightingEngine::optimize_lights() {
    // Remove lights that don't contribute
    lights_.erase(std::remove_if(lights_.begin(), lights_.end(),
        [this](const Light& light) {
            return light.intensity < 0.001f;
        }), lights_.end());
}

float LightingEngine::get_lighting_update_time() const {
    return lighting_update_time_;
}

void LightingEngine::compute_cascade_matrices(const glm::mat4& view_matrix,
                                             const glm::mat4& proj_matrix) {
    // Compute cascade matrices for shadow mapping
    for (int i = 0; i < shadow_config_.cascade_levels; ++i) {
        float near_plane = 0.1f * (i + 1);
        float far_plane = 100.0f * (i + 1);

        // Build cascade projection
        pimpl_->cascade_matrices[i] =
            glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane) * view_matrix;
    }
}

glm::vec3 LightingEngine::fresnel_schlick(float cos_theta, const glm::vec3& F0) const {
    return F0 + (1.0f - F0) * glm::pow(glm::clamp(1.0f - cos_theta, 0.0f, 1.0f), 5.0f);
}

float LightingEngine::distribution_ggx(const glm::vec3& N, const glm::vec3& H, float roughness) const {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = glm::max(glm::dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = 3.14159f * denom * denom;

    return nom / glm::max(denom, 0.0001f);
}

float LightingEngine::geometry_schlick_ggx(float NdotV, float roughness) const {
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;

    float nom = NdotV;
    float denom = NdotV * (1.0f - k) + k;

    return nom / glm::max(denom, 0.0001f);
}

void LightingEngine::update_lighting_matrices() {
    // Update view matrices for all lights
}

void LightingEngine::compute_shadow_bias() {
    // Compute bias for shadow acne reduction
    shadow_config_.bias = glm::clamp(shadow_config_.bias, 0.0f, 0.01f);
}

} // namespace bendcore::rendering
