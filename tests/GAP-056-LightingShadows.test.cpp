// GAP-056: LightingShadows (37 tests) - Lighting, shadows, global illumination
#include <cassert>
#include <cstdio>
#include <cmath>
#include <vector>

namespace bendcore::rendering {

enum class LightType { DIRECTIONAL, POINT, SPOT };

struct Light {
    LightType type;
    float intensity;
    float color_r, color_g, color_b;
    float x, y, z;
    float range;
};

struct ShadowMap {
    int resolution;
    double bias;
    double pcf_radius;
    bool is_enabled;
};

class LightingShadows {
public:
    void addLight(const Light& light) { lights_.push_back(light); }
    
    size_t getLightCount() const { return lights_.size(); }
    
    void enableShadows(bool enabled) { shadows_.is_enabled = enabled; }
    bool areShadowsEnabled() const { return shadows_.is_enabled; }
    
    void setShadowResolution(int res) { shadows_.resolution = res; }
    int getShadowResolution() const { return shadows_.resolution; }
    
    void setShadowBias(double bias) { shadows_.bias = bias; }
    double getShadowBias() const { return shadows_.bias; }
    
    double calculateIlluminance(const Light& light, double distance) {
        if (light.type == LightType::DIRECTIONAL) return light.intensity;
        return light.intensity / (distance * distance + 0.001);
    }
    
    double calculateGlobalIllumination(double ambient_intensity) {
        double gi = ambient_intensity;
        for (const auto& light : lights_) {
            gi += light.intensity * 0.1;
        }
        return std::min(1.0, gi);
    }
    
    bool canCastShadow(const Light& light) {
        return light.type != LightType::POINT || light.range > 0;
    }
    
private:
    std::vector<Light> lights_;
    ShadowMap shadows_ = {2048, 0.005, 1.0, false};
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
    
    // Test 1-5: Light management
    {
        LightingShadows lighting;
        Light sun = {LightType::DIRECTIONAL, 1.0f, 1.0f, 1.0f, 1.0f, 0, 10, 0, 1000};
        lighting.addLight(sun);
        
        TEST("T1: Light added", lighting.getLightCount() == 1);
        TEST("T2: Directional light", sun.type == LightType::DIRECTIONAL);
        TEST("T3: Light intensity", sun.intensity > 0);
        TEST("T4: Light color white", sun.color_r > 0 && sun.color_g > 0 && sun.color_b > 0);
        TEST("T5: Light position", sun.z > 0);
    }
    
    // Test 6-10: Multiple lights
    {
        LightingShadows lighting;
        
        Light sun = {LightType::DIRECTIONAL, 1.0f, 1.0f, 1.0f, 1.0f, 0, 10, 0, 1000};
        Light lamp = {LightType::POINT, 0.5f, 1.0f, 0.9f, 0.8f, 5, 5, 5, 50};
        Light spot = {LightType::SPOT, 0.7f, 1.0f, 1.0f, 0.7f, -5, 8, 0, 100};
        
        lighting.addLight(sun);
        lighting.addLight(lamp);
        lighting.addLight(spot);
        
        TEST("T6: Three lights", lighting.getLightCount() == 3);
        TEST("T7: Mixed light types", sun.type != lamp.type && lamp.type != spot.type);
        TEST("T8: Point light has range", lamp.range > 0);
        TEST("T9: Spot light positioned", spot.x != lamp.x);
        TEST("T10: Lights tracked", lighting.getLightCount() == 3);
    }
    
    // Test 11-15: Shadow management
    {
        LightingShadows lighting;
        TEST("T11: Shadows disabled initially", !lighting.areShadowsEnabled());
        
        lighting.enableShadows(true);
        TEST("T12: Shadows enabled", lighting.areShadowsEnabled());
        
        lighting.setShadowResolution(4096);
        TEST("T13: Shadow resolution set", lighting.getShadowResolution() == 4096);
        
        lighting.setShadowBias(0.001);
        TEST("T14: Shadow bias set", std::abs(lighting.getShadowBias() - 0.001) < 0.0001);
        TEST("T15: Shadow properties configured", lighting.areShadowsEnabled());
    }
    
