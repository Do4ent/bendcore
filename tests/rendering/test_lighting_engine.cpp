#include <gtest/gtest.h>
#include "bendcore/rendering/lighting_engine.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace bendcore::rendering;

class LightingEngineTest : public ::testing::Test {
protected:
    LightingEngine engine;
    ShadowConfig default_config;

    void SetUp() override {
        engine.initialize(default_config);
    }

    void TearDown() override {
        engine.shutdown();
    }
};

// Initialization tests
TEST_F(LightingEngineTest, Initialize) {
    LightingEngine eng;
    eng.initialize(default_config);
    eng.shutdown();
}

TEST_F(LightingEngineTest, SetLightingModel) {
    engine.set_lighting_model(LightingModel::PHONG);
    EXPECT_EQ(engine.get_lighting_model(), LightingModel::PHONG);

    engine.set_lighting_model(LightingModel::PBR);
    EXPECT_EQ(engine.get_lighting_model(), LightingModel::PBR);
}

TEST_F(LightingEngineTest, SetAmbientLight) {
    glm::vec3 ambient(0.3f, 0.3f, 0.3f);
    engine.set_ambient_light(ambient);
    EXPECT_EQ(engine.get_ambient_light(), ambient);
}

// Light management
TEST_F(LightingEngineTest, AddDirectionalLight) {
    Light light;
    light.type = LightType::DIRECTIONAL;
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);

    uint32_t id = engine.add_light(light);
    EXPECT_GT(id, 0);

    Light retrieved = engine.get_light(id);
    EXPECT_EQ(retrieved.type, LightType::DIRECTIONAL);
}

TEST_F(LightingEngineTest, AddPointLight) {
    Light light;
    light.type = LightType::POINT;
    light.position = glm::vec3(5.0f, 5.0f, 5.0f);
    light.intensity = 1.0f;

    uint32_t id = engine.add_light(light);
    EXPECT_GT(id, 0);

    Light retrieved = engine.get_light(id);
    EXPECT_EQ(retrieved.type, LightType::POINT);
    EXPECT_EQ(retrieved.position, light.position);
}

TEST_F(LightingEngineTest, AddSpotLight) {
    Light light;
    light.type = LightType::SPOT;
    light.position = glm::vec3(0.0f, 10.0f, 0.0f);
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    light.cutoff_angle = 45.0f;

    uint32_t id = engine.add_light(light);
    EXPECT_GT(id, 0);
}

TEST_F(LightingEngineTest, AddMultipleLights) {
    std::vector<uint32_t> ids;
    for (int i = 0; i < 5; ++i) {
        Light light;
        light.type = LightType::POINT;
        light.position = glm::vec3(i, 0, 0);
        ids.push_back(engine.add_light(light));
    }

    EXPECT_EQ(engine.get_light_count(), 5);

    // All IDs should be unique
    for (size_t i = 0; i < ids.size(); ++i) {
        for (size_t j = i + 1; j < ids.size(); ++j) {
            EXPECT_NE(ids[i], ids[j]);
        }
    }
}

TEST_F(LightingEngineTest, UpdateLight) {
    Light light;
    light.position = glm::vec3(0.0f, 0.0f, 0.0f);
    uint32_t id = engine.add_light(light);

    light.position = glm::vec3(5.0f, 5.0f, 5.0f);
    light.intensity = 0.5f;
    engine.update_light(id, light);

    Light retrieved = engine.get_light(id);
    EXPECT_EQ(retrieved.position, light.position);
    EXPECT_EQ(retrieved.intensity, light.intensity);
}

TEST_F(LightingEngineTest, RemoveLight) {
    Light light;
    uint32_t id = engine.add_light(light);
    EXPECT_EQ(engine.get_light_count(), 1);

    engine.remove_light(id);
    EXPECT_EQ(engine.get_light_count(), 0);
}

TEST_F(LightingEngineTest, GetAllLights) {
    for (int i = 0; i < 3; ++i) {
        Light light;
        light.position = glm::vec3(i, 0, 0);
        engine.add_light(light);
    }

    auto lights = engine.get_all_lights();
    EXPECT_EQ(lights.size(), 3);
}

