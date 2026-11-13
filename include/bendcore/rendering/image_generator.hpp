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
    HighQualityImageGeneration();
    void setResolution(int w, int h);
    void setSamplesPerPixel(int spp);
    void setMaxBounces(int bounces);
    void enableRayTracing(bool enable);
    void setExposure(double exp);
    RenderSettings getSettings() const;
    RenderResult render();
    bool isHighQuality() const;
    double estimateRenderTime() const;

private:
    RenderSettings settings_;
};

} // namespace bendcore::rendering
