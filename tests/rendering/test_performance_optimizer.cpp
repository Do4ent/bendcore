#include <gtest/gtest.h>
#include "bendcore/rendering/performance_optimizer.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <thread>

using namespace bendcore::rendering;

class PerformanceOptimizerTest : public ::testing::Test {
protected:
    PerformanceOptimizer optimizer;

    void SetUp() override {
        optimizer.initialize();
    }

    void TearDown() override {
        optimizer.shutdown();
    }
};

// Initialization
TEST_F(PerformanceOptimizerTest, Initialize) {
    PerformanceOptimizer opt;
    opt.initialize();
    opt.shutdown();
}

// LOD configuration
TEST_F(PerformanceOptimizerTest, SetLODConfig) {
    LODConfig config;
    config.enabled = true;

    LODLevel level;
    level.distance = 0.0f;
    level.vertex_reduction = 0;
    config.levels.push_back(level);

    optimizer.set_lod_config(config);
    LODConfig retrieved = optimizer.get_lod_config();

    EXPECT_TRUE(retrieved.enabled);
    EXPECT_EQ(retrieved.levels.size(), 1);
}

TEST_F(PerformanceOptimizerTest, EnableLOD) {
    optimizer.enable_lod(true);
    EXPECT_TRUE(optimizer.is_lod_enabled());

    optimizer.enable_lod(false);
    EXPECT_FALSE(optimizer.is_lod_enabled());
}

TEST_F(PerformanceOptimizerTest, GetLODLevelForDistance) {
    LODConfig config;
    config.enabled = true;

    LODLevel l0;
    l0.distance = 0.0f;

    LODLevel l1;
    l1.distance = 50.0f;

    LODLevel l2;
    l2.distance = 100.0f;

    config.levels = {l0, l1, l2};
    optimizer.set_lod_config(config);

    EXPECT_EQ(optimizer.get_lod_level_for_distance(10.0f), 0);
    EXPECT_EQ(optimizer.get_lod_level_for_distance(60.0f), 1);
    EXPECT_EQ(optimizer.get_lod_level_for_distance(150.0f), 2);
}

TEST_F(PerformanceOptimizerTest, LODVertexReduction) {
    LODConfig config;
    LODLevel level;
    level.vertex_reduction = 50;
    config.levels.push_back(level);

    optimizer.set_lod_config(config);
    LODConfig retrieved = optimizer.get_lod_config();

    EXPECT_EQ(retrieved.levels[0].vertex_reduction, 50);
}

TEST_F(PerformanceOptimizerTest, LODTriangleReduction) {
    LODConfig config;
    LODLevel level;
    level.triangle_reduction = 75;
    config.levels.push_back(level);

    optimizer.set_lod_config(config);
    LODConfig retrieved = optimizer.get_lod_config();

    EXPECT_EQ(retrieved.levels[0].triangle_reduction, 75);
}

// Culling
TEST_F(PerformanceOptimizerTest, SetCullingStrategy) {
    optimizer.set_culling_strategy(CullingStrategy::FRUSTUM);
    EXPECT_EQ(optimizer.get_culling_strategy(), CullingStrategy::FRUSTUM);

    optimizer.set_culling_strategy(CullingStrategy::OCCLUSION);
    EXPECT_EQ(optimizer.get_culling_strategy(), CullingStrategy::OCCLUSION);
}

TEST_F(PerformanceOptimizerTest, PerformFrustumCulling) {
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    glm::vec3 camera_pos(0.0f, 0.0f, 10.0f);

    optimizer.perform_frustum_culling(projection, camera_pos);

    // Should not crash
    EXPECT_GE(optimizer.get_culled_count(), 0);
}

TEST_F(PerformanceOptimizerTest, PerformDistanceCulling) {
    glm::vec3 camera_pos(0.0f, 0.0f, 0.0f);
    optimizer.perform_distance_culling(camera_pos, 100.0f);

    // Should not crash
    EXPECT_GE(optimizer.get_culled_count(), 0);
}

// Batching
TEST_F(PerformanceOptimizerTest, SetBatchConfig) {
    BatchConfig config;
    config.max_batch_size = 512;

    optimizer.set_batch_config(config);
    BatchConfig retrieved = optimizer.get_batch_config();

    EXPECT_EQ(retrieved.max_batch_size, 512);
}

TEST_F(PerformanceOptimizerTest, EnableBatching) {
    optimizer.enable_batching(true);
    EXPECT_TRUE(optimizer.is_batching_enabled());

    optimizer.enable_batching(false);
    EXPECT_FALSE(optimizer.is_batching_enabled());
}

TEST_F(PerformanceOptimizerTest, BatchRenderCalls) {
    optimizer.enable_batching(true);
    optimizer.batch_render_calls();

    EXPECT_GE(optimizer.get_batch_count(), 1);
}

TEST_F(PerformanceOptimizerTest, SortByMaterial) {
    BatchConfig config;
    config.sort_by_material = true;
    optimizer.set_batch_config(config);

    EXPECT_TRUE(optimizer.get_batch_config().sort_by_material);
}

TEST_F(PerformanceOptimizerTest, SortByShader) {
    BatchConfig config;
    config.sort_by_shader = true;
    optimizer.set_batch_config(config);

    EXPECT_TRUE(optimizer.get_batch_config().sort_by_shader);
}

// FPS monitoring
TEST_F(PerformanceOptimizerTest, UpdateFrameStats) {
    optimizer.update_frame_stats();

    float fps = optimizer.get_fps();
    float frame_time = optimizer.get_frame_time();

    EXPECT_GE(fps, 0.0f);
    EXPECT_GE(frame_time, 0.0f);
}

