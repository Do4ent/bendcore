#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

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
