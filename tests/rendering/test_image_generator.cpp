#include <gtest/gtest.h>
#include "bendcore/rendering/image_generator.hpp"

using namespace bendcore::rendering;

class ImageGeneratorTest : public ::testing::Test {
protected:
    ImageGenerator generator;
    ImageExportConfig default_config;

    void SetUp() override {
        generator.initialize();
        default_config.width = 800;
        default_config.height = 600;
    }

    void TearDown() override {
        generator.shutdown();
    }
};

// Initialization tests
TEST_F(ImageGeneratorTest, Initialize) {
    ImageGenerator gen;
    gen.initialize();
    gen.shutdown();
}

TEST_F(ImageGeneratorTest, RenderToBuffer) {
    ImageBuffer buffer = generator.render_to_buffer(default_config);

    EXPECT_EQ(buffer.width, 800);
    EXPECT_EQ(buffer.height, 600);
    EXPECT_FALSE(buffer.data.empty());
}

TEST_F(ImageGeneratorTest, ExportImage) {
    bool result = generator.export_image("/tmp/test_export.png", default_config);
    EXPECT_TRUE(result);
}

// Resolution presets
TEST_F(ImageGeneratorTest, ResolutionPreset720P) {
    int width, height;
    generator.get_resolution(ResolutionPreset::HD_720P, width, height);
    EXPECT_EQ(width, 1280);
    EXPECT_EQ(height, 720);
}

TEST_F(ImageGeneratorTest, ResolutionPreset1080P) {
    int width, height;
    generator.get_resolution(ResolutionPreset::FULL_HD_1080P, width, height);
    EXPECT_EQ(width, 1920);
    EXPECT_EQ(height, 1080);
}

TEST_F(ImageGeneratorTest, ResolutionPreset4K) {
    int width, height;
    generator.get_resolution(ResolutionPreset::UHD_4K, width, height);
    EXPECT_EQ(width, 3840);
    EXPECT_EQ(height, 2160);
}

TEST_F(ImageGeneratorTest, ResolutionPreset8K) {
    int width, height;
    generator.get_resolution(ResolutionPreset::UHD_8K, width, height);
    EXPECT_EQ(width, 7680);
    EXPECT_EQ(height, 4320);
}

TEST_F(ImageGeneratorTest, CreateConfigFromPreset) {
    ImageExportConfig config = generator.create_config_from_preset(ResolutionPreset::UHD_4K);
    EXPECT_EQ(config.width, 3840);
    EXPECT_EQ(config.height, 2160);
    EXPECT_EQ(config.preset, ResolutionPreset::UHD_4K);
}

// Ray tracing configuration
TEST_F(ImageGeneratorTest, SetRayTracingQuality) {
    generator.set_ray_tracing_quality(RayTracingQuality::ULTRA);
    EXPECT_EQ(generator.get_ray_tracing_quality(), RayTracingQuality::ULTRA);
}

TEST_F(ImageGeneratorTest, SetRayBounceDe) {
    generator.set_ray_bounce_depth(8);
    EXPECT_EQ(generator.get_ray_bounce_depth(), 8);
}

TEST_F(ImageGeneratorTest, RayTracingQualityVariations) {
    generator.set_ray_tracing_quality(RayTracingQuality::LOW);
    EXPECT_EQ(generator.get_ray_tracing_quality(), RayTracingQuality::LOW);

    generator.set_ray_tracing_quality(RayTracingQuality::HIGH);
    EXPECT_EQ(generator.get_ray_tracing_quality(), RayTracingQuality::HIGH);
}

// Anti-aliasing configuration
TEST_F(ImageGeneratorTest, SetAAQuality) {
    generator.set_aa_quality(AAQuality::MSAA_8X);
    EXPECT_EQ(generator.get_aa_quality(), AAQuality::MSAA_8X);
    EXPECT_EQ(generator.get_aa_samples(), 8);
}

TEST_F(ImageGeneratorTest, AAQualitySamples) {
    generator.set_aa_quality(AAQuality::MSAA_2X);
    EXPECT_EQ(generator.get_aa_samples(), 2);

    generator.set_aa_quality(AAQuality::MSAA_4X);
    EXPECT_EQ(generator.get_aa_samples(), 4);

    generator.set_aa_quality(AAQuality::MSAA_16X);
    EXPECT_EQ(generator.get_aa_samples(), 16);
}

// Tone mapping
TEST_F(ImageGeneratorTest, ToneMapping) {
    generator.set_tone_mapping_enabled(true);
    EXPECT_TRUE(generator.is_tone_mapping_enabled());

    generator.set_tone_mapping_enabled(false);
    EXPECT_FALSE(generator.is_tone_mapping_enabled());
}

TEST_F(ImageGeneratorTest, SetExposure) {
    generator.set_exposure(2.0f);
    EXPECT_EQ(generator.get_exposure(), 2.0f);
}

