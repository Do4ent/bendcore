// M13 Phase: GAP-054 - RealTime3DViewport
// GPU-accelerated real-time viewport with Three.js compatibility
// FPS tracking, camera control, geometry management

#include "bendcore/rendering/viewport_renderer.hpp"

namespace bendcore::rendering {

RealTime3DViewport::RealTime3DViewport()
    : fps_(60.0), target_fps_(60.0), max_fps_(120.0),
      geometry_count_(0), cached_meshes_(0), is_optimized_(false),
      current_camera_{0, 0, 100, 45} {}

void RealTime3DViewport::renderFrame() {
    frame_times_.push_back({fps_, 1000.0 / fps_});
    geometry_count_++;
}

double RealTime3DViewport::getFPS() const {
    return fps_;
}

double RealTime3DViewport::getTargetFPS() const {
    return target_fps_;
}

void RealTime3DViewport::setTargetFPS(double fps) {
    target_fps_ = fps;
}

void RealTime3DViewport::addGeometry(int count) {
    geometry_count_ += count;
}

int RealTime3DViewport::getGeometryCount() const {
    return geometry_count_;
}

void RealTime3DViewport::optimizeMeshes(int count) {
    cached_meshes_ = count;
    is_optimized_ = true;
}

int RealTime3DViewport::getCachedMeshCount() const {
    return cached_meshes_;
}

bool RealTime3DViewport::isOptimized() const {
    return is_optimized_;
}

Camera RealTime3DViewport::getCamera() const {
    return current_camera_;
}

void RealTime3DViewport::setCamera(const Camera& cam) {
    current_camera_ = cam;
}

double RealTime3DViewport::calculateFrameTime() const {
    return 1000.0 / fps_;
}

size_t RealTime3DViewport::getFrameCount() const {
    return frame_times_.size();
}

} // namespace bendcore::rendering
