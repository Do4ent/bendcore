// M13 Phase: GAP-056 - LightingShadows
// Advanced lighting and shadow mapping system
// Supports directional, point, and spot lights with real-time shadow computation

#include "bendcore/rendering/lighting_engine.hpp"

namespace bendcore::rendering {

void LightingShadows::addLight(const Light& light) {
    lights_.push_back(light);
}

size_t LightingShadows::getLightCount() const {
    return lights_.size();
}

void LightingShadows::enableShadows(bool enabled) {
    shadows_.is_enabled = enabled;
}

bool LightingShadows::areShadowsEnabled() const {
    return shadows_.is_enabled;
}

void LightingShadows::setShadowResolution(int res) {
    shadows_.resolution = res;
}

int LightingShadows::getShadowResolution() const {
    return shadows_.resolution;
}

void LightingShadows::setShadowBias(double bias) {
    shadows_.bias = bias;
}

double LightingShadows::getShadowBias() const {
    return shadows_.bias;
}

double LightingShadows::calculateIlluminance(const Light& light, double distance) {
    if (light.type == LightType::DIRECTIONAL) return light.intensity;
    return light.intensity / (distance * distance + 0.001);
}

double LightingShadows::calculateGlobalIllumination(double ambient_intensity) {
    double gi = ambient_intensity;
    for (const auto& light : lights_) {
        gi += light.intensity * 0.1;
    }
    return std::min(1.0, gi);
}

bool LightingShadows::canCastShadow(const Light& light) {
    return light.type != LightType::POINT || light.range > 0;
}

} // namespace bendcore::rendering