// Shadow configuration
TEST_F(LightingEngineTest, SetShadowConfig) {
    ShadowConfig config;
    config.technique = ShadowTechnique::CASCADE_SHADOW_MAP;
    config.shadow_map_size = 4096;
    config.cascade_levels = 8;

    engine.set_shadow_config(config);

    auto retrieved = engine.get_shadow_config();
    EXPECT_EQ(retrieved.technique, ShadowTechnique::CASCADE_SHADOW_MAP);
    EXPECT_EQ(retrieved.shadow_map_size, 4096);
}

TEST_F(LightingEngineTest, EnableShadows) {
    engine.enable_shadows(true);
    EXPECT_TRUE(engine.shadows_enabled());

    engine.enable_shadows(false);
    EXPECT_FALSE(engine.shadows_enabled());
}

TEST_F(LightingEngineTest, UpdateShadowMaps) {
    engine.enable_shadows(true);

    for (int i = 0; i < 10; ++i) {
        engine.update_shadow_maps();
    }

    float update_time = engine.get_lighting_update_time();
    EXPECT_GE(update_time, 0.0f);
}

// Lighting computations
TEST_F(LightingEngineTest, ComputePhongLighting) {
    engine.set_lighting_model(LightingModel::PHONG);

    Light light;
    light.type = LightType::DIRECTIONAL;
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    light.diffuse = glm::vec3(1.0f, 1.0f, 1.0f);

    glm::vec3 position(0.0f, 0.0f, 0.0f);
    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    glm::vec3 view_dir(0.0f, 0.0f, 1.0f);
    glm::vec3 albedo(0.8f, 0.8f, 0.8f);

    glm::vec3 result = engine.compute_phong_lighting(position, normal, view_dir, albedo, light);

    EXPECT_GT(glm::length(result), 0.0f);
}

TEST_F(LightingEngineTest, ComputePBRLighting) {
    engine.set_lighting_model(LightingModel::PBR);

    Light light;
    light.type = LightType::DIRECTIONAL;
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);

    glm::vec3 position(0.0f, 0.0f, 0.0f);
    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    glm::vec3 view_dir(0.0f, 0.0f, 1.0f);
    glm::vec3 albedo(0.8f, 0.8f, 0.8f);

    glm::vec3 result = engine.compute_pbr_lighting(position, normal, view_dir, albedo, 0.5f, 0.0f, light);

    EXPECT_GT(glm::length(result), 0.0f);
}

TEST_F(LightingEngineTest, PhongVsPBR) {
    Light light;
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);

    glm::vec3 position(0.0f, 0.0f, 0.0f);
    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    glm::vec3 view_dir(0.0f, 0.0f, 1.0f);
    glm::vec3 albedo(0.8f, 0.8f, 0.8f);

    engine.set_lighting_model(LightingModel::PHONG);
    glm::vec3 phong = engine.compute_phong_lighting(position, normal, view_dir, albedo, light);

    engine.set_lighting_model(LightingModel::PBR);
    glm::vec3 pbr = engine.compute_pbr_lighting(position, normal, view_dir, albedo, 0.5f, 0.0f, light);

    // Different models should produce different results
    EXPECT_NE(phong, pbr);
}

// Shadow computation
TEST_F(LightingEngineTest, ComputeShadowFactor) {
    Light light;
    light.type = LightType::DIRECTIONAL;
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);

    glm::vec3 position(0.0f, 0.0f, 0.0f);
    glm::vec3 normal(0.0f, 1.0f, 0.0f);

    float shadow = engine.compute_shadow_factor(position, normal, light);
    EXPECT_GE(shadow, 0.0f);
    EXPECT_LE(shadow, 1.0f);
}

TEST_F(LightingEngineTest, ShadowFactorVariation) {
    Light light;
    light.type = LightType::DIRECTIONAL;
    light.direction = glm::vec3(0.0f, -1.0f, 0.0f);

    glm::vec3 position(0.0f, 0.0f, 0.0f);

    // Front-facing normal
    glm::vec3 front_normal(0.0f, 1.0f, 0.0f);
    float front_shadow = engine.compute_shadow_factor(position, front_normal, light);

    // Back-facing normal
    glm::vec3 back_normal(0.0f, -1.0f, 0.0f);
    float back_shadow = engine.compute_shadow_factor(position, back_normal, light);

    EXPECT_GT(front_shadow, back_shadow);
}

