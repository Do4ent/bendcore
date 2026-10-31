#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <chrono>

namespace bendcore::rendering {

// Level of Detail configuration
struct LODLevel {
    float distance = 0.0f;
    int vertex_reduction = 0;  // Percentage
    int triangle_reduction = 0; // Percentage
};

// LOD configuration
struct LODConfig {
    bool enabled = true;
    std::vector<LODLevel> levels;
    float transition_distance = 1.0f;
};

// Culling strategy
enum class CullingStrategy {
    NONE,
    FRUSTUM,
    OCCLUSION,
    DISTANCE,
    COMBINED
};

// Batch configuration
struct BatchConfig {
    bool enabled = true;
    int max_batch_size = 1024;
    bool sort_by_material = true;
    bool sort_by_shader = true;
};

// Performance statistics
struct PerformanceStats {
    float fps = 0.0f;
    float frame_time_ms = 0.0f;
    float cpu_time_ms = 0.0f;
    float gpu_time_ms = 0.0f;

    int visible_objects = 0;
    int culled_objects = 0;
    int batch_count = 0;
    int draw_call_count = 0;

    int vertices_rendered = 0;
    int triangles_rendered = 0;
    size_t memory_used = 0;
};

// Performance optimizer for real-time rendering
class PerformanceOptimizer {
public:
    PerformanceOptimizer();
    ~PerformanceOptimizer();

    // Initialization
    void initialize();
    void shutdown();

    // LOD management
    void set_lod_config(const LODConfig& config);
    LODConfig get_lod_config() const;
    void enable_lod(bool enabled);
    bool is_lod_enabled() const;
    int get_lod_level_for_distance(float distance) const;

    // Culling
    void set_culling_strategy(CullingStrategy strategy);
    CullingStrategy get_culling_strategy() const;
    void perform_frustum_culling(const glm::mat4& projection_matrix,
                                const glm::vec3& camera_position);
    void perform_distance_culling(const glm::vec3& camera_position, float max_distance);
    int get_culled_count() const;

    // Render batching
    void set_batch_config(const BatchConfig& config);
    BatchConfig get_batch_config() const;
    void enable_batching(bool enabled);
    bool is_batching_enabled() const;
    void batch_render_calls();

    // FPS monitoring
    void update_frame_stats();
    float get_fps() const;
    float get_frame_time() const;
    float get_average_fps(int frame_window = 60) const;

    // Detailed statistics
    void enable_profiling(bool enabled);
    bool is_profiling_enabled() const;
    PerformanceStats get_stats() const;

    // Performance tuning
    void set_target_fps(float target);
    float get_target_fps() const;
    void auto_adjust_quality();
    void set_quality_level(float level); // 0.0 - 1.0

    // Draw call optimization
    int get_draw_call_count() const;
    int get_batch_count() const;
    void optimize_draw_calls();

    // Memory management
    size_t get_memory_usage() const;
    void optimize_memory();
    float get_memory_fragmentation() const;

    // Viewport optimization
    void optimize_viewport_rendering(int screen_width, int screen_height);
    void set_screen_resolution(int width, int height);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    LODConfig lod_config_;
    CullingStrategy culling_strategy_ = CullingStrategy::FRUSTUM;
    BatchConfig batch_config_;

    float target_fps_ = 60.0f;
    float current_fps_ = 0.0f;
    float frame_time_ms_ = 0.0f;
    float quality_level_ = 1.0f;

    int visible_objects_ = 0;
    int culled_objects_ = 0;
    int draw_call_count_ = 0;
    int batch_count_ = 0;

    bool profiling_enabled_ = false;
    PerformanceStats stats_;

    std::vector<float> frame_times_;
    std::chrono::high_resolution_clock::time_point last_frame_time_;

    void update_lod_levels();
    void optimize_batch_size();
    void collect_performance_data();
};

} // namespace bendcore::rendering
