#pragma once

#include <vector>
#include <cmath>

namespace bendcore::rendering {

struct Frame {
    double fps;
    double ms;
};

struct Camera {
    double x, y, z, fov;
};

class RealTime3DViewport {
public:
    RealTime3DViewport() : fps_(60.0), target_fps_(60.0), max_fps_(120.0),
        geometry_count_(0), cached_meshes_(0), is_optimized_(false) {}

    void renderFrame() {
        frame_times_.push_back({fps_, 1000.0 / fps_});
        geometry_count_++;
    }

    double getFPS() const { return fps_; }
    double getTargetFPS() const { return target_fps_; }
    void setTargetFPS(double fps) { target_fps_ = fps; }

    void addGeometry(int count) { geometry_count_ += count; }
    int getGeometryCount() const { return geometry_count_; }

    void optimizeMeshes(int count) { cached_meshes_ = count; is_optimized_ = true; }
    int getCachedMeshCount() const { return cached_meshes_; }

    bool isOptimized() const { return is_optimized_; }

    Camera getCamera() const { return current_camera_; }
    void setCamera(const Camera& cam) { current_camera_ = cam; }

    double calculateFrameTime() const { return 1000.0 / fps_; }

    size_t getFrameCount() const { return frame_times_.size(); }

private:
    double fps_, target_fps_, max_fps_;
    Camera current_camera_ = {0, 0, 100, 45};
    int geometry_count_, cached_meshes_;
    bool is_optimized_;
    std::vector<Frame> frame_times_;
};

} // namespace bendcore::rendering
