#pragma once

#include <cmath>

namespace bendcore::rendering {

struct RenderSettings {
    int width, height;
    int samples_per_pixel;
    int max_bounces;
    bool ray_tracing_enabled;
    double exposure;
};

struct RenderResult {
    int pixel_count;
    int total_samples;
    double render_time_ms;
    bool is_complete;
};

class HighQualityImageGeneration {
public:
    HighQualityImageGeneration() : settings_{1920, 1080, 256, 8, false, 1.0} {}

    void setResolution(int w, int h) { settings_.width = w; settings_.height = h; }
    void setSamplesPerPixel(int spp) { settings_.samples_per_pixel = spp; }
    void setMaxBounces(int bounces) { settings_.max_bounces = bounces; }
    void enableRayTracing(bool enable) { settings_.ray_tracing_enabled = enable; }
    void setExposure(double exp) { settings_.exposure = exp; }

    RenderSettings getSettings() const { return settings_; }

    RenderResult render() {
        RenderResult result;
        result.pixel_count = settings_.width * settings_.height;
        result.total_samples = result.pixel_count * settings_.samples_per_pixel;
        result.render_time_ms = result.total_samples * 0.001;  // 1ms per 1000 samples
        result.is_complete = true;
        return result;
    }

    bool isHighQuality() const {
        return settings_.samples_per_pixel >= 256 && settings_.max_bounces >= 8;
    }

    double estimateRenderTime() const {
        int pixels = settings_.width * settings_.height;
        int total_samples = pixels * settings_.samples_per_pixel;
        return total_samples * 0.001;
    }

private:
    RenderSettings settings_;
};

} // namespace bendcore::rendering
