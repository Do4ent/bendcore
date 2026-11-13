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
    void addLight(const Light& light);
    size_t getLightCount() const;
    void enableShadows(bool enabled);
    bool areShadowsEnabled() const;
    void setShadowResolution(int res);
    int getShadowResolution() const;
    void setShadowBias(double bias);
    double getShadowBias() const;
    double calculateIlluminance(const Light& light, double distance);
    double calculateGlobalIllumination(double ambient_intensity);
    bool canCastShadow(const Light& light);

private:
    std::vector<Light> lights_;
    ShadowMap shadows_ = {2048, 0.005, 1.0, false};
};

} // namespace bendcore::rendering
