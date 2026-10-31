#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace bendcore::rendering {

// GPU-accelerated viewport renderer for real-time CAD visualization
// Compatible with Three.js rendering pipeline

struct ViewportSettings {
    int width = 1920;
    int height = 1080;
    float fov = 45.0f;
    float near_plane = 0.1f;
    float far_plane = 10000.0f;
    bool enable_vsync = true;
    bool enable_msaa = true;
    int msaa_samples = 4;
};

struct RenderObject {
    uint32_t id;
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;
    glm::mat4 transform;
    bool visible = true;
};

struct CameraState {
    glm::vec3 position{0.0f, 0.0f, 10.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float rotation_x = 0.0f;
    float rotation_y = 0.0f;
    float zoom = 1.0f;
};

struct ViewportEvent {
    enum Type {
        MOUSE_MOVE,
        MOUSE_CLICK,
        MOUSE_WHEEL,
        KEY_PRESS,
        TOUCH
    };

    Type type;
    float x = 0.0f;
    float y = 0.0f;
    float delta_x = 0.0f;
    float delta_y = 0.0f;
    int button = -1;
    int key_code = 0;
};

class ViewportRenderer {
public:
    ViewportRenderer(const ViewportSettings& settings);
    ~ViewportRenderer();

    // Core rendering
    void initialize();
    void render();
    void shutdown();

    // Object management
    uint32_t add_object(const RenderObject& obj);
    void update_object(uint32_t id, const RenderObject& obj);
    void remove_object(uint32_t id);
    void clear_objects();

    // Camera control
    void set_camera(const CameraState& camera);
    CameraState get_camera() const;
    void reset_view();
    void fit_view_to_objects();

    // Event handling
    void handle_event(const ViewportEvent& event);
    void set_event_handler(std::function<void(const ViewportEvent&)> handler);

    // Performance
    void set_lod_enabled(bool enabled);
    bool get_lod_enabled() const;
    float get_fps() const;
    void set_target_fps(float fps);

    // Rendering options
    void set_viewport_size(int width, int height);
    void set_clear_color(const glm::vec4& color);
    void set_wireframe_mode(bool enabled);

    // GPU backend queries
    bool supports_gpu_acceleration() const;
    std::string get_gpu_info() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

    ViewportSettings settings_;
    CameraState camera_;
    std::vector<RenderObject> render_objects_;
    float current_fps_ = 0.0f;
    float target_fps_ = 60.0f;
    std::function<void(const ViewportEvent&)> event_handler_;

    void update_camera_matrix();
    void perform_viewport_culling();
    void batch_render_objects();
};

} // namespace bendcore::rendering
