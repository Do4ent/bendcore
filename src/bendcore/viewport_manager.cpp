/**
 * BendCore Viewport Manager Implementation
 */

#include "viewport_manager.hpp"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace bendcore {

void ViewportManager::set_camera_state(const CameraState& state) {
  camera_state_ = state;

  // Clamp distance
  camera_state_.distance = std::max(MIN_DISTANCE, std::min(MAX_DISTANCE, camera_state_.distance));

  // Clamp pitch to prevent gimbal lock
  camera_state_.pitch = std::max(-89.0f, std::min(89.0f, camera_state_.pitch));
}

void ViewportManager::set_camera_position(float x, float y, float z) {
  camera_state_.position = glm::vec3(x, y, z);
}

void ViewportManager::orbit_camera(float delta_pitch, float delta_yaw) {
  camera_state_.pitch += delta_pitch * ROTATION_SENSITIVITY;
  camera_state_.yaw += delta_yaw * ROTATION_SENSITIVITY;

  // Clamp pitch to prevent gimbal lock
  camera_state_.pitch = std::max(-89.0f, std::min(89.0f, camera_state_.pitch));

  // Wrap yaw
  while (camera_state_.yaw > 360.0f) {
    camera_state_.yaw -= 360.0f;
  }
  while (camera_state_.yaw < 0.0f) {
    camera_state_.yaw += 360.0f;
  }
}

void ViewportManager::pan_camera(float delta_x, float delta_y) {
  // Calculate pan amount based on distance and FOV
  float pan_amount = (camera_state_.distance / 100.0f) * PAN_SENSITIVITY;

  // Apply pan to target
  camera_state_.target.x += delta_x * pan_amount;
  camera_state_.target.y += delta_y * pan_amount;
}

void ViewportManager::zoom_camera(float factor) {
  camera_state_.distance /= factor;
  camera_state_.distance =
      std::max(MIN_DISTANCE, std::min(MAX_DISTANCE, camera_state_.distance));
}

void ViewportManager::fit_to_view(const std::vector<std::string>& object_ids) {
  // In a full implementation, this would get bounding boxes for specified objects
  // and adjust camera to fit them all in view

  // Default: go to isometric view
  camera_state_.pitch = 35.26f;
  camera_state_.yaw = 45.0f;
  camera_state_.distance = 120.0f;
  camera_state_.target = glm::vec3(0, 0, 0);
}

void ViewportManager::home_view() {
  camera_state_.position = glm::vec3(0, 0, 100);
  camera_state_.target = glm::vec3(0, 0, 0);
  camera_state_.distance = 100;
  camera_state_.pitch = 0;
  camera_state_.yaw = 0;
  camera_state_.fov = 50;
}

void ViewportManager::switch_to_view(const std::string& view_name) {
  if (view_name == "Front") {
    camera_state_.pitch = 0;
    camera_state_.yaw = 0;
    camera_state_.distance = 100;
  } else if (view_name == "Top") {
    camera_state_.pitch = 90;
    camera_state_.yaw = 0;
    camera_state_.distance = 100;
  } else if (view_name == "Right") {
    camera_state_.pitch = 0;
    camera_state_.yaw = 90;
    camera_state_.distance = 100;
  } else if (view_name == "Isometric") {
    camera_state_.pitch = 35.26f;
    camera_state_.yaw = 45.0f;
    camera_state_.distance = 120.0f;
  }

  camera_state_.target = glm::vec3(0, 0, 0);
}

void ViewportManager::handle_window_resize(int width, int height) {
  viewport_width_ = std::max(1, width);
  viewport_height_ = std::max(1, height);
}

json ViewportManager::to_json() const {
  return json{
      {"camera", camera_state_.to_json()},
      {"viewport", json{
                       {"width", viewport_width_},
                       {"height", viewport_height_},
                   }},
      {"rendering", json{
                        {"show_grid", show_grid_},
                        {"show_axes", show_axes_},
                        {"show_origin", show_origin_},
                    }},
  };
}

}  // namespace bendcore