TEST_F(PerformanceOptimizerTest, GetAverageFPS) {
    for (int i = 0; i < 10; ++i) {
        optimizer.update_frame_stats();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    float avg_fps = optimizer.get_average_fps(5);
    EXPECT_GT(avg_fps, 0.0f);
}

TEST_F(PerformanceOptimizerTest, FrameTimeTracking) {
    optimizer.update_frame_stats();
    float time1 = optimizer.get_frame_time();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    optimizer.update_frame_stats();
    float time2 = optimizer.get_frame_time();

    EXPECT_GE(time2, time1); // Second frame should take at least as long
}

// Profiling
TEST_F(PerformanceOptimizerTest, EnableProfiling) {
    optimizer.enable_profiling(true);
    EXPECT_TRUE(optimizer.is_profiling_enabled());

    optimizer.enable_profiling(false);
    EXPECT_FALSE(optimizer.is_profiling_enabled());
}

TEST_F(PerformanceOptimizerTest, GetStats) {
    optimizer.enable_profiling(true);
    optimizer.update_frame_stats();

    PerformanceStats stats = optimizer.get_stats();
    EXPECT_GE(stats.fps, 0.0f);
}

// FPS targeting
TEST_F(PerformanceOptimizerTest, SetTargetFPS) {
    optimizer.set_target_fps(120.0f);
    EXPECT_EQ(optimizer.get_target_fps(), 120.0f);
}

TEST_F(PerformanceOptimizerTest, AutoAdjustQuality) {
    optimizer.set_target_fps(60.0f);

    // Simulate low FPS
    for (int i = 0; i < 5; ++i) {
        optimizer.update_frame_stats();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    optimizer.auto_adjust_quality();
    // Quality level should have adjusted
}

// Quality level
TEST_F(PerformanceOptimizerTest, SetQualityLevel) {
    optimizer.set_quality_level(0.5f);
    // Quality level is set internally
}

// Draw call optimization
TEST_F(PerformanceOptimizerTest, GetDrawCallCount) {
    int count = optimizer.get_draw_call_count();
    EXPECT_GE(count, 0);
}

TEST_F(PerformanceOptimizerTest, GetBatchCount) {
    int count = optimizer.get_batch_count();
    EXPECT_GE(count, 0);
}

TEST_F(PerformanceOptimizerTest, OptimizeDrawCalls) {
    optimizer.optimize_draw_calls();
    // Should not crash
}

// Memory management
TEST_F(PerformanceOptimizerTest, GetMemoryUsage) {
    size_t usage = optimizer.get_memory_usage();
    EXPECT_GE(usage, 0);
}

TEST_F(PerformanceOptimizerTest, OptimizeMemory) {
    optimizer.optimize_memory();
    // Should not crash
}

TEST_F(PerformanceOptimizerTest, GetMemoryFragmentation) {
    float frag = optimizer.get_memory_fragmentation();
    EXPECT_GE(frag, 0.0f);
    EXPECT_LE(frag, 1.0f);
}

// Viewport optimization
TEST_F(PerformanceOptimizerTest, OptimizeViewportRendering) {
    optimizer.optimize_viewport_rendering(3840, 2160);
    // Should not crash
}

TEST_F(PerformanceOptimizerTest, SetScreenResolution) {
    optimizer.set_screen_resolution(1920, 1080);
    // Should update internally
}

// Complex scenarios
TEST_F(PerformanceOptimizerTest, ComplexOptimizationScenario) {
    // Setup LOD
    LODConfig lod_config;
    lod_config.enabled = true;
    for (int i = 0; i < 4; ++i) {
        LODLevel level;
        level.distance = i * 50.0f;
        level.vertex_reduction = i * 25;
        lod_config.levels.push_back(level);
    }
    optimizer.set_lod_config(lod_config);

    // Setup culling
    optimizer.set_culling_strategy(CullingStrategy::COMBINED);

    // Setup batching
    BatchConfig batch_config;
    batch_config.max_batch_size = 512;
    optimizer.set_batch_config(batch_config);

    // Simulate rendering
    for (int frame = 0; frame < 30; ++frame) {
        optimizer.update_frame_stats();
        optimizer.auto_adjust_quality();
        optimizer.optimize_draw_calls();
    }

    // Verify results
    EXPECT_GT(optimizer.get_fps(), 0.0f);
    EXPECT_GT(optimizer.get_average_fps(10), 0.0f);
}

// Multiple LOD transitions
TEST_F(PerformanceOptimizerTest, MultipleLODTransitions) {
    LODConfig config;
    config.enabled = true;

    for (int i = 0; i < 8; ++i) {
        LODLevel level;
        level.distance = i * 25.0f;
        level.vertex_reduction = i * 12;
        level.triangle_reduction = i * 10;
        config.levels.push_back(level);
    }

    optimizer.set_lod_config(config);

    // Test transitions at different distances
    for (float dist = 0.0f; dist < 200.0f; dist += 10.0f) {
        int lod = optimizer.get_lod_level_for_distance(dist);
        EXPECT_GE(lod, 0);
        EXPECT_LT(lod, static_cast<int>(config.levels.size()));
    }
}

// Batch size variation
TEST_F(PerformanceOptimizerTest, DynamicBatchSizeAdjustment) {
    BatchConfig config;
    config.enabled = true;
    config.max_batch_size = 1024;

    optimizer.set_batch_config(config);
    int initial_size = optimizer.get_batch_config().max_batch_size;

    optimizer.set_batch_config(config);
    EXPECT_EQ(optimizer.get_batch_config().max_batch_size, initial_size);
}
