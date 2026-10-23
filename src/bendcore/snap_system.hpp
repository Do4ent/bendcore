/**
 * BendCore Snap System
 * Manages snap point detection and snap constraints
 */

#pragma once

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace bendcore {

using json = nlohmann::json;

/**
 * SnapMode enumeration
 */
enum class SnapMode {
  ENDPOINT,
  CENTER,
  GRID,
  AXIS,
  FACE,
  EDGE,
};

std::string snap_mode_to_string(SnapMode mode);
SnapMode snap_mode_from_string(const std::string& str);

/**
 * SnapPoint: Detected snap point in space
 */
struct SnapPoint {
  glm::vec3 position;
  SnapMode type;
  glm::vec3 normal{0, 0, 1};
  float distance{0};

  json to_json() const {
    return json{
        {"position", json{
                        {"x", position.x},
                        {"y", position.y},
                        {"z", position.z},
                    }},
        {"type", snap_mode_to_string(type)},
        {"normal", json{
                       {"x", normal.x},
                       {"y", normal.y},
                       {"z", normal.z},
                   }},
        {"distance", distance},
    };
  }
};

/**
 * SnapSystem: Manages snap points and snap detection
 */
class SnapSystem {
 public:
  static SnapSystem& instance() {
    static SnapSystem system;
    return system;
  }

  // Snap mode management
  void set_snap_mode(SnapMode mode, bool enabled);
  void set_snap_modes(const std::vector<SnapMode>& modes);
  bool is_snap_enabled(SnapMode mode) const;
  std::vector<SnapMode> get_enabled_modes() const;

  // Snap radius
  void set_snap_radius(float pixels) { snap_radius_ = std::max(1.0f, pixels); }
  float get_snap_radius() const { return snap_radius_; }

  // Enable/disable snap
  void enable_snap(bool enabled) { snap_enabled_ = enabled; }
  bool is_snap_enabled() const { return snap_enabled_; }

  // Snap point detection
  std::vector<SnapPoint> detect_snap_points(
      float cursor_x,
      float cursor_y,
      const glm::mat4& view_matrix,
      const glm::mat4& projection_matrix,
      int viewport_width,
      int viewport_height
  );

  // JSON serialization
  json to_json() const;

 private:
  SnapSystem() = default;

  bool snap_enabled_ = true;
  float snap_radius_ = 8.0f;  // pixels
  std::vector<SnapMode> enabled_modes_{
      SnapMode::GRID,
      SnapMode::ENDPOINT,
  };

  // Snap point generation
  std::vector<SnapPoint> generate_grid_snap_points() const;
  std::vector<SnapPoint> generate_axis_snap_points() const;
  std::vector<SnapPoint> generate_endpoint_snap_points() const;
  std::vector<SnapPoint> generate_center_snap_points() const;

  // Utility functions
  glm::vec2 world_to_screen(
      const glm::vec3& position,
      const glm::mat4& view_matrix,
      const glm::mat4& projection_matrix,
      int viewport_width,
      int viewport_height
  ) const;
};

}  // namespace bendcore
