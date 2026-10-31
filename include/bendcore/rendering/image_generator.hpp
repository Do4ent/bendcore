#pragma once

#include <vector>
#include <memory>
#include <string>
#include <glm/glm.hpp>

namespace bendcore::rendering {

// Image export formats
enum class ImageFormat {
    PNG,
    JPEG,
    TIFF,
    HDR,
    EXR
};

// Image resolution presets
enum class ResolutionPreset {
    HD_720P,         // 1280x720
    FULL_HD_1080P,   // 1920x1080
    UHD_4K,          // 3840x2160
    UHD_6K,          // 6016x3384
    UHD_8K           // 7680x4320
};

// Ray tracing quality levels
enum class RayTracingQuality {
    NONE,
    LOW,      // 1 sample per pixel
    MEDIUM,   // 4 samples per pixel
    HIGH,     // 16 samples per pixel
    ULTRA     // 64+ samples per pixel
};

// Multi-sample anti-aliasing configurations
enum class AAQuality {
    NONE,
    FXAA,          // Fast approximate AA
    MSAA_2X,       // 2x multisample
    MSAA_4X,       // 4x multisample
    MSAA_8X,       // 8x multisample
    MSAA_16X,      // 16x multisample
    TEMPORAL_AA    // TAA with temporal filtering
};

// Image export configuration
struct ImageExportConfig {
    int width = 1920;
    int height = 1080;
    ImageFormat format = ImageFormat::PNG;
    ResolutionPreset preset = ResolutionPreset::FULL_HD_1080P;

    // Ray tracing
    RayTracingQuality ray_tracing = RayTracingQuality::HIGH;
    int ray_bounce_depth = 4;

    // Anti-aliasing
    AAQuality aa_quality = AAQuality::MSAA_4X;

    // Rendering options
    bool enable_motion_blur = false;
    bool enable_depth_of_field = false;
    float dof_aperture = 2.8f;
    float dof_focus_distance = 5.0f;

    bool enable_tone_mapping = true;
    float exposure = 1.0f;
    float gamma = 2.2f;

    // Performance
    bool use_denoise = true;
    int denoiser_samples = 3;
};

// Image buffer
struct ImageBuffer {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;
    int channels = 3;
};

// High-quality image generator with ray tracing and MSAA
class ImageGenerator {
public:
    ImageGenerator();
    ~ImageGenerator();

    // Initialization
    void initialize();
    void shutdown();

    // Image export
    ImageBuffer render_to_buffer(const ImageExportConfig& config);
    bool export_image(const std::string& filepath, const ImageExportConfig& config);

    // Resolution presets
    void get_resolution(ResolutionPreset preset, int& width, int& height) const;
    ImageExportConfig create_config_from_preset(ResolutionPreset preset) const;

    // Ray tracing configuration
    void set_ray_tracing_quality(RayTracingQuality quality);
    RayTracingQuality get_ray_tracing_quality() const;
    void set_ray_bounce_depth(int depth);
    int get_ray_bounce_depth() const;

    // Anti-aliasing configuration
    void set_aa_quality(AAQuality quality);
    AAQuality get_aa_quality() const;
    int get_aa_samples() const;

    // Post-processing
    void set_tone_mapping_enabled(bool enabled);
    bool is_tone_mapping_enabled() const;
    void set_exposure(float exposure);
    float get_exposure() const;
    void set_gamma(float gamma);
    float get_gamma() const;

    // Denoising
    void set_denoise_enabled(bool enabled);
    bool is_denoise_enabled() const;
    void set_denoiser_samples(int samples);
    int get_denoiser_samples() const;

    // Depth of field
    void set_dof_enabled(bool enabled);
    bool is_dof_enabled() const;
    void set_dof_aperture(float aperture);
    float get_dof_aperture() const;
    void set_dof_focus_distance(float distance);
    float get_dof_focus_distance() const;

    // Motion blur
    void set_motion_blur_enabled(bool enabled);
    bool is_motion_blur_enabled() const;

    // Rendering statistics
    float get_render_time() const;
    float get_frame_rate() const;
    int get_samples_rendered() const;

    // Quality metrics
    float estimate_quality_score(const ImageExportConfig& config) const;
    bool validate_config(const ImageExportConfig& config) const;

    // Memory management
    size_t get_memory_usage() const;
    void clear_cache();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    RayTracingQuality ray_tracing_quality_ = RayTracingQuality::HIGH;
    AAQuality aa_quality_ = AAQuality::MSAA_4X;
    int ray_bounce_depth_ = 4;

    bool tone_mapping_enabled_ = true;
    float exposure_ = 1.0f;
    float gamma_ = 2.2f;

    bool denoise_enabled_ = true;
    int denoiser_samples_ = 3;

    bool dof_enabled_ = false;
    float dof_aperture_ = 2.8f;
    float dof_focus_distance_ = 5.0f;

    bool motion_blur_enabled_ = false;

    float render_time_ = 0.0f;
    float frame_rate_ = 0.0f;
    int samples_rendered_ = 0;

    ImageBuffer trace_rays(const ImageExportConfig& config);
    ImageBuffer apply_anti_aliasing(const ImageBuffer& buffer, AAQuality quality);
    ImageBuffer apply_tone_mapping(const ImageBuffer& buffer);
    ImageBuffer apply_denoise(const ImageBuffer& buffer);
    ImageBuffer apply_depth_of_field(const ImageBuffer& buffer);
    ImageBuffer apply_motion_blur(const ImageBuffer& buffer);

    glm::vec3 trace_ray(const glm::vec3& origin, const glm::vec3& direction, int depth);
    glm::vec3 calculate_pixel_color(float x, float y, const ImageExportConfig& config);
};

} // namespace bendcore::rendering
