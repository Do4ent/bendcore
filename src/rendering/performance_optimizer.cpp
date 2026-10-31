#include "bendcore/rendering/performance_optimizer.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace bendcore::rendering {

struct PerformanceOptimizer::Impl {
    bool is_initialized = false;
    int screen_width = 1920;
    int screen_height = 1080;
};

PerformanceOptimizer::PerformanceOptimizer() : pimpl_(std::make_unique<Impl>()) {
    last_frame_time_ = std::chrono::high_resolution_clock::now();

    // Initialize default LOD levels
    LODLevel level0;
    level0.distance = 0.0f;
    level0.vertex_reduction = 0;
    level0.triangle_reduction = 0;

    LODLevel level1;
    level1.distance = 50.0f;
    level1.vertex_reduction = 25;
    level1.triangle_reduction = 25;

    LODLevel level2;
    level2.distance = 100.0f;
    level2.vertex_reduction = 50;
    level2.triangle_reduction = 50;

    lod_config_.levels = {level0, level1, level2};
}

PerformanceOptimizer::~PerformanceOptimizer() {
    shutdown();
}

void PerformanceOptimizer::initialize() {
    if (pimpl_->is_initialized) return;
    pimpl_->is_initialized = true;
}

void PerformanceOptimizer::shutdown() {
    if (!pimpl_->is_initialized) return;
    frame_times_.clear();
    pimpl_->is_initialized = false;
}

void PerformanceOptimizer::set_lod_config(const LODConfig& config) {
    lod_config_ = config;
    update_lod_levels();
}

LODConfig PerformanceOptimizer::get_lod_config() const {
    return lod_config_;
}

void PerformanceOptimizer::enable_lod(bool enabled) {
    lod_config_.enabled = enabled;
}

bool PerformanceOptimizer::is_lod_enabled() const {
    return lod_config_.enabled;
}

int PerformanceOptimizer::get_lod_level_for_distance(float distance) const {
    if (!lod_config_.enabled || lod_config_.levels.empty()) {
        return 0;
    }

    for (size_t i = lod_config_.levels.size() - 1; i >= 0; --i) {
        if (distance >= lod_config_.levels[i].distance) {
            return static_cast<int>(i);
        }
    }

    return 0;
}

void PerformanceOptimizer::set_culling_strategy(CullingStrategy strategy) {
    culling_strategy_ = strategy;
}

CullingStrategy PerformanceOptimizer::get_culling_strategy() const {
    return culling_strategy_;
}

void PerformanceOptimizer::perform_frustum_culling(const glm::mat4& projection_matrix,
                                                   const glm::vec3& camera_position) {
    // Frustum culling implementation
    // This is a placeholder - real implementation would extract frustum planes
    // and test bounding boxes against them
    culled_objects_ = 0;
}

void PerformanceOptimizer::perform_distance_culling(const glm::vec3& camera_position,
                                                    float max_distance) {
    // Distance-based culling
    // Objects beyond max_distance from camera are culled
}

int PerformanceOptimizer::get_culled_count() const {
    return culled_objects_;
}

void PerformanceOptimizer::set_batch_config(const BatchConfig& config) {
    batch_config_ = config;
    optimize_batch_size();
}

BatchConfig PerformanceOptimizer::get_batch_config() const {
    return batch_config_;
}

void PerformanceOptimizer::enable_batching(bool enabled) {
    batch_config_.enabled = enabled;
}

bool PerformanceOptimizer::is_batching_enabled() const {
    return batch_config_.enabled;
}

void PerformanceOptimizer::batch_render_calls() {
    if (!batch_config_.enabled) {
        batch_count_ = 1; // No batching
        return;
    }

    // Calculate batches based on max batch size
    // In a real implementation, this would group geometry by material, shader, etc.
    batch_count_ = std::max(1, draw_call_count_ / batch_config_.max_batch_size);
}

void PerformanceOptimizer::update_frame_stats() {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_frame_time_).count();

    frame_time_ms_ = static_cast<float>(elapsed);
    last_frame_time_ = now;

    if (frame_time_ms_ > 0.0f) {
        current_fps_ = 1000.0f / frame_time_ms_;
    }

    // Keep rolling average of frame times
    frame_times_.push_back(frame_time_ms_);
    if (frame_times_.size() > 300) {
        frame_times_.erase(frame_times_.begin());
    }

    // Collect performance data if profiling is enabled
    if (profiling_enabled_) {
        collect_performance_data();
    }
}

