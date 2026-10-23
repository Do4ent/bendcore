/**
 * BendCore Viewport Manager
 * Manages viewport state and rendering coordination
 */

#pragma once

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>

namespace bendcore {

using json = nlohmann::json;

/**
 * CameraState: Represents camera position and orientation
 */
struct CameraState {
  glm::vec3 position{0, 0, 100};
  glm::vec3 target{0, 0, 0};
  float distance{100};
  float pitch{0};    // Vertical rotation in degrees
  float yaw{0};      // Horizontal rotation in degrees
  float fov{50};
  float near_plane{0.1f};
  float far_plane{20000.0f};

  json to_json() const {
    return json{
        {"position", json{{"x", position.x}, {"y", position.y}, {"z", position.z}}},
        {"target", json{{"x", target.x}, {"y", target.y}, {"z", target.z}}},
        {"distance", distance},
        {"pitch", pitch},
        {"yaw", yaw},
        {"fov", fov},
        {"near_plane", near_plane},
        {"far_plane", far_plane},
    };
  }
};

/**
 * ViewportManager: Manages viewport rendering state
 */
class ViewportManager {
 public:
  static ViewportManager& instance() {
    static ViewportManager manager;
    return manager;
  }

  // Camera management
  void set_camera_state(const CameraState& state);
  CameraState get_camera_state() const { return camera_state_; }

  void set_camera_position(float x, float y, float z);
  void orbit_camera(float delta_pitch, float delta_yaw);
  void pan_camera(float delta_x, float delta_y);
  void zoom_camera(float factor);

  // View management
  void fit_to_view(const std::vector<std::string>& object_ids = {});
  void home_view();
  void switch_to_view(const std::string& view_name);

  // Rendering hints
  void set_show_grid(bool show) { show_grid_ = show; }
  void set_show_axes(bool show) { show_axes_ = show; }
  void set_show_origin(bool show) { show_origin_ = show; }
  bool get_show_grid() const { return show_grid_; }
  bool get_show_axes() const { return show_axes_; }
  bool get_show_origin() const { return show_origin_; }

  // Window management
  void handle_window_resize(int width, int height);
  int get_viewport_width() const { return viewport_width_; }
  int get_viewport_height() const { return viewport_height_; }

  // JSON serialization
  json to_json() const;

 private:
  ViewportManager() = default;

  CameraState camera_state_;
  int viewport_width_ = 1200;
  int viewport_height_ = 800;

  // Rendering flags
  bool show_grid_ = true;
  bool show_axes_ = true;
  bool show_origin_ = true;

  // Constraints
  static constexpr float MIN_DISTANCE = 1.0f;
  static constexpr float MAX_DISTANCE = 10000.0f;
  static constexpr float ROTATION_SENSITIVITY = 0.5f;
  static constexpr float PAN_SENSITIVITY = 0.1f;
  static constexpr float ZOOM_SENSITIVITY = 1.1f;
};

}  // namespace bendcore