// Light limits
TEST_F(LightingEngineTest, SetMaxLights) {
    engine.set_max_lights(5);
    EXPECT_EQ(engine.get_max_lights(), 5);

    // Try to add more lights than max
    for (int i = 0; i < 10; ++i) {
        Light light;
        light.position = glm::vec3(i, 0, 0);
        uint32_t id = engine.add_light(light);

        if (i < 5) {
            EXPECT_GT(id, 0);
        } else {
            EXPECT_EQ(id, 0); // Should fail when exceeding max
        }
    }
}

// Light optimization
TEST_F(LightingEngineTest, OptimizeLights) {
    // Add some lights with very low intensity
    for (int i = 0; i < 3; ++i) {
        Light light;
        light.intensity = 0.0001f; // Very low
        engine.add_light(light);
    }

    EXPECT_EQ(engine.get_light_count(), 3);

    engine.optimize_lights();
    EXPECT_EQ(engine.get_light_count(), 0);
}

// Cascade matrices
TEST_F(LightingEngineTest, ComputeCascadeMatrices) {
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f),
                                 glm::vec3(0.0f, 0.0f, 0.0f),
                                 glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);

    engine.compute_cascade_matrices(view, proj);
}

// Light intensity
TEST_F(LightingEngineTest, LightIntensity) {
    Light light;
    light.intensity = 0.5f;
    uint32_t id = engine.add_light(light);

    Light retrieved = engine.get_light(id);
    EXPECT_EQ(retrieved.intensity, 0.5f);
}

// Attenuation
TEST_F(LightingEngineTest, LightAttenuation) {
    Light light;
    light.type = LightType::POINT;
    light.position = glm::vec3(5.0f, 5.0f, 5.0f);
    light.constant_attenuation = 1.0f;
    light.linear_attenuation = 0.09f;
    light.quadratic_attenuation = 0.032f;

    uint32_t id = engine.add_light(light);
    Light retrieved = engine.get_light(id);

    EXPECT_EQ(retrieved.linear_attenuation, 0.09f);
}

// Spot light cutoff
TEST_F(LightingEngineTest, SpotLightCutoff) {
    Light light;
    light.type = LightType::SPOT;
    light.cutoff_angle = 30.0f;
    light.outer_cutoff = 60.0f;

    uint32_t id = engine.add_light(light);
    Light retrieved = engine.get_light(id);

    EXPECT_EQ(retrieved.cutoff_angle, 30.0f);
    EXPECT_EQ(retrieved.outer_cutoff, 60.0f);
}

// Complex lighting scenario
TEST_F(LightingEngineTest, ComplexLightingScenario) {
    // Add directional light (sun)
    Light sun;
    sun.type = LightType::DIRECTIONAL;
    sun.direction = glm::vec3(-0.5f, -1.0f, -0.5f);
    sun.diffuse = glm::vec3(1.0f, 1.0f, 0.9f);
    engine.add_light(sun);

    // Add point lights
    for (int i = 0; i < 3; ++i) {
        Light point;
        point.type = LightType::POINT;
        point.position = glm::vec3(i * 5.0f, 2.0f, 0.0f);
        point.intensity = 0.7f;
        engine.add_light(point);
    }

    // Add spot light
    Light spot;
    spot.type = LightType::SPOT;
    spot.position = glm::vec3(0.0f, 10.0f, 0.0f);
    spot.direction = glm::vec3(0.0f, -1.0f, 0.0f);
    engine.add_light(spot);

    EXPECT_EQ(engine.get_light_count(), 5);

    // Update shadows
    engine.enable_shadows(true);
    engine.update_shadow_maps();
    engine.update_shadow_maps();
}

// Light removal
TEST_F(LightingEngineTest, RemoveMultipleLights) {
    std::vector<uint32_t> ids;
    for (int i = 0; i < 5; ++i) {
        Light light;
        ids.push_back(engine.add_light(light));
    }

    EXPECT_EQ(engine.get_light_count(), 5);

    // Remove every other light
    for (size_t i = 0; i < ids.size(); i += 2) {
        engine.remove_light(ids[i]);
    }

    EXPECT_EQ(engine.get_light_count(), 2);
}

// Update timing
TEST_F(LightingEngineTest, LightingUpdateTiming) {
    engine.update_shadow_maps();
    float time1 = engine.get_lighting_update_time();

    engine.update_shadow_maps();
    float time2 = engine.get_lighting_update_time();

    EXPECT_GE(time1, 0.0f);
    EXPECT_GE(time2, 0.0f);
}
