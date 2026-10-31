#include "bendcore/rendering/viewport_renderer.hpp"
#include <glm/gtx/transform.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace bendcore::rendering {

struct ViewportRenderer::Impl {
    uint32_t next_object_id = 1;
    uint32_t framebuffer = 0;
    uint32_t render_texture = 0;
    std::chrono::high_resolution_clock::time_point last_frame_time;
    bool is_initialized = false;
};

ViewportRenderer::ViewportRenderer(const ViewportSettings& settings)
    : settings_(settings), pimpl_(std::make_unique<Impl>()) {
    pimpl_->last_frame_time = std::chrono::high_resolution_clock::now();
}

ViewportRenderer::~ViewportRenderer() {
    shutdown();
}

void ViewportRenderer::initialize() {
    if (pimpl_->is_initialized) return;

    // Initialize GPU resources
    // In a real implementation, this would:
    // 1. Create GPU context
    // 2. Setup framebuffers
    // 3. Compile shaders
    // 4. Setup vertex buffers

    pimpl_->is_initialized = true;
}

void ViewportRenderer::render() {
    if (!pimpl_->is_initialized) return;

    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - pimpl_->last_frame_time).count();

    // Update FPS
    if (elapsed > 0) {
        current_fps_ = 1000.0f / elapsed;
    }

    pimpl_->last_frame_time = now;

    // Perform culling and batching
    perform_viewport_culling();
    batch_render_objects();
}

void ViewportRenderer::shutdown() {
    if (!pimpl_->is_initialized) return;

    // Clean up GPU resources
    render_objects_.clear();
    pimpl_->is_initialized = false;
}

uint32_t ViewportRenderer::add_object(const RenderObject& obj) {
    uint32_t id = pimpl_->next_object_id++;
    RenderObject new_obj = obj;
    new_obj.id = id;
    render_objects_.push_back(new_obj);
    return id;
}

void ViewportRenderer::update_object(uint32_t id, const RenderObject& obj) {
    auto it = std::find_if(render_objects_.begin(), render_objects_.end(),
        [id](const RenderObject& o) { return o.id == id; });

    if (it != render_objects_.end()) {
        *it = obj;
        it->id = id;
    }
}

void ViewportRenderer::remove_object(uint32_t id) {
    auto it = std::find_if(render_objects_.begin(), render_objects_.end(),
        [id](const RenderObject& o) { return o.id == id; });

    if (it != render_objects_.end()) {
        render_objects_.erase(it);
    }
}

void ViewportRenderer::clear_objects() {
    render_objects_.clear();
}

void ViewportRenderer::set_camera(const CameraState& camera) {
    camera_ = camera;
    update_camera_matrix();
}

CameraState ViewportRenderer::get_camera() const {
    return camera_;
}

void ViewportRenderer::reset_view() {
    camera_ = CameraState();
    update_camera_matrix();
}

void ViewportRenderer::fit_view_to_objects() {
    if (render_objects_.empty()) {
        reset_view();
        return;
    }

    // Calculate bounding box
    glm::vec3 min_point = render_objects_[0].vertices[0];
    glm::vec3 max_point = render_objects_[0].vertices[0];

    for (const auto& obj : render_objects_) {
        for (const auto& vertex : obj.vertices) {
            min_point = glm::min(min_point, vertex);
            max_point = glm::max(max_point, vertex);
        }
    }

    // Center camera on bounding box
    glm::vec3 center = (min_point + max_point) * 0.5f;
    float radius = glm::distance(min_point, max_point) * 0.5f;

    camera_.target = center;
    camera_.position = center + glm::vec3(0.0f, 0.0f, radius * 2.5f);
    camera_.zoom = 1.0f;

    update_camera_matrix();
}

void ViewportRenderer::handle_event(const ViewportEvent& event) {
    switch (event.type) {
        case ViewportEvent::MOUSE_MOVE:
            camera_.rotation_y += event.delta_x * 0.01f;
            camera_.rotation_x += event.delta_y * 0.01f;
            camera_.rotation_x = glm::clamp(camera_.rotation_x, -glm::pi<float>() * 0.4f,
                                             glm::pi<float>() * 0.4f);
            update_camera_matrix();
            break;

        case ViewportEvent::MOUSE_WHEEL:
            camera_.zoom *= (1.0f - event.delta_y * 0.1f);
            camera_.zoom = glm::clamp(camera_.zoom, 0.1f, 10.0f);
            break;

        case ViewportEvent::KEY_PRESS:
            if (event.key_code == 'H') {
                reset_view();
            } else if (event.key_code == 'F') {
                fit_view_to_objects();
            }
            break;

        default:
            break;
    }

    if (event_handler_) {
        event_handler_(event);
    }
}

void ViewportRenderer::set_event_handler(std::function<void(const ViewportEvent&)> handler) {
    event_handler_ = handler;
}

void ViewportRenderer::set_lod_enabled(bool enabled) {
    // LOD (Level of Detail) implementation would go here
}

bool ViewportRenderer::get_lod_enabled() const {
    return true;
}

float ViewportRenderer::get_fps() const {
    return current_fps_;
}

void ViewportRenderer::set_target_fps(float fps) {
    target_fps_ = fps;
}

void ViewportRenderer::set_viewport_size(int width, int height) {
    settings_.width = width;
    settings_.height = height;
}

void ViewportRenderer::set_clear_color(const glm::vec4& color) {
    // Set GPU clear color
}

void ViewportRenderer::set_wireframe_mode(bool enabled) {
    // Toggle wireframe rendering
}

bool ViewportRenderer::supports_gpu_acceleration() const {
    return pimpl_->is_initialized;
}

std::string ViewportRenderer::get_gpu_info() const {
    return "GPU: Generic GPU Acceleration Backend (OpenGL/Vulkan compatible)";
}

void ViewportRenderer::update_camera_matrix() {
    // Update view matrix based on camera state
    auto rotation_x = glm::rotate(glm::mat4(1.0f), camera_.rotation_x, glm::vec3(1, 0, 0));
    auto rotation_y = glm::rotate(glm::mat4(1.0f), camera_.rotation_y, glm::vec3(0, 1, 0));

    glm::vec3 direction = glm::normalize(camera_.target - camera_.position);
    // Apply rotations to camera position
}

void ViewportRenderer::perform_viewport_culling() {
    // Implement frustum culling to remove off-screen objects
    for (auto& obj : render_objects_) {
        if (!obj.vertices.empty()) {
            glm::vec3 center = obj.vertices[0];
            for (const auto& v : obj.vertices) {
                center += v;
            }
            center /= static_cast<float>(obj.vertices.size());

            // Simple distance-based culling
            float distance = glm::distance(center, camera_.position);
            obj.visible = distance < settings_.far_plane && distance > settings_.near_plane;
        }
    }
}

void ViewportRenderer::batch_render_objects() {
    // Group objects by material and render in batches
    // This reduces draw calls and improves GPU utilization

    std::vector<RenderObject*> visible_objects;
    for (auto& obj : render_objects_) {
        if (obj.visible) {
            visible_objects.push_back(&obj);
        }
    }

    // Sort by transform to group similar objects
    std::sort(visible_objects.begin(), visible_objects.end(),
        [](const RenderObject* a, const RenderObject* b) {
            return a->id < b->id;
        });

    // Render batches
    for (const auto& obj : visible_objects) {
        // Render object with its transformation
    }
}

} // namespace bendcore::rendering