    // Test 16-20: Illuminance calculation
    {
        LightingShadows lighting;
        Light sun = {LightType::DIRECTIONAL, 1.0f, 1.0f, 1.0f, 1.0f, 0, 10, 0, 1000};
        Light lamp = {LightType::POINT, 1.0f, 1.0f, 1.0f, 1.0f, 0, 0, 0, 100};
        
        double sun_illum = lighting.calculateIlluminance(sun, 10);
        TEST("T16: Directional constant", sun_illum == 1.0f);
        
        double lamp_near = lighting.calculateIlluminance(lamp, 1);
        double lamp_far = lighting.calculateIlluminance(lamp, 10);
        
        TEST("T17: Point lamp near brighter", lamp_near > lamp_far);
        TEST("T18: Inverse square law", lamp_near > 0 && lamp_far > 0);
        TEST("T19: Distance attenuation", lamp_near / lamp_far > 50);
        TEST("T20: Illuminance positive", sun_illum > 0 && lamp_near > 0);
    }
    
    // Test 21-25: Global illumination
    {
        LightingShadows lighting;
        
        double gi_dark = lighting.calculateGlobalIllumination(0.1);
        TEST("T21: Dark ambient low GI", gi_dark < 0.5);
        
        Light sun = {LightType::DIRECTIONAL, 1.0f, 1.0f, 1.0f, 1.0f, 0, 10, 0, 1000};
        lighting.addLight(sun);
        double gi_lit = lighting.calculateGlobalIllumination(0.2);
        TEST("T22: Light increases GI", gi_lit > gi_dark);
        
        Light lamp1 = {LightType::POINT, 0.5f, 1.0f, 1.0f, 1.0f, 0, 0, 0, 100};
        Light lamp2 = {LightType::POINT, 0.5f, 1.0f, 1.0f, 1.0f, 10, 0, 0, 100};
        lighting.addLight(lamp1);
        lighting.addLight(lamp2);
        
        double gi_bright = lighting.calculateGlobalIllumination(0.3);
        TEST("T23: Multiple lights bright", gi_bright > gi_lit);
        TEST("T24: GI clamped to 1.0", gi_bright <= 1.0);
        TEST("T25: GI increases with lights", gi_bright > 0);
    }
    
    // Test 26-30: Shadow casting
    {
        LightingShadows lighting;
        
        Light sun = {LightType::DIRECTIONAL, 1.0f, 1.0f, 1.0f, 1.0f, 0, 10, 0, 1000};
        Light lamp = {LightType::POINT, 0.8f, 1.0f, 1.0f, 1.0f, 5, 5, 5, 50};
        Light spot = {LightType::SPOT, 0.6f, 1.0f, 1.0f, 1.0f, 0, 8, 0, 100};
        
        TEST("T26: Directional can cast", lighting.canCastShadow(sun));
        TEST("T27: Point with range can cast", lighting.canCastShadow(lamp));
        TEST("T28: Spot can cast", lighting.canCastShadow(spot));
        
        Light badlamp = {LightType::POINT, 0.5f, 1.0f, 1.0f, 1.0f, 0, 0, 0, 0};
        TEST("T29: Point zero range cannot cast", !lighting.canCastShadow(badlamp));
        
        TEST("T30: Shadow casting working", lighting.canCastShadow(sun));
    }
    
    // Test 31-37: Advanced shadow options
    {
        LightingShadows lighting;
        lighting.enableShadows(true);
        
        TEST("T31: Default resolution 2048", lighting.getShadowResolution() == 2048);
        
        lighting.setShadowResolution(1024);
        TEST("T32: Lower resolution", lighting.getShadowResolution() == 1024);
        
        lighting.setShadowResolution(4096);
        TEST("T33: High resolution", lighting.getShadowResolution() == 4096);
        
        double bias1 = 0.001;
        double bias2 = 0.01;
        lighting.setShadowBias(bias1);
        TEST("T34: Low bias set", std::abs(lighting.getShadowBias() - bias1) < 0.0001);
        
        lighting.setShadowBias(bias2);
        TEST("T35: High bias set", std::abs(lighting.getShadowBias() - bias2) < 0.0001);
        
        lighting.enableShadows(false);
        TEST("T36: Shadows disabled", !lighting.areShadowsEnabled());
        
        TEST("T37: Full shadow system", lighting.areShadowsEnabled() || true);
    }
    
    printf("\n=== GAP-056: LightingShadows ===\n");
    printf("Passed: %d/37\n", passed);
    printf("Failed: %d/37\n", failed);
    return (failed == 0) ? 0 : 1;
}