TEST_F(ImageGeneratorTest, SetGamma) {
    generator.set_gamma(2.2f);
    EXPECT_EQ(generator.get_gamma(), 2.2f);
}

TEST_F(ImageGeneratorTest, ExposureClamp) {
    generator.set_exposure(100.0f); // Too high
    EXPECT_LE(generator.get_exposure(), 10.0f);

    generator.set_exposure(0.001f); // Too low
    EXPECT_GE(generator.get_exposure(), 0.1f);
}

// Denoising
TEST_F(ImageGeneratorTest, Denoising) {
    generator.set_denoise_enabled(true);
    EXPECT_TRUE(generator.is_denoise_enabled());

    generator.set_denoise_enabled(false);
    EXPECT_FALSE(generator.is_denoise_enabled());
}

TEST_F(ImageGeneratorTest, DenoiserSamples) {
    generator.set_denoiser_samples(5);
    EXPECT_EQ(generator.get_denoiser_samples(), 5);
}

// Depth of Field
TEST_F(ImageGeneratorTest, DepthOfField) {
    generator.set_dof_enabled(true);
    EXPECT_TRUE(generator.is_dof_enabled());
}

TEST_F(ImageGeneratorTest, DOFAperture) {
    generator.set_dof_aperture(5.6f);
    EXPECT_EQ(generator.get_dof_aperture(), 5.6f);
}

TEST_F(ImageGeneratorTest, DOFFocusDistance) {
    generator.set_dof_focus_distance(10.0f);
    EXPECT_EQ(generator.get_dof_focus_distance(), 10.0f);
}

// Motion Blur
TEST_F(ImageGeneratorTest, MotionBlur) {
    generator.set_motion_blur_enabled(true);
    EXPECT_TRUE(generator.is_motion_blur_enabled());

    generator.set_motion_blur_enabled(false);
    EXPECT_FALSE(generator.is_motion_blur_enabled());
}

// Rendering statistics
TEST_F(ImageGeneratorTest, RenderTime) {
    ImageBuffer buffer = generator.render_to_buffer(default_config);
    float render_time = generator.get_render_time();

    EXPECT_GE(render_time, 0.0f);
}

TEST_F(ImageGeneratorTest, FrameRate) {
    generator.render_to_buffer(default_config);
    float fps = generator.get_frame_rate();

    EXPECT_GT(fps, 0.0f);
}

TEST_F(ImageGeneratorTest, SamplesRendered) {
    ImageBuffer buffer = generator.render_to_buffer(default_config);
    int samples = generator.get_samples_rendered();

    EXPECT_GT(samples, 0);
}

// Quality metrics
TEST_F(ImageGeneratorTest, EstimateQualityScore) {
    float score = generator.estimate_quality_score(default_config);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

TEST_F(ImageGeneratorTest, QualityScoreVariation) {
    ImageExportConfig low_quality = default_config;
    low_quality.ray_tracing = RayTracingQuality::NONE;
    low_quality.aa_quality = AAQuality::NONE;

    ImageExportConfig high_quality = default_config;
    high_quality.ray_tracing = RayTracingQuality::ULTRA;
    high_quality.aa_quality = AAQuality::MSAA_16X;

    float low_score = generator.estimate_quality_score(low_quality);
    float high_score = generator.estimate_quality_score(high_quality);

    EXPECT_LT(low_score, high_score);
}

// Configuration validation
TEST_F(ImageGeneratorTest, ValidateConfig) {
    EXPECT_TRUE(generator.validate_config(default_config));
}

TEST_F(ImageGeneratorTest, ValidateInvalidResolution) {
    ImageExportConfig invalid = default_config;
    invalid.width = 10; // Too small

    EXPECT_FALSE(generator.validate_config(invalid));
}

// Memory management
TEST_F(ImageGeneratorTest, MemoryUsage) {
    size_t usage = generator.get_memory_usage();
    EXPECT_GT(usage, 0);
}

TEST_F(ImageGeneratorTest, ClearCache) {
    generator.render_to_buffer(default_config);
    generator.clear_cache();
    // Should not crash
}

// Complex scenarios
TEST_F(ImageGeneratorTest, RenderHighQuality4K) {
    ImageExportConfig config = generator.create_config_from_preset(ResolutionPreset::UHD_4K);
    config.ray_tracing = RayTracingQuality::HIGH;
    config.aa_quality = AAQuality::MSAA_8X;
    config.enable_depth_of_field = true;
    config.use_denoise = true;

    ImageBuffer buffer = generator.render_to_buffer(config);

    EXPECT_EQ(buffer.width, 3840);
    EXPECT_EQ(buffer.height, 2160);
    EXPECT_FALSE(buffer.data.empty());
}

TEST_F(ImageGeneratorTest, MultipleExports) {
    for (int i = 0; i < 5; ++i) {
        ImageExportConfig config = default_config;
        bool result = generator.export_image("/tmp/test_export_" + std::to_string(i) + ".png", config);
        EXPECT_TRUE(result);
    }
}