float PerformanceOptimizer::get_fps() const {
    return current_fps_;
}

float PerformanceOptimizer::get_frame_time() const {
    return frame_time_ms_;
}

float PerformanceOptimizer::get_average_fps(int frame_window) const {
    if (frame_times_.empty()) return 0.0f;

    size_t count = std::min(static_cast<size_t>(frame_window), frame_times_.size());
    float sum = 0.0f;

    for (size_t i = frame_times_.size() - count; i < frame_times_.size(); ++i) {
        if (frame_times_[i] > 0.0f) {
            sum += 1000.0f / frame_times_[i];
        }
    }

    return sum / static_cast<float>(count);
}

void PerformanceOptimizer::enable_profiling(bool enabled) {
    profiling_enabled_ = enabled;
}

bool PerformanceOptimizer::is_profiling_enabled() const {
    return profiling_enabled_;
}

PerformanceStats PerformanceOptimizer::get_stats() const {
    return stats_;
}

void PerformanceOptimizer::set_target_fps(float target) {
    target_fps_ = glm::max(1.0f, target);
}

float PerformanceOptimizer::get_target_fps() const {
    return target_fps_;
}

void PerformanceOptimizer::auto_adjust_quality() {
    float fps_ratio = current_fps_ / target_fps_;

    if (fps_ratio < 0.9f) {
        // Performance is below target, reduce quality
        quality_level_ = glm::max(0.1f, quality_level_ - 0.05f);
    } else if (fps_ratio > 1.1f) {
        // Performance is above target, can increase quality
        quality_level_ = glm::min(1.0f, quality_level_ + 0.05f);
    }
}

void PerformanceOptimizer::set_quality_level(float level) {
    quality_level_ = glm::clamp(level, 0.0f, 1.0f);
}

int PerformanceOptimizer::get_draw_call_count() const {
    return draw_call_count_;
}

int PerformanceOptimizer::get_batch_count() const {
    return batch_count_;
}

void PerformanceOptimizer::optimize_draw_calls() {
    // Reduce draw calls through:
    // 1. Batching similar geometry
    // 2. Instancing
    // 3. LOD switching
    // 4. Culling

    batch_render_calls();
}

size_t PerformanceOptimizer::get_memory_usage() const {
    return stats_.memory_used;
}

void PerformanceOptimizer::optimize_memory() {
    // Free unused resources, compact buffers, etc.
    // This is a placeholder implementation
}

float PerformanceOptimizer::get_memory_fragmentation() const {
    // Return fragmentation ratio 0.0 - 1.0
    return 0.0f;
}

void PerformanceOptimizer::optimize_viewport_rendering(int screen_width, int screen_height) {
    pimpl_->screen_width = screen_width;
    pimpl_->screen_height = screen_height;

    // Adjust rendering strategy based on screen resolution
    // For very large screens, may need more aggressive culling/LOD
}

void PerformanceOptimizer::set_screen_resolution(int width, int height) {
    pimpl_->screen_width = width;
    pimpl_->screen_height = height;
}

void PerformanceOptimizer::update_lod_levels() {
    // Ensure LOD levels are sorted by distance
    std::sort(lod_config_.levels.begin(), lod_config_.levels.end(),
        [](const LODLevel& a, const LODLevel& b) {
            return a.distance < b.distance;
        });
}

void PerformanceOptimizer::optimize_batch_size() {
    // Dynamically adjust batch size based on performance
    if (current_fps_ < target_fps_ * 0.8f) {
        batch_config_.max_batch_size = std::max(256, batch_config_.max_batch_size / 2);
    } else if (current_fps_ > target_fps_ * 1.2f) {
        batch_config_.max_batch_size = std::min(4096, batch_config_.max_batch_size * 2);
    }
}

void PerformanceOptimizer::collect_performance_data() {
    stats_.fps = current_fps_;
    stats_.frame_time_ms = frame_time_ms_;
    stats_.visible_objects = visible_objects_;
    stats_.culled_objects = culled_objects_;
    stats_.batch_count = batch_count_;
    stats_.draw_call_count = draw_call_count_;
}

} // namespace bendcore::rendering
