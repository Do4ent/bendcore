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
    RealTime3DViewport();

    void renderFrame();
    double getFPS() const;
    double getTargetFPS() const;
    void setTargetFPS(double fps);
    void addGeometry(int count);
    int getGeometryCount() const;
    void optimizeMeshes(int count);
    int getCachedMeshCount() const;
    bool isOptimized() const;
    Camera getCamera() const;
    void setCamera(const Camera& cam);
    double calculateFrameTime() const;
    size_t getFrameCount() const;

private:
    double fps_, target_fps_, max_fps_;
    Camera current_camera_;
    int geometry_count_, cached_meshes_;
    bool is_optimized_;
    std::vector<Frame> frame_times_;
};

} // namespace bendcore::rendering
